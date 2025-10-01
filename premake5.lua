-- premake5.lua

local tools = require ('tools')
local ns3version = "3.36.1"
local PROTO_PATH    = "."
local PROTO_CC_PATH = "."

--local pkgconfig = require 'pkgconfig'
--print ( pkgconfig.load ( 'zlib' ) )
--print ( pkgconfig.load ( 'glib-2.0' ) )

local PROTOC = tools.check_bin ( 'protoc' )

newoption {
    trigger     = "generate-protobuf",
    description = "Generate/Regenerate protocol buffers with protobuf compiler"
}

workspace "ns3-federate"
    configurations { "Debug", "Release" }

project "ns3-federate"
    kind "ConsoleApp"
    language "C++"
    targetdir "bin/%{cfg.buildcfg}"
    buildoptions { "-std=c++17" }

    files { "src/**.h"
          , "src/**.cc" 
          , PROTO_CC_PATH .. "/ClientServerChannelMessages.pb.h"
          , PROTO_CC_PATH .. "/ClientServerChannelMessages.pb.cc"
          }

    includedirs { "/usr/include"
                , "/usr/include/libxml2"
                , "src"
                , "/mnt/c/Users/geo27059/Documents/Code/ns-" .. ns3version .. "/build/include"
                , PROTO_CC_PATH
                }

    libdirs { "/usr/lib"
            , "/mnt/c/Users/geo27059/Documents/Code/ns-" .. ns3version .. "/build/lib"
            }

    links { "pthread"
          , "protobuf"
          , "xml2"
          }

    filter "options:generate-protobuf"
        prebuildcommands { PROTOC .. " --cpp_out=" .. PROTO_CC_PATH
                                  .. " --proto_path=" .. PROTO_PATH
                                  .. " ClientServerChannelMessages.proto"
                         }

    filter "configurations:Debug"
        defines { "DEBUG"
                , "NS3_LOG_ENABLE"
                , "NS3_ASSERT_ENABLE" }
        symbols "On"
        libdirs { "bin/Debug" }
        links { "ns" .. ns3version .. "-antenna-default"
            -- , "ns" .. ns3version .. "-aodv-default"
            , "ns" .. ns3version .. "-applications-default"
            , "ns" .. ns3version .. "-bridge-default"
            -- , "ns" .. ns3version .. "-buildings-default"
            , "ns" .. ns3version .. "-config-store-default"
            , "ns" .. ns3version .. "-core-default"
            , "ns" .. ns3version .. "-csma-default"
            -- , "ns" .. ns3version .. "-csma-layout-default"
            -- , "ns" .. ns3version .. "-dsdv-default"
            -- , "ns" .. ns3version .. "-dsr-default"
            , "ns" .. ns3version .. "-energy-default"
            -- , "ns" .. ns3version .. "-fd-net-device-default"
            -- , "ns" .. ns3version .. "-flow-monitor-default"
            , "ns" .. ns3version .. "-internet-apps-default"
            , "ns" .. ns3version .. "-internet-default"
            -- , "ns" .. ns3version .. "-lr-wpan-default"
            , "ns" .. ns3version .. "-lte-default"
            -- , "ns" .. ns3version .. "-mesh-default"
            , "ns" .. ns3version .. "-mobility-default"
            -- , "ns" .. ns3version .. "-netanim-default"
            , "ns" .. ns3version .. "-network-default"
            -- , "ns" .. ns3version .. "-nix-vector-routing-default"
            -- , "ns" .. ns3version .. "-olsr-default"
            , "ns" .. ns3version .. "-point-to-point-default"
            -- , "ns" .. ns3version .. "-point-to-point-layout-default"
            , "ns" .. ns3version .. "-propagation-default"
            -- , "ns" .. ns3version .. "-sixlowpan-default"
            , "ns" .. ns3version .. "-spectrum-default"
            , "ns" .. ns3version .. "-stats-default"
            -- , "ns" .. ns3version .. "-tap-bridge-default"
            -- , "ns" .. ns3version .. "-test-default"
            -- , "ns" .. ns3version .. "-topology-read-default"
            , "ns" .. ns3version .. "-traffic-control-default"
            -- , "ns" .. ns3version .. "-uan-default"
            -- , "ns" .. ns3version .. "-virtual-net-device-default"
            , "ns" .. ns3version .. "-wave-default"
            , "ns" .. ns3version .. "-wifi-default"
            -- , "ns" .. ns3version .. "-wimax-default"
           }

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
        libdirs { "bin/Release" }
        links { "ns3" .. ns3version .. "antenna-optimized"
              -- , "ns3" .. ns3version .. "aodv-optimized"
              , "ns3" .. ns3version .. "applications-optimized"
              , "ns3" .. ns3version .. "bridge-optimized"
              -- , "ns3" .. ns3version .. "buildings-optimized"
              , "ns3" .. ns3version .. "config-store-optimized"
              , "ns3" .. ns3version .. "core-optimized"
              , "ns3" .. ns3version .. "csma-optimized"
              -- , "ns3" .. ns3version .. "csma-layout-optimized"
              -- , "ns3" .. ns3version .. "dsdv-optimized"
              -- , "ns3" .. ns3version .. "dsr-optimized"
              , "ns3" .. ns3version .. "energy-optimized"
              -- , "ns3" .. ns3version .. "fd-net-device-optimized"
              -- , "ns3" .. ns3version .. "flow-monitor-optimized"
              , "ns3" .. ns3version .. "internet-apps-optimized"
              , "ns3" .. ns3version .. "internet-optimized"
              -- , "ns3" .. ns3version .. "lr-wpan-optimized"
              , "ns3" .. ns3version .. "lte-optimized"
              -- , "ns3" .. ns3version .. "mesh-optimized"
              , "ns3" .. ns3version .. "mobility-optimized"
              -- , "ns3" .. ns3version .. "netanim-optimized"
              , "ns3" .. ns3version .. "network-optimized"
              -- , "ns3" .. ns3version .. "nix-vector-routing-optimized"
              -- , "ns3" .. ns3version .. "olsr-optimized"
              , "ns3" .. ns3version .. "point-to-point-optimized"
              -- , "ns3" .. ns3version .. "point-to-point-layout-optimized"
              , "ns3" .. ns3version .. "propagation-optimized"
              -- , "ns3" .. ns3version .. "sixlowpan-optimized"
              , "ns3" .. ns3version .. "spectrum-optimized"
              , "ns3" .. ns3version .. "stats-optimized"
              -- , "ns3" .. ns3version .. "tap-bridge-optimized"
              -- , "ns3" .. ns3version .. "test-optimized"
              -- , "ns3" .. ns3version .. "topology-read-optimized"
              , "ns3" .. ns3version .. "traffic-control-optimized"
              -- , "ns3" .. ns3version .. "uan-optimized"
              -- , "ns3" .. ns3version .. "virtual-net-device-optimized"
              , "ns3" .. ns3version .. "wave-optimized"
              , "ns3" .. ns3version .. "wifi-optimized"
              -- , "ns3" .. ns3version .. "wimax-optimized"
              }
