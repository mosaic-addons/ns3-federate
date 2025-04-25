# MOSAIC ns-3 Federate

This repository provides code to integrate the [ns-3](https://www.nsnam.org/) network simulator with the Mobility Simulation Framework [Eclipse MOSAIC](https://github.com/eclipse/mosaic). The federate is a wrapper around ns-3 simulator and provides a socket-based communication between this federate and the `Ns3Ambassador`, thus enabling a coupling between those two tools.

The federate can be build using the `ns3_installer.sh` script which is bundled with each Eclipse MOSAIC distribution. Find the documentation on [MOSAIC's website](https://www.eclipse.dev/mosaic/docs/simulators/network_simulator_ns3) for additional and detailed instructions.
If you just want to use the coupling between MOSAIC and ns-3, you can use the `ns3_installer.sh` script.

In the case that you are an advanced users who wants to alter the source code of ns3 or the ns3-federate please have a look at the [ns3-federate documentation](https://eclipse.dev/mosaic/docs/extending_mosaic/ns3_federate/)

