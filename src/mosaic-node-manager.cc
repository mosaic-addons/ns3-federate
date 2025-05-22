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

#include "mosaic-node-manager.h"

#include "ClientServerChannel.h" // early import required for wave-net-device.h

#include "ns3/wave-net-device.h"
#include "ns3/string.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/log.h"
#include "ns3/constant-velocity-mobility-model.h"
#include "ns3/wifi-net-device.h"
#include "ns3/node-list.h"

#include "mosaic-ns3-server.h" 
#include "mosaic-proxy-app.h"

NS_LOG_COMPONENT_DEFINE("MosaicNodeManager");

namespace ns3 {
    
    NS_OBJECT_ENSURE_REGISTERED(MosaicNodeManager);

    TypeId MosaicNodeManager::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::MosaicNodeManager")
                .SetParent<Object>()
                .AddConstructor<MosaicNodeManager>()
                .AddAttribute("LossModel", "The used loss model",
                StringValue("ns3::FriisPropagationLossModel"),
                MakeStringAccessor(&MosaicNodeManager::m_lossModel),
                MakeStringChecker())
                .AddAttribute("DelayModel", "The used delay model",
                StringValue("ns3::ConstantSpeedPropagationDelayModel"),
                MakeStringAccessor(&MosaicNodeManager::m_delayModel),
                MakeStringChecker());
        return tid;
    }

    MosaicNodeManager::MosaicNodeManager() : m_ipAddressHelper("10.1.0.0", "255.255.0.0") {
    }

    void MosaicNodeManager::Configure(MosaicNs3Server* serverPtr) {
        NS_LOG_INFO("Initialize Node Infrastructure...");
        m_serverPtr = serverPtr;

        // init m_helpers
        m_wifiChannelHelper.AddPropagationLoss(m_lossModel);
        m_wifiChannelHelper.SetPropagationDelay(m_delayModel);
        Ptr<YansWifiChannel> channel = m_wifiChannelHelper.Create();
        m_wifiPhyHelper.SetChannel(channel);
        m_lteHelper = CreateObject<LteHelper> ();
        // init helpers
        InternetStackHelper internet;   
        Ipv4StaticRoutingHelper ipv4RoutingHelper;
        MobilityHelper mobility;

        // EPC Helper
        // Ptr<NoBackhaulEpcHelper> epcHelper = CreateObject<NoBackhaulEpcHelper> (); // EPC without connecting the eNBs with the core network. It just creates the network elements of the core network
        Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> (); // This EpcHelper creates point-to-point links between the eNBs and the SGW = 3 extra nodes
        m_lteHelper->SetEpcHelper (epcHelper);
        
        // TODO: this has to come from RTI interaction or configuration file
        NS_LOG_INFO("Setup eNodeB's...");
        m_enbNodes.Create (1);
        mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        mobility.Install (m_enbNodes);
        m_enbDevs = m_lteHelper->InstallEnbDevice (m_enbNodes);

        NS_LOG_INFO("Setup mobileNode's...");
        m_mobileNodes.Create (5);

        NS_LOG_INFO("Install ConstantVelocityMobilityModel");
        mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
        mobility.Install (m_mobileNodes);

        NS_LOG_INFO("Install WAVE devices");
        internet.Install(m_mobileNodes);
        NetDeviceContainer netDevices = m_wifi80211pHelper.Install(m_wifiPhyHelper, m_waveMacHelper, m_mobileNodes);
        m_ipAddressHelper.Assign(netDevices);

        NS_LOG_INFO("Install LTE devices");
        NetDeviceContainer ueDevs = m_lteHelper->InstallUeDevice (m_mobileNodes);
        m_lteHelper->Attach (ueDevs, m_enbDevs.Get(0));

        // assign IP address to UEs
        for (uint32_t u = 0; u < m_mobileNodes.GetN (); ++u)
        {
            Ptr<Node> ue = m_mobileNodes.Get (u);
            Ptr<NetDevice> ueLteDevice = ueDevs.Get (u);
            Ipv4InterfaceContainer ueIpIface;
            ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevice));
            // set the default gateway for the UE
            Ptr<Ipv4StaticRouting> ueStaticRouting;
            ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ue->GetObject<Ipv4> ());
            ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
        }

        NS_LOG_INFO("Install MosaicProxyApp application");
        for (uint32_t i = 0; i < m_mobileNodes.GetN(); ++i)
        {
            Ptr<Node> node = m_mobileNodes.Get(i);
            Ptr<MosaicProxyApp> app = CreateObject<MosaicProxyApp>();
            app->SetNodeManager(this);
            node->AddApplication(app);
            app->SetSockets();
        }
    }

    uint32_t MosaicNodeManager::GetNs3NodeId(uint32_t mosaicNodeId) {
        if (m_mosaic2nsdrei.find(mosaicNodeId) == m_mosaic2nsdrei.end()){
            NS_LOG_ERROR("Node ID " << mosaicNodeId << " not found in m_mosaic2nsdrei");
            NS_LOG_INFO("Have m_mosaic2nsdrei");
            for(const auto& elem : m_mosaic2nsdrei)
            {
               NS_LOG_INFO(elem.first << " : " << elem.second);
            }
            NS_LOG_INFO("END m_mosaic2nsdrei");
            exit(1);
        } 
        uint32_t res = m_mosaic2nsdrei[mosaicNodeId];
        return res;
    }

    uint32_t MosaicNodeManager::GetMosaicNodeId(uint32_t ns3NodeId) {
        if (m_nsdrei2mosaic.find(ns3NodeId) == m_nsdrei2mosaic.end()){
            NS_LOG_ERROR("Node ID " << ns3NodeId << " not found in m_nsdrei2mosaic");
            NS_LOG_INFO("Have m_nsdrei2mosaic");
            for(const auto& elem : m_nsdrei2mosaic)
            {
               NS_LOG_INFO(elem.first << " : " << elem.second);
            }
            NS_LOG_INFO("END m_nsdrei2mosaic");
            exit(1);
        } 
        uint32_t res = m_nsdrei2mosaic[ns3NodeId];
        return res;
    }

    void MosaicNodeManager::CreateMosaicNode(uint32_t mosaicNodeId, Vector position) {
        if (m_mosaic2nsdrei.find(mosaicNodeId) != m_mosaic2nsdrei.end()){
            NS_LOG_ERROR("Cannot create node with id=" << mosaicNodeId << " multiple times.");
            exit(1);
        }

        for (uint32_t i = 0; i < m_mobileNodes.GetN(); ++i)
        {
            Ptr<Node> node = m_mobileNodes.Get(i);
            if (m_nsdrei2mosaic.find(node->GetId()) == m_nsdrei2mosaic.end()){
                // the node is not used yet, add it to the lookup tables
                NS_LOG_INFO("Activate node " << mosaicNodeId << "->" << node->GetId());
                m_mosaic2nsdrei[mosaicNodeId] = node->GetId();
                m_nsdrei2mosaic[node->GetId()] = mosaicNodeId;
                break;
            }
        }
        
        if (m_mosaic2nsdrei.find(mosaicNodeId) == m_mosaic2nsdrei.end()){
            NS_LOG_ERROR("No available node found. Increase mobile number of nodes!");
            exit(1);
        }

        UpdateNodePosition(mosaicNodeId, position);
    }

    void MosaicNodeManager::SendMsg(uint32_t mosaicNodeId, Ipv4Address dstAddr, ClientServerChannelSpace::RADIO_CHANNEL channel, uint32_t msgID, uint32_t payLength) {
        uint32_t nodeId = GetNs3NodeId(mosaicNodeId);
        if (m_isDeactivated[nodeId]) {
            return;
        }
        NS_LOG_INFO("[node=" << nodeId << "] dst=" << dstAddr << " ch=" << channel << " msgID=" << msgID << " len=" << payLength);

        Ptr<Node> node = NodeList::GetNode(nodeId);
        Ptr<MosaicProxyApp> app = DynamicCast<MosaicProxyApp> (node->GetApplication(0));
        if (app == nullptr) {
            NS_LOG_ERROR("Node " << nodeId << " was not initialized properly, MosaicProxyApp is missing");
            return;
        }
        app->TransmitPacket(dstAddr, channel, msgID, payLength);
    }

    void MosaicNodeManager::AddRecvPacket(unsigned long long recvTime, Ptr<Packet> pack, uint32_t ns3NodeId, int msgID) {
        if (m_isDeactivated[ns3NodeId]) {
            return;
        }
        uint32_t nodeId = GetMosaicNodeId(ns3NodeId);
        
        m_serverPtr->AddRecvPacket(recvTime, pack, nodeId, msgID);
    }

    void MosaicNodeManager::UpdateNodePosition(uint32_t mosaicNodeId, Vector position) {
        uint32_t nodeId = GetNs3NodeId(mosaicNodeId);
        if (m_isDeactivated[nodeId]) {
            return;
        }

        NS_LOG_INFO("[node=" << nodeId << "] x=" << position.x << " y=" << position.y);

        Ptr<Node> node = NodeList::GetNode(nodeId);
        Ptr<MobilityModel> mobModel = node->GetObject<MobilityModel> ();
        mobModel->SetPosition(position);
    }

    void MosaicNodeManager::DeactivateNode(uint32_t mosaicNodeId) {
        uint32_t nodeId = GetNs3NodeId(mosaicNodeId);
        if (m_isDeactivated[nodeId]) {
            return;
        }
        
        Ptr<Node> node = NodeList::GetNode(nodeId);
        Ptr<WifiNetDevice> netDev = DynamicCast<WifiNetDevice> (node->GetDevice(1));
        
        if (netDev == nullptr) {
            NS_LOG_ERROR("Node " << nodeId << " has no WifiNetDevice");
            return;
        }
        netDev->GetPhy()->SetOffMode();
        
        m_isDeactivated[nodeId] = true;
    }

    void MosaicNodeManager::ConfigureNodeRadio(uint32_t mosaicNodeId, bool radioTurnedOn, double transmitPower) {
        uint32_t nodeId = GetNs3NodeId(mosaicNodeId);
        if (m_isDeactivated[nodeId]) {
            return;
        }
        NS_LOG_INFO("[node=" << nodeId << "] radioTurnedOn="<< radioTurnedOn << " txPow=" << transmitPower);
        
        Ptr<Node> node = NodeList::GetNode(nodeId);

        Ptr<Application> app = node->GetApplication(0);
        Ptr<MosaicProxyApp> ssa = app->GetObject<MosaicProxyApp>();
        if (!ssa) {
            NS_LOG_ERROR("No app found on node " << nodeId << " !");
            return;
        }
        if (radioTurnedOn) {
            ssa->Enable();
            if (transmitPower > -1) {
                Ptr<WifiNetDevice> netDev = DynamicCast<WifiNetDevice> (node->GetDevice(1));
                if (netDev == nullptr) {
                    NS_LOG_ERROR("Inconsistency: no matching NetDevice found on node while configuring");
                    return;
                }                        
                Ptr<YansWifiPhy> wavePhy = DynamicCast<YansWifiPhy> (netDev->GetPhy());
                NS_LOG_INFO("[node=" << nodeId << "] Adjust settings on dev="<< netDev << " phy=" << wavePhy);
                if (wavePhy != 0) {
                    double txDBm = 10 * log10(transmitPower);
                    wavePhy->SetTxPowerStart(txDBm);
                    wavePhy->SetTxPowerEnd(txDBm);
                }
            }
        } else {
            ssa->Disable();
        }
    }
}
