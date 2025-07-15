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

#ifndef CLIENT_SERVER_CHANNEL_H
#define CLIENT_SERVER_CHANNEL_H

#undef NaN
#include "ClientServerChannelMessages.pb.h"

#include <memory> // shared_ptr

typedef int SOCKET;
constexpr const int SOCKET_ERROR = -1;
constexpr const int INVALID_SOCKET = -1;
constexpr const int PROTOCOL_VERSION = 2;

/**
 * Abstraction of socket communication between Ambassador and Federate (e.g. ns-3 or OMNeT++).
 */
namespace ClientServerChannelSpace {

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
		~ClientServerChannel();

		/**
		 * Provides server socket to listen for incoming connections from ns3 Ambassador
		 *
		 * @param host own hostname (hostaddress)
		 * @param port port to listen on for incoming connections. If no port is given, a random port is assigned.
		 * @return assigned port number
		 */
		int	prepareConnection(std::string host, uint32_t port);

		/**
		 * @brief Accepts a connection (blocking)
		 * The resulting connection is stored in the working socket
		 */
		void connect();

		/*################## READING ####################*/

		/**
		 * Gets command from NS3 Ambassador to select dedicated action.
		 *
		 * @return command from Ambassador
		 *
		 */
		CommandMessage_CommandType readCommand();
		
		/**
		 * Reads an InitMessage from the Channel
		 *
		 * @return InitMessage message
		 */
		InitMessage readInitMessage();

		/**
		 * Reads a TimeMessage from the channel
		 *
		 * @return the time as long
		 */
		int64_t readTimeMessage();

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
		 * @return ConfigureWifiRadio message
		 */
		ConfigureWifiRadio readConfigureWifiRadio(void);

		/**
		 * Reads a SendWifiMessage message from the channel
		 *
		 * @return SendWifiMessage message
		 */
		SendWifiMessage readSendWifiMessage(void);

		/**
		 * Reads a ConfigureCellRadio message from the channel
		 *
		 * @return ConfigureCellRadio message
		 */
		ConfigureCellRadio readConfigureCellRadio(void);

		/**
		 * Reads a SendCellMessage message from the channel
		 *
		 * @return SendCellMessage message
		 */
		SendCellMessage readSendCellMessage(void);

		/*################## WRITING ####################*/

		/**
		 * Sends own control commands to ambassador
		 * Such control commands must be written onto the channel before every data body
		 *
		 * @param cmd command to be written to ambassador
		 */
		void writeCommand(CommandMessage_CommandType cmd);

		/**
		 * Sends port to ambassador. Write a message containing a port number to the output
		 *
		 * @param port port
		 */
		void writePort(uint32_t port);

		/**
		 * Writes a time onto the channel and thereby request a time advance from the RTI
		 *
		 * @param time the time to write
		 */
		void writeTimeMessage(int64_t time);

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
		
		/**
		 * Writes a ReceiveCellMessage message onto the channel.
		 *
		 * @param time the simulation time at which the message receive occured
		 * @param node_id the id of the receiving node
		 * @param message_id the id of the received message
		 */
		void writeReceiveCellMessage(uint64_t time, int node_id, int message_id);

	private:
		/** Initial server socket
		 * always on the lookout for new connections on that port
		 * accepts connection from Ambassador
		 */
		SOCKET servsock;

		/** Working sock for communication. */
		SOCKET sock;

		/**
		 * @brief Reads a variable length integer from the socket and returns it
		 *
		 * Protobuf messages are not self delimiting and have thus to be prefixed with the length of the message.
		 * When sent from Java, before every message there will be a variable length integer sent.
		 * This method reads such an integer of variable length
		 *
		 */
		std::shared_ptr < uint32_t > readVarintPrefix(SOCKET sock);
};

} // namespace ClientServerChannelSpace
#endif /* CLIENT_SERVER_CHANNEL_H */
