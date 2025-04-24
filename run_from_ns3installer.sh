#!/bin/bash

port=$1
cmdport=$2

if [[ -z $cmdport ]]; then
  cmdport=0
fi

ns3Version="3.36.1"

cd ns3
LD_LIBRARY_PATH=../ns-allinone-$ns3Version/ns-$ns3Version/build ../ns-allinone-$ns3Version/ns-$ns3Version/build/scratch/mosaic_starter --port=$port --cmdPort=$cmdport --configFile=scratch/ns3_federate_config.xml
