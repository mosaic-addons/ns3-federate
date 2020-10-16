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

#ifndef MOSAIC_SIMULATOR_IMPL_H
#define MOSAIC_SIMULATOR_IMPL_H

#include "ns3/simulator-impl.h"
#include "mosaic-ns3-server.h"
#include "ns3/scheduler.h"
#include "ns3/event-impl.h"
#include "ns3/ptr.h"

#include <list>

namespace ns3 {

    /**
     * @class MosaicSimulatorImpl
     * @brief the MOSAIC simulator implementation extends the NS3 standard simulator 
     * implementation with the methods AttachNS3Server and RunOneEvent.
     * Unfortunately, the default simulator, provided by ns-3 does not allow to use the
     * processOneEvent() function. Hence we need to copy this class and provide the function
     * here. The Event class, providing a global static interface can not call
     * processOneEvent() either, so in the MOSAIC server, we need to obtain a direct pointer
     * to this instance and call it directly.
     */
    class MosaicSimulatorImpl : public SimulatorImpl {
    public:

        MosaicSimulatorImpl();
        ~MosaicSimulatorImpl() = default;

        static TypeId GetTypeId(void);

        void AttachNS3Server(MosaicNs3Server* instance);
        
        virtual EventId Schedule(Time const &time, EventImpl *event);
        virtual void Destroy();
        virtual bool IsFinished(void) const;
        virtual Time Next(void) const;
        virtual void Stop(void);
        virtual void Stop(Time const &time);
        virtual void ScheduleWithContext(uint32_t context, Time const &time, EventImpl *event);
        virtual EventId ScheduleNow(EventImpl *event);
        virtual EventId ScheduleDestroy(EventImpl *event);
        virtual void Remove(const EventId &ev);
        virtual void Cancel(const EventId &ev);
        virtual bool IsExpired(const EventId &ev) const;
        virtual void Run(void);
        virtual void RunOneEvent(void);
        virtual Time Now(void) const;
        virtual Time GetDelayLeft(const EventId &id) const;
        virtual Time GetMaximumSimulationTime(void) const;
        virtual void SetScheduler(ObjectFactory schedulerFactory);
        virtual uint32_t GetSystemId(void) const;
        virtual uint32_t GetContext(void) const;
        virtual void SetCurrentTs(Time time);

    private:

        virtual void DoDispose(void);
        void ProcessOneEvent(void);
        uint64_t NextTs(void) const;
        typedef std::list<EventId> DestroyEvents;

        DestroyEvents m_destroyEvents;
        bool m_stop;
        Ptr<Scheduler> m_events;
        uint32_t m_uid;
        uint32_t m_currentUid;
        uint64_t m_currentTs;
        uint32_t m_currentContext;
        // number of events that have been inserted but not yet scheduled,
        // not counting the "destroy" events; this is used for validation
        int m_unscheduledEvents;
        MosaicNs3Server* m_server;

    };
} // namespace ns3
#endif /* DEFAULT_SIMULATOR_IMPL_H */
