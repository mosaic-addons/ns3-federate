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

#include "mosaic-simulator-impl.h"
#include "ns3/simulator.h"
#include "ns3/event-impl.h"
#include "ns3/ptr.h"
#include "ns3/pointer.h"
#include "ns3/assert.h"
#include "ns3/log.h"

#include <math.h>

NS_LOG_COMPONENT_DEFINE("MosaicSimulatorImpl");

namespace ns3 {

    NS_OBJECT_ENSURE_REGISTERED(MosaicSimulatorImpl);

    TypeId MosaicSimulatorImpl::GetTypeId(void) {
        static TypeId tid = TypeId("ns3::MosaicSimulatorImpl").SetParent<SimulatorImpl> ().AddConstructor<MosaicSimulatorImpl> ();
        return tid;
    }

    MosaicSimulatorImpl::MosaicSimulatorImpl() {
        m_stop = false;
        // uids are allocated from 4.
        // uid 0 is "invalid" events
        // uid 1 is "now" events
        // uid 2 is "destroy" events
        m_uid = 4;
        // before ::Run is entered, the m_currentUid will be zero
        m_currentUid = 0;
        m_currentTs = 0;
        m_currentContext = 0xffffffff;
        m_unscheduledEvents = 0;
        m_eventCount = 0;
    }

    void MosaicSimulatorImpl::DoDispose(void) {
        while (!m_events->IsEmpty()) {
            Scheduler::Event next = m_events->RemoveNext();
            next.impl->Unref();
        }
        m_events = 0;
        SimulatorImpl::DoDispose();
    }

    void MosaicSimulatorImpl::Destroy() {
        while (!m_destroyEvents.empty()) {
            Ptr<EventImpl> ev = m_destroyEvents.front().PeekEventImpl();
            m_destroyEvents.pop_front();
            NS_LOG_LOGIC("handle destroy " << ev);
            if (!ev->IsCancelled()) {
                ev->Invoke();
            }
        }
    }

    void MosaicSimulatorImpl::SetScheduler(ObjectFactory schedulerFactory) {
        Ptr<Scheduler> scheduler = schedulerFactory.Create<Scheduler> ();

        if (m_events != 0) {
            while (!m_events->IsEmpty()) {
                Scheduler::Event next = m_events->RemoveNext();
                scheduler->Insert(next);
            }
        }
        m_events = scheduler;
    }

    // System ID for non-distributed simulation is always zero

    uint32_t MosaicSimulatorImpl::GetSystemId(void) const {
        return 0;
    }

    void MosaicSimulatorImpl::ProcessOneEvent(void) {
        Scheduler::Event next = m_events->RemoveNext();
        NS_ASSERT(next.key.m_ts >= m_currentTs);
        m_unscheduledEvents--;
        m_eventCount++;

        NS_LOG_LOGIC("handle " << next.key.m_ts);
        m_currentTs = next.key.m_ts;
        m_currentContext = next.key.m_context;
        m_currentUid = next.key.m_uid;
        next.impl->Invoke();
        next.impl->Unref();
    }

    bool MosaicSimulatorImpl::IsFinished(void) const {
        return m_events->IsEmpty() || m_stop;
    }

    uint64_t MosaicSimulatorImpl::NextTs(void) const {
        NS_ASSERT(!m_events->IsEmpty());
        Scheduler::Event ev = m_events->PeekNext();
        return ev.key.m_ts;
    }

    Time MosaicSimulatorImpl::Next(void) const {
        return TimeStep(NextTs());
    }

    void MosaicSimulatorImpl::Run(void) {
        m_stop = false;
        while (!m_events->IsEmpty() && !m_stop) {
            ProcessOneEvent();
        }

        // If the simulator stopped naturally by lack of events, make a
        // consistency test to check that we didn't lose any events along the way.
        NS_ASSERT(!m_events->IsEmpty() || m_unscheduledEvents == 0);
    }

    void MosaicSimulatorImpl::RunOneEvent(void) {
        ProcessOneEvent();
    }

    uint64_t MosaicSimulatorImpl::GetEventCount(void) const {
        return m_eventCount;
    }

    void MosaicSimulatorImpl::Stop(void) {
        m_stop = true;
    }

    void MosaicSimulatorImpl::Stop(Time const &time) {
        Simulator::Schedule(time, &Simulator::Stop);
    }

    EventId MosaicSimulatorImpl::Schedule(Time const &time, EventImpl *event) {

        Time tAbsolute = time + TimeStep(m_currentTs);

        NS_ASSERT(tAbsolute.IsPositive());

        Scheduler::Event ev;
        ev.impl = event;
        ev.key.m_ts = (uint64_t) tAbsolute.GetTimeStep();
        ev.key.m_context = GetContext();
        ev.key.m_uid = m_uid;
        m_uid++;
        m_unscheduledEvents++;
        m_events->Insert(ev);
        m_server->writeNextTime(ev.key.m_ts);

        return EventId(event, ev.key.m_ts, ev.key.m_context, ev.key.m_uid);
    }

    void MosaicSimulatorImpl::ScheduleWithContext(uint32_t context, Time const &time, EventImpl *event) {
        NS_LOG_FUNCTION(this << context << time.GetTimeStep() << m_currentTs << event);

        Scheduler::Event ev;
        ev.impl = event;
        ev.key.m_ts = m_currentTs + time.GetTimeStep();
        ev.key.m_context = context;
        ev.key.m_uid = m_uid;
        m_uid++;
        m_unscheduledEvents++;
        m_events->Insert(ev);
        m_server->writeNextTime(ev.key.m_ts);
    }

    EventId MosaicSimulatorImpl::ScheduleNow(EventImpl *event) {

        Scheduler::Event ev;
        ev.impl = event;
        ev.key.m_ts = m_currentTs;
        ev.key.m_context = GetContext();
        ev.key.m_uid = m_uid;
        m_uid++;
        m_unscheduledEvents++;
        m_events->Insert(ev);

        return EventId(event, ev.key.m_ts, ev.key.m_context, ev.key.m_uid);
    }

    EventId MosaicSimulatorImpl::ScheduleDestroy(EventImpl *event) {

        EventId id(Ptr<EventImpl> (event, false), m_currentTs, 0xffffffff, 2);
        m_destroyEvents.push_back(id);
        m_uid++;

        return id;
    }

    Time MosaicSimulatorImpl::Now(void) const {

        return TimeStep(m_currentTs);
    }

    void MosaicSimulatorImpl::SetCurrentTs(Time time) {

        m_currentTs = time.GetNanoSeconds();
    }

    Time MosaicSimulatorImpl::GetDelayLeft(const EventId &id) const {

        if (IsExpired(id)) {
            return TimeStep(0);
        } else {
            return TimeStep(id.GetTs() - m_currentTs);
        }
    }

    void MosaicSimulatorImpl::Remove(const EventId &id) {

        if (id.GetUid() == 2) {
            // destroy events.
            for (DestroyEvents::iterator i = m_destroyEvents.begin(); i != m_destroyEvents.end(); i++) {
                if (*i == id) {
                    m_destroyEvents.erase(i);
                    break;
                }
            }
            return;
        }
        if (IsExpired(id)) {
            return;
        }
        Scheduler::Event event;
        event.impl = id.PeekEventImpl();
        event.key.m_ts = id.GetTs();
        event.key.m_context = id.GetContext();
        event.key.m_uid = id.GetUid();
        m_events->Remove(event);
        event.impl->Cancel();
        // whenever we remove an event from the event list, we have to unref it.
        event.impl->Unref();

        m_unscheduledEvents--;
    }

    void MosaicSimulatorImpl::Cancel(const EventId &id) {
        if (!IsExpired(id)) {
            id.PeekEventImpl()->Cancel();
        }
    }

    bool MosaicSimulatorImpl::IsExpired(const EventId &ev) const {
        if (ev.GetUid() == 2) {
            if (ev.PeekEventImpl() == 0 ||
                    ev.PeekEventImpl()->IsCancelled()) {
                return true;
            }
            // destroy events.
            for (DestroyEvents::const_iterator i = m_destroyEvents.begin(); i != m_destroyEvents.end(); i++) {
                if (*i == ev) {
                    return false;
                }
            }
            return true;
        }
        if (ev.PeekEventImpl() == 0 ||
                ev.GetTs() < m_currentTs ||
                (ev.GetTs() == m_currentTs &&
                ev.GetUid() <= m_currentUid) ||
                ev.PeekEventImpl()->IsCancelled()) {
            return true;
        } else {
            return false;
        }
    }

    Time MosaicSimulatorImpl::GetMaximumSimulationTime(void) const {
        return TimeStep(0x7fffffffffffffffLL);
    }

    uint32_t MosaicSimulatorImpl::GetContext(void) const {
        return m_currentContext;
    }

    void MosaicSimulatorImpl::AttachNS3Server(MosaicNs3Server* server) {
        m_server = server;
    }

} // namespace ns3
