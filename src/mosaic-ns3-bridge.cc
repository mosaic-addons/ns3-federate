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

#include "mosaic-ns3-bridge.h"

#include "mosaic-simulator-impl.h"

NS_LOG_COMPONENT_DEFINE("MosaicNs3Bridge");

namespace ns3 {
    using namespace ClientServerChannelSpace;

    MosaicNs3Bridge::MosaicNs3Bridge(int port, int cmdPort) {
        std::cout << "Starting ns3 federate on OutPort=" << port << " CmdPort=" << cmdPort << std::endl;

        m_sim = DynamicCast<MosaicSimulatorImpl> (Simulator::GetImplementation());
        if (nullptr == m_sim) {
            NS_LOG_ERROR("Could not find MosaicSimulatorImpl");
            m_closeConnection = true;
            return;
        }
        m_sim->AttachBridge(this);

        m_nodeManager = CreateObject<MosaicNodeManager>();
        m_nodeManager->Configure(this);
        // m_sim->Schedule(Seconds(10), MakeEvent(&MosaicNodeManager::RejectAnyUeConnectionRequest, m_nodeManager));
        
        m_closeConnection = false;
        m_didRunOnStart = false;
        
        if (Time::GetResolution () == Time::NS) {
            NS_LOG_INFO("Have time scale NS - use factor 1.");
            m_timeFactor = 1;
        } else if (Time::GetResolution () == Time::US) {
            NS_LOG_INFO("Have time scale US - use factor 1000.");
            m_timeFactor = 1000;
        } else if (Time::GetResolution () == Time::MS) {
            NS_LOG_INFO("Have time scale MS - use factor 1000000.");
            m_timeFactor = 1000000;
        } else {
            NS_LOG_ERROR("Unknown time scale. " << Time::GetResolution());
            exit(1);
        }

        /* Initialize federateAmbassadorChannel (mostly for SENDING) */
        NS_LOG_INFO("Initialize federateAmbassadorChannel");
        federateAmbassadorChannel.prepareConnection("0.0.0.0", port);
        federateAmbassadorChannel.connect();
        federateAmbassadorChannel.writeCommand(CommandMessage_CommandType_INIT);

        /* Initialize ambassadorFederateChannel (mostly for RECEIVING) */
        NS_LOG_INFO("Initialize ambassadorFederateChannel");
        uint16_t assignedPort = ambassadorFederateChannel.prepareConnection("0.0.0.0", cmdPort);
        if (assignedPort < 1) {
            std::cout << "Could not prepare port for Command Channel" << std::endl;
            exit(1);
        }
        federateAmbassadorChannel.writePort(assignedPort);
        ambassadorFederateChannel.connect();
        if (ambassadorFederateChannel.readCommand() == CommandMessage_CommandType_INIT) {
            InitMessage message = ambassadorFederateChannel.readInitMessage();
            if (message.simulation_start_time() >= 0 
                    && message.simulation_end_time() >= 0 
                    && message.simulation_end_time() >= message.simulation_start_time()) {
                ambassadorFederateChannel.writeCommand(CommandMessage_CommandType_SUCCESS);
            } else {
                // AbstractNetworkAmbassador.java only checks if (CMD.SUCCESS != ...
                ambassadorFederateChannel.writeCommand(CommandMessage_CommandType_SHUT_DOWN);
                NS_LOG_ERROR("Did not receive meaningful times in first CMD_INIT");
                exit(1);
            }
        } else {
            NS_LOG_ERROR("Did not receive CMD_INIT as first message");
            exit(1);
        }
        NS_LOG_INFO("Created new connection on port " << port);
    }

    MosaicNs3Bridge::~MosaicNs3Bridge() {
        m_closeConnection = true;
    }

    void MosaicNs3Bridge::run() {
        try {
            if (m_closeConnection) {
                return;                
            }

            NS_LOG_INFO("Now enter the infinite simulation loop...");
            while (!m_closeConnection) {
                dispatchCommand();
            }

        } catch (std::invalid_argument &e) {
            NS_LOG_ERROR("Invalid argument in run() " << e.what());
            m_closeConnection = true;
        }
        NS_LOG_INFO("Finishing server.... ");
    }

    void MosaicNs3Bridge::dispatchCommand() {
        //read the commandId from the channel
        CommandMessage_CommandType commandId = ambassadorFederateChannel.readCommand();
        switch (commandId) {
            case CommandMessage_CommandType_INIT:
                //CMD_INIT is not permitted after the initialization of the MosaicNs3Bridge
                NS_LOG_ERROR("Received CMD_INIT");
                break;

            case CommandMessage_CommandType_ADD_NODE:
            {
                AddNode message = ambassadorFederateChannel.readAddNode();
                Time tNext = NanoSeconds(message.time());
                Time tDelay = tNext - m_sim->Now();

                if (message.type() == AddNode_NodeType_RADIO_NODE) {
                    NS_LOG_DEBUG("Received ADD_RADIO_NODE: mosNID=" << message.node_id() << " pos(x=" << message.x() << " y=" << message.y() << " z=" << message.z() << ") tNext=" << tNext);
                    if (!m_didRunOnStart) {
                        m_nodeManager->CreateRadioNode(message.node_id(), Vector(message.x(), message.y(), message.z()));
                    } else {
                        m_sim->Schedule(tDelay, MakeEvent(&MosaicNodeManager::ActivateRadioNode, m_nodeManager, message.node_id(), Vector(message.x(), message.y(), message.z())));
                    }
                } else if (message.type() == AddNode_NodeType_WIRED_NODE) {
                    NS_LOG_DEBUG("Received ADD_WIRED_NODE: mosNID=" << message.node_id() << " tNext=" << tNext);
                    if (!m_didRunOnStart) {
                        m_nodeManager->CreateWiredNode(message.node_id());
                    } else {
                        m_sim->Schedule(tDelay, MakeEvent(&MosaicNodeManager::CreateWiredNode, m_nodeManager, message.node_id()));
                    }
                } else if (message.type() == AddNode_NodeType_NODE_B) {
                    NS_LOG_DEBUG("Received ADD_NODE_B: pos(x=" << message.x() << " y=" << message.y() << " z=" << message.z() << ") tNext=" << tNext);
                    if (!m_didRunOnStart) {
                        m_nodeManager->CreateNodeB(Vector(message.x(), message.y(), message.z()));
                    } else {
                        NS_LOG_ERROR("Can only add eNBs before simulation start");
                        exit(1);
                    }
                } else {
                    NS_LOG_ERROR("Received unhandeled ADD_..._NODE message");
                    m_closeConnection = true;
                    return;
                }
                ambassadorFederateChannel.writeCommand(CommandMessage_CommandType_SUCCESS);
                break;
            }
            case CommandMessage_CommandType_UPDATE_NODE:
            {
                UpdateNode message = ambassadorFederateChannel.readUpdateNode();
                Time tNext = NanoSeconds(message.time());
                Time tDelay = tNext - m_sim->Now();

                for ( size_t i = 0; i < message.properties_size(); i++ ) { //fill the update messages into our struct
                    UpdateNode_NodeData node_data = message.properties(i);
                    m_sim->Schedule(tDelay, MakeEvent(&MosaicNodeManager::UpdateNodePosition, m_nodeManager, node_data.id(), Vector(node_data.x(), node_data.y(), node_data.z())));
                    NS_LOG_DEBUG("Received UPDATE_NODE(S): mosNID=" << node_data.id() << " pos(x=" << node_data.x() << " y=" << node_data.y() << " z=" << node_data.z() << ") tNext=" << tNext);
                }
                ambassadorFederateChannel.writeCommand(CommandMessage_CommandType_SUCCESS);
                break;
            }
            case CommandMessage_CommandType_REMOVE_NODE:
            {
                RemoveNode message = ambassadorFederateChannel.readRemoveNode();
                Time tNext = NanoSeconds(message.time());
                Time tDelay = tNext - m_sim->Now();
                
                m_sim->Schedule(tDelay, MakeEvent(&MosaicNodeManager::RemoveNode, m_nodeManager, message.node_id()));
                NS_LOG_DEBUG("Received REMOVE_NODE: mosNID=" << message.node_id() << " tNext=" << tNext);

                ambassadorFederateChannel.writeCommand(CommandMessage_CommandType_SUCCESS);
                break;
            }
            // advance the next time step and run the simulation read the next time step
            case CommandMessage_CommandType_ADVANCE_TIME:
            {
                uint64_t advancedTime;
                advancedTime = ambassadorFederateChannel.readTimeMessage();
                Time tNext = NanoSeconds(advancedTime);

                if (tNext == NanoSeconds(0)) {
                    // We need that TrafficControlLayer::DoInitialize() (triggered by Node::Initialize()) 
                    // is called _after_ LteHelper::AddX2Interface()
                    NS_LOG_DEBUG("Ignoring ADVANCE_TIME " << advancedTime);
                    federateAmbassadorChannel.writeCommand(CommandMessage_CommandType_END);
                    federateAmbassadorChannel.writeTimeMessage(Simulator::Now().GetNanoSeconds());
                    break;
                }

                if (tNext > NanoSeconds(0) && !m_didRunOnStart) {
                    m_nodeManager->OnStart();
                    m_didRunOnStart = true;
                }

                m_countTimeAdvanceGrant++;

                // NS_LOG_DEBUG("Received ADVANCE_TIME " << advancedTime); // LTE schedules events every 1ms
                //run the simulation while the time of the next event is smaller than the next time step
                while (!Simulator::IsFinished() && NanoSeconds(advancedTime) >= m_sim->Next()) {
                    m_sim->RunOneEvent();
                }

                // write the confirmation at the end of the sequence
                // this acknowledgement is exceptionally on the other channel (federate->ambassador)
                federateAmbassadorChannel.writeCommand(CommandMessage_CommandType_END);
                federateAmbassadorChannel.writeTimeMessage(Simulator::Now().GetNanoSeconds());
                break;
            }
            case CommandMessage_CommandType_CONF_WIFI_RADIO:
            {
                try {
                    ConfigureWifiRadio message = ambassadorFederateChannel.readConfigureWifiRadio();
                    Time tNext = NanoSeconds(message.time());
                    Time tDelay = tNext - m_sim->Now();
                    double transmitPower = -1;
                    Ipv4Address ip;

                    if (message.radio_number() == ConfigureWifiRadio_RadioNumber_SINGLE_RADIO) {
                        transmitPower = message.primary_radio_configuration().transmission_power();
                        ip.Set(message.primary_radio_configuration().ip_address());
                    } else {
                        NS_LOG_ERROR("Currently only SINGLE_RADIO is supported");
                        exit(1);
                    }

                    m_sim->Schedule(tDelay, MakeEvent(&MosaicNodeManager::ConfigureWifiRadio, m_nodeManager, message.node_id(), transmitPower, ip));
                    NS_LOG_DEBUG("Received CONF_WIFI_RADIO: mosNID=" << message.node_id() << " tNext=" << tNext);

                } catch (int e) {
                    NS_LOG_ERROR("Error while reading configuration message");
                    m_closeConnection = true;
                }
                
                ambassadorFederateChannel.writeCommand(CommandMessage_CommandType_SUCCESS);
                break;
            }
            case CommandMessage_CommandType_SEND_WIFI_MSG:
            {
                try {
                    SendWifiMessage message = ambassadorFederateChannel.readSendWifiMessage();
                    Ipv4Address ip(message.topological_address().ip_address());

                    Time tNext = NanoSeconds(message.time());
                    // ns3 does not like to send packets at time zero, use 1ns instead
                    if (tNext == NanoSeconds(0)) {
                        tNext = NanoSeconds(1);
                    }
                    Time tDelay = tNext - m_sim->Now();
                    m_sim->Schedule(tDelay, MakeEvent(&MosaicNodeManager::SendWifiMsg, m_nodeManager, message.node_id(), ip, message.channel_id(), message.message_id(), message.length()));
                    NS_LOG_DEBUG("Received SEND_WIFI_MSG: mosNID=" << message.node_id() << " id=" << message.message_id() << " sendTime=" << message.time() << " length=" << message.length());
                } catch (int e) {
                    NS_LOG_ERROR("Error while sending message");
                }

                ambassadorFederateChannel.writeCommand(CommandMessage_CommandType_SUCCESS);
                break;
            }
            case CommandMessage_CommandType_CONF_CELL_RADIO:
            {
                try {
                    ConfigureCellRadio message = ambassadorFederateChannel.readConfigureCellRadio();
                    Time tNext = NanoSeconds(message.time());
                    Time tDelay = tNext - m_sim->Now();
                    Ipv4Address ip;
                    ip.Set(message.ip_address());

                    NS_LOG_DEBUG("Received CONF_CELL_RADIO: mosNID=" << message.node_id() << " tNext=" << tNext);
                    if (!m_didRunOnStart) {
                        m_nodeManager->ConfigureCellRadio(message.node_id(), ip);
                    } else {
                        m_sim->Schedule(tDelay, MakeEvent(&MosaicNodeManager::ConfigureCellRadio, m_nodeManager, message.node_id(), ip));
                    }

                } catch (int e) {
                    NS_LOG_ERROR("Error while reading configuration message");
                    m_closeConnection = true;
                }
                
                ambassadorFederateChannel.writeCommand(CommandMessage_CommandType_SUCCESS);
                break;
            }
            case CommandMessage_CommandType_SEND_CELL_MSG:
            {
                try {
                    SendCellMessage message = ambassadorFederateChannel.readSendCellMessage();
                    Ipv4Address ip(message.topological_address().ip_address());

                    Time tNext = NanoSeconds(message.time());
                    // ns3 does not like to send packets at time zero, use 1ns instead
                    if (tNext == NanoSeconds(0)) {
                        tNext = NanoSeconds(1);
                    }
                    Time tDelay = tNext - m_sim->Now();
                    m_sim->Schedule(tDelay, MakeEvent(&MosaicNodeManager::SendCellMsg, m_nodeManager, message.node_id(), ip, message.message_id(), message.length()));
                    NS_LOG_DEBUG("Received SEND_CELL_MSG: mosNID=" << message.node_id() << " id=" << message.message_id() << " sendTime=" << message.time() << " length=" << message.length());
                } catch (int e) {
                    NS_LOG_ERROR("Error while sending message");
                }

                ambassadorFederateChannel.writeCommand(CommandMessage_CommandType_SUCCESS);
                break;
            }
            case CommandMessage_CommandType_SHUT_DOWN:
                NS_LOG_INFO("Received CMD_SHUT_DOWN");
                m_nodeManager->OnShutdown();
                NS_LOG_INFO("m_countTimeAdvanceGrant=" << m_countTimeAdvanceGrant);
                NS_LOG_INFO("m_countNextEventRequest=" << m_countNextEventRequest);
                NS_LOG_INFO("Disable log...");
                LogComponentDisableAll(LOG_LEVEL_ALL);
                m_closeConnection = true;
                Simulator::Destroy();
                break;

            default:
                NS_LOG_ERROR("Command " << commandId << " not implemented");
                m_closeConnection = true;
                return;
        }
    }

    void MosaicNs3Bridge::writeNextTime(unsigned long long nextTime) {
        if (m_reportedTimes.find (nextTime) != m_reportedTimes.end()) {
            return;
        }
        m_reportedTimes.insert(nextTime);
        while (m_reportedTimes.size() > 1000) {
            m_reportedTimes.erase(m_reportedTimes.begin());
        }

        nextTime *= m_timeFactor; // convert to nanoseconds
        m_countNextEventRequest++;
        federateAmbassadorChannel.writeCommand(CommandMessage_CommandType_NEXT_EVENT);
        federateAmbassadorChannel.writeTimeMessage(nextTime);
    }

    void MosaicNs3Bridge::writeReceiveWifiMessage(unsigned long long recvTime, int nodeID, int msgID) {
        m_countNextEventRequest++;
        federateAmbassadorChannel.writeCommand(CommandMessage_CommandType_RECV_WIFI_MSG);
        federateAmbassadorChannel.writeReceiveWifiMessage(recvTime, nodeID, msgID, RadioChannel::PROTO_CCH, 0);
        // FIXME: RSSI is hardcoded
    }

    void MosaicNs3Bridge::writeReceiveCellMessage(unsigned long long recvTime, int nodeID, int msgID) {
        m_countNextEventRequest++;
        federateAmbassadorChannel.writeCommand(CommandMessage_CommandType_RECV_CELL_MSG);
        federateAmbassadorChannel.writeReceiveCellMessage(recvTime, nodeID, msgID);
    }

} // namespace ns3
