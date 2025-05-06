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

#include "mosaic-ns3-server.h"

#include "ns3/node-list.h"
#include "ns3/log.h"

#include "mosaic-simulator-impl.h"

NS_LOG_COMPONENT_DEFINE("MosaicNs3Server");

namespace ns3 {
    using namespace ClientServerChannelSpace;

    MosaicNs3Server::MosaicNs3Server(int port, int cmdPort) {
        std::cout << "Starting ns3 federate on port=" << port << " cmdPort=" << cmdPort << std::endl;

        m_nodeManager = CreateObject<MosaicNodeManager>();
        m_nodeManager->Configure(this);
        m_closeConnection = false;

        /* Initialize federateAmbassadorChannel (mostly for SENDING) */
        NS_LOG_INFO("Initialize federateAmbassadorChannel");
        federateAmbassadorChannel.prepareConnection("0.0.0.0", port);
        federateAmbassadorChannel.connect();
        federateAmbassadorChannel.writeCommand(CMD_INIT);

        /* Initialize ambassadorFederateChannel (mostly for RECEIVING) */
        NS_LOG_INFO("Initialize ambassadorFederateChannel");
        uint16_t assignedPort = ambassadorFederateChannel.prepareConnection("0.0.0.0", cmdPort);
        if (assignedPort < 1) {
            std::cout << "Could not prepare port for Command Channel" << std::endl;
            exit(1);
        }
        federateAmbassadorChannel.writePort(assignedPort);
        ambassadorFederateChannel.connect();
        if (ambassadorFederateChannel.readCommand() == CMD_INIT) {
            CSC_init_return init_message;
            ambassadorFederateChannel.readInit(init_message);
            unsigned long long startTime = init_message.start_time;
            unsigned long long endTime = init_message.end_time;
            if (startTime >= 0 && endTime >= 0 && endTime >= startTime) {
                ambassadorFederateChannel.writeCommand(CMD_SUCCESS);
            } else {
                ambassadorFederateChannel.writeCommand(CMD_END);
            }
        } else {
            NS_LOG_ERROR("Did not receive CMD_INIT as first message");
            exit(1);
        }
        NS_LOG_INFO("Created new connection on port " << port);
    }

    MosaicNs3Server::~MosaicNs3Server() {
        m_closeConnection = true;
    }

    void MosaicNs3Server::run() {
        try {
            if (m_closeConnection) {
                return;                
            }

            m_sim = DynamicCast<MosaicSimulatorImpl> (Simulator::GetImplementation());
            if (nullptr == m_sim) {
                NS_LOG_ERROR("Could not find MosaicSimulatorImpl");
                m_closeConnection = true;
                return;
            }
            m_sim->AttachNS3Server(this);

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

    void MosaicNs3Server::dispatchCommand() {
        //read the commandId from the channel
        CMD commandId = ambassadorFederateChannel.readCommand();
        switch (commandId) {
            case CMD_INIT:
                //CMD_INIT is not permitted after the initialization of the MosaicNs3Server
                NS_LOG_ERROR("Received CMD_INIT");
                break;
            case CMD_UPDATE_NODE:
            {
                CSC_update_node_return update_node_message;
                ambassadorFederateChannel.readUpdateNode(update_node_message);
                Time tNext = NanoSeconds(update_node_message.time);
                Time tDelay = tNext - m_sim->Now();
                for (std::vector<CSC_node_data>::iterator it = update_node_message.properties.begin(); it != update_node_message.properties.end(); ++it) {

                    if (update_node_message.type == UPDATE_ADD_RSU) {

                        m_sim->Schedule(tDelay, MakeEvent(&MosaicNodeManager::CreateMosaicNode, m_nodeManager, it->id, Vector(it->x, it->y, 0.0)));
                        NS_LOG_DEBUG("Received ADD_RSU: ID=" << it->id << " posx=" << it->x << " posy=" << it->y << " tNext=" << tNext);

                    } else if (update_node_message.type == UPDATE_ADD_VEHICLE) {

                        m_sim->Schedule(tDelay, MakeEvent(&MosaicNodeManager::CreateMosaicNode, m_nodeManager, it->id, Vector(it->x, it->y, 0.0)));
                        NS_LOG_DEBUG("Received ADD_VEHICLE: ID=" << it->id << " posx=" << it->x << " posy=" << it->y << " tNext=" << tNext);

                    } else if (update_node_message.type == UPDATE_MOVE_NODE) {

                        m_sim->Schedule(tDelay, MakeEvent(&MosaicNodeManager::UpdateNodePosition, m_nodeManager, it->id, Vector(it->x, it->y, 0.0)));
                        NS_LOG_DEBUG("Received MOVE_NODES: ID=" << it->id << " posx=" << it->x << " posy=" << it->y << " tNext=" << tNext);

                    } else if (update_node_message.type == UPDATE_REMOVE_NODE) {

                        //It is not allowed to delete a node during the simulation step -> the node will be deactivated
                        m_sim->Schedule(tDelay, MakeEvent(&MosaicNodeManager::DeactivateNode, m_nodeManager, it->id));
                        NS_LOG_DEBUG("Received REMOVE_NODES: ID=" << it->id << " tNext=" << tNext);
                    }
                }
                ambassadorFederateChannel.writeCommand(CMD_SUCCESS);
                break;
            }

            // advance the next time step and run the simulation read the next time step
            case CMD_ADVANCE_TIME:
                uint64_t advancedTime;
                advancedTime = ambassadorFederateChannel.readTimeMessage();

                NS_LOG_DEBUG("Received ADVANCE_TIME " << advancedTime);
                //run the simulation while the time of the next event is smaller than the next time step
                while (!Simulator::IsFinished() && NanoSeconds(advancedTime) >= m_sim->Next()) {
                    m_sim->RunOneEvent();
                }

                //write the confirmation at the end of the sequence
                federateAmbassadorChannel.writeCommand(CMD_END);
                federateAmbassadorChannel.writeTimeMessage(Simulator::Now().GetNanoSeconds());
                break;

            case CMD_CONF_RADIO:

                try {
                    CSC_config_message config_message;
                    ambassadorFederateChannel.readConfigurationMessage(config_message);
                    Time tNext = NanoSeconds(config_message.time);
                    Time tDelay = tNext - m_sim->Now();
                    double transmitPower = -1;
                    bool radioTurnedOn = false;
                    if (config_message.num_radios == SINGLE_RADIO) {
                        radioTurnedOn = true; //other modes currently not supported, other modes turn off radio
                        transmitPower = config_message.primary_radio.tx_power;
                    }

                    m_sim->Schedule(tDelay, MakeEvent(&MosaicNodeManager::ConfigureNodeRadio, m_nodeManager, config_message.node_id, radioTurnedOn, transmitPower));

                } catch (int e) {
                    NS_LOG_ERROR("Error while reading configuration message");
                    m_closeConnection = true;
                }
                break;

            case CMD_MSG_SEND:
            {
                try {
                    CSC_send_message send_message;
                    ambassadorFederateChannel.readSendMessage(send_message);
                    //Convert the IP address
                    Ipv4Address ip(send_message.topo_address.ip_address);
                    int id = m_nodeManager->GetNs3NodeId(send_message.node_id);
                    NS_LOG_DEBUG("Received V2X_MESSAGE_TRANSMISSION id: " << id << " sendTime: " << send_message.time << " length: " << send_message.length);

                    Time tNext = NanoSeconds(send_message.time);
                    Time tDelay = tNext - m_sim->Now();
                    m_sim->Schedule(tDelay, MakeEvent(&MosaicNodeManager::SendMsg, m_nodeManager, send_message.node_id, 0, send_message.message_id, send_message.length, ip));
                } catch (int e) {
                    NS_LOG_ERROR("Error while sending message");
                }
                break;
            }

            case CMD_SHUT_DOWN:
                m_closeConnection = true;
                Simulator::Destroy();
                break;

            default:
                NS_LOG_ERROR("Command " << commandId << " not implemented");
                m_closeConnection = true;
                return;
        }
    }

    void MosaicNs3Server::writeNextTime(unsigned long long nextTime) {
        federateAmbassadorChannel.writeCommand(CMD_NEXT_EVENT);
        federateAmbassadorChannel.writeTimeMessage(nextTime);
    }

    void MosaicNs3Server::AddRecvPacket(unsigned long long recvTime, Ptr<Packet> pack, int nodeID, int msgID) {
        federateAmbassadorChannel.writeCommand(CMD_MSG_RECV);
        federateAmbassadorChannel.writeReceiveMessage(recvTime, nodeID, msgID, CCH, 0);
    }

} //END Namespace
