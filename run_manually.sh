#!/bin/bash

port=$1
cmdport=$2

if [[ -z $port ]]; then
    port=5011
fi

if [[ -z $cmdport ]]; then
    cmdport=0
fi

ns3Version="3.45"

make -j1 || exit 1

LD_LIBRARY_PATH=../ns-allinone-$ns3Version/ns-$ns3Version/build/lib\
    ./bin/Debug/ns3-federate\
    --port=$port --cmdPort=$cmdport --configFile=ns3_federate_config.xml
    
