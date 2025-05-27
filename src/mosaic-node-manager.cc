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
#include "ns3/log.h"
#include "ns3/constant-velocity-mobility-model.h"
#include "ns3/wifi-net-device.h"
#include "ns3/node-list.h"

#include "ns3/internet-stack-helper.h"
#include "ns3/point-to-point-helper.h"

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

        NS_LOG_INFO("Setup server...");
        NodeContainer remoteHostContainer;
        remoteHostContainer.Create (1);
        internet.Install (remoteHostContainer);

        NS_LOG_INFO("Install MosaicProxyApp application");
        for (uint32_t i = 0; i < remoteHostContainer.GetN(); ++i)
        {
            Ptr<Node> node = remoteHostContainer.Get(i);
            Ptr<MosaicProxyApp> app = CreateObject<MosaicProxyApp>();
            // app->SetNodeManager(this);
            node->AddApplication(app);
            app->SetSockets();
            app->Enable();
        }

        NS_LOG_INFO("Setup core...");
        // This EpcHelper creates point-to-point links between the eNBs and the EPCore (3 nodes)
        Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> (); 
        m_lteHelper->SetEpcHelper (epcHelper);

        PointToPointHelper p2ph;
        p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
        p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
        p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
        Ptr<Node> remoteHost = remoteHostContainer.Get (0);
        Ptr<Node> pgw = epcHelper->GetPgwNode ();
        NetDeviceContainer p2pDevices = p2ph.Install (remoteHost, pgw);

        NS_LOG_INFO("Assign IPs (for both server and core) and add routing...");
        m_ipAddressHelper.SetBase ("10.5.0.0", "255.255.0.0");
        Ipv4InterfaceContainer p2pIpIfaces = m_ipAddressHelper.Assign (p2pDevices);
        Ipv4Address remoteHostAddr = p2pIpIfaces.GetAddress (0);

        // add routing
        Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
        remoteHostStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), p2pDevices.Get(0)->GetIfIndex());

        // logging for remoteHost
        NS_LOG_DEBUG("[node=" << remoteHost->GetId() << "] SERVER");
        NS_LOG_DEBUG("Server interfaces:");
        for (uint32_t i = 0; i < remoteHost->GetObject<Ipv4> ()->GetNInterfaces (); i++ )
        {
            Ipv4InterfaceAddress iaddr = remoteHost->GetObject<Ipv4> ()->GetAddress (i, 0);
            NS_LOG_DEBUG("  if_" << i << " dev=" << remoteHost->GetDevice(i) << " iaddr=" << iaddr);
        }
        std::stringstream remoteHostRouting;
        remoteHostRouting << "Server routing:" << std::endl;
        remoteHost->GetObject<Ipv4> ()->GetRoutingProtocol ()->PrintRoutingTable (new OutputStreamWrapper(&remoteHostRouting));
        NS_LOG_LOGIC(remoteHostRouting.str());

        // logging for PGW
        NS_LOG_DEBUG("[node=" << pgw->GetId() << "] PGW");
        NS_LOG_DEBUG("PGW interfaces:");
        for (uint32_t i = 0; i < pgw->GetObject<Ipv4> ()->GetNInterfaces (); i++ )
        {
            Ipv4InterfaceAddress iaddr = pgw->GetObject<Ipv4> ()->GetAddress (i, 0);
            NS_LOG_DEBUG("  if_" << i << " dev=" << pgw->GetDevice(i) << " iaddr=" << iaddr);
        }
        std::stringstream pgwRouting;
        pgwRouting << "PGW routing:" << std::endl;
        pgw->GetObject<Ipv4> ()->GetRoutingProtocol ()->PrintRoutingTable (new OutputStreamWrapper(&pgwRouting));
        NS_LOG_LOGIC(pgwRouting.str());

        // logging for SGW
        Ptr<Node> sgw = epcHelper->GetSgwNode ();
        NS_LOG_DEBUG("[node=" << sgw->GetId() << "] SGW");
        NS_LOG_DEBUG("SGW interfaces:");
        for (uint32_t i = 0; i < sgw->GetObject<Ipv4> ()->GetNInterfaces (); i++ )
        {
            Ipv4InterfaceAddress iaddr = sgw->GetObject<Ipv4> ()->GetAddress (i, 0);
            NS_LOG_DEBUG("  if_" << i << " dev=" << sgw->GetDevice(i) << " iaddr=" << iaddr);
        }
        std::stringstream sgwRouting;
        sgwRouting << "SGW routing:" << std::endl;
        sgw->GetObject<Ipv4> ()->GetRoutingProtocol ()->PrintRoutingTable (new OutputStreamWrapper(&pgwRouting));
        NS_LOG_LOGIC(sgwRouting.str());

        // [node=3] see no-backhaul-epc-helper:m_mme ... MME network element
        
        // TODO: this has to come from RTI interaction or configuration file
        NS_LOG_INFO("Setup eNodeB's...");
        m_enbNodes.Create (2);
        mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        mobility.Install (m_enbNodes);
        m_lteHelper->SetHandoverAlgorithmType ("ns3::NoOpHandoverAlgorithm"); // before InstallEnbDevice
        m_enbDevs = m_lteHelper->InstallEnbDevice (m_enbNodes);
        NS_LOG_DEBUG("[node=" << m_enbNodes.Get(0)->GetId() << "] dev=" << m_enbDevs.Get(0));
        m_lteHelper->AddX2Interface (m_enbNodes); // for handover

        // Set position of eNB nr.2
        Ptr<Node> node = m_enbNodes.Get(1);
        Ptr<MobilityModel> mobModel = node->GetObject<MobilityModel> ();
        mobModel->SetPosition(Vector(1000, 1000, 0.0));


        NS_LOG_INFO("Setup mobileNode's...");
        m_mobileNodes.Create (5);

        NS_LOG_INFO("Install ConstantVelocityMobilityModel");
        mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
        mobility.Install (m_mobileNodes);

        NS_LOG_INFO("Install WAVE devices");
        internet.Install(m_mobileNodes);
        NetDeviceContainer netDevices = m_wifi80211pHelper.Install(m_wifiPhyHelper, m_waveMacHelper, m_mobileNodes);
        m_ipAddressHelper.SetBase ("10.1.0.0", "255.255.0.0");
        Ipv4InterfaceContainer waveIpIface = m_ipAddressHelper.Assign(netDevices);
        for (uint32_t i = 0; i < waveIpIface.GetN (); ++i)
        {
            NS_LOG_DEBUG("[node=" << netDevices.Get(i)->GetNode()->GetId() << "] dev=" << netDevices.Get(i) << " wifiAddr=" << waveIpIface.GetAddress(i));
        }

        NS_LOG_INFO("Install LTE devices");
        NetDeviceContainer ueDevs = m_lteHelper->InstallUeDevice (m_mobileNodes);

        // assign IP address to UEs
        NS_LOG_DEBUG("[LTE GW] addr=" << epcHelper->GetUeDefaultGatewayAddress ());
        for (uint32_t u = 0; u < m_mobileNodes.GetN (); ++u)
        {
            Ptr<Node> ue = m_mobileNodes.Get (u);
            Ptr<NetDevice> ueLteDevice = ueDevs.Get (u);
            Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevice));
            NS_LOG_DEBUG("[node=" << ue->GetId() << "] dev=" << ueLteDevice << " lteAddr=" << ueIpIface.GetAddress(0));
            // set the default gateway for the UE
            Ptr<Ipv4StaticRouting> ueStaticRouting;
            ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ue->GetObject<Ipv4> ());
            ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), ueLteDevice->GetIfIndex ());
        }
        std::stringstream ss;
        m_mobileNodes.Get (0)->GetObject<Ipv4> ()->GetRoutingProtocol ()->PrintRoutingTable (new OutputStreamWrapper(&ss));
        NS_LOG_LOGIC(ss.str());

        NS_LOG_INFO("Install MosaicProxyApp application");
        for (uint32_t i = 0; i < m_mobileNodes.GetN(); ++i)
        {
            Ptr<Node> node = m_mobileNodes.Get(i);
            Ptr<MosaicProxyApp> app = CreateObject<MosaicProxyApp>();
            app->SetNodeManager(this);
            node->AddApplication(app);
            app->SetSockets();
        }

        NS_LOG_INFO("Attach UEs to specific eNB...");
        // this has to be done _after_ IP address assignment, otherwise the route EPC -> UE is broken
        m_lteHelper->Attach (ueDevs, m_enbDevs.Get(0));

        NS_LOG_INFO("Schedule manual handovers...");
        m_lteHelper->HandoverRequest (Seconds (3.000), ueDevs.Get (1), m_enbDevs.Get (0), m_enbDevs.Get (1));
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
