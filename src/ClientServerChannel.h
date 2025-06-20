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

#ifndef __CLIENTSERVERCHANNEL_H__
#define __CLIENTSERVERCHANNEL_H__

#undef NaN
#include "ClientServerChannelMessages.pb.h"

#include <memory> // shared_ptr

typedef int SOCKET;
constexpr const int SOCKET_ERROR = -1;
constexpr const int INVALID_SOCKET = -1;

/**
 * Abstraction of socket communication between Ambassador and Federate (e.g. ns-3 or OMNeT++).
 */
namespace ClientServerChannelSpace {

enum RADIO_NUMBER {
	NO_RADIO=0,
	SINGLE_RADIO=1,
	DUAL_RADIO=2
};

enum CHANNEL_MODE {
	SINGLE_CHANNEL=1,		/* Radio stays on one channel the whole time */
    DUAL_CHANNEL=2	/* Radio alternates between two channels */
};

enum UPDATE_NODE_TYPE {
	UPDATE_ADD_RSU = 1,
	UPDATE_ADD_VEHICLE = 2,
	UPDATE_MOVE_NODE = 3,
	UPDATE_REMOVE_NODE = 4
};

enum RADIO_CHANNEL {
	SCH1 = 0,
	SCH2 = 1,
	SCH3 = 2,
	CCH = 3,
	SCH4 = 4,
	SCH5 = 5,
	SCH6 = 6,
	UNDEF_CHANNEL = 7,
	CELL = 8,
};

struct CSC_init_return{
    int64_t start_time;
    int64_t end_time;
};

struct CSC_node_data{
	int id;
	double x;
	double y;
};

struct CSC_radio_config{
	bool turnedOn;
	uint32_t ip_address;
	uint32_t subnet;
	double tx_power;
	CHANNEL_MODE channelmode;
	RADIO_CHANNEL primary_channel;
	RADIO_CHANNEL secondary_channel;
};

struct CSC_config_message{
	int64_t time;
    int msg_id;
    int node_id;
    RADIO_NUMBER num_radios;
    CSC_radio_config primary_radio;
    CSC_radio_config secondary_radio;
};

struct CSC_update_node_return{
	UPDATE_NODE_TYPE type;
	int64_t time;
	std::vector<CSC_node_data> properties;
};

struct CSC_topo_address{
	uint32_t ip_address;
	int ttl;
};

struct CSC_send_message{
	int64_t time;
	int node_id;
	RADIO_CHANNEL channel_id;
	int message_id;
	int length;
	CSC_topo_address topo_address;
};

class ClientServerChannel {

	public:
		/** 
		 * @brief Constructor
		 */
		ClientServerChannel();

		/**
		 * @brief Destructor
		 *
		 * Closes existing network connections.
		 */
		virtual ~ClientServerChannel();

		/**
		 * Provides server socket to listen for incoming connections from ns3 Ambassador
		 *
		 * @param host own hostname (hostaddress)
		 * @param port port to listen on for incoming connections. If no port is given, a random port is assigned.
		 * @return assigned port number
		 */
		virtual int	prepareConnection(std::string host, uint32_t port);

		/**
		 * @brief Accepts a connection (blocking)
		 * The resulting connection is stored in the working socket
		 */
		virtual void connect();

		/*################## READING ####################*/

		/** reads a command via protobuf and returns it */
		virtual CommandMessage_CommandType	readCommand();

		/** reads an initialization message and returns it */
		virtual int readInit(CSC_init_return &return_value);

		/** reads a add RSU message and returns it */
		virtual int readUpdateNode(CSC_update_node_return &return_value);

		/** Reads a configuration message from the channel and returns it */
		virtual int readConfigurationMessage(CSC_config_message &return_value);

		/** Reads a send message command and returns the corresponding message struct */
		virtual int readSendMessage(CSC_send_message &return_value);

		/** Reads TimeMessage from the channel and returns the contained time as a long */
		virtual int64_t readTimeMessage();

		/*################## WRITING ####################*/

		/** Byte protocol control method for writeCommand. */
		virtual void writeCommand(CommandMessage_CommandType cmd);

		/** Write a message containing a port number to the output */
		virtual void writePort(uint32_t port);

		/** Request a time advance from the RTI */
		virtual void writeTimeMessage(int64_t time);

		/** Signal and hand a received Message to the RTI */
		virtual void writeReceiveMessage(uint64_t time, int node_id, int message_id, RADIO_CHANNEL channel, int rssi);

	private:
		/** Initial server socket
		 * always on the lookout for new connections on that port
		 * accepts connection from Ambassador
		 */
		SOCKET servsock;

		/** Working sock for communication. */
		SOCKET sock;

		/** Reads a Varint from a socket and returns it */
		virtual std::shared_ptr < uint32_t > readVarintPrefix(SOCKET sock);

		/** converts a channel given as a protobuf internal enum to our channel enum */
		virtual RADIO_CHANNEL protoChannelToChannel(RadioChannel protoChannel);

		/** converts a channel given as our channel enum to a protobuf internal channel enum */
		virtual RadioChannel channelToProtoChannel(RADIO_CHANNEL channel);
};

}//END NAMESPACE
#endif
