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
#include "ns3/data-rate.h"

#include "mosaic-node-manager.h"

namespace ns3 {

    class MosaicProxyApp : public Application {
    public:

        MosaicProxyApp() = default;

        virtual ~MosaicProxyApp() = default;

        static TypeId GetTypeId(void);

        void SetRecvCallback(Callback<void, unsigned long long, uint32_t, int> cb);

        void SetSockets(int outDevice);
        
        void TransmitPacket(Ipv4Address dstAddr, uint32_t msgID, uint32_t payLength);
        
        void Enable();
        
        void Disable();
        
        virtual void DoDispose(void);
        
        //Must be public to be accessible for ns-3 object system
        uint16_t m_port = 0;

    private:

        int TranslateNumberToIndex(int outDevice);

        void Receive(Ptr<Socket> socket);

        Ptr<Socket> m_socket{nullptr};
                
        int m_outDevice = 0;
        uint16_t m_sendCount = 0;
        uint64_t m_recvCount = 0;

        bool m_active = false;

        Callback<void, unsigned long long, uint32_t, int> m_recvCallback;
    };

} // namespace ns3

#endif   /* MOSAIC_PROXY_APP_H */
