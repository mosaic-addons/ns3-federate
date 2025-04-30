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

#ifndef MOSAIC_NS3_SERVER_H
#define MOSAIC_NS3_SERVER_H

#include "ClientServerChannel.h"
#include "mosaic-node-manager.h"

#include <atomic>

namespace ns3 {

    /**
     * @brief The central class of the MOSAIC-NS3 coupling
     */
    class MosaicNs3Server {
    public:
        MosaicNs3Server() = delete;

        /**
         * @brief initialize the MosaicNs3Server, listen on port and wait for CMD_INIT
         *
         * @param port    port for sending channel
         * @param cmdPort port of command channel, for receiving the commands from MOSAIC
         */
        MosaicNs3Server(int port, int cmdPort);        

        /**
         * @brief main loop of simulation, call to start simulation
         *
         * NS3 Magic: a specialized entry-point is needed to create this class from a end-user function. The call of the constructor is forbidden by the NS3.
         * this function is called by the starter function and obtains the whole simulation
         * the function will call the dispatcher after the initialization of MosaicNs3Server and the creation of the first dummy event
         */
        void processCommandsUntilSimStep();

        /**
         * @brief add a packet to the receive-list
         *
         * @param recvTime  time of the receipt
         * @param pack  pointer of the packet
         * @param nodeID    id of the node
         * @param msgID the id of the message
         */
        bool AddRecvPacket(unsigned long long recvTime, Ptr<Packet> pack, int nodeID, int msgID);

        /**
         * @brief write the next Time to the channel
         * @param nextTime the next simulation step
         */
        void writeNextTime(unsigned long long nextTime);

    private:

        /**
         * @brief This function dispatch all commands from MOSAIC and sends the results back to the framework
         */
        void dispatchCommand();

        /**
         * @brief control the simulator and run all simulation steps to the next simulation step
         *
         * @param time (end-) time of this simulation step
         */
        bool RunSimStep(unsigned long long time);

        /**
         * @brief update the node position by calling the node manager
         *
         * @param ID id of the node
         * @param position the new node position as a Vector
         */
        bool UpdateNodePosition(int ID, Vector position);

        /**
         * @brief start the sending of a message on a node
         *
         * @param ID id of the node
         * @param msgID the msgID of the message
         * @param payLenght the lenght of the message
         * @param pay the payload
         * @param add the IPv4 destination address
         */
        void SendMsg(int nodeID, int msgID, int payLenght, Ipv4Address add);

        /**
         * @brief configure a nodes radio (On/Off)
         *
         * @param nodeId id of the node
         */
        void ConfigureRadio(int nodeId, bool radioTurnedOn, int transmitPower);

        /**
         * @brief create a new node by calling the node manager
         *
         * @param ID id of the node
         * @param posx the x-position of the node
         * @param posy the y-position of the node
         */
        void CreateNode(int ID, int posx, int posy);

        void Close();

        void DeactivateNode(uint32_t nodeId);
        
        std::string Int2String(int n);

        ClientServerChannelSpace::ClientServerChannel ambassadorFederateChannel, federateAmbassadorChannel;        
        unsigned long long m_startTime, m_endTime;
        std::vector<int> m_deactivatedNodes;        
        std::atomic_bool m_closeConnection;
        bool m_eventSentUp = false;
        Ptr<MosaicNodeManager> m_nodeManager;
    };
}
#endif
