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

#include "ns3/node-list.h"
#include "ns3/wifi-net-device.h"
#include "ns3/lte-ue-net-device.h"
#include "ns3/lte-ue-rrc.h"
#include "ns3/lte-enb-net-device.h"
#include "ns3/lte-enb-rrc.h"
#include "ns3/loopback-net-device.h"
#include "ns3/csma-net-device.h"
#include "ns3/point-to-point-net-device.h"

#include "mosaic-ns3-bridge.h" 
#include "mosaic-proxy-app.h"

NS_LOG_COMPONENT_DEFINE("MosaicNodeManager");

namespace ns3 {
    
    NS_OBJECT_ENSURE_REGISTERED(MosaicNodeManager);

    TypeId MosaicNodeManager::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::MosaicNodeManager")
                .SetParent<Object>()
                .AddConstructor<MosaicNodeManager>()
                // Attributes are only set _after_ constructor ran
                .AddAttribute("numExtraRadioNodes", "Number of extra spare radio nodes, usable after simulation started",
                UintegerValue(10),
                MakeUintegerAccessor(&MosaicNodeManager::m_numExtraRadioNodes),
                MakeUintegerChecker<uint16_t> ())
                ;
        return tid;
    }

    MosaicNodeManager::MosaicNodeManager() 
      : m_backboneAddressHelper("5.0.0.0", "255.0.0.0"),
        m_wifiAddressHelper("6.0.0.0", "255.0.0.0", "0.0.0.2") {

        /** Helpers **/
        // Wifi
        m_wifiChannelHelper.AddPropagationLoss("ns3::FriisPropagationLossModel");
        m_wifiChannelHelper.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
        Ptr<YansWifiChannel> channel = m_wifiChannelHelper.Create();
        m_wifiPhyHelper.SetChannel(channel);
        // ns3::WifiPhy::ChannelWidth|ChannelNumber|Frequency are set via ns3_federate_config.xml
        m_wifiMacHelper.SetType ("ns3::AdhocWifiMac", "QosSupported", BooleanValue (true));
        m_wifiHelper.SetStandard (WIFI_STANDARD_80211p);
        m_wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("OfdmRate6MbpsBW10MHz"),
                                "ControlMode", StringValue ("OfdmRate6MbpsBW10MHz"),
                                "NonUnicastMode", StringValue ("OfdmRate6MbpsBW10MHz"));

        // LTE
        m_lteHelper = CreateObject<LteHelper> ();
        // This EpcHelper creates point-to-point links between the eNBs and the EPCore (3 nodes)
        m_epcHelper = CreateObject<PointToPointEpcHelper> (); 
        m_lteHelper->SetEpcHelper (m_epcHelper);
        m_lteHelper->Initialize ();
        // Wired
        m_csmaHelper.SetChannelAttribute("DataRate", StringValue("100Gb/s"));
        m_csmaHelper.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
        
        /* enable for debugging null-pointer-exceptions (log spam!)*/
        // m_lteHelper->EnableLogComponents();
    }

    void MosaicNodeManager::Configure(MosaicNs3Bridge* serverPtr) {
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
    }

    void MosaicNodeManager::OnStart() {
        NS_LOG_INFO ("Do the final configuration...");

        m_lteHelper->AddX2Interface (m_enbNodes); // required for handover capabilities

        // NS_LOG_INFO("Schedule manual handovers...");
        // m_lteHelper->HandoverRequest (Seconds (3.000), lteDevices.Get (1), m_enbDevices.Get (0), m_enbDevices.Get (1));

        /* 
         * We create extra radioNodes now, because ns3 does not allow to create them after simulation start.
         * see "Cannot create UE devices after simulation started" at https://gitlab.com/nsnam/ns-3-dev/-/blob/master/src/lte/model/lte-ue-phy.cc#L144
         */ 
        NS_LOG_INFO("Setup extra radioNode's...");
        for (uint32_t i = 0; i < m_numExtraRadioNodes; i++ ){
            Ptr<Node> node = CreateRadioNodeHelper();
            m_extraRadioNodes.Add (node);
        }

        PrintNodeConfigs(m_enbNodes, 10);
        PrintNodeConfigs(m_backboneNodes, 10);
        PrintNodeConfigs(m_radioNodes, 10);
        PrintNodeConfigs(m_extraRadioNodes, 10);
    }

    void MosaicNodeManager::OnShutdown() {
        NS_LOG_FUNCTION (this);

        NS_LOG_DEBUG("Print IP assignment for all radioNodes");
        PrintNodeConfigs(m_radioNodes);
    }

    void MosaicNodeManager::PrintNodeConfigsDeviceAgnostic(NodeContainer nodes, uint32_t maxNum) {
        for (uint32_t u = 0; u < nodes.GetN () && u < maxNum; ++u)
        {
            Ptr<Node> node = nodes.Get(u);
            Ptr<Ipv4> ipv4proto = node->GetObject<Ipv4>();
            // NS_ASSERT(node->GetObject<Ipv4> ()->GetNInterfaces () == node->GetNDevices ()); // fails. Ipv4Interfaces require IP address
            // device->GetIfIndex () and node->GetDevice(i) are unrelated to indexing in ipv4proto! e.g. ipv4proto->GetAddress (i, 0) is not the same 'i'

            NS_LOG_DEBUG("[node=" << node->GetId () << "]");
            for (uint32_t i = 0; i < node->GetNDevices (); i++ )
            {
                Ipv4InterfaceAddress iaddr;
                Ptr<NetDevice> device = node->GetDevice(i);
                int32_t ipif = DynamicCast<Ipv4L3Protocol>(ipv4proto)->GetInterfaceForDevice(device);
                if (ipif != -1) {
                    iaddr = ipv4proto->GetAddress (ipif, 0);
                    NS_LOG_DEBUG("  if_" << i << " dev=" << device << " type=" << device->GetInstanceTypeId ().GetName () << " iaddr=" << iaddr);
                } else {
                    NS_LOG_DEBUG("  if_" << i << " dev=" << device << " type=" << device->GetInstanceTypeId ().GetName ());
                }
            }
        }
    }

    void MosaicNodeManager::PrintNodeConfigs(NodeContainer nodes, uint32_t maxNum) {
        for (uint32_t u = 0; u < nodes.GetN () && u < maxNum; ++u)
        {
            Ptr<Node> node = nodes.Get(u);
            Ptr<Ipv4> ipv4proto = node->GetObject<Ipv4>();

            NS_LOG_DEBUG("[node=" << node->GetId () << "]");
            for (uint32_t i = 0; i < node->GetNDevices (); i++ )
            {
                Ptr<NetDevice> device = node->GetDevice (i);

                int32_t ipif = DynamicCast<Ipv4L3Protocol>(ipv4proto)->GetInterfaceForDevice(device);
                std::stringstream ipAddressString;
                if (ipif != -1) {
                    for (uint32_t j = 0; j < ipv4proto->GetNAddresses (ipif); j++ ) {
                        Ipv4InterfaceAddress iaddr = ipv4proto->GetAddress (ipif, j);
                        ipAddressString << "|" << iaddr.GetLocal ();
                    }
                }


                if (DynamicCast<LoopbackNetDevice>(device)) {
                    // nop
                }
                else if (DynamicCast<CsmaNetDevice>(device)) {
                    NS_LOG_DEBUG("  if_" << i 
                        << " dev=" << device 
                        << " ETH"
                        << " \taddr=" << ipAddressString.str()
                    );
                }
                else if (DynamicCast<PointToPointNetDevice>(device)) {
                    NS_LOG_DEBUG("  if_" << i 
                        << " dev=" << device 
                        << " P2P"
                        << " \taddr=" << ipAddressString.str()
                    );
                }
                else if (DynamicCast<WifiNetDevice>(device)) {
                    NS_LOG_DEBUG("  if_" << i 
                        << " dev=" << device 
                        << " WIFI"
                        << " \taddr=" << ipAddressString.str()
                    );
                }
                else if (DynamicCast<LteUeNetDevice>(device)) {
                    NS_LOG_DEBUG("  if_" << i
                        << " dev=" << device 
                        << " UE"
                        << " \taddr=" << ipAddressString.str()
                        << " rrc=" << device->GetObject<LteUeNetDevice> ()->GetRrc ()
                        << " imsi=" << device->GetObject<LteUeNetDevice> ()->GetRrc ()->GetImsi ()
                    );
                } 
                else if (DynamicCast<LteEnbNetDevice>(device)) {
                    NS_LOG_DEBUG("  if_" << i
                        << " dev=" << device 
                        << " ENB"
                        << " \taddr=" << ipAddressString.str()
                    );
                }
                else {
                    NS_LOG_DEBUG("  if_" << i
                        << " dev=" << device
                        << " type=" << device->GetInstanceTypeId ().GetName ()
                        << " \taddr=" << ipAddressString.str()
                    );
                }
            }
        }
        if (nodes.GetN() > 0) {
            std::stringstream ss;
            nodes.Get (0)->GetObject<Ipv4> ()->GetRoutingProtocol ()->PrintRoutingTable (new OutputStreamWrapper(&ss));
            NS_LOG_LOGIC(ss.str());
        }
    }

    void MosaicNodeManager::RejectAnyUeConnectionRequest() {
        NS_LOG_FUNCTION (this);
        NS_LOG_WARN("-------------------- change eNB settings now");
        NS_LOG_WARN("-------------------- only accept handover algorithm triggers");
        NS_LOG_WARN("-------------------- UEs cannot recover, if connection got lost once");
        Config::SetDefault("ns3::LteEnbRrc::AdmitRrcConnectionRequest", BooleanValue(false));
        for (uint32_t i = 0; i < m_enbDevices.GetN (); i++ ){
            Ptr<LteEnbNetDevice> device = m_enbDevices.Get (i)->GetObject<LteEnbNetDevice> ();
            Ptr<LteEnbRrc> rrc = device->GetRrc ();
            rrc->m_admitRrcConnectionRequest = false; // changes in ns3 required: make variable public
        }
    }

    uint32_t MosaicNodeManager::GetNs3NodeId(uint32_t mosaicNodeId) {
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

    uint32_t MosaicNodeManager::GetMosaicNodeId(uint32_t ns3NodeId) {
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

    void MosaicNodeManager::CreateNodeB(Vector position) {
        Ptr<Node> node = CreateObject<Node>();
        m_enbNodes.Add (node);
        m_mobilityHelper.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        m_mobilityHelper.Install (node);
        Ptr<NetDevice> device = m_lteHelper->InstallEnbDevice (node).Get(0);
        m_enbDevices.Add (device);
        NS_LOG_INFO("[node=" << node->GetId() << "] Create eNodeB: dev=" << device);

        // set position
        Ptr<MobilityModel> mobModel = node->GetObject<MobilityModel> ();
        mobModel->SetPosition(position);
    }

    void MosaicNodeManager::CreateWiredNode(uint32_t mosaicNodeId) {
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
        Ptr<MosaicProxyApp> app = CreateObject<MosaicProxyApp>();
        app->SetRecvCallback(MakeCallback(&MosaicNodeManager::RecvCellMsg, this));
        node->AddApplication(app);
        app->SetSockets(interface_e::ETH);
    }

    Ptr<Node> MosaicNodeManager::CreateRadioNodeHelper(void) {
        /* create node */
        Ptr<Node> node = CreateObject<Node>();

        m_internetHelper.Install(node);
        Ptr<Ipv4> ipv4proto = node->GetObject<Ipv4>();

        /* Install ConstantVelocityMobilityModel */
        m_mobilityHelper.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
        m_mobilityHelper.Install (node);

        /* Install WIFI devices */
        NetDeviceContainer wifiDevices = m_wifiHelper.Install(m_wifiPhyHelper, m_wifiMacHelper, node);
        Ipv4InterfaceContainer wifiIpIfaces = m_wifiAddressHelper.Assign(wifiDevices);

        /* Install LTE devices */
        NetDeviceContainer lteDevices = m_lteHelper->InstallUeDevice (node);
        m_epcHelper->AssignUeIpv4Address (lteDevices);

        // set the default gateway for the UE
        uint32_t ifIndex = 2;
        Ptr<Ipv4StaticRouting> ueStaticRouting = m_ipv4RoutingHelper.GetStaticRouting (ipv4proto);
        ueStaticRouting->SetDefaultRoute (m_epcHelper->GetUeDefaultGatewayAddress (), ifIndex); // DefaultGateway is 7.0.0.1

        /* Install ProxyApp applications */
        Ptr<MosaicProxyApp> wifiApp = CreateObject<MosaicProxyApp>();
        wifiApp->SetRecvCallback(MakeCallback(&MosaicNodeManager::RecvWifiMsg, this));
        node->AddApplication(wifiApp);
        wifiApp->SetSockets(interface_e::WIFI);

        Ptr<MosaicProxyApp> cellApp = CreateObject<MosaicProxyApp>();
        cellApp->SetRecvCallback(MakeCallback(&MosaicNodeManager::RecvCellMsg, this));
        node->AddApplication(cellApp);
        cellApp->SetSockets(interface_e::CELL);

        return node;
    }

    void MosaicNodeManager::CreateRadioNode(uint32_t mosaicNodeId, Vector position) {
        if (m_mosaic2nsdrei.find(mosaicNodeId) != m_mosaic2nsdrei.end()){
            NS_LOG_ERROR("Cannot create node with id=" << mosaicNodeId << " multiple times.");
            exit(1);
        }

        Ptr<Node> node = CreateRadioNodeHelper();

        NS_LOG_INFO("Create radio node " << mosaicNodeId << "->" << node->GetId());
        m_mosaic2nsdrei[mosaicNodeId] = node->GetId();
        m_nsdrei2mosaic[node->GetId()] = mosaicNodeId;
        m_isRadioNode[node->GetId()] = true;
        m_radioNodes.Add (node);
        
        UpdateNodePosition(mosaicNodeId, position);
    }

    void MosaicNodeManager::ActivateRadioNode(uint32_t mosaicNodeId, Vector position) {
        if (m_mosaic2nsdrei.find(mosaicNodeId) != m_mosaic2nsdrei.end()){
            NS_LOG_ERROR("Cannot create node with id=" << mosaicNodeId << " multiple times.");
            exit(1);
        }

        Ptr<Node> node;
        for (uint32_t i = 0; i < m_extraRadioNodes.GetN(); ++i)
        {
            node = m_extraRadioNodes.Get(i);
            if (m_nsdrei2mosaic.find(node->GetId()) == m_nsdrei2mosaic.end()){
                // the node is not used yet, add it to the lookup tables
                NS_LOG_INFO("Activate radio node " << mosaicNodeId << "->" << node->GetId());
                m_mosaic2nsdrei[mosaicNodeId] = node->GetId();
                m_nsdrei2mosaic[node->GetId()] = mosaicNodeId;
                m_isRadioNode[node->GetId()] = true;
                m_radioNodes.Add (node);
                break;
            }
        }
        
        if (m_mosaic2nsdrei.find(mosaicNodeId) == m_mosaic2nsdrei.end()){
            NS_LOG_ERROR("No available node found. Increase number of extra radio nodes!");
            exit(1);
        }

        UpdateNodePosition(mosaicNodeId, position);
    }

    void MosaicNodeManager::UpdateNodePosition(uint32_t mosaicNodeId, Vector position) {
        uint32_t nodeId = GetNs3NodeId(mosaicNodeId);
        if (m_isDeactivated[nodeId]) {
            return;
        }

        // NS_LOG_INFO("[node=" << nodeId << "] x=" << position.x << " y=" << position.y << " z=" << position.z);

        Ptr<Node> node = NodeList::GetNode(nodeId);
        Ptr<MobilityModel> mobModel = node->GetObject<MobilityModel> ();
        mobModel->SetPosition(position);
    }

    void MosaicNodeManager::RemoveNode(uint32_t mosaicNodeId) {
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
            Ptr<MosaicProxyApp> app = DynamicCast<MosaicProxyApp> (node->GetApplication(0));
            if (!app) {
                NS_LOG_ERROR("No app with index=" << i << " found on node " << nodeId << " !");
                exit(1);
            }
            app->Disable();
        }

        m_isDeactivated[nodeId] = true;
    }

    void MosaicNodeManager::ConfigureWifiRadio(uint32_t mosaicNodeId, double transmitPower, Ipv4Address ip) {
        uint32_t nodeId = GetNs3NodeId(mosaicNodeId);
        if (m_isDeactivated[nodeId]) {
            return;
        }
        if (m_isWifiRadioConfigured[nodeId]) {
            NS_LOG_ERROR("Cannot configure WIFI interface multiple times. Ignoring.");
            return;
        }
        m_isWifiRadioConfigured[nodeId] = true;

        NS_ASSERT_MSG(m_isRadioNode[nodeId], "Cannot have a wifi interface on a wired node.");

        NS_LOG_INFO("[node=" << nodeId << "] txPow=" << transmitPower << " ip=" << ip);
        
        Ptr<Node> node = NodeList::GetNode(nodeId);
        Ptr<MosaicProxyApp> wifiApp = DynamicCast<MosaicProxyApp> (node->GetApplication(0));
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
            Ptr<YansWifiPhy> phy = DynamicCast<YansWifiPhy> (netDev->GetPhy());
            NS_LOG_INFO("[node=" << nodeId << "] Adjust settings on dev="<< netDev << " phy=" << phy);
            if (phy != 0) {
                double txDBm = 10 * log10(transmitPower);
                phy->SetTxPowerStart(txDBm);
                phy->SetTxPowerEnd(txDBm);
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

    void MosaicNodeManager::ConfigureCellRadio(uint32_t mosaicNodeId, Ipv4Address ip) {
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
            Ptr<MosaicProxyApp> cellApp = DynamicCast<MosaicProxyApp> (node->GetApplication(1));
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

            NS_LOG_INFO("Attach UE to specific eNB...");
            NS_LOG_INFO("ATTENTION: This requires about 21ms to fully connect");
            // this has to be done _after_ IP address assignment, otherwise the route EPC -> UE is broken
            m_lteHelper->AttachToClosestEnb (device, m_enbDevices);

        } else if (m_isWiredNode[nodeId]) {

            NS_ASSERT_MSG(partOf105 || partOf106, "The ip for wired nodes must be part of 10.5.0.0/16 or 10.6.0.0/16 network.");

            /* activate application */
            Ptr<MosaicProxyApp> csmaApp = DynamicCast<MosaicProxyApp> (node->GetApplication(0));
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

        } else {
            NS_LOG_ERROR("Invalid State: Node has to be either radio or wired node.");
            exit(1);
        }
    }

    void MosaicNodeManager::SendWifiMsg(uint32_t mosaicNodeId, Ipv4Address dstAddr, ClientServerChannelSpace::RadioChannel channel, uint32_t msgID, uint32_t payLength) {
        uint32_t nodeId = GetNs3NodeId(mosaicNodeId);
        if (m_isDeactivated[nodeId]) {
            return;
        }
        if (channel != ClientServerChannelSpace::RadioChannel::PROTO_CCH) {
            NS_LOG_ERROR("Ns3 only supports one pre-configured wifi channel. Expect value CCH.");
            exit(1);
        }
        NS_LOG_DEBUG("[node=" << nodeId << "] dst=" << dstAddr << " msgID=" << msgID << " len=" << payLength);

        NS_ASSERT_MSG(m_isRadioNode[nodeId], "Cannot use Wifi communication on wired nodes.");
        Ptr<Node> node = NodeList::GetNode(nodeId);
        Ptr<MosaicProxyApp> app = DynamicCast<MosaicProxyApp> (node->GetApplication(0));
        if (app == nullptr) {
            NS_LOG_ERROR("Node " << nodeId << " was not initialized properly, MosaicProxyApp is missing");
            return;
        }
        app->TransmitPacket(dstAddr, msgID, payLength);
    }

    void MosaicNodeManager::SendCellMsg(uint32_t mosaicNodeId, Ipv4Address dstAddr, uint32_t msgID, uint32_t payLength) {
        uint32_t nodeId = GetNs3NodeId(mosaicNodeId);
        if (m_isDeactivated[nodeId]) {
            return;
        }
        NS_LOG_DEBUG("[node=" << nodeId << "] dst=" << dstAddr << " msgID=" << msgID << " len=" << payLength);


        Ptr<Node> node = NodeList::GetNode(nodeId);
        Ptr<MosaicProxyApp> app;
        if (m_isRadioNode[nodeId]) {
            app = DynamicCast<MosaicProxyApp> (node->GetApplication(1));
        } else if (m_isWiredNode[nodeId]) {
            app = DynamicCast<MosaicProxyApp> (node->GetApplication(0));
        }
        if (app == nullptr) {
            NS_LOG_ERROR("Node " << nodeId << " was not initialized properly, MosaicProxyApp is missing");
            return;
        }
        app->TransmitPacket(dstAddr, msgID, payLength);
    }

    void MosaicNodeManager::RecvWifiMsg(unsigned long long recvTime, uint32_t ns3NodeId, int msgID) {
        if (m_isDeactivated[ns3NodeId]) {
            return;
        }
        uint32_t nodeId = GetMosaicNodeId(ns3NodeId);
        
        m_serverPtr->writeReceiveWifiMessage(recvTime, nodeId, msgID);
    }


    void MosaicNodeManager::RecvCellMsg(unsigned long long recvTime, uint32_t ns3NodeId, int msgID) {
        if (m_isDeactivated[ns3NodeId]) {
            return;
        }
        uint32_t nodeId = GetMosaicNodeId(ns3NodeId);
        
        m_serverPtr->writeReceiveCellMessage(recvTime, nodeId, msgID);
    }
} // namespace ns3

