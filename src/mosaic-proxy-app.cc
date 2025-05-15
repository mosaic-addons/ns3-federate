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

    void MosaicProxyApp::SetSockets(void) {
        NS_LOG_FUNCTION(GetNode()->GetId());

        if (!m_socket) {

            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
            m_socket->Bind(local);
            m_socket->SetAllowBroadcast(true);

            m_socket->SetRecvCallback(MakeCallback(&MosaicProxyApp::Receive, this));
        } else {
            NS_FATAL_ERROR("creation attempt of a socket for MosaicProxyApp that has already a socket active");
            return;
        }
    }

    void MosaicProxyApp::TransmitPacket(uint32_t protocolID, uint32_t msgID, uint32_t payLength, Ipv4Address address) {
        NS_LOG_FUNCTION(GetNode()->GetId() << protocolID << msgID << payLength << address);

        if (!m_active) {
            return;
        }

        Ptr<Packet> packet = Create<Packet> (payLength);
        //Flow tag is used to match the sent message
        FlowIdTag msgIDTag;
        msgIDTag.SetFlowId(msgID);
        packet->AddByteTag(msgIDTag);

        m_sendCount++;
        NS_LOG_INFO("[node=" << GetNode()->GetId() << "] Sending packet no. " << m_sendCount << " msgID=" << msgID << " PacketID=" << packet->GetUid() << " now=" << Simulator::Now().GetNanoSeconds() << "ns size=" << packet->GetSize());

        //call the socket of this node to send the packet
        InetSocketAddress ipSA = InetSocketAddress(address, m_port);
        m_socket->SendTo(packet, 0, ipSA);
    }

    /*
     * @brief Receive a packet from the socket
     * This method is called by the callback which is defined in the method MosaicProxyApp::SetSockets
     */
    void MosaicProxyApp::Receive(Ptr<Socket> socket) {
        NS_LOG_FUNCTION(GetNode()->GetId());
        if (!m_active) {
            return;
        }

        Ptr<Packet> packet;
        NS_LOG_INFO("[node=" << GetNode()->GetId() << "] Start receiving...");
        packet = socket->Recv();

        m_recvCount++;

        FlowIdTag Tag;
        int msgID;
        //get the flowIdTag
        if (packet->FindFirstMatchingByteTag(Tag)) {
            //send the MsgID
            msgID = Tag.GetFlowId();
            //find the message and send it back
        } else {
            NS_LOG_ERROR("Error, message has no msgIdTag");
            msgID = -1;
        }

        //report the received messages to the MosaicNs3Server instance
        m_nodeManager->AddRecvPacket(Simulator::Now().GetNanoSeconds(), packet, GetNode()->GetId(), msgID);
        NS_LOG_INFO("[node=" << GetNode()->GetId() << "] Received message no. " << m_recvCount << " msgID=" << msgID << " PacketID=" << packet->GetUid() << " now=" << Simulator::Now().GetNanoSeconds() << "ns size=" << packet->GetSize());
    }
} // namespace ns3
