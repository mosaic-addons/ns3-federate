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

struct CSC_init_return{
    int64_t start_time;
    int64_t end_time;
};

struct CSC_topo_address{
	uint32_t ip_address;
	int ttl;
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

		/**
		 * Reads an AddNode message from the channel.
		 *
		 * @return AddNode message
		 */
		AddNode readAddNode(void);

		/**
		 * Reads an update Node message from the channel.
		 *
		 * @return UpdateNode message
		 */
		UpdateNode readUpdateNode(void);

		/**
		 * Reads an RemoveNode message from the channel.
		 *
		 * @return RemoveNode message
		 */
		RemoveNode readRemoveNode(void);
		
		/**
		 * Reads a ConfigureWifiRadio message from the channel
		 *
		 * @param return_value the struct to fill the data in
		 * @return 0 if successful
		 */
		ConfigureWifiRadio readConfigureWifiRadio(void);

		/**
		 * Reads a SendWifiMessage message from the channel
		 *
		 * @param return_value the struct to fill the data in
		 * @return 0 if successful
		 */
		SendWifiMessage readSendWifiMessage();

		/** Reads TimeMessage from the channel and returns the contained time as a long */
		virtual int64_t readTimeMessage();

		/*################## WRITING ####################*/

		/** Byte protocol control method for writeCommand. */
		virtual void writeCommand(CommandMessage_CommandType cmd);

		/** Write a message containing a port number to the output */
		virtual void writePort(uint32_t port);

		/** Request a time advance from the RTI */
		virtual void writeTimeMessage(int64_t time);

		/**
		 * Writes a ReceiveWifiMessage message onto the channel.
		 *
		 * @param time the simulation time at which the message receive occured
		 * @param node_id the id of the receiving node
		 * @param message_id the id of the received message
		 * @param channel the receiver channel
		 * @param rssi the rssi during the receive event
		 */
		void writeReceiveWifiMessage(uint64_t time, int node_id, int message_id, RadioChannel channel, int rssi);

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
};

} // namespace ClientServerChannelSpace
#endif
