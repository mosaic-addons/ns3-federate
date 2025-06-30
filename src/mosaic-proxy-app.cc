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

#include "mosaic-proxy-app.h"

#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/flow-id-tag.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE("MosaicProxyApp");

namespace ns3 {

    NS_OBJECT_ENSURE_REGISTERED(MosaicProxyApp);

    TypeId MosaicProxyApp::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::MosaicProxyApp")
                .SetParent<Application> ()
                .AddConstructor<MosaicProxyApp> ()
                .AddAttribute("Port", "The socket port for messages",
                UintegerValue(8010),
                MakeUintegerAccessor(&MosaicProxyApp::m_port),
                MakeUintegerChecker<uint16_t> ())
                ;

        return tid;
    }

    void MosaicProxyApp::SetNodeManager(MosaicNodeManager* nodeManager) {
        m_nodeManager = nodeManager;
    }

    void MosaicProxyApp::DoDispose(void) {
        NS_LOG_FUNCTION_NOARGS();
        m_socket = 0;
        Application::DoDispose();
    }

    void MosaicProxyApp::Enable(void) {
        m_active = true;
    }

    void MosaicProxyApp::Disable(void) {
        m_active = false;
    }

    void MosaicProxyApp::SetSockets(int outDevice) {
        NS_LOG_FUNCTION(GetNode()->GetId());

        if (m_socket) {
            NS_FATAL_ERROR("Ignore creation attempt of a socket for MosaicProxyApp that has already a socket active. ");
            return;
        }

        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
        m_socket->Bind(local);
        if(GetNode()->GetNDevices() == 3) {
            // Devices are 0:Loopback 1:Wifi 2:LTE
            m_socket->BindToNetDevice (GetNode()->GetDevice(outDevice));
        } else {
            NS_LOG_WARN("Installing app on node which has not exactly 3 devices...");
        }
        m_socket->SetAllowBroadcast(true);
        m_socket->SetRecvCallback(MakeCallback(&MosaicProxyApp::Receive, this));
        m_outDevice = outDevice;
    }

    void MosaicProxyApp::TransmitPacket(Ipv4Address dstAddr, uint32_t msgID, uint32_t payLength) {
        NS_LOG_FUNCTION(GetNode()->GetId() << dstAddr << msgID << payLength);

        if (!m_active) {
            NS_LOG_WARN("[node=" << GetNode()->GetId() << "." << m_outDevice << "] This app is disabled but should transmit a packet. Ignore.");
            return;
        }

        Ptr<Packet> packet = Create<Packet> (payLength);
        //Flow tag is used to match the sent message
        FlowIdTag msgIDTag;
        msgIDTag.SetFlowId(msgID);
        packet->AddByteTag(msgIDTag);

        m_sendCount++;
        NS_LOG_INFO("[node=" << GetNode()->GetId() << "." << m_outDevice << "] dst=" << dstAddr << " msgID=" << msgID << " len=" << payLength << " PacketID=" << packet->GetUid() << " PacketCount=" << m_sendCount);
        NS_LOG_INFO("[node=" << GetNode()->GetId() << "." << m_outDevice << "] Sending packet no. " << m_sendCount << " msgID=" << msgID << " PacketID=" << packet->GetUid());
        LogComponentEnable ("TrafficControlLayer", (LogLevel)(LOG_DEBUG | LOG_PREFIX_NODE));

        //call the socket of this node to send the packet
        InetSocketAddress ipSA = InetSocketAddress(dstAddr, m_port);
        int result = m_socket->SendTo(packet, 0, ipSA);
        if (result == -1) {
            NS_LOG_ERROR("[node=" << GetNode()->GetId() << "." << m_outDevice << "] Sending packet failed!");
            exit(1);
        }
    }

    /*
     * @brief Receive a packet from the socket
     * This method is called by the callback which is defined in the method MosaicProxyApp::SetSockets
     */
    void MosaicProxyApp::Receive(Ptr<Socket> socket) {
        NS_LOG_FUNCTION(GetNode()->GetId());
        if (!m_active) {
            NS_LOG_WARN("[node=" << GetNode()->GetId() << "." << m_outDevice << "] This app is disabled but it received a packet. Ignore.");
            return;
        }

        Ptr<Packet> packet;
        NS_LOG_INFO("[node=" << GetNode()->GetId() << "." << m_outDevice << "] Start receiving...");
        packet = socket->Recv();

        m_recvCount++;

        FlowIdTag Tag;
        int msgID;
        //get the flowIdTag
        if (packet->FindFirstMatchingByteTag(Tag)) {
            //send the MsgID
            msgID = Tag.GetFlowId();
        } else {
            NS_LOG_ERROR("Error, message has no msgIdTag");
            msgID = -1;
        }

        NS_LOG_INFO("[node=" << GetNode()->GetId() << "." << m_outDevice << "] Received message no. " << m_recvCount << " msgID=" << msgID << " PacketID=" << packet->GetUid() << " now=" << Simulator::Now().GetNanoSeconds() << "ns len=" << packet->GetSize());
        LogComponentDisable ("TrafficControlLayer", LOG_DEBUG);

        if (m_nodeManager != 0) {
            //report the received messages to the MosaicNs3Server instance
            m_nodeManager->AddRecvPacket(Simulator::Now().GetNanoSeconds(), packet, GetNode()->GetId(), msgID);
        } else {
            // as server (currently m_nodeManager == 0): ping pong a packet back to fixed IP
            /* Add one slash, to enable this test 
            std::cout << std::endl;
            Ipv4Address ip("7.0.0.4");
            TransmitPacket(ip, 1234, 124);
            //*/
        }
    }
} // namespace ns3
