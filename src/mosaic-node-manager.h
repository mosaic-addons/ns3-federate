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

#ifndef MOSAIC_NODE_MANAGER_H
#define MOSAIC_NODE_MANAGER_H

#include <unordered_map>

#include "ns3/node-container.h"
#include "ns3/vector.h"

#include "ns3/yans-wifi-phy.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/wifi-80211p-helper.h"

#include "ns3/lte-helper.h"
#include "ns3/point-to-point-epc-helper.h"

#include "ns3/csma-helper.h"

#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/mobility-helper.h"

#include "client-server-channel.h"

namespace ns3 {

    //Forward declaration to prevent circular dependency
    class MosaicNs3Server;

    /**
     * @class MosaicNodeManager
     * @brief The class MosaicNodeManager manages the creation, the initial 
     * placement, and the position updates of ns3 nodes. It also manages the
     * node ID translation between MOSAIC and NS3 domain.
     */
    class MosaicNodeManager : public Object {
    public:
        static TypeId GetTypeId(void);

        MosaicNodeManager();
        virtual ~MosaicNodeManager() = default;

        void Configure(MosaicNs3Server* serverPtr);

        void OnShutdown(void);

        /**
         * @brief create a new radio node
         *
         * @param mosaicNodeId id of the node
         * @param position the new node position as a Vector
         */
        void CreateRadioNode(uint32_t mosaicNodeId, Vector position);

        /**
         * @brief create a new wired node
         *
         * @param mosaicNodeId id of the node
         */
        void CreateWiredNode(uint32_t mosaicNodeId);

        /**
         * @brief update the node position
         *
         * @param mosaicNodeId id of the node
         * @param position the new node position as a Vector
         */
        void UpdateNodePosition(uint32_t mosaicNodeId, Vector position);
        
        /**
         * @brief Remove the node as good as possible
         * It is not allowed to delete a node during the simulation.
         * The node will be deactivated as good as possible.
         *
         * @param mosaicNodeId id of the node
         */
        void RemoveNode(uint32_t mosaicNodeId);

        /**
         * @brief Evaluates configuration message and applies it to the node
         */
        void ConfigureWifiRadio(uint32_t mosaicNodeId, double transmitPower, Ipv4Address ip);

        /**
         * @brief Sets the provided configuration, and attaches the UE to an eNB
         */
        void ConfigureCellRadio(uint32_t mosaicNodeId, Ipv4Address ip);

        /**
         * @brief start the sending of a wifi message on a node
         *
         * @param mosaicNodeId id of the node
         * @param dstAddr the IPv4 destination address
         * @param channel the channel where to send the message on
         * @param msgID the msgID of the message
         * @param payLenght the lenght of the message
         */
        void SendWifiMsg(uint32_t mosaicNodeId, Ipv4Address dstAddr, RadioChannel channel, uint32_t msgID, uint32_t payLenght);

        /**
         * @brief start the sending of a cell message on a node
         *
         * @param mosaicNodeId id of the node
         * @param dstAddr the IPv4 destination address
         * @param msgID the msgID of the message
         * @param payLenght the lenght of the message
         */
        void SendCellMsg(uint32_t mosaicNodeId, Ipv4Address dstAddr, uint32_t msgID, uint32_t payLenght);

        void RecvWifiMsg(unsigned long long recvTime, uint32_t ns3NodeId, int msgID);

        void RecvCellMsg(unsigned long long recvTime, uint32_t ns3NodeId, int msgID);

        // Must be public to be accessible by ns-3 object creation routine
        uint16_t m_numRadioNodes;

    private:

        /**
         * @brief translate the MOSAIC node IDs to Ns3 node IDs
         */
        uint32_t GetNs3NodeId(uint32_t mosaicNodeId);

        /**
         * @brief translate the Ns3 node IDs to MOSAIC node IDs
         */
        uint32_t GetMosaicNodeId(uint32_t ns3NodeId);

        MosaicNs3Server *m_serverPtr;
        std::map<uint32_t, uint32_t> m_mosaic2nsdrei;
        std::map<uint32_t, uint32_t> m_nsdrei2mosaic;
        std::unordered_map<uint32_t, bool> m_isRadioNode;
        std::unordered_map<uint32_t, bool> m_isWiredNode;
        std::unordered_map<uint32_t, bool> m_isCellRadioConfigured;
        std::unordered_map<uint32_t, bool> m_isWifiRadioConfigured;
        std::unordered_map<uint32_t, bool> m_isDeactivated;

        /** Helpers **/
        // Wifi
        YansWifiChannelHelper m_wifiChannelHelper;
        YansWifiPhyHelper m_wifiPhyHelper;
        NqosWaveMacHelper m_waveMacHelper;
        Wifi80211pHelper m_wifi80211pHelper;
        // LTE
        Ptr<LteHelper> m_lteHelper; // problematic if not stored as pointer
        Ptr<PointToPointEpcHelper> m_epcHelper;
        // Wired
        CsmaHelper m_csmaHelper;
        // Internet
        InternetStackHelper m_internetHelper;   
        Ipv4StaticRoutingHelper m_ipv4RoutingHelper;
        // IP
        Ipv4AddressHelper m_backboneAddressHelper;
        Ipv4AddressHelper m_wifiAddressHelper;
        // Mobility
        MobilityHelper m_mobilityHelper;

        /** Nodes and Devices **/
        NodeContainer m_backboneNodes;
        NetDeviceContainer m_backboneDevices;
        NodeContainer m_enbNodes;
        NetDeviceContainer m_enbDevices;
        NodeContainer m_radioNodes;
    };
} // namespace ns3
#endif /* MOSAIC_NODE_MANAGER_H */
