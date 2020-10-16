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

#ifndef MOSAIC_PROXY_APP_H
#define MOSAIC_PROXY_APP_H

#include "ns3/application.h"
#include "mosaic-node-manager.h"
#include "ns3/data-rate.h"

namespace ns3 {

    class MosaicProxyApp : public Application {
    public:

        MosaicProxyApp() = default;

        virtual ~MosaicProxyApp() = default;

        static TypeId GetTypeId(void);

        void SetNodeManager(MosaicNodeManager* nodeManager);

        void SetSockets(void);
        
        void TransmitPacket(uint32_t protocolID, uint32_t msgID, uint32_t payLength, Ipv4Address address);
        
        void Enable();
        
        void Disable();
        
        virtual void DoDispose(void);
        
        //Must be public to be accessible for ns-3 object system
        uint16_t m_port = 0;

    private:

        void Receive(Ptr<Socket> socket);

        Ptr<Socket> m_socket{nullptr};
                
        uint16_t m_sendCount = 0;
        uint64_t m_recvCount = 0;

        bool m_active = false;

        MosaicNodeManager* m_nodeManager;
    };
} // namespace ns3

#endif   /* MOSAIC_PROXY_APP_H */
