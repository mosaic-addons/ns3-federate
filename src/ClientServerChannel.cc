/*
 * Copyright (c) 2020 Fraunhofer FOKUS and others. All rights reserved.
 *
 * Contact: mosaic@fokus.fraunhofer.de
 *
 * This class is developed for the MOSAIC-NS-3 coupling.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "ClientServerChannel.h"

#include <arpa/inet.h>
#include <google/protobuf/message.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <netdb.h>
#include <errno.h>
#include <iomanip>
#include <poll.h>

#include <ns3/log.h>

NS_LOG_COMPONENT_DEFINE("ClientServerChannel");

namespace std {
    ostream& operator<< ( ostream& out, ClientServerChannelSpace::CMD cmd ) {
        switch ( cmd ) {
            case ClientServerChannelSpace::CMD::CMD_UNDEF: out << "CMD undefined"; break;
            case ClientServerChannelSpace::CMD::CMD_INIT: out << "CMD init"; break;
            case ClientServerChannelSpace::CMD::CMD_SHUT_DOWN: out << "CMD shut down"; break;
            case ClientServerChannelSpace::CMD::CMD_UPDATE_NODE: out << "CMD update node"; break;
            case ClientServerChannelSpace::CMD::CMD_REMOVE_NODE: out << "CMD remove node"; break;
            case ClientServerChannelSpace::CMD::CMD_ADVANCE_TIME: out << "CMD advance time"; break;
            case ClientServerChannelSpace::CMD::CMD_NEXT_EVENT: out << "CMD next event"; break;
            case ClientServerChannelSpace::CMD::CMD_MSG_RECV: out << "CMD message receive"; break;
            case ClientServerChannelSpace::CMD::CMD_MSG_SEND: out << "CMD message send"; break;
            case ClientServerChannelSpace::CMD::CMD_CONF_RADIO: out << "CMD conf radio"; break;
            case ClientServerChannelSpace::CMD::CMD_END: out << "CMD end"; break;
            case ClientServerChannelSpace::CMD::CMD_SUCCESS: out << "CMD success"; break;
        }
        return out;
    }
    ostream& operator<< ( ostream& out, ClientServerChannelSpace::UPDATE_NODE_TYPE type ) {
        switch ( type ) {
            case ClientServerChannelSpace::UPDATE_NODE_TYPE::UPDATE_ADD_RSU: out << "UPDATE add rsu"; break;
            case ClientServerChannelSpace::UPDATE_NODE_TYPE::UPDATE_ADD_VEHICLE: out << "UPDATE add vehicle"; break;
            case ClientServerChannelSpace::UPDATE_NODE_TYPE::UPDATE_MOVE_NODE: out << "UPDATE move node"; break;
            case ClientServerChannelSpace::UPDATE_NODE_TYPE::UPDATE_REMOVE_NODE: out << "UPDATE remove node"; break;
        }
        return out;
    }
    ostream& operator<< ( ostream& out, ClientServerChannelSpace::RADIO_NUMBER num ) {
        switch ( num ) {
            case ClientServerChannelSpace::RADIO_NUMBER::NO_RADIO: out << "RADIO_NUMBER no radio"; break;
            case ClientServerChannelSpace::RADIO_NUMBER::SINGLE_RADIO: out << "RADIO_NUMBER single radio"; break;
            case ClientServerChannelSpace::RADIO_NUMBER::DUAL_RADIO: out << "RADIO_NUMBER dual radio"; break;
        }
        return out;
    }
    ostream& operator<< ( ostream& out, ClientServerChannelSpace::CHANNEL_MODE mode ) {
        switch ( mode ) {
            case ClientServerChannelSpace::CHANNEL_MODE::SINGLE_CHANNEL: out << "CHANNEL_MODE single channel"; break;
            case ClientServerChannelSpace::CHANNEL_MODE::DUAL_CHANNEL: out << "CHANNEL_MODE dual channel"; break;
        }
        return out;
    }
    ostream& operator<< ( ostream& out, ClientServerChannelSpace::RADIO_CHANNEL channel ) {
        switch ( channel ) {
            case ClientServerChannelSpace::RADIO_CHANNEL::SCH1: out << "RADIO_CHANNEL sch1"; break;
            case ClientServerChannelSpace::RADIO_CHANNEL::SCH2: out << "RADIO_CHANNEL sch2"; break;
            case ClientServerChannelSpace::RADIO_CHANNEL::SCH3: out << "RADIO_CHANNEL sch3"; break;
            case ClientServerChannelSpace::RADIO_CHANNEL::SCH4: out << "RADIO_CHANNEL sch4"; break;
            case ClientServerChannelSpace::RADIO_CHANNEL::SCH5: out << "RADIO_CHANNEL sch5"; break;
            case ClientServerChannelSpace::RADIO_CHANNEL::SCH6: out << "RADIO_CHANNEL sch6"; break;
            case ClientServerChannelSpace::RADIO_CHANNEL::CCH: out << "RADIO_CHANNEL cch"; break;
            case ClientServerChannelSpace::RADIO_CHANNEL::UNDEF_CHANNEL: out << "RADIO_CHANNEL undef"; break;
        }
        return out;
    }

} // namespace std

namespace ClientServerChannelSpace {

std::string uint32_to_ip ( const unsigned int ip ) {
    unsigned char bytes[4];
    //bytes = reinterpret_cast < unsigned char[4] > ( ip );
    bytes[0] = ip & 0xFF;
    bytes[1] = (ip >> 8) & 0xFF;
    bytes[2] = (ip >> 16) & 0xFF;
    bytes[3] = (ip >> 24) & 0xFF;
    std::string out = std::to_string ( bytes[3] ) + "." + std::to_string ( bytes[2] )
                                         + "." + std::to_string ( bytes[1] ) + "." + std::to_string ( bytes[0] );
    return out;
}

/**
 * Constructor.
 */
ClientServerChannel::ClientServerChannel() {
    servsock = INVALID_SOCKET;
    sock = INVALID_SOCKET;
}

/**
 * Provides server socket for incoming messages from ns3 Ambassador using given port on host.
 *
 * @param host own hostname (hostaddress)
 * @param port port to listen on for incoming connections
 * @return assigned port number
 */
int ClientServerChannel::prepareConnection ( std::string host, uint32_t port ) {
    in_addr addr;
    struct hostent* host_ent;
    struct in_addr saddr;

    saddr.s_addr = inet_addr ( host.c_str() );
    if ( saddr.s_addr != static_cast < unsigned int > ( -1 ) ) {
        addr = saddr;
    } else if ( ( host_ent = gethostbyname ( host.c_str() ) ) ) {
        addr = *( ( struct in_addr* ) host_ent->h_addr_list[0] );
    } else {
        std::cerr << "Error: ClientServerChannel got invalid host address: " << host.c_str() << std::endl;
        return 0;
    }

    sockaddr_in servaddr;
    memset( (char*)&servaddr, 0, sizeof(servaddr) );
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = addr.s_addr;

    servsock = socket(AF_INET,SOCK_STREAM, 0 );
    if (servsock < 0) {
            std::cerr << "Error: ClientServerChannel could not create socket to connect to Ambassador - " << strerror(errno) << std::endl;
    }

    int reuseYes = 1;
    if ( setsockopt ( servsock, SOL_SOCKET, SO_REUSEADDR, &reuseYes, sizeof(int) ) < 0) {
        std::cerr << "Error: ClientServerChannel could not use SO_REUSEADDR on socket to Ambassador - " << strerror(errno) << std::endl;
    }

    if ( bind ( servsock, (struct sockaddr*) &servaddr, sizeof(servaddr) ) < 0) {
        std::cerr << "Warn: ClientServerChannel could not bind socket to Ambassador - " << strerror(errno) << std::endl;
    }

    listen(servsock, 3);
    int len = sizeof(servaddr);
    getsockname ( servsock, (struct sockaddr*) &servaddr,(socklen_t*) &len);

    return ntohs(servaddr.sin_port);
}

/**
 * Accepts connection to socket (blocking)
 *
 */
void ClientServerChannel::connect(void) {
    sockaddr_in address;
    size_t len = sizeof(address);
    sock = accept ( servsock, (struct sockaddr*) &address, (socklen_t*) &len );

    if (sock < 0) {
        std::cerr << "Error: ClientServerChannel could not accept connection from Ambassador - " << strerror(errno) << std::endl;
    }

    int x = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&x, sizeof(x));
}

/**
 * Closes existing network connections.
 *
 */
ClientServerChannel::~ClientServerChannel() {

    if (sock >= 0) {
        close(sock);
        sock = -1;
    }
    if (servsock >= 0) {
        close(servsock);
        servsock = -1;
    }
}

//#####################################################
//  Public read-methods
//#####################################################

/**
 * Formats buffer as string, each byte is formatted as decimal.
 */
std::string debug_byte_array ( const char* buffer, const size_t buffer_size ) {
    std::stringstream array;
    array << std::dec << "size: " << buffer_size << ", bytes: ";
    if(buffer_size > 16)
        array << std::endl;  // begin multiline print in new line
    for ( size_t i=0; i < buffer_size; i++ ) {
        const char c = buffer[i];
        array << std::dec << static_cast<int>(c);
        array << (((i + 1) % 16 == 0) ? '\n' : ' ');
    }
    array << std::endl;
    return array.str();
}

/**
 * Gets command from NS3 Ambassador to select dedicated action.
 *
 * @return command from Ambassador
 *
 * TODO: return type should be maybe
 */
CMD  ClientServerChannel::readCommand() {
    NS_LOG_FUNCTION(this);
    //Read the mandatory prefixed size
    const std::shared_ptr < uint32_t > message_size = readVarintPrefix ( sock );
    if ( !message_size || *message_size < 0 ) {
        std::cerr << "ERROR: reading of mandatory message size failed!" << std::endl;
        return CMD_UNDEF;
    }
    NS_LOG_LOGIC("read command announced message size: " << *message_size);
    //Allocate a fitting buffer and read message from stream
    char message_buffer[*message_size];
    size_t res = recv ( sock, message_buffer, *message_size, MSG_WAITALL );
    NS_LOG_LOGIC("readCommand recv result: " << res);
    if ( *message_size > 0 && res != *message_size ) {
        std::cerr << "ERROR: expected " << *message_size << " bytes, but red " << res << " bytes. poll ... " << std::endl;
        struct pollfd socks[1];
        socks[0].fd = sock;
        socks[0].events = POLLRDNORM | POLLERR;
        socks[0].revents = 0;
        int poll_res = 0;
        int retries = 3;
        do {
            poll_res = poll(socks, 1, 1000);
            NS_LOG_LOGIC("poll res: " << poll_res);
            retries--;
            if ( retries == 0) { break; }
            sleep(1);
            NS_LOG_LOGIC("poll ...");
        } while ( poll_res < 1 );
        res = recv ( sock, message_buffer, *message_size, MSG_WAITALL );
        if ( retries != 3 && res < 1 ) {
            std::cerr << "ERROR: socket is ready, but cannot receive any bytes (" << res << "). Message sent?" << std::endl;
            return CMD_UNDEF;
        }
    }
    if ( res != *message_size ) {
        std::cerr << "ERROR: reading of message body failed! Socket not ready." << std::endl;
        return CMD_UNDEF;
    }
    if ( *message_size > 0 ) {
        NS_LOG_LOGIC("message buffer as byte array: " << debug_byte_array ( message_buffer, *message_size ));
        //Create the streams that can parse the received data into the protobuf class
        google::protobuf::io::ArrayInputStream arrayIn ( message_buffer, *message_size );
        google::protobuf::io::CodedInputStream codedIn ( &arrayIn );

        CommandMessage commandMessage;
        commandMessage.ParseFromCodedStream(&codedIn);  //parse message
        //pick the needed data from the protobuf message class and return it
        const CMD cmd = protoCMDToCMD(commandMessage.command_type());
        NS_LOG_INFO("read command: " << cmd);
        return cmd;
    }
    return CMD_UNDEF;
}

/**
 * Reads an Init-Message from the Channel
 *
 * @param return_value the struct to fill the data in
 * @return 0 if successful
 */
int ClientServerChannel::readInit ( CSC_init_return &return_value ) {
    NS_LOG_FUNCTION(this);
    const std::shared_ptr < uint32_t > message_size = readVarintPrefix(sock);
    if ( !message_size ) { return -1; }
    NS_LOG_LOGIC("read init announced message size: " << *message_size);
    char message_buffer[*message_size];
    const size_t count = recv ( sock, message_buffer, *message_size, MSG_WAITALL );
    NS_LOG_LOGIC("read init received message size: " << count);

    google::protobuf::io::ArrayInputStream arrayIn ( message_buffer, *message_size );
    google::protobuf::io::CodedInputStream codedIn ( &arrayIn);

    InitMessage init_message;
    init_message.ParseFromCodedStream ( &codedIn );

    return_value.start_time = init_message.start_time();
    return_value.end_time = init_message.end_time();

    NS_LOG_INFO("read init start time: " << return_value.start_time);
    NS_LOG_INFO("read init end time: " << return_value.end_time);

    return 0;
}

/**
 * Reads an update Node message from the channel.
 *
 * @param return_value the struct to fill the data in
 * @return 0 if successful
 */
int ClientServerChannel::readUpdateNode ( CSC_update_node_return &return_value ) {
    NS_LOG_FUNCTION(this);
    const std::shared_ptr < uint32_t > message_size = readVarintPrefix ( sock );
    if ( !message_size ) { return -1; }
    NS_LOG_LOGIC("read update note announced message size: " << *message_size);
    if ( *message_size < 0 ) {
        return 0;
    }

    char message_buffer[*message_size];
    const size_t count = recv ( sock, message_buffer, *message_size, MSG_WAITALL );
    NS_LOG_LOGIC("read update node received message size: " << count);

    if ( *message_size != count ) {
        std::cerr << "ERROR: expected " << *message_size << " bytes, but red " << count << " bytes!" << std::endl;
        return -1;
    }

    google::protobuf::io::ArrayInputStream arrayIn ( message_buffer, *message_size );
    google::protobuf::io::CodedInputStream codedIn ( &arrayIn );

    UpdateNode update_message;
    update_message.ParseFromCodedStream ( &codedIn );  //Parse message

    switch ( update_message.update_type() ) { //Convert the types from protobuf enum to our update message types
        case UpdateNode_UpdateType_ADD_RSU: return_value.type = UPDATE_ADD_RSU; break;
        case UpdateNode_UpdateType_ADD_VEHICLE: return_value.type = UPDATE_ADD_VEHICLE; break;
        case UpdateNode_UpdateType_MOVE_NODE: return_value.type = UPDATE_MOVE_NODE; break;
        case UpdateNode_UpdateType_REMOVE_NODE: return_value.type = UPDATE_REMOVE_NODE; break;
        default:
            std::cerr << "ERROR: update type unknown: " << update_message.update_type() << std::endl;
            return_value.type = (UPDATE_NODE_TYPE)0; return 1;     //1 signals an error
    }
    NS_LOG_INFO("read update message update type " << return_value.type);

    return_value.time = update_message.time();
    NS_LOG_INFO("read update message update time " << return_value.time);

    for ( size_t i = 0; i < update_message.properties_size(); i++ ) { //fill the update messages into our struct
        UpdateNode_NodeData node_data = update_message.properties(i);
        CSC_node_data returned_node_data;

        returned_node_data.id = node_data.id();
        returned_node_data.x = node_data.x();
        returned_node_data.y = node_data.y();

        NS_LOG_INFO("read update message update node index=" << i
                                << " id=" << returned_node_data.id
                                << " x=" << returned_node_data.x
                                << " y=" << returned_node_data.y);

        return_value.properties.push_back(returned_node_data);
    }

    return 0;
}

/**
 * Reads a Time-Message from the channel
 *
 * @return the read time as an int64_t
 */
int64_t ClientServerChannel::readTimeMessage() {
    NS_LOG_FUNCTION(this);
    const std::shared_ptr < uint32_t > message_size = readVarintPrefix(sock);
    if ( !message_size ) { return -1; }
    NS_LOG_LOGIC("read time announced message size: " << *message_size);

    char message_buffer[*message_size];
    const size_t count = recv ( sock, message_buffer, *message_size, MSG_WAITALL );
    NS_LOG_LOGIC("read time received message size: " << count);

    google::protobuf::io::ArrayInputStream arrayIn ( message_buffer, *message_size );
    google::protobuf::io::CodedInputStream codedIn ( &arrayIn );

    TimeMessage time_message;
    time_message.ParseFromCodedStream ( &codedIn );

    int64_t time = time_message.time();
    NS_LOG_INFO("read time message: " << time);
    return time;
}

/**
 * Reads a configuration message from the command channel and returns it
 *
 * @param return_value the struct to fill the data in
 * @return 0 if successful
 */
int ClientServerChannel::readConfigurationMessage(CSC_config_message &return_value) {
    NS_LOG_FUNCTION(this);
    const std::shared_ptr < uint32_t > message_size = readVarintPrefix ( sock );
    if ( !message_size ) { return -1; }
    NS_LOG_LOGIC("read config announced message size: " << *message_size);

    char message_buffer[*message_size];
    const size_t count = recv ( sock, message_buffer, *message_size, MSG_WAITALL );
    NS_LOG_LOGIC("read config received message size: " << count);

    google::protobuf::io::ArrayInputStream arrayIn ( message_buffer, *message_size );
    google::protobuf::io::CodedInputStream codedIn ( &arrayIn );

    ConfigureRadioMessage conf_message;
    conf_message.ParseFromCodedStream ( &codedIn );

    return_value.time = conf_message.time();
    return_value.msg_id = conf_message.message_id();
    return_value.node_id = conf_message.external_id();

    NS_LOG_INFO("read config message time: " << return_value.time);
    NS_LOG_INFO("read config message msg id: " << return_value.msg_id);
    NS_LOG_INFO("read config message node id: " << return_value.node_id);

    if ( conf_message.radio_number() == ConfigureRadioMessage_RadioNumber_SINGLE_RADIO ) {
        return_value.num_radios = SINGLE_RADIO;
    } else if ( conf_message.radio_number() == ConfigureRadioMessage_RadioNumber_DUAL_RADIO ) {
        return_value.num_radios = DUAL_RADIO;
    } else if ( conf_message.radio_number() == ConfigureRadioMessage_RadioNumber_NO_RADIO ) {
        return_value.num_radios = NO_RADIO;
    }
    NS_LOG_INFO("read config message num_radios: " << return_value.num_radios);

    if ( return_value.num_radios == SINGLE_RADIO || return_value.num_radios == DUAL_RADIO ) {
        return_value.primary_radio.turnedOn = conf_message.primary_radio_configuration().receiving_messages();
        return_value.primary_radio.ip_address = conf_message.primary_radio_configuration().ip_address();
        return_value.primary_radio.subnet = conf_message.primary_radio_configuration().subnet_address();
        return_value.primary_radio.tx_power = conf_message.primary_radio_configuration().transmission_power();
        return_value.primary_radio.primary_channel
            = protoChannelToChannel ( conf_message.primary_radio_configuration().primary_radio_channel() );
        NS_LOG_INFO("read config message primary radio turned on: "
            << std::boolalpha << return_value.primary_radio.turnedOn);
        NS_LOG_INFO("read config message primary radio ip address: "
            << uint32_to_ip ( return_value.primary_radio.ip_address ));
        NS_LOG_INFO("read config message primary radio subnet: "
            << uint32_to_ip ( return_value.primary_radio.subnet ));
        NS_LOG_INFO("read config message primary radio tx_power: "
            << return_value.primary_radio.tx_power);
        NS_LOG_INFO("read config message primary radio primary channel: "
            << return_value.primary_radio.primary_channel);

        if ( conf_message.primary_radio_configuration().radio_mode()
                    == ConfigureRadioMessage_RadioConfiguration_RadioMode_SINGLE_CHANNEL ) {
            return_value.primary_radio.channelmode = SINGLE_CHANNEL;
        } else if ( conf_message.primary_radio_configuration().radio_mode()
                                == ConfigureRadioMessage_RadioConfiguration_RadioMode_DUAL_CHANNEL) {
            return_value.primary_radio.channelmode = DUAL_CHANNEL;
            return_value.primary_radio.secondary_channel
                = protoChannelToChannel ( conf_message.primary_radio_configuration().secondary_radio_channel() );
        }
        NS_LOG_INFO("read config message primary radio channel mode: "
            << return_value.primary_radio.channelmode);
        if ( conf_message.primary_radio_configuration().radio_mode()
                    == ConfigureRadioMessage_RadioConfiguration_RadioMode_DUAL_CHANNEL) {
            NS_LOG_INFO("read config message primary radio secondary channel: "
                << return_value.primary_radio.secondary_channel);
        }
    }

    if(return_value.num_radios == DUAL_RADIO) {
        return_value.secondary_radio.turnedOn = conf_message.secondary_radio_configuration().receiving_messages();
        return_value.secondary_radio.ip_address = conf_message.secondary_radio_configuration().ip_address();
        return_value.secondary_radio.subnet = conf_message.secondary_radio_configuration().subnet_address();
        return_value.secondary_radio.tx_power = conf_message.secondary_radio_configuration().transmission_power();
        return_value.secondary_radio.primary_channel
            = protoChannelToChannel ( conf_message.secondary_radio_configuration().primary_radio_channel() );
        NS_LOG_INFO("read config message secondary radio turned on: "
            << std::boolalpha <<  return_value.secondary_radio.turnedOn);
        NS_LOG_INFO("read config message secondary radio ip address: "
            << uint32_to_ip ( return_value.secondary_radio.ip_address ));
        NS_LOG_INFO("read config message secondary radio subnet: "
            << uint32_to_ip ( return_value.secondary_radio.subnet ));
        NS_LOG_INFO("read config message secondary radio tx_power: "
            << return_value.secondary_radio.tx_power);
        NS_LOG_INFO("read config message secondary radio primary channel: "
            << return_value.secondary_radio.primary_channel);

        if ( conf_message.secondary_radio_configuration().radio_mode()
                    == ConfigureRadioMessage_RadioConfiguration_RadioMode_SINGLE_CHANNEL) {
            return_value.secondary_radio.channelmode = SINGLE_CHANNEL;
        } else if ( conf_message.secondary_radio_configuration().radio_mode()
                                    == ConfigureRadioMessage_RadioConfiguration_RadioMode_DUAL_CHANNEL) {
            return_value.secondary_radio.channelmode = DUAL_CHANNEL;
            return_value.secondary_radio.secondary_channel
                = protoChannelToChannel(conf_message.secondary_radio_configuration().secondary_radio_channel());
        }
        NS_LOG_INFO("read config message secondary radio channel mode: "
            << return_value.secondary_radio.channelmode);
        if ( conf_message.primary_radio_configuration().radio_mode()
                    == ConfigureRadioMessage_RadioConfiguration_RadioMode_DUAL_CHANNEL) {
            NS_LOG_INFO("read config message secondary radio secondary channel: "
                << return_value.secondary_radio.secondary_channel);
        }
    }
    writeCommand(CMD_SUCCESS);

    return 0;
}

/**
 * Reads a sendMessage body from the channel
 *
 * @param return_value the struct to fill the data in
 * @return 0 if successful
 */
int ClientServerChannel::readSendMessage ( CSC_send_message &return_value ) {
    NS_LOG_FUNCTION(this);
    std::shared_ptr < uint32_t > message_size = readVarintPrefix ( sock );
    if ( !message_size ) { return -1; }
    NS_LOG_LOGIC("read send announced message size: " << *message_size);

    char message_buffer[*message_size];
    const size_t count = recv ( sock, message_buffer, *message_size, MSG_WAITALL );
    NS_LOG_LOGIC("read send received message size: " << count);

    google::protobuf::io::ArrayInputStream arrayIn ( message_buffer, *message_size );
    google::protobuf::io::CodedInputStream codedIn ( &arrayIn );

    SendMessageMessage send_message;
    send_message.ParseFromCodedStream(&codedIn);

    return_value.time = send_message.time();
    return_value.node_id = send_message.node_id();

    NS_LOG_INFO("read send message time: " << return_value.time);
    NS_LOG_INFO("read send message node id: " << return_value.node_id);

    return_value.channel_id = protoChannelToChannel(send_message.channel_id());
    return_value.message_id = send_message.message_id();
    return_value.length = send_message.length();

    NS_LOG_INFO("read send message channel id: " << return_value.channel_id);
    NS_LOG_INFO("read send message message id: " << return_value.message_id);
    NS_LOG_INFO("read send message length: " << return_value.length);

    if (send_message.has_topo_address() ) {
        return_value.topo_address.ip_address = send_message.topo_address().ip_address();
        return_value.topo_address.ttl = send_message.topo_address().ttl();
        NS_LOG_INFO("read send message topo address ip: " << return_value.topo_address.ip_address);
        NS_LOG_INFO("read send message topo address ttl: " << return_value.topo_address.ttl);
    } else if (send_message.has_rectangle_address() ) {  //Not yet implemented
        return_value.topo_address.ip_address = send_message.rectangle_address().ip_address();
        return_value.topo_address.ttl = 10;
        NS_LOG_INFO("read send message topo address ip: " << return_value.topo_address.ip_address);
        NS_LOG_INFO("read send message topo address ttl: " << return_value.topo_address.ttl);
    } else if (send_message.has_circle_address() ) {  //Not yet implemented
        return_value.topo_address.ip_address = send_message.circle_address().ip_address();
        return_value.topo_address.ttl = 10;
        NS_LOG_INFO("read send message topo address ip: " << return_value.topo_address.ip_address);
        NS_LOG_INFO("read send message topo address ttl: " << return_value.topo_address.ttl);
    }
    writeCommand(CMD_SUCCESS);

    return 0;
}

//#####################################################
//  Public write-methods
//#####################################################

/**
 * Sends own control commands to ambassador
 * Such control commands must be written onto the channel before every data body
 *
 * @param cmd command to be written to ambassador
 */
void ClientServerChannel::writeCommand(CMD cmd) {
    NS_LOG_FUNCTION(this << cmd);
    CommandMessage commandMessage;
    commandMessage.set_command_type(cmdToProtoCMD(cmd));
    int varintsize = google::protobuf::io::CodedOutputStream::VarintSize32(commandMessage.ByteSizeLong());
    NS_LOG_LOGIC("write command varint size: " << varintsize);
    int buffer_size = varintsize+commandMessage.ByteSizeLong();
    NS_LOG_LOGIC("write command buffer size: " << buffer_size);
    char message_buffer[buffer_size];

    google::protobuf::io::ArrayOutputStream arrayOut ( message_buffer, buffer_size );
    google::protobuf::io::CodedOutputStream codedOut ( &arrayOut );

    codedOut.WriteVarint32(commandMessage.ByteSizeLong());
    commandMessage.SerializeToCodedStream(&codedOut);
    const size_t count = send ( sock, message_buffer, buffer_size, 0 );
    NS_LOG_LOGIC("write command send bytes: " << count);
}

/**
 * Writes a receiveMessage body onto the channel.
 *
 * @param time the simulation time at which the message receive occured
 * @param node_id the id of the receiving node
 * @param message_id the id of the received message
 * @param channel the receiver channel
 * @param rssi the rssi during the receive event
 */
void ClientServerChannel::writeReceiveMessage(uint64_t time, int node_id, int message_id, RADIO_CHANNEL channel, int rssi) {
    NS_LOG_FUNCTION(this << time << node_id << message_id << channel << rssi);
    ReceiveMessage receive_message;
    receive_message.set_time(time);
    receive_message.set_node_id(node_id);
    receive_message.set_message_id(message_id);
    receive_message.set_channel_id(channelToProtoChannel(channel));
    receive_message.set_rssi(rssi);
    int varintsize = google::protobuf::io::CodedOutputStream::VarintSize32(receive_message.ByteSizeLong());
    NS_LOG_LOGIC("write receive message varint size: " << varintsize);
    int buffer_size = varintsize+receive_message.ByteSizeLong();
    NS_LOG_LOGIC("write receive message buffer size: " << buffer_size);
    char message_buffer[buffer_size];

    google::protobuf::io::ArrayOutputStream arrayOut ( message_buffer, buffer_size );
    google::protobuf::io::CodedOutputStream codedOut ( &arrayOut );

    codedOut.WriteVarint32 ( receive_message.ByteSizeLong() );
    receive_message.SerializeToCodedStream ( &codedOut );
    const size_t count = send ( sock, message_buffer, buffer_size, 0 );
    NS_LOG_LOGIC("write receive message send bytes: " << count);
}

/**
 * Writes a time onto the channel
 *
 * @param time the time to write
 */
void ClientServerChannel::writeTimeMessage(int64_t time) {
    NS_LOG_FUNCTION(this << time);
    TimeMessage time_message;
    time_message.set_time ( time );
    int varintsize = google::protobuf::io::CodedOutputStream::VarintSize32 ( time_message.ByteSizeLong() );
    NS_LOG_LOGIC("write time message varint size: " << varintsize);
    int buffer_size = varintsize+time_message.ByteSizeLong();
    NS_LOG_LOGIC("write time message buffer size: " << buffer_size);
    char message_buffer[buffer_size];

    google::protobuf::io::ArrayOutputStream arrayOut ( message_buffer, buffer_size );
    google::protobuf::io::CodedOutputStream codedOut ( &arrayOut);

    codedOut.WriteVarint32 ( time_message.ByteSizeLong() );
    time_message.SerializeToCodedStream ( &codedOut );
    const size_t count = send ( sock, message_buffer, buffer_size, 0 );
    NS_LOG_LOGIC("write time message send bytes: " << count);
}

/**
 * Sends port to ambassador.
 *
 * @param port port
 */
void ClientServerChannel::writePort(uint32_t port) {
    NS_LOG_FUNCTION(this << port);
    PortExchange port_exchange;
    port_exchange.set_port_number ( port );
    NS_LOG_LOGIC("write port exchange: " << port_exchange.port_number());
    int varintsize = google::protobuf::io::CodedOutputStream::VarintSize32(port_exchange.ByteSizeLong());
    NS_LOG_LOGIC("write port message varint size: " << varintsize);
    int buffer_size = varintsize+port_exchange.ByteSizeLong();
    NS_LOG_LOGIC("write port message buffer size: " << buffer_size);
    char message_buffer[buffer_size];

    google::protobuf::io::ArrayOutputStream arrayOut ( message_buffer, buffer_size );
    google::protobuf::io::CodedOutputStream codedOut ( &arrayOut);

    codedOut.WriteVarint32(port_exchange.ByteSizeLong());
    port_exchange.SerializeToCodedStream(&codedOut);
    const size_t count = send ( sock, message_buffer, buffer_size, 0 );
    NS_LOG_LOGIC("write port message send bytes: " << count);
}

//#####################################################
//  Private helpers
//#####################################################

/**
 * @brief Reads a variable length integer from the channel
 *
 * Protobuf messages are not self delimiting and have thus to be prefixed with the length of the message.
 * When sent from Java, before every message there will be a variable length integer sent.
 * This method reads such an integer of variable length
 *
 */
std::shared_ptr < uint32_t > ClientServerChannel::readVarintPrefix(SOCKET sock) {
    NS_LOG_FUNCTION(this << sock);
    int num_bytes=0;
    char current_byte;

    //first receive one byte from the channel
    const size_t count = recv ( sock, &current_byte, 1, 0 );

    num_bytes++;
    if(count<0) {   //If we could not read one byte, return error
        return std::shared_ptr < uint32_t> ();
    }
    int return_value = ( current_byte & 0x7f );   //We get effectively 7 bits per byte
    while ( current_byte & 0x80 ) { //as long as the msb is set, there comes another byte
        current_byte = 0;
        const size_t count = recv ( sock, &current_byte, 1, 0 );  //receive another byte
        num_bytes++;
        if ( count < 0 || num_bytes > 4) {          //If we have too many bytes or reading failed return error
            return std::shared_ptr < uint32_t>();
        }
        return_value |= ( current_byte & 0x7F ) << ( 7 * (num_bytes - 1 ) );    //Add the next 7 bits
        }
    NS_LOG_LOGIC("readVarintPrefix return value: " << return_value);
    return std::make_shared < uint32_t > ( return_value );
}

CommandMessage_CommandType ClientServerChannel::cmdToProtoCMD(CMD cmd) {
    switch(cmd) {
        case CMD_UNDEF: return CommandMessage_CommandType_UNDEF;
        case CMD_SUCCESS: return CommandMessage_CommandType_SUCCESS;
        case CMD_INIT: return CommandMessage_CommandType_INIT;
        case CMD_SHUT_DOWN: return CommandMessage_CommandType_SHUT_DOWN;

        case CMD_UPDATE_NODE: return CommandMessage_CommandType_UPDATE_NODE;
        case CMD_REMOVE_NODE: return CommandMessage_CommandType_REMOVE_NODE;

        case CMD_ADVANCE_TIME: return CommandMessage_CommandType_ADVANCE_TIME;
        case CMD_NEXT_EVENT: return CommandMessage_CommandType_NEXT_EVENT;
        case CMD_MSG_RECV: return CommandMessage_CommandType_MSG_RECV;

        case CMD_MSG_SEND: return CommandMessage_CommandType_MSG_SEND;
        case CMD_CONF_RADIO: return CommandMessage_CommandType_CONF_RADIO;

        case CMD_END: return CommandMessage_CommandType_END;

        default: return CommandMessage_CommandType_UNDEF;
    }
}

CMD ClientServerChannel::protoCMDToCMD(CommandMessage_CommandType cmd) {
    switch(cmd) {
        case CommandMessage_CommandType_UNDEF: return CMD_UNDEF;
        case CommandMessage_CommandType_SUCCESS: return CMD_SUCCESS;
        case CommandMessage_CommandType_INIT: return CMD_INIT;
        case CommandMessage_CommandType_SHUT_DOWN: return CMD_SHUT_DOWN;

        case CommandMessage_CommandType_UPDATE_NODE: return CMD_UPDATE_NODE;
        case CommandMessage_CommandType_REMOVE_NODE: return CMD_REMOVE_NODE;

        case CommandMessage_CommandType_ADVANCE_TIME: return CMD_ADVANCE_TIME;
        case CommandMessage_CommandType_NEXT_EVENT: return CMD_NEXT_EVENT;
        case CommandMessage_CommandType_MSG_RECV: return CMD_MSG_RECV;

        case CommandMessage_CommandType_MSG_SEND: return CMD_MSG_SEND;
        case CommandMessage_CommandType_CONF_RADIO: return CMD_CONF_RADIO;

        case CommandMessage_CommandType_END: return  CMD_END;

        default: return CMD_UNDEF;
    }
}

RADIO_CHANNEL ClientServerChannel::protoChannelToChannel(RadioChannel protoChannel) {
    switch(protoChannel) {
        case PROTO_SCH1: return SCH1;
        case PROTO_SCH2: return SCH2;
        case PROTO_SCH3: return SCH3;
        case PROTO_CCH: return CCH;
        case PROTO_SCH4: return SCH4;
        case PROTO_SCH5: return SCH5;
        case PROTO_SCH6: return SCH6;
        default: return UNDEF_CHANNEL;
    }
}

RadioChannel ClientServerChannel::channelToProtoChannel(RADIO_CHANNEL channel) {
    switch(channel) {
        case SCH1: return PROTO_SCH1;
        case SCH2: return PROTO_SCH2;
        case SCH3: return PROTO_SCH3;
        case CCH: return PROTO_CCH;
        case SCH4: return PROTO_SCH4;
        case SCH5: return PROTO_SCH5;
        case SCH6: return PROTO_SCH6;
        default: return PROTO_UNDEF;
    }
}
}//END NAMESPACE
