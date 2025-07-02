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

#include "client-server-channel.h"
#include "mosaic-node-manager.h"

#include <atomic>

namespace ns3 {

    // Forward declaration to prevent circular dependency
    class MosaicSimulatorImpl;

    /**
     * @brief The central class of the MOSAIC-NS3 coupling
     */
    class MosaicNs3Server {
    public:
        MosaicNs3Server() = delete;

        /**
         * @brief Constructor: initialize the MosaicNs3Server, listen on port and wait for CMD_INIT
         *
         * @param port    port for sending channel
         * @param cmdPort port of command channel, for receiving the commands from MOSAIC
         */
        MosaicNs3Server(int port, int cmdPort);  

        /**
         * @brief Destructor
         */
        ~MosaicNs3Server();      

        /**
         * @brief main loop of simulation, call to start simulation
         *
         * NS3 Magic: a specialized entry-point is needed to create this class from a end-user function. The call of the constructor is forbidden by the NS3.
         * this function is called by the starter function and obtains the whole simulation
         * the function will call the dispatcher after the initialization of MosaicNs3Server and the creation of the first dummy event
         */
        void run();

        /**
         * @brief write the next Time to the channel
         * @param nextTime the next simulation step
         */
        void writeNextTime(unsigned long long nextTime);

        /**
         * @brief write ReceiveWifiMessage to the channel
         *
         * @param recvTime  time of the receipt
         * @param nodeID    id of the node
         * @param msgID     id of the message
         */
        void writeReceiveWifiMessage(unsigned long long recvTime, int nodeID, int msgID);

        /**
         * @brief write ReceiveCellMessage to the channel
         *
         * @param recvTime  time of the receipt
         * @param nodeID    id of the node
         * @param msgID     id of the message
         */
        void writeReceiveCellMessage(unsigned long long recvTime, int nodeID, int msgID);

    private:

        /**
         * @brief This function dispatches all commands from MOSAIC to the ns3 simulator
         */
        void dispatchCommand();

        ClientServerChannel ambassadorFederateChannel, federateAmbassadorChannel;        
        std::atomic_bool m_closeConnection;
        Ptr<MosaicNodeManager> m_nodeManager;
        Ptr<MosaicSimulatorImpl> m_sim;
    };
} // namespace ns3
#endif /* MOSAIC_NS3_SERVER_H */
