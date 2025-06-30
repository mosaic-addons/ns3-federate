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
    ostream& operator<< ( ostream& out, ClientServerChannelSpace::CommandMessage_CommandType cmd ) {
        switch ( cmd ) {
            case ClientServerChannelSpace::CommandMessage_CommandType::CommandMessage_CommandType_UNDEF: out << "CommandType_UNDEF"; break;
            case ClientServerChannelSpace::CommandMessage_CommandType::CommandMessage_CommandType_INIT: out << "CommandType_INIT"; break;
            case ClientServerChannelSpace::CommandMessage_CommandType::CommandMessage_CommandType_SHUT_DOWN: out << "CommandType_SHUT_DOWN"; break;
            case ClientServerChannelSpace::CommandMessage_CommandType::CommandMessage_CommandType_SUCCESS: out << "CommandType_SUCCESS"; break;
            case ClientServerChannelSpace::CommandMessage_CommandType::CommandMessage_CommandType_NEXT_EVENT: out << "CommandType_NEXT_EVENT"; break;
            case ClientServerChannelSpace::CommandMessage_CommandType::CommandMessage_CommandType_ADVANCE_TIME: out << "CommandType_ADVANCE_TIME"; break;
            case ClientServerChannelSpace::CommandMessage_CommandType::CommandMessage_CommandType_END: out << "CommandType_END"; break;
            case ClientServerChannelSpace::CommandMessage_CommandType::CommandMessage_CommandType_ADD_NODE: out << "CommandType_ADD_NODE"; break;
            case ClientServerChannelSpace::CommandMessage_CommandType::CommandMessage_CommandType_UPDATE_NODE: out << "CommandType_UPDATE_NODE"; break;
            case ClientServerChannelSpace::CommandMessage_CommandType::CommandMessage_CommandType_REMOVE_NODE: out << "CommandType_REMOVE_NODE"; break;
            case ClientServerChannelSpace::CommandMessage_CommandType::CommandMessage_CommandType_CONF_WIFI_RADIO: out << "CommandType_CONF_WIFI_RADIO"; break;
            case ClientServerChannelSpace::CommandMessage_CommandType::CommandMessage_CommandType_SEND_WIFI_MSG: out << "CommandType_SEND_WIFI_MSG"; break;
            case ClientServerChannelSpace::CommandMessage_CommandType::CommandMessage_CommandType_RECV_WIFI_MSG: out << "CommandType_RECV_WIFI_MSG"; break;
            case ClientServerChannelSpace::CommandMessage_CommandType::CommandMessage_CommandType_CONF_CELL_RADIO: out << "CommandType_CONF_CELL_RADIO"; break;
            case ClientServerChannelSpace::CommandMessage_CommandType::CommandMessage_CommandType_SEND_CELL_MSG: out << "CommandType_SEND_CELL_MSG"; break;
            case ClientServerChannelSpace::CommandMessage_CommandType::CommandMessage_CommandType_RECV_CELL_MSG: out << "CommandType_RECV_CELL_MSG"; break;
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

ClientServerChannel::ClientServerChannel() {
    servsock = INVALID_SOCKET;
    sock = INVALID_SOCKET;
}

int ClientServerChannel::prepareConnection ( std::string host, uint32_t port ) {
    NS_LOG_FUNCTION(this << host.c_str() << port);

    in_addr addr;
    struct hostent* host_ent;
    struct in_addr saddr;

    // assemble saddr
    saddr.s_addr = inet_addr ( host.c_str() );
    if ( saddr.s_addr != static_cast < unsigned int > ( -1 ) ) {
        addr = saddr;
    } else if ( ( host_ent = gethostbyname ( host.c_str() ) ) ) {
        addr = *( ( struct in_addr* ) host_ent->h_addr_list[0] );
    } else {
        std::cerr << "Error: ClientServerChannel got invalid host address: " << host.c_str() << std::endl;
        exit(1);
    }

    // assemble servaddr
    sockaddr_in servaddr;
    memset( (char*)&servaddr, 0, sizeof(servaddr) );
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = addr.s_addr;
    NS_LOG_DEBUG("servaddr: " << uint32_to_ip(servaddr.sin_addr.s_addr) << ":" << ntohs(servaddr.sin_port));

    // create socket
    servsock = socket(AF_INET, SOCK_STREAM, 0);
    if (servsock < 0) {
        std::cerr << "Error: ClientServerChannel could not create socket to connect - " << strerror(errno) << std::endl;
        exit(1);
    }
    NS_LOG_DEBUG("servsock=" << servsock);

    // set socket options
    int reuseYes = 1;
    if ( setsockopt ( servsock, SOL_SOCKET, SO_REUSEADDR, &reuseYes, sizeof(int) ) < 0) {
        std::cerr << "Error: ClientServerChannel could not use SO_REUSEADDR on socket - " << strerror(errno) << std::endl;
        exit(1);
    }

    // bind
    if ( bind ( servsock, (struct sockaddr*) &servaddr, sizeof(servaddr) ) < 0) {
        std::cerr << "Error: ClientServerChannel could not bind socket - " << strerror(errno) << std::endl;
        exit(1);
    }

    // listen
    listen(servsock, 3);

    // get assigned_port
    int len = sizeof(servaddr);
    getsockname ( servsock, (struct sockaddr*) &servaddr,(socklen_t*) &len);
    int assigned_port = ntohs(servaddr.sin_port);
    NS_LOG_DEBUG("assigned_port=" << assigned_port);

    return assigned_port;
}

void ClientServerChannel::connect(void) {
    NS_LOG_FUNCTION(this);
    sockaddr_in clientaddr;
    size_t len = sizeof(clientaddr);
    sock = accept ( servsock, (struct sockaddr*) &clientaddr, (socklen_t*) &len ); 

    if (sock < 0) {
        std::cerr << "Error: ClientServerChannel could not accept connection from Ambassador - " << strerror(errno) << std::endl;
    }
    NS_LOG_DEBUG("sock=" << sock);
    NS_LOG_DEBUG("clientaddr: " << uint32_to_ip(clientaddr.sin_addr.s_addr) << ":" << ntohs(clientaddr.sin_port));

    int x = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&x, sizeof(x));
}

ClientServerChannel::~ClientServerChannel() {
    if (sock >= 0) {
        close(sock);
        sock = INVALID_SOCKET;
    }
    if (servsock >= 0) {
        close(servsock);
        servsock = INVALID_SOCKET;
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
CommandMessage_CommandType ClientServerChannel::readCommand() {
    NS_LOG_FUNCTION(this);
    //Read the mandatory prefixed size
    const std::shared_ptr < uint32_t > message_size = readVarintPrefix ( sock );
    if ( !message_size || *message_size < 0 ) {
        std::cerr << "ERROR: reading of mandatory message size failed!" << std::endl;
        return CommandMessage_CommandType_UNDEF;
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
            return CommandMessage_CommandType_UNDEF;
        }
    }
    if ( res != *message_size ) {
        std::cerr << "ERROR: reading of message body failed! Socket not ready." << std::endl;
        return CommandMessage_CommandType_UNDEF;
    }
    if ( *message_size > 0 ) {
        NS_LOG_LOGIC("message buffer as byte array: " << debug_byte_array ( message_buffer, *message_size ));
        //Create the streams that can parse the received data into the protobuf class
        google::protobuf::io::ArrayInputStream arrayIn ( message_buffer, *message_size );
        google::protobuf::io::CodedInputStream codedIn ( &arrayIn );

        CommandMessage commandMessage;
        commandMessage.ParseFromCodedStream(&codedIn);  //parse message
        //pick the needed data from the protobuf message class and return it
        const CommandMessage_CommandType cmd = commandMessage.command_type();
        NS_LOG_INFO("read command: " << cmd);
        return cmd;
    }
    return CommandMessage_CommandType_UNDEF;
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

AddNode ClientServerChannel::readAddNode(void) {
    NS_LOG_FUNCTION(this);
    const std::shared_ptr < uint32_t > message_size = readVarintPrefix(sock);
    if (!message_size) { 
        NS_LOG_ERROR("Cannot access message size");
        exit(1);
    }
    if (*message_size < 0) {
        NS_LOG_ERROR("Message size smaller zero");
        exit(1);
    }
    char message_buffer[*message_size];
    const size_t count = recv(sock, message_buffer, *message_size, MSG_WAITALL);
    if (*message_size != count) {
        NS_LOG_ERROR("Expected " << *message_size << " bytes, but read " << count << " bytes");
        exit(1);
    }
    google::protobuf::io::ArrayInputStream arrayIn ( message_buffer, *message_size );
    google::protobuf::io::CodedInputStream codedIn ( &arrayIn );
    // TODO: code until here is duplicated in the following functions. dedup!

    AddNode msg;
    msg.ParseFromCodedStream (&codedIn);
    return msg;
}

UpdateNode ClientServerChannel::readUpdateNode(void) {
    NS_LOG_FUNCTION(this);
    const std::shared_ptr < uint32_t > message_size = readVarintPrefix(sock);
    if (!message_size) { 
        NS_LOG_ERROR("Cannot access message size");
        exit(1);
    }
    if (*message_size < 0) {
        NS_LOG_ERROR("Message size smaller zero");
        exit(1);
    }
    char message_buffer[*message_size];
    const size_t count = recv(sock, message_buffer, *message_size, MSG_WAITALL);
    if (*message_size != count) {
        NS_LOG_ERROR("Expected " << *message_size << " bytes, but read " << count << " bytes");
        exit(1);
    }
    google::protobuf::io::ArrayInputStream arrayIn ( message_buffer, *message_size );
    google::protobuf::io::CodedInputStream codedIn ( &arrayIn );

    UpdateNode msg;
    msg.ParseFromCodedStream (&codedIn);
    return msg;
}

RemoveNode ClientServerChannel::readRemoveNode(void) {
    NS_LOG_FUNCTION(this);
    const std::shared_ptr < uint32_t > message_size = readVarintPrefix(sock);
    if (!message_size) { 
        NS_LOG_ERROR("Cannot access message size");
        exit(1);
    }
    if (*message_size < 0) {
        NS_LOG_ERROR("Message size smaller zero");
        exit(1);
    }
    char message_buffer[*message_size];
    const size_t count = recv(sock, message_buffer, *message_size, MSG_WAITALL);
    if (*message_size != count) {
        NS_LOG_ERROR("Expected " << *message_size << " bytes, but read " << count << " bytes");
        exit(1);
    }
    google::protobuf::io::ArrayInputStream arrayIn ( message_buffer, *message_size );
    google::protobuf::io::CodedInputStream codedIn ( &arrayIn );

    RemoveNode msg;
    msg.ParseFromCodedStream (&codedIn);
    return msg;
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

ConfigureWifiRadio ClientServerChannel::readConfigureWifiRadio(void) {
    NS_LOG_FUNCTION(this);
    const std::shared_ptr < uint32_t > message_size = readVarintPrefix(sock);
    if (!message_size) { 
        NS_LOG_ERROR("Cannot access message size");
        exit(1);
    }
    if (*message_size < 0) {
        NS_LOG_ERROR("Message size smaller zero");
        exit(1);
    }
    char message_buffer[*message_size];
    const size_t count = recv(sock, message_buffer, *message_size, MSG_WAITALL);
    if (*message_size != count) {
        NS_LOG_ERROR("Expected " << *message_size << " bytes, but read " << count << " bytes");
        exit(1);
    }
    google::protobuf::io::ArrayInputStream arrayIn ( message_buffer, *message_size );
    google::protobuf::io::CodedInputStream codedIn ( &arrayIn );

    ConfigureWifiRadio message;
    message.ParseFromCodedStream ( &codedIn );
    return message;
}

SendWifiMessage ClientServerChannel::readSendWifiMessage(void) {
    NS_LOG_FUNCTION(this);
    const std::shared_ptr < uint32_t > message_size = readVarintPrefix(sock);
    if (!message_size) { 
        NS_LOG_ERROR("Cannot access message size");
        exit(1);
    }
    if (*message_size < 0) {
        NS_LOG_ERROR("Message size smaller zero");
        exit(1);
    }
    char message_buffer[*message_size];
    const size_t count = recv(sock, message_buffer, *message_size, MSG_WAITALL);
    if (*message_size != count) {
        NS_LOG_ERROR("Expected " << *message_size << " bytes, but read " << count << " bytes");
        exit(1);
    }
    google::protobuf::io::ArrayInputStream arrayIn ( message_buffer, *message_size );
    google::protobuf::io::CodedInputStream codedIn ( &arrayIn );

    SendWifiMessage message;
    message.ParseFromCodedStream(&codedIn);

    if (message.has_topo_address() ) {
        // all good
    } else if (message.has_rectangle_address() ) {
        NS_LOG_ERROR("Not yet implemented.");
        exit(1);
    } else if (message.has_circle_address() ) {
        NS_LOG_ERROR("Not yet implemented.");
        exit(1);
    } else {
        NS_LOG_ERROR("Address is missing.");
        exit(1);
    }

    return message;
}

ConfigureCellRadio ClientServerChannel::readConfigureCellRadio(void) {
    NS_LOG_FUNCTION(this);
    const std::shared_ptr < uint32_t > message_size = readVarintPrefix(sock);
    if (!message_size) { 
        NS_LOG_ERROR("Cannot access message size");
        exit(1);
    }
    if (*message_size < 0) {
        NS_LOG_ERROR("Message size smaller zero");
        exit(1);
    }
    char message_buffer[*message_size];
    const size_t count = recv(sock, message_buffer, *message_size, MSG_WAITALL);
    if (*message_size != count) {
        NS_LOG_ERROR("Expected " << *message_size << " bytes, but read " << count << " bytes");
        exit(1);
    }
    google::protobuf::io::ArrayInputStream arrayIn ( message_buffer, *message_size );
    google::protobuf::io::CodedInputStream codedIn ( &arrayIn );

    ConfigureCellRadio message;
    message.ParseFromCodedStream ( &codedIn );
    return message;
}

SendCellMessage ClientServerChannel::readSendCellMessage(void) {
    NS_LOG_FUNCTION(this);
    const std::shared_ptr < uint32_t > message_size = readVarintPrefix(sock);
    if (!message_size) { 
        NS_LOG_ERROR("Cannot access message size");
        exit(1);
    }
    if (*message_size < 0) {
        NS_LOG_ERROR("Message size smaller zero");
        exit(1);
    }
    char message_buffer[*message_size];
    const size_t count = recv(sock, message_buffer, *message_size, MSG_WAITALL);
    if (*message_size != count) {
        NS_LOG_ERROR("Expected " << *message_size << " bytes, but read " << count << " bytes");
        exit(1);
    }
    google::protobuf::io::ArrayInputStream arrayIn ( message_buffer, *message_size );
    google::protobuf::io::CodedInputStream codedIn ( &arrayIn );

    SendCellMessage message;
    message.ParseFromCodedStream(&codedIn);

    if (!message.has_topological_address()) {
        NS_LOG_ERROR("Address is missing.");
        exit(1);
    }

    return message;
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
void ClientServerChannel::writeCommand(CommandMessage_CommandType cmd) {
    NS_LOG_FUNCTION(this << cmd);
    CommandMessage commandMessage;
    commandMessage.set_command_type(cmd);
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

void ClientServerChannel::writeReceiveWifiMessage(uint64_t time, int node_id, int message_id, RadioChannel channel, int rssi) {
    NS_LOG_FUNCTION(this << time << node_id << message_id << channel << rssi);
    ReceiveWifiMessage receive_message;
    receive_message.set_time(time);
    receive_message.set_node_id(node_id);
    receive_message.set_message_id(message_id);
    receive_message.set_channel_id(channel);
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

} // namespace ClientServerChannelSpace
