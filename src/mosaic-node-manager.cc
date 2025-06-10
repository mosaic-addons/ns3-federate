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

    MosaicNodeManager::MosaicNodeManager() 
      : m_backboneAddressHelper("5.0.0.0", "255.0.0.0"),
        m_wifiAddressHelper("6.0.0.0", "255.0.0.0", "0.0.0.2") {
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
        NetDeviceContainer coreDevices = p2ph.Install (remoteHost, pgw);

        NS_LOG_INFO("Assign IPs (for both server and core) and add routing...");
        Ipv4InterfaceContainer coreIpIfaces = m_backboneAddressHelper.Assign (coreDevices);
        Ipv4Address remoteHostAddr = coreIpIfaces.GetAddress (0);

        // add routing for remoteHost
        Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
        remoteHostStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), coreDevices.Get(0)->GetIfIndex());

        // logging for remoteHost
        NS_LOG_DEBUG("[node=" << remoteHost->GetId() << "] SERVER");
        NS_LOG_DEBUG("Server interfaces:");
        for (uint32_t i = 0; i < remoteHost->GetObject<Ipv4> ()->GetNInterfaces (); i++)
        {
            Ipv4InterfaceAddress iaddr = remoteHost->GetObject<Ipv4> ()->GetAddress (i, 0);
            NS_LOG_DEBUG("  if_" << i << " dev=" << remoteHost->GetDevice(i) << " iaddr=" << iaddr);
        }
        std::stringstream remoteHostRouting;
        remoteHostRouting << "Server routing:" << std::endl;
        remoteHost->GetObject<Ipv4> ()->GetRoutingProtocol ()->PrintRoutingTable (new OutputStreamWrapper(&remoteHostRouting));
        NS_LOG_LOGIC(remoteHostRouting.str());

        // add routing for PGW
        Ptr<Ipv4StaticRouting> pgwStaticRouting = ipv4RoutingHelper.GetStaticRouting (pgw->GetObject<Ipv4> ());
        // Devices are 0:Loopback  1:TunDevice 2:SGW 3:server
        pgwStaticRouting->AddNetworkRouteTo (Ipv4Address("10.0.0.0"), "255.0.0.0", 1); 

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
        m_enbDevices = m_lteHelper->InstallEnbDevice (m_enbNodes);
        NS_LOG_DEBUG("[node=" << m_enbNodes.Get(0)->GetId() << "] dev=" << m_enbDevices.Get(0));
        NS_LOG_DEBUG("[node=" << m_enbNodes.Get(1)->GetId() << "] dev=" << m_enbDevices.Get(1));
        m_lteHelper->AddX2Interface (m_enbNodes); // for handover

        // Set position of eNB nr.2
        Ptr<Node> node = m_enbNodes.Get(1);
        Ptr<MobilityModel> mobModel = node->GetObject<MobilityModel> ();
        mobModel->SetPosition(Vector(1000, 1000, 0.0));


        NS_LOG_INFO("Setup mobileNode's...");
        /* 
         * We create all mobileNodes now, because ns3 does not allow to create them after simulation start.
         * see "Cannot create UE devices after simulation started" at https://gitlab.com/nsnam/ns-3-dev/-/blob/master/src/lte/model/lte-ue-phy.cc#L144
         */ 
        m_mobileNodes.Create (5);
        internet.Install(m_mobileNodes);

        NS_LOG_INFO("Install ConstantVelocityMobilityModel");
        mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
        mobility.Install (m_mobileNodes);

        NS_LOG_INFO("Install WAVE devices");
        NetDeviceContainer wifiDevices = m_wifi80211pHelper.Install(m_wifiPhyHelper, m_waveMacHelper, m_mobileNodes);
        Ipv4InterfaceContainer wifiIpIfaces = m_wifiAddressHelper.Assign(wifiDevices);
        for (uint32_t u = 0; u < m_mobileNodes.GetN (); ++u)
        {
            Ptr<Node> node = m_mobileNodes.Get(u);
            Ptr<NetDevice> device = wifiDevices.Get(u);
            Ptr<Ipv4> ipv4proto = node->GetObject<Ipv4>();
            int32_t ifIndex = ipv4proto->GetInterfaceForDevice(device);

            // logging
            std::stringstream ss;
            for (uint32_t j = 0; j < ipv4proto->GetNAddresses (ifIndex); j++ ) {
                Ipv4InterfaceAddress iaddr = ipv4proto->GetAddress (ifIndex, j);
                ss << "|" << iaddr.GetLocal ();
            }
            NS_LOG_DEBUG("[node=" << node->GetId () << "]" 
                << " dev=" << node->GetDevice(ifIndex) 
                << " wifiAddr=" << ss.str()
            );
        }

        NS_LOG_INFO("Install LTE devices");
        NetDeviceContainer lteDevices = m_lteHelper->InstallUeDevice (m_mobileNodes);
        Ipv4InterfaceContainer lteIpIfaces = epcHelper->AssignUeIpv4Address (lteDevices);

        // assign IP address to UEs
        NS_LOG_DEBUG("[LTE GW] addr=" << epcHelper->GetUeDefaultGatewayAddress ());
        for (uint32_t u = 0; u < m_mobileNodes.GetN (); ++u)
        {
            Ptr<Node> node = m_mobileNodes.Get (u);
            Ptr<NetDevice> device = lteDevices.Get (u);
            Ptr<Ipv4> ipv4proto = node->GetObject<Ipv4>();
            uint32_t ifIndex = device->GetIfIndex ();

            // logging
            std::stringstream ss;
            for (uint32_t j = 0; j < ipv4proto->GetNAddresses (ifIndex); j++ ) {
                Ipv4InterfaceAddress iaddr = ipv4proto->GetAddress (ifIndex, j);
                ss << "|" << iaddr.GetLocal ();
            }
            NS_LOG_DEBUG("[node=" << node->GetId() << "]"
                << " dev=" << device 
                << " lteAddr=" << ss.str()
                << " rrc=" << device->GetObject<LteUeNetDevice> ()->GetRrc ()
                << " imsi=" << device->GetObject<LteUeNetDevice> ()->GetRrc ()->GetImsi ()
            );

            // set the default gateway for the UE
            Ptr<Ipv4StaticRouting> ueStaticRouting;
            ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (node->GetObject<Ipv4> ());
            ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), ifIndex);
        }

        // logging for mobile nodes
        for (uint32_t u = 0; u < m_mobileNodes.GetN (); ++u)
        {
            Ptr<Node> node = m_mobileNodes.Get(u);
            NS_LOG_LOGIC("[node=" << node->GetId () << "]");
            for (uint32_t i = 0; i < node->GetObject<Ipv4> ()->GetNInterfaces (); i++ )
            {
                std::stringstream ss;
                for (uint32_t j = 0; j < node->GetObject<Ipv4> ()->GetNAddresses (i); j++ ) {
                    Ipv4InterfaceAddress iaddr = node->GetObject<Ipv4> ()->GetAddress (i, j);
                    ss << "|" << iaddr.GetLocal ();
                }
                NS_LOG_LOGIC("  if_" << i << " dev=" << node->GetDevice(i) << " addr=" << ss.str());
            }
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

        NS_LOG_INFO("Schedule manual handovers...");
        m_lteHelper->HandoverRequest (Seconds (3.000), lteDevices.Get (1), m_enbDevices.Get (0), m_enbDevices.Get (1));
    }

    void MosaicNodeManager::OnShutdown() {
        NS_LOG_FUNCTION (this);

        NS_LOG_DEBUG("Print IP assignment for all used mobileNodes");
        for (uint32_t u = 0; u < m_mosaic2nsdrei.size(); ++u)
        {
            Ptr<Node> node = m_mobileNodes.Get(u);
            NS_LOG_DEBUG("[node=" << node->GetId () << "]");
            for (uint32_t i = 0; i < node->GetObject<Ipv4> ()->GetNInterfaces (); i++ )
            {
                if (i==0) {
                    // 0: Loopback
                    continue;
                }
                std::stringstream ss;
                for (uint32_t j = 0; j < node->GetObject<Ipv4> ()->GetNAddresses (i); j++ ) {
                    Ipv4InterfaceAddress iaddr = node->GetObject<Ipv4> ()->GetAddress (i, j);
                    ss << "|" << iaddr.GetLocal ();
                }
                NS_LOG_DEBUG("  if_" << i << " dev=" << node->GetDevice(i) << " addr=" << ss.str());
            }
        }
        std::stringstream ss;
        m_mobileNodes.Get (0)->GetObject<Ipv4> ()->GetRoutingProtocol ()->PrintRoutingTable (new OutputStreamWrapper(&ss));
        NS_LOG_LOGIC(ss.str());
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

    void MosaicNodeManager::ConfigureWifiRadio(uint32_t mosaicNodeId, bool radioTurnedOn, double transmitPower, Ipv4Address ip) {
        uint32_t nodeId = GetNs3NodeId(mosaicNodeId);
        if (m_isDeactivated[nodeId]) {
            return;
        }
        NS_LOG_INFO("[node=" << nodeId << "] radioTurnedOn="<< radioTurnedOn << " txPow=" << transmitPower << " ip=" << ip);
        
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

            // FIXME: Somehow do the following logic only exactly once, with first config message.

            // Devices are 0:Loopback 1:Wifi 2:LTE
            Ptr<NetDevice> device =node->GetDevice(1);
            Ptr<Ipv4> ipv4proto = node->GetObject<Ipv4>();
            int32_t ifIndex = ipv4proto->GetInterfaceForDevice(device);

            // Additionally assign an extra IPv4 Address (without ipv4 helper)
            Ipv4InterfaceAddress ipv4Addr = Ipv4InterfaceAddress(ip, "255.0.0.0");
            ipv4proto->AddAddress(ifIndex, ipv4Addr);

            // logging
            std::stringstream ss;
            for (uint32_t j = 0; j < ipv4proto->GetNAddresses (ifIndex); j++ ) {
                Ipv4InterfaceAddress iaddr = ipv4proto->GetAddress (ifIndex, j);
                ss << "|" << iaddr.GetLocal ();
            }
            NS_LOG_DEBUG("[node=" << node->GetId () << "]" 
                << " dev=" << node->GetDevice(ifIndex) 
                << " wifiAddr=" << ss.str()
            );

        } else {
            ssa->Disable();
        }
    }

    void MosaicNodeManager::ConfigureLteRadio(uint32_t mosaicNodeId, bool radioTurnedOn, Ipv4Address ip) {
        uint32_t nodeId = GetNs3NodeId(mosaicNodeId);
        if (m_isDeactivated[nodeId]) {
            return;
        }
        NS_LOG_INFO("[node=" << nodeId << "] radioTurnedOn="<< radioTurnedOn << " ip=" << ip);
        
        Ptr<Node> node = NodeList::GetNode(nodeId);

        Ptr<Application> app = node->GetApplication(0);
        Ptr<MosaicProxyApp> ssa = app->GetObject<MosaicProxyApp>();
        if (!ssa) {
            NS_LOG_ERROR("No app found on node " << nodeId << " !");
            return;
        }
        if (radioTurnedOn) {
            ssa->Enable();

            // FIXME: Somehow do the following logic only exactly once, with first config message.
            // When applying this multiple times (with different IPs) then routing might break or require fixes...

            // Devices are 0:Loopback 1:Wifi 2:LTE
            Ptr<NetDevice> device =node->GetDevice(2);
            Ptr<Ipv4> ipv4proto = node->GetObject<Ipv4>();
            uint32_t ifIndex = device->GetIfIndex ();

            // Additionally assign an extra IPv4 Address (without ipv4 helper)
            // ATTENTION: This currently requires changes in NoBackhaulEpcHelper::ActivateEpsBearer to fully work
            Ipv4InterfaceAddress ipv4Addr = Ipv4InterfaceAddress(ip, "255.0.0.0");
            ipv4proto->AddAddress(ifIndex, ipv4Addr);

            // logging
            std::stringstream ss;
            for (uint32_t j = 0; j < ipv4proto->GetNAddresses (ifIndex); j++ ) {
                Ipv4InterfaceAddress iaddr = ipv4proto->GetAddress (ifIndex, j);
                ss << "|" << iaddr.GetLocal ();
            }
            NS_LOG_DEBUG("[node=" << node->GetId() << "]"
                << " dev=" << device 
                << " lteAddr=" << ss.str()
                << " rrc=" << device->GetObject<LteUeNetDevice> ()->GetRrc ()
                << " imsi=" << device->GetObject<LteUeNetDevice> ()->GetRrc ()->GetImsi ()
            );

            NS_LOG_INFO("Attach UE to specific eNB...");
            NS_LOG_INFO("ATTENTION: This requires about 21ms to fully connect");
            // this has to be done _after_ IP address assignment, otherwise the route EPC -> UE is broken
            Ptr<Node> node = NodeList::GetNode(GetNs3NodeId(mosaicNodeId));
            m_lteHelper->Attach (device, m_enbDevices.Get(0));
        } else {
            ssa->Disable();
        }
    }
}
