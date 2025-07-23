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

#include "proxy-app.h"

#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/flow-id-tag.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE("ProxyApp");

namespace ns3 {

    NS_OBJECT_ENSURE_REGISTERED(ProxyApp);

    TypeId ProxyApp::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::ProxyApp")
                .SetParent<Application> ()
                .AddConstructor<ProxyApp> ()
                .AddAttribute("Port", "The socket port for messages",
                UintegerValue(8010),
                MakeUintegerAccessor(&ProxyApp::m_port),
                MakeUintegerChecker<uint16_t> ())
                ;

        return tid;
    }

    void ProxyApp::SetRecvCallback(Callback<void, unsigned long long, uint32_t, int> cb) {
        NS_LOG_FUNCTION (this << &cb);
        m_recvCallback = cb;
    }

    void ProxyApp::DoDispose(void) {
        NS_LOG_FUNCTION_NOARGS();
        m_socket = 0;
        m_recvCallback = MakeNullCallback<void, unsigned long long, uint32_t, int> ();
        Application::DoDispose();
    }

    void ProxyApp::Enable(void) {
        m_active = true;
        m_trace = GetLogComponent("ProxyApp").IsEnabled(LOG_DEBUG);
    }

    void ProxyApp::Disable(void) {
        m_active = false;
    }

    int ProxyApp::TranslateNumberToIndex(int outDevice) {
        // Expected Input is 1:Wifi 2:LTE 3:Csma
        // Radio Devices are 0:Loopback 1:Wifi 2:LTE
        // Wired Devices are 0:Loopback 1:Csma
        switch (outDevice){
            case 1: 
                return 1;
            case 2: 
                return 2;
            case 3: 
                return 1;
            default: return -1;
        }
    }

    void ProxyApp::SetSockets(int outDevice) {
        NS_LOG_FUNCTION(GetNode()->GetId());

        if (m_socket) {
            NS_FATAL_ERROR("Ignore creation attempt of a socket for ProxyApp that has already a socket active. ");
            return;
        }

        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
        m_socket->Bind(local);
        if(outDevice > 0) {
            int outDeviceIndex = TranslateNumberToIndex(outDevice);
            m_socket->BindToNetDevice (GetNode()->GetDevice(outDeviceIndex));
        }
        m_socket->SetAllowBroadcast(true);
        m_socket->SetRecvCallback(MakeCallback(&ProxyApp::Receive, this));
        m_outDevice = outDevice;
    }

    void ProxyApp::TransmitPacket(Ipv4Address dstAddr, uint32_t msgID, uint32_t payLength) {
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
        NS_LOG_DEBUG("[node=" << GetNode()->GetId() << "." << m_outDevice << "] dst=" << dstAddr << " msgID=" << msgID << " len=" << payLength << " PacketID=" << packet->GetUid() << " PacketCount=" << m_sendCount);
        NS_LOG_DEBUG("[node=" << GetNode()->GetId() << "." << m_outDevice << "] Sending packet no. " << m_sendCount << " msgID=" << msgID << " PacketID=" << packet->GetUid());
        if (m_trace) {
            LogComponentEnable ("TrafficControlLayer", (LogLevel)(LOG_DEBUG | LOG_PREFIX_NODE));
        }


        //call the socket of this node to send the packet
        InetSocketAddress ipSA = InetSocketAddress(dstAddr, m_port);
        int result = m_socket->SendTo(packet, 0, ipSA);
        if (result == -1) {
            NS_LOG_ERROR("[node=" << GetNode()->GetId() << "." << m_outDevice << "] Sending packet failed!");
            if (m_socket->GetErrno () == Socket::SocketErrno::ERROR_MSGSIZE) {
                NS_LOG_ERROR("Can only use up to MAX_IPV4_UDP_DATAGRAM_SIZE = 65507 Bytes per packet");
            } else {
                NS_LOG_ERROR("Errno:" << m_socket->GetErrno ());
            }
            exit(1);
        }
    }

    /*
     * @brief Receive a packet from the socket
     * This method is called by the callback which is defined in the method ProxyApp::SetSockets
     */
    void ProxyApp::Receive(Ptr<Socket> socket) {
        NS_LOG_FUNCTION(GetNode()->GetId());
        if (!m_active) {
            // This happens e.g. for wifi broadcasts on un-initialized ns3 nodes (aka unused by mosaic)
            // NS_LOG_WARN("[node=" << GetNode()->GetId() << "." << m_outDevice << "] This app is disabled but it received a packet. Ignore.");
            return;
        }

        Ptr<Packet> packet;
        NS_LOG_DEBUG("[node=" << GetNode()->GetId() << "." << m_outDevice << "] Start receiving...");
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

        NS_LOG_DEBUG("[node=" << GetNode()->GetId() << "." << m_outDevice << "] Received message no. " << m_recvCount << " msgID=" << msgID << " PacketID=" << packet->GetUid() << " now=" << Simulator::Now().GetNanoSeconds() << "ns len=" << packet->GetSize());
        if (m_trace) {
            LogComponentDisable ("TrafficControlLayer", LOG_DEBUG);
        }

        if (!m_recvCallback.IsNull()) {
            m_recvCallback(Simulator::Now().GetNanoSeconds(), GetNode()->GetId(), msgID);
        } else {
            NS_LOG_ERROR("Received a packet but have no possibility to forward up. Ignore.");
        }

        /* Add one slash, to enable this development test 
        if (m_outDevice == 3 && msgID == 1) {
            // ping pong a packet back to fixed IP
            Ipv4Address dst("10.3.0.20");
            TransmitPacket(dst, msgID, 1234);
        }
        //*/
    }
} // namespace ns3
