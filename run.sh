#!/bin/bash

port=$1
cmdport=$2

if [[ -z $cmdport ]]; then
  cmdport=0
fi

cd ns3
LD_LIBRARY_PATH=../ns-allinone-3.33/ns-3.33/build ../ns-allinone-3.33/ns-3.33/build/scratch/mosaic_starter --port=$port --cmdPort=$cmdport --configFile=scratch/ns3_federate_config.xml
