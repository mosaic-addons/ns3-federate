# MOSAIC ns-3 Federate

This repository provides code to integrate the [ns-3](https://www.nsnam.org/) network simulator with the Mobility Simulation Framework [Eclipse MOSAIC](https://github.com/eclipse/mosaic). The federate is a wrapper around ns-3 simulator and provides a socket-based communication between this federate and the `Ns3Ambassador`, thus enabling a coupling between those two tools.

The federate can be build using the `ns3_installer.sh` script which is bundled with each Eclipse MOSAIC distribution. Find the documentation on [MOSAIC's website](https://www.eclipse.dev/mosaic/docs/simulators/network_simulator_ns3) for additional and detailed instructions.
If you just want to use the coupling between MOSAIC and ns-3, you can use the `ns3_installer.sh` script.

In the case that you are an advanced users who wants to alter the source code of ns3 or the ns3-federate please have a look at the [ns3-federate documentation](https://eclipse.dev/mosaic/docs/extending_mosaic/ns3_federate/)

### Overview
- Purpose: Couple the MOSAIC co-simulation framework with ns-3, enabling synchronized, time-stepped network simulation. The bridge receives commands (e.g., create nodes, configure radios, send packets) from MOSAIC and returns acknowledgments, time grants, and delivery notifications back to MOSAIC.
- Simulation core: A custom ns-3 SimulatorImpl aligns ns-3 event processing with external time grants from MOSAIC.
- Two TCP channels with distinct roles:
  - Ambassador→Federate channel (ambassadorFederateChannel): used by MOSAIC to issue commands (INIT, ADD_NODE, ADVANCE_TIME, SEND_*_MSG, etc.) and data messages to ns-3.
  - Federate→Ambassador channel (federateAmbassadorChannel): primarily used by ns-3 to send acknowledgments, time messages, port info, and “received message” notifications to MOSAIC.
- Control loop:
  - MOSAIC sends command messages; the bridge applies them (creating/configuring nodes, scheduling transmits) and acknowledges.
  - For ADVANCE_TIME, the bridge runs ns-3 events up to the granted time
  - When ns-3 receives packets, the bridge reports them back to MOSAIC.

![Architecture](/docs/Ns3Federate.drawio.png)

### Key components

| Component/File                  | Responsibility                                                                                                   | Key interactions                                                                                                   |
|---------------------------------|-------------------------------------------------------------------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------|
| ClientServerChannelMessages.proto| Protobuf schema for all commands and data messages exchanged between MOSAIC and ns-3.                             | Generated classes used by ClientServerChannel read/write methods.                                                   |
| client-server-channel.cc        | TCP server wrapper with protobuf serialization; read/write commands and typed messages.                           | Provides read/write for CommandMessage, TimeMessage, Add/Update/Remove/Config/Send/Receive messages.                |
| main.cc                         | Entry point; parses args; loads XML config; sets logging; instantiates and runs the bridge.                        | Sets ns-3 globals (scheduler, impl); applies XML-configured log levels; starts run loop.                            |
| mosaic-simulator-impl.cc        | Custom SimulatorImpl integrating with external time grants and (optionally) next-event publishing.                | Runs and schedules events; informs bridge about next events (currently disabled in bridge).                         |
| mosaic-ns3-bridge.cc            | Orchestrates channels, time loop, and command dispatch; translates commands to ns-3 actions; sends acks/results. | Talks to both ClientServerChannel instances; schedules work on MosaicSimulatorImpl; delegates to MosaicNodeManager. |
| mosaic-node-manager.cc          | Builds and manages ns-3 topology and nodes (eNBs, UEs, Wi-Fi, CSMA); configures IPs/routing; handles send/recv.  | Uses LTE/EPC helpers, Wi-Fi 802.11p, CSMA; installs MosaicProxyApp for UDP I/O; calls bridge on received packets.   |
| mosaic-proxy-app.cc             | Thin UDP app bound to a specific interface (Wi-Fi, LTE, or CSMA); tags packets with FlowId; invokes callbacks.    | Receives/sends UDP on port 8010; forwards receive events (time, nodeId, msgId) to NodeManager/Bridge.               |

### Messaging protocol
- Protobuf schema: ClientServerChannelMessages.proto
- Framing: varint32 length-prefixed messages.
- Commands (subset, as used):
  - INIT, SHUT_DOWN, SUCCESS, NEXT_EVENT, ADVANCE_TIME, END
  - ADD_NODE, UPDATE_NODE, REMOVE_NODE
  - CONF_WIFI_RADIO, SEND_WIFI_MSG, RECV_WIFI_MSG
  - CONF_CELL_RADIO, SEND_CELL_MSG, RECV_CELL_MSG

| Command                         | Action                                                                                                           | Reply                                 |
|---------------------------------|------------------------------------------------------------------------------------------------------------------|----------------------------------------|
| INIT                            | Read InitMessage, check protocol and times.                                                                      | CMD_SUCCESS or CMD_SHUT_DOWN           |
| ADD_NODE (RADIO/WIRED/NODE_B)   | Create radio node (UE + 802.11p), wired node (CSMA), or eNB; if after start, schedule activation.                | CMD_SUCCESS                            |
| UPDATE_NODE                     | Update node positions (scheduled at given time).                                                                 | CMD_SUCCESS                            |
| REMOVE_NODE                     | Disable node (turn off Wi-Fi PHY, disable apps).                                                                 | CMD_SUCCESS                            |
| CONF_WIFI_RADIO                 | Enable Wi-Fi app, set TX power if provided, add Wi-Fi IP address to device.                                      | CMD_SUCCESS                            |
| SEND_WIFI_MSG                   | Schedule UDP send via Wi-Fi app; channel/TTL ignored for now.                                                    | CMD_SUCCESS                            |
| CONF_CELL_RADIO                 | Enable cell/CSMA app; add IP; for UEs attach to closest eNB; adjust routes for wired nodes.                      | CMD_SUCCESS                            |
| SEND_CELL_MSG                   | Schedule UDP send via LTE (radio node) or CSMA (wired node).                                                     | CMD_SUCCESS                            |
| SHUT_DOWN                       | Log stats, disable logging, destroy simulator, close loop.                                                       | —                                      |

### Networking and routing notes
- Backbone: CSMA at 100 Gb/s, PGW and Servers on this backbone
- IP constraints:
  - All node IPs must be within 10.0.0.0/8.
  - Wired nodes must use 10.5.0.0/16 or 10.6.0.0/16; radio nodes must not.
- Extra radio nodes are pre-created to allow activation later (limitation by ns-3).

### Configuration and logging
- XML config (ns3_federate_config.xml) sets default values per component.
- XML config (ns3_federate_config.xml) sets log levels per component.
- Each MOSAIC simulation scenario can bring their own ns3_federate_config.xml for scenario-specific configuration.

### Not yet supported or simplified:
- Geographical addressing (rectangle, circle) is not implemented; only topological addresses are used.
- Wi-Fi channel selection not applied (channel field is currently ignored).
- RSSI is hardcoded (placeholder) in receive notifications.

<!-- ### Data flow -->
<!-- ### Connection and handshake -->
<!-- ### Lifecycle and timing -->