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

#include "node-manager.h"

#include "ns3/node-list.h"
#include "ns3/wifi-net-device.h"
#include "ns3/lte-ue-net-device.h"
#include "ns3/lte-ue-rrc.h"

#include "mosaic-ns3-server.h" 
#include "proxy-app.h"

NS_LOG_COMPONENT_DEFINE("NodeManager");

namespace ns3 {
    
    NS_OBJECT_ENSURE_REGISTERED(NodeManager);

    TypeId NodeManager::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::NodeManager")
                .SetParent<Object>()
                .AddConstructor<NodeManager>()
                // Attributes are only set _after_ constructor ran
                .AddAttribute("numRadioNodes", "Number of mobile and stationary radio nodes",
                UintegerValue(10),
                MakeUintegerAccessor(&NodeManager::m_numRadioNodes),
                MakeUintegerChecker<uint16_t> ())
                ;
        return tid;
    }

    NodeManager::NodeManager() 
      : m_backboneAddressHelper("5.0.0.0", "255.0.0.0"),
        m_wifiAddressHelper("6.0.0.0", "255.0.0.0", "0.0.0.2") {

        /** Helpers **/
        // Wifi
        m_wifiChannelHelper.AddPropagationLoss("ns3::FriisPropagationLossModel");
        m_wifiChannelHelper.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
        Ptr<YansWifiChannel> channel = m_wifiChannelHelper.Create();
        m_wifiPhyHelper.SetChannel(channel);
        m_waveMacHelper = NqosWaveMacHelper::Default();
        m_wifi80211pHelper = Wifi80211pHelper::Default();
        // LTE
        m_lteHelper = CreateObject<LteHelper> ();
        // This EpcHelper creates point-to-point links between the eNBs and the EPCore (3 nodes)
        m_epcHelper = CreateObject<PointToPointEpcHelper> (); 
        m_lteHelper->SetEpcHelper (m_epcHelper);
        // Wired
        m_csmaHelper.SetChannelAttribute("DataRate", StringValue("100Gb/s"));
        m_csmaHelper.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
    }

    void NodeManager::Configure(MosaicNs3Server* serverPtr) {
        NS_LOG_INFO("Initialize Node Infrastructure...");
        m_serverPtr = serverPtr;

        NS_LOG_INFO("Setup core...");
        Ptr<Node> pgw = m_epcHelper->GetPgwNode ();
        Ptr<Node> sgw = m_epcHelper->GetSgwNode ();

        NS_LOG_INFO("Setup backbone connection...");
        m_backboneNodes.Add (pgw);
        m_backboneDevices = m_csmaHelper.Install(m_backboneNodes);
        m_backboneAddressHelper.Assign (m_backboneDevices);

        NS_LOG_INFO("Configure routing...");
        // add routing for PGW
        Ptr<Ipv4StaticRouting> pgwStaticRouting = m_ipv4RoutingHelper.GetStaticRouting (pgw->GetObject<Ipv4> ());
        // Devices are 0:Loopback 1:TunDevice 2:SGW 3:backbone
        pgwStaticRouting->AddNetworkRouteTo (Ipv4Address("10.0.0.0"), "255.0.0.0", 1);
        pgwStaticRouting->AddNetworkRouteTo (Ipv4Address("10.5.0.0"), "255.255.0.0", 3);
        pgwStaticRouting->AddNetworkRouteTo (Ipv4Address("10.6.0.0"), "255.255.0.0", 3);

        NS_LOG_INFO("Do logging...");

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

        // [node=2] see no-backhaul-epc-helper:m_mme ... MME network element
        
        // TODO: this has to come from RTI interaction or configuration file
        NS_LOG_INFO("Setup eNodeB's...");
        m_enbNodes.Create (2);
        m_mobilityHelper.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        m_mobilityHelper.Install (m_enbNodes);
        m_enbDevices = m_lteHelper->InstallEnbDevice (m_enbNodes);
        NS_LOG_DEBUG("[node=" << m_enbNodes.Get(0)->GetId() << "] dev=" << m_enbDevices.Get(0));
        NS_LOG_DEBUG("[node=" << m_enbNodes.Get(1)->GetId() << "] dev=" << m_enbDevices.Get(1));
        m_lteHelper->AddX2Interface (m_enbNodes); // for handover

        // Set position of eNB nr.2
        Ptr<Node> node = m_enbNodes.Get(1);
        Ptr<MobilityModel> mobModel = node->GetObject<MobilityModel> ();
        mobModel->SetPosition(Vector(1000, 1000, 0.0));


        NS_LOG_INFO("Setup radioNode's...");
        /* 
         * We create all mobileNodes now, because ns3 does not allow to create them after simulation start.
         * see "Cannot create UE devices after simulation started" at https://gitlab.com/nsnam/ns-3-dev/-/blob/master/src/lte/model/lte-ue-phy.cc#L144
         */ 
        m_radioNodes.Create (m_numRadioNodes);
        m_internetHelper.Install(m_radioNodes);

        NS_LOG_INFO("Install ConstantVelocityMobilityModel");
        m_mobilityHelper.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
        m_mobilityHelper.Install (m_radioNodes);

        NS_LOG_INFO("Install WAVE devices");
        NetDeviceContainer wifiDevices = m_wifi80211pHelper.Install(m_wifiPhyHelper, m_waveMacHelper, m_radioNodes);
        Ipv4InterfaceContainer wifiIpIfaces = m_wifiAddressHelper.Assign(wifiDevices);
        for (uint32_t u = 0; u < m_radioNodes.GetN (); ++u)
        {
            Ptr<Node> node = m_radioNodes.Get(u);
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
        NetDeviceContainer lteDevices = m_lteHelper->InstallUeDevice (m_radioNodes);
        Ipv4InterfaceContainer lteIpIfaces = m_epcHelper->AssignUeIpv4Address (lteDevices);

        // assign IP address to UEs
        NS_LOG_DEBUG("[LTE GW] addr=" << m_epcHelper->GetUeDefaultGatewayAddress ());
        for (uint32_t u = 0; u < m_radioNodes.GetN (); ++u)
        {
            Ptr<Node> node = m_radioNodes.Get (u);
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
            ueStaticRouting = m_ipv4RoutingHelper.GetStaticRouting (node->GetObject<Ipv4> ());
            ueStaticRouting->SetDefaultRoute (m_epcHelper->GetUeDefaultGatewayAddress (), ifIndex);
        }

        // logging for mobile nodes
        for (uint32_t u = 0; u < m_radioNodes.GetN (); ++u)
        {
            Ptr<Node> node = m_radioNodes.Get(u);
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
        m_radioNodes.Get (0)->GetObject<Ipv4> ()->GetRoutingProtocol ()->PrintRoutingTable (new OutputStreamWrapper(&ss));
        NS_LOG_LOGIC(ss.str());

        NS_LOG_INFO("Install ProxyApp applications...");
        for (uint32_t i = 0; i < m_radioNodes.GetN(); ++i)
        {
            Ptr<Node> node = m_radioNodes.Get(i);

            Ptr<ProxyApp> wifiApp = CreateObject<ProxyApp>();
            wifiApp->SetRecvCallback(MakeCallback(&NodeManager::RecvWifiMsg, this));
            node->AddApplication(wifiApp);
            wifiApp->SetSockets(1);

            Ptr<ProxyApp> cellApp = CreateObject<ProxyApp>();
            cellApp->SetRecvCallback(MakeCallback(&NodeManager::RecvCellMsg, this));
            node->AddApplication(cellApp);
            cellApp->SetSockets(2);
        }

        // NS_LOG_INFO("Schedule manual handovers...");
        // m_lteHelper->HandoverRequest (Seconds (3.000), lteDevices.Get (1), m_enbDevices.Get (0), m_enbDevices.Get (1));
    }

    void NodeManager::OnShutdown() {
        NS_LOG_FUNCTION (this);

        uint32_t usedMobileNodes = m_mosaic2nsdrei.size() - (m_backboneNodes.GetN() - 1);

        NS_LOG_DEBUG("Print IP assignment for all used mobileNodes");
        for (uint32_t u = 0; u < usedMobileNodes; ++u)
        {
            Ptr<Node> node = m_radioNodes.Get(u);
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
        m_radioNodes.Get (0)->GetObject<Ipv4> ()->GetRoutingProtocol ()->PrintRoutingTable (new OutputStreamWrapper(&ss));
        NS_LOG_LOGIC(ss.str());
    }

    uint32_t NodeManager::GetNs3NodeId(uint32_t mosaicNodeId) {
        if (m_mosaic2nsdrei.find(mosaicNodeId) == m_mosaic2nsdrei.end()){
            NS_LOG_ERROR("Node ID " << mosaicNodeId << " not found in m_mosaic2nsdrei");
            NS_LOG_INFO("Have m_mosaic2nsdrei");
            for(const auto& elem : m_mosaic2nsdrei)
            {
               NS_LOG_INFO(elem.first << "->" << elem.second);
            }
            NS_LOG_INFO("END m_mosaic2nsdrei");
            exit(1);
        } 
        uint32_t res = m_mosaic2nsdrei[mosaicNodeId];
        return res;
    }

    uint32_t NodeManager::GetMosaicNodeId(uint32_t ns3NodeId) {
        if (m_nsdrei2mosaic.find(ns3NodeId) == m_nsdrei2mosaic.end()){
            NS_LOG_ERROR("Node ID " << ns3NodeId << " not found in m_nsdrei2mosaic");
            NS_LOG_INFO("Have m_nsdrei2mosaic");
            for(const auto& elem : m_nsdrei2mosaic)
            {
               NS_LOG_INFO(elem.first << "<-" << elem.second);
            }
            NS_LOG_INFO("END m_nsdrei2mosaic");
            exit(1);
        } 
        uint32_t res = m_nsdrei2mosaic[ns3NodeId];
        return res;
    }

    void NodeManager::CreateRadioNode(uint32_t mosaicNodeId, Vector position) {
        if (m_mosaic2nsdrei.find(mosaicNodeId) != m_mosaic2nsdrei.end()){
            NS_LOG_ERROR("Cannot create node with id=" << mosaicNodeId << " multiple times.");
            exit(1);
        }

        for (uint32_t i = 0; i < m_radioNodes.GetN(); ++i)
        {
            Ptr<Node> node = m_radioNodes.Get(i);
            if (m_nsdrei2mosaic.find(node->GetId()) == m_nsdrei2mosaic.end()){
                // the node is not used yet, add it to the lookup tables
                NS_LOG_INFO("Activate radio node " << mosaicNodeId << "->" << node->GetId());
                m_mosaic2nsdrei[mosaicNodeId] = node->GetId();
                m_nsdrei2mosaic[node->GetId()] = mosaicNodeId;
                m_isRadioNode[node->GetId()] = true;
                break;
            }
        }
        
        if (m_mosaic2nsdrei.find(mosaicNodeId) == m_mosaic2nsdrei.end()){
            NS_LOG_ERROR("No available node found. Increase mobile number of nodes!");
            exit(1);
        }

        UpdateNodePosition(mosaicNodeId, position);
    }

    void NodeManager::CreateWiredNode(uint32_t mosaicNodeId) {
        if (m_mosaic2nsdrei.find(mosaicNodeId) != m_mosaic2nsdrei.end()){
            NS_LOG_ERROR("Cannot create node with id=" << mosaicNodeId << " multiple times.");
            exit(1);
        }

        /* create node */
        Ptr<Node> node = CreateObject<Node>();
        NS_LOG_INFO("Create wired node " << mosaicNodeId << "->" << node->GetId());
        m_mosaic2nsdrei[mosaicNodeId] = node->GetId();
        m_nsdrei2mosaic[node->GetId()] = mosaicNodeId;
        m_isWiredNode[node->GetId()] = true;
        m_backboneNodes.Add (node);

        /* install internet stack */
        m_internetHelper.Install (node);
        Ptr<Ipv4> ipv4proto = node->GetObject<Ipv4>();

        /* install csma device */
        Ptr<CsmaChannel> ch = DynamicCast<CsmaChannel>(m_backboneDevices.Get(0)->GetChannel());
        Ptr<NetDevice> device = m_csmaHelper.Install(node, ch).Get(0);
        m_backboneDevices.Add (device);
        m_backboneAddressHelper.Assign (device);
        int32_t ifIndex = ipv4proto->GetInterfaceForDevice(device); // has to be done after m_backboneAddressHelper

        /* install application */
        Ptr<ProxyApp> app = CreateObject<ProxyApp>();
        app->SetRecvCallback(MakeCallback(&NodeManager::RecvCellMsg, this));
        node->AddApplication(app);
        app->SetSockets(3); // see ProxyApp::TranslateNumberToIndex
    }

    void NodeManager::UpdateNodePosition(uint32_t mosaicNodeId, Vector position) {
        uint32_t nodeId = GetNs3NodeId(mosaicNodeId);
        if (m_isDeactivated[nodeId]) {
            return;
        }

        NS_LOG_INFO("[node=" << nodeId << "] x=" << position.x << " y=" << position.y << " z=" << position.z);

        Ptr<Node> node = NodeList::GetNode(nodeId);
        Ptr<MobilityModel> mobModel = node->GetObject<MobilityModel> ();
        mobModel->SetPosition(position);
    }

    void NodeManager::RemoveNode(uint32_t mosaicNodeId) {
        uint32_t nodeId = GetNs3NodeId(mosaicNodeId);
        if (m_isDeactivated[nodeId]) {
            return;
        }
        
        Ptr<Node> node = NodeList::GetNode(nodeId);

        /* deactivate Wifi */
        if (m_isRadioNode[nodeId]) {
            // Devices are 0:Loopback 1:Wifi 2:LTE
            Ptr<WifiNetDevice> netDev = DynamicCast<WifiNetDevice> (node->GetDevice(1));
            if (netDev == nullptr) {
                NS_LOG_ERROR("Node " << nodeId << " has no WifiNetDevice");
                return;
            }
            netDev->GetPhy()->SetOffMode();
        }
        
        /* deactivate Apps */
        int numApps = m_isRadioNode[nodeId] ? 2 : 1;
        for (uint32_t i = 0; i < numApps; i++ ) {
            Ptr<ProxyApp> app = DynamicCast<ProxyApp> (node->GetApplication(0));
            if (!app) {
                NS_LOG_ERROR("No app with index=" << i << " found on node " << nodeId << " !");
                exit(1);
            }
            app->Disable();
        }

        m_isDeactivated[nodeId] = true;
    }

    void NodeManager::ConfigureWifiRadio(uint32_t mosaicNodeId, double transmitPower, Ipv4Address ip) {
        uint32_t nodeId = GetNs3NodeId(mosaicNodeId);
        if (m_isDeactivated[nodeId]) {
            return;
        }
        if (m_isWifiRadioConfigured[nodeId]) {
            NS_LOG_ERROR("Cannot configure WIFI interface multiple times. Ignoring.");
            return;
        }
        m_isWifiRadioConfigured[nodeId] = true;

        NS_ASSERT_MSG(!m_isRadioNode[nodeId], "Cannot have a wifi interface on a wired node.");

        NS_LOG_INFO("[node=" << nodeId << "] txPow=" << transmitPower << " ip=" << ip);
        
        Ptr<Node> node = NodeList::GetNode(nodeId);
        Ptr<ProxyApp> wifiApp = DynamicCast<ProxyApp> (node->GetApplication(0));
        if (!wifiApp) {
            NS_LOG_ERROR("No wifi app found on node " << nodeId << " !");
            exit(1);
        }
        wifiApp->Enable();
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

        // Devices are 0:Loopback 1:Wifi 2:LTE
        Ptr<NetDevice> device = node->GetDevice(1);
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
    }

    void NodeManager::ConfigureCellRadio(uint32_t mosaicNodeId, Ipv4Address ip) {
        uint32_t nodeId = GetNs3NodeId(mosaicNodeId);
        if (m_isDeactivated[nodeId]) {
            return;
        }
        if (m_isCellRadioConfigured[nodeId]) {
            NS_LOG_ERROR("Cannot configure CELL interface multiple times. Ignoring.");
            // When applying this multiple times (with different IPs) then currently the routing will break...
            return;
        }
        m_isCellRadioConfigured[nodeId] = true;

        NS_LOG_INFO("[node=" << nodeId << "] ip=" << ip);
        Ptr<Node> node = NodeList::GetNode(nodeId);

        /* check for valid IP, required to match routing configuration */
        bool partOf10 = ip.CombineMask("255.0.0.0").Get() == Ipv4Address("10.0.0.0").Get();
        bool partOf105 = ip.CombineMask("255.255.0.0").Get() == Ipv4Address("10.5.0.0").Get();
        bool partOf106 = ip.CombineMask("255.255.0.0").Get() == Ipv4Address("10.6.0.0").Get();
        NS_ASSERT_MSG(partOf10, "The ip for all nodes must be part of 10.0.0.0/8 network.");

        if (m_isRadioNode[nodeId]) {

            NS_ASSERT_MSG(!partOf105, "The ip for radio nodes must not be part of 10.5.0.0/16 network.");
            NS_ASSERT_MSG(!partOf106, "The ip for radio nodes must not be part of 10.6.0.0/16 network.");

            /* activate application */
            Ptr<ProxyApp> cellApp = DynamicCast<ProxyApp> (node->GetApplication(1));
            if (!cellApp) {
                NS_LOG_ERROR("No cell app found on node " << nodeId << " !");
                exit(1);
            }
            cellApp->Enable();

            // Devices are 0:Loopback 1:Wifi 2:LTE
            Ptr<NetDevice> device = node->GetDevice(2);
            Ptr<Ipv4> ipv4proto = node->GetObject<Ipv4>();
            uint32_t ifIndex = device->GetIfIndex ();

            /* assign extra IPv4 Address (without ipv4 helper) */
            // ATTENTION: This currently requires changes in NoBackhaulEpcHelper::ActivateEpsBearer to fully work
            // On CSMA with ARP this would not work: 10.5.x.x and 10.6.x.x messages have to be sent directly to the default gateway, without searching the receiver via ARP
            Ipv4InterfaceAddress ipv4Addr = Ipv4InterfaceAddress(ip, "255.0.0.0");
            ipv4proto->AddAddress(ifIndex, ipv4Addr);

            /* do logging */
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
            m_lteHelper->Attach (device, m_enbDevices.Get(0));

        } else if (m_isWiredNode[nodeId]) {

            NS_ASSERT_MSG(partOf105 || partOf106, "The ip for wired nodes must be part of 10.5.0.0/16 or 10.6.0.0/16 network.");

            /* activate application */
            Ptr<ProxyApp> csmaApp = DynamicCast<ProxyApp> (node->GetApplication(0));
            if (!csmaApp) {
                NS_LOG_ERROR("No csma app found on node " << nodeId << " !");
                exit(1);
            }
            csmaApp->Enable();

            // Devices are 0:Loopback 1:Csma
            Ptr<NetDevice> device = node->GetDevice(1);
            Ptr<Ipv4> ipv4proto = node->GetObject<Ipv4>();
            uint32_t ifIndex = device->GetIfIndex ();

            /* assign extra IPv4 Address (without ipv4 helper) */
            // Require netmask 255.255.0.0 such that address like 10.3. is not requested via ARP (and subsequently dropped)
            // Downside of this network separation: messages from 10.5. to 10.6. will always be relayed by the PGW
            Ipv4InterfaceAddress ipv4Addr = Ipv4InterfaceAddress(ip, "255.255.0.0"); 
            ipv4proto->AddAddress(ifIndex, ipv4Addr);

            /* add routing */
            Ptr<Ipv4StaticRouting> serverStaticRouting = m_ipv4RoutingHelper.GetStaticRouting (node->GetObject<Ipv4> ());
            serverStaticRouting->SetDefaultRoute (Ipv4Address("5.0.0.1"), ifIndex); 
            // We cannot use any IP address of PGW (that worked with point-to-point, but not anymore)
            // We have to use the IP address of PGW that is actually connected to the CSMA, in order for ARP to function properly

            /* do logging */
            std::stringstream ss;
            NS_ASSERT_MSG(ifIndex >= 0, "No valid interface index given.");
            for (uint32_t j = 0; j < ipv4proto->GetNAddresses (ifIndex); j++ ) {
                Ipv4InterfaceAddress iaddr = ipv4proto->GetAddress (ifIndex, j);
                ss << "|" << iaddr.GetLocal ();
            }
            NS_LOG_DEBUG("[node=" << node->GetId () << "]" 
                << " dev=" << node->GetDevice(ifIndex) 
                << " csmaAddr=" << ss.str()
            );

            std::stringstream serverRouting;
            serverRouting << "Server routing:" << std::endl;
            node->GetObject<Ipv4> ()->GetRoutingProtocol ()->PrintRoutingTable (new OutputStreamWrapper(&serverRouting));
            NS_LOG_LOGIC(serverRouting.str());

        } else {
            NS_LOG_ERROR("Invalid State: Node has to be either radio or wired node.");
            exit(1);
        }
    }

    void NodeManager::SendWifiMsg(uint32_t mosaicNodeId, Ipv4Address dstAddr, RadioChannel channel, uint32_t msgID, uint32_t payLength) {
        uint32_t nodeId = GetNs3NodeId(mosaicNodeId);
        if (m_isDeactivated[nodeId]) {
            return;
        }
        NS_LOG_INFO("[node=" << nodeId << "] dst=" << dstAddr << " ch=" << channel << " msgID=" << msgID << " len=" << payLength);

        Ptr<Node> node = NodeList::GetNode(nodeId);
        Ptr<ProxyApp> app = DynamicCast<ProxyApp> (node->GetApplication(0));
        if (app == nullptr) {
            NS_LOG_ERROR("Node " << nodeId << " was not initialized properly, ProxyApp is missing");
            return;
        }
        // TODO: use channel 
        app->TransmitPacket(dstAddr, msgID, payLength);
    }

    void NodeManager::SendCellMsg(uint32_t mosaicNodeId, Ipv4Address dstAddr, uint32_t msgID, uint32_t payLength) {
        uint32_t nodeId = GetNs3NodeId(mosaicNodeId);
        if (m_isDeactivated[nodeId]) {
            return;
        }
        NS_LOG_INFO("[node=" << nodeId << "] dst=" << dstAddr << " msgID=" << msgID << " len=" << payLength);

        Ptr<Node> node = NodeList::GetNode(nodeId);
        Ptr<ProxyApp> app = DynamicCast<ProxyApp> (node->GetApplication(1));
        if (app == nullptr) {
            NS_LOG_ERROR("Node " << nodeId << " was not initialized properly, ProxyApp is missing");
            return;
        }
        app->TransmitPacket(dstAddr, msgID, payLength);
    }

    void NodeManager::RecvWifiMsg(unsigned long long recvTime, uint32_t ns3NodeId, int msgID) {
        if (m_isDeactivated[ns3NodeId]) {
            return;
        }
        uint32_t nodeId = GetMosaicNodeId(ns3NodeId);
        
        m_serverPtr->writeReceiveWifiMessage(recvTime, nodeId, msgID);
    }


    void NodeManager::RecvCellMsg(unsigned long long recvTime, uint32_t ns3NodeId, int msgID) {
        if (m_isDeactivated[ns3NodeId]) {
            return;
        }
        uint32_t nodeId = GetMosaicNodeId(ns3NodeId);
        
        m_serverPtr->writeReceiveCellMessage(recvTime, nodeId, msgID);
    }
} // namespace ns3

