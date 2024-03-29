# MOSAIC ns-3 Federate

This repository provides code to integrate the [ns-3](https://www.nsnam.org/) network simulator with the Mobility Simulation Framework [Eclipse MOSAIC](https://github.com/eclipse/mosaic). The federate is a wrapper around ns-3 simulator and provides a socket-based communication between this federate and the `Ns3Ambassador`, thus enabling a coupling between those two tools.

The federate can be build using the `ns3_installer.sh` script which is bundled with each Eclipse MOSAIC distribution. See [MOSAIC's website](https://www.eclipse.org/mosaic/docs/simulators/network_simulator_ns3) for additional and detailed instructions.

The subsequent instructions therefore adresses advanced users who want to alter the source code of the federate. If you just want to use the coupling between MOSAIC and ns-3, you can use the `ns3_installer.sh` script.


## Dependencies

The ns3-federate has the following dependencies:

* `protobuf`
* `gtest`
* `ns3` (tested with version `3.36.1`)

Building ns3-federate requires further dependencies:

* `premake` 
* `premake-autoconf`

The source code and binary tar balls for Windows, Linux and Mac OS X of ```premake``` are available at https://premake.github.io/download.html.
When you choose the binary version then the tar ball extracts just one single binary called ```premake5``` for direct use. Please keep in mind that ```premake``` is
written as a c-binding to a modified variant of ```lua``` that is statically linked in. The currently (```premake5-alpha12```) linked in ```lua``` version is ```5.3``` which should be considered when placing extensions of ```premake```.

The building of ```premake5``` from source for linux is strait forward and is done with ```GNU make```.

```bash
~$ make -f Bootstrap.mak linux
~# cp bin/release/premake5 /usr/local/bin
```

The second dependency ```premake5-autoconf``` can be downloaded from https://github.com/Blizzard/premake-autoconf.git.
It consists of a flat directory of ```lua``` files. These can be directly placed in the ```ns3-federate``` build directory or
in a directory your ```premake5``` can find it. Usually it will show the search directories that is looking into for modules when a module was not found, i.e. ```/usr/local/share/lua/5.3/```. The ```premake5-autoconf``` module introduces 1. support for ```autoconf``` style checks for headers and libraries and 2. full ```clang``` support.

Ubuntu 16.04 xenial users can use a package from "PPA for Paul McEnery", see https://launchpad.net/~pmcenery/+archive/ubuntu/ppa/+packages?field.name_filter=premake&field.status_filter=published&field.series_filter=.

## Build from ```MOSAIC``` source

```bash
~$ premake5 gmake
~# make config=debug clean
~# make config=debug # config=release
```

Some source file are generated by the protobuf compiler. To regenerate these files
pass the option ```--generate-protobuf```. The regeneration is done during ```make```.

## Install from ```MOSAIC``` source

To trigger the install target pass ```--install``` to ```premake5``` and run ```make``` as super user.

```bash
~$ premake5 gmake --install
~# make config=debug clean
~# make config=debug
```
