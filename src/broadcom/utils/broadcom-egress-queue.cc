/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Copyright (c) 2006 Georgia Tech Research Corporation, INRIA
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
* Author: Yibo Zhu <yibzh@microsoft.com>, Hwijoon Lim <wjuni@kaist.ac.kr>, Youngmok Jung <tom418@kaist.ac.kr>
*/
#include <iostream>
#include <stdio.h>
#include "ns3/log.h"
#include "ns3/enum.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/simulator.h"
#include "ns3/tlt-tag.h"
#include "ns3/drop-tail-queue.h"
#include "broadcom-egress-queue.h"
#include "ns3/ipv4-header.h"
#include "ns3/tcp-header.h"
#include "ns3/ppp-header.h"

// For measuring PFC PAUSE time of TCP flow
#include "ns3/pfc-experience-tag.h"
#include <unordered_map>



namespace ns3 {

	std::unordered_map<int32_t, Time> PausedTime;
	std::unordered_map<int32_t, Time> PausedTimeStart;

	NS_LOG_COMPONENT_DEFINE("BEgressQueue");

	NS_OBJECT_ENSURE_REGISTERED(BEgressQueue);

	TypeId BEgressQueue::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::BEgressQueue")
			.SetParent<Queue<Packet>>()
			.AddConstructor<BEgressQueue>()
			.AddAttribute("MaxBytes",
				"The maximum number of bytes accepted by this BEgressQueue.",
				DoubleValue(1000.0 * 1024 * 1024),
				MakeDoubleAccessor(&BEgressQueue::m_maxBytes),
				MakeDoubleChecker<double>());

		return tid;
	}

	BEgressQueue::BEgressQueue() :
		Queue()
	{
		NS_LOG_FUNCTION_NOARGS();
		m_bytesInQueueTotal = 0;
		//youngmok

		m_shareused = 0;
		m_rrlast = 0;
		for (uint32_t i = 0; i < fCnt; i++)
		{
			m_bytesInQueue[i] = 0;
			m_queues.push_back(CreateObject<DropTailQueue<Packet>>());
		}
		m_fcount = 1; //reserved for highest priority
		for (uint32_t i = 0; i < qCnt; i++)
		{
			m_bwsatisfied[i] = Time(0);
			m_minBW[i] = DataRate("10Gb/s");
		}
		m_minBW[3] = DataRate("10Gb/s");
		Simulator::Schedule(Seconds(999), &BEgressQueue::PrintStat, this);
	}
	void
	BEgressQueue::PrintStat() {
		printf("%.9lf\tBEQUEUE_PFC_PASS\t%d\t%d\n", Simulator::Now().GetSeconds(), nodeId, stat_pfc_pass);
		printf("%.9lf\tBEQUEUE_PFC_BLOCK\t%d\t%d\n", Simulator::Now().GetSeconds(), nodeId, stat_pfc_block);
		fflush(stdout);
	}

	BEgressQueue::~BEgressQueue()
	{
		NS_LOG_FUNCTION_NOARGS();
	}

	bool
		BEgressQueue::DoEnqueue(Ptr<Packet> p, uint32_t qIndex)
	{
		NS_LOG_FUNCTION(this << p);

		if (m_bytesInQueueTotal + p->GetSize() < m_maxBytes)  //infinite queue
		{
			if(!m_queues[qIndex]->Enqueue(p)) {
				NS_ABORT_MSG("BEgressQueue implementation error: must not drop here");
			}
			m_bytesInQueueTotal += p->GetSize();
			m_bytesInQueue[qIndex] += p->GetSize();
		}
		else
		{
			NS_ABORT_MSG("BEgressQueue implementation error: must not drop here");
			return false;
		}
		return true;
	}

	Ptr<Packet>
		BEgressQueue::DoDequeue(bool paused[]) //this is for switch only
	{
		NS_LOG_FUNCTION(this);

		if (m_bytesInQueueTotal == 0)
		{
			NS_LOG_LOGIC("Queue empty");
			return 0;
		}

		//strict
		bool found = false;
		uint32_t qIndex;
		for (qIndex = 1; qIndex <= qCnt; qIndex++)
		{
			if (!paused[(qCnt - qIndex) % qCnt] && m_queues[(qCnt - qIndex) % qCnt]->GetNPackets() > 0)
			{
				found = true;
				break;
			}
		}
		qIndex = (qCnt - qIndex) % qCnt;
		if (found)
		{
			Ptr<Packet> p = m_queues[qIndex]->Dequeue();
			m_bytesInQueueTotal -= p->GetSize();
			m_bytesInQueue[qIndex] -= p->GetSize();
			m_rrlast = qIndex;
			NS_LOG_LOGIC("Popped " << p);
			NS_LOG_LOGIC("Number bytes " << m_bytesInQueueTotal);
			return p;
		}
		NS_LOG_LOGIC("Nothing can be sent");
		return 0;
	}


	Ptr<Packet>
		BEgressQueue::DoDequeueNIC(bool paused[])
	{
		NS_LOG_FUNCTION(this);
		if (m_bytesInQueueTotal == 0)
		{
			NS_LOG_LOGIC("Queue empty");
			return 0;
		}
		bool found = false;
		uint32_t qIndex;
		for (qIndex = 1; qIndex <= qCnt; qIndex++)  //round robin
		{
			
			// check if this was blocked by only PAUSE
			if (paused[(qIndex + m_rrlast) % qCnt] && m_queues[(qIndex + m_rrlast) % qCnt]->GetNPackets() > 0) {
				// This was blocked by PAUSE!
				stat_pfc_block++;
				PfcExperienceTag pet;
				Ptr<Packet> p = ConstCast<Packet, const Packet>(m_queues[(qIndex + m_rrlast) % qCnt]->Peek());
				if (p->PeekPacketTag(pet)) {
					// std::cout << "PAUSED : " << (int)pet.m_socketId << std::endl;
					int32_t socketId = (int32_t)pet.m_socketId;
					if(PausedTimeStart.find(socketId) == PausedTimeStart.end()) {
						PausedTimeStart[socketId] = Simulator::Now();
					}
				}
				else
				{
					std::cerr << "PAUSED : -1" << std::endl;
				}
				// if (p->RemovePacketTag(pet))
				// {
				// 	if(!pet.m_start) {
				// 		pet.m_start = static_cast<uint64_t>(Simulator::Now().GetNanoSeconds());
				// 	}
				// 	p->AddPacketTag(pet);
				// } else {
				// 	pet.m_start = static_cast<uint64_t>(Simulator::Now().GetNanoSeconds());
				// 	p->AddPacketTag(pet);
				// }

			}

			if (!paused[(qIndex + m_rrlast) % qCnt] && m_queues[(qIndex + m_rrlast) % qCnt]->GetNPackets() > 0)
			{
				stat_pfc_pass++;
				found = true;
				break;
			}
		}
		qIndex = (qIndex + m_rrlast) % qCnt;
		if (found)
		{
			Ptr<Packet> p = m_queues[qIndex]->Dequeue();
			{
				// Check if the flow has been blocked by PFC
				PfcExperienceTag pet;
				if(p->RemovePacketTag(pet)) {
					int32_t socketId = (int32_t)pet.m_socketId;
					if(PausedTimeStart.find(socketId) != PausedTimeStart.end()) {
						PausedTime[socketId] = PausedTime[socketId] + (Simulator::Now() - PausedTimeStart[socketId]);
						PausedTimeStart.erase(socketId);
					}

					// if(pet.m_start) {
					// 	pet.m_accumulate += Simulator::Now().GetNanoSeconds() - pet.m_start;
					// 	pet.m_start = 0;
					// }
					p->AddPacketTag(pet);
				}
			}
			m_bytesInQueueTotal -= p->GetSize();
			m_bytesInQueue[qIndex] -= p->GetSize();
			m_rrlast = qIndex;
			NS_LOG_LOGIC("Popped " << p);
			NS_LOG_LOGIC("Number bytes " << m_bytesInQueueTotal);
			m_qlast = qIndex;
			return p;
		}
		NS_LOG_LOGIC("Nothing can be sent");
		return 0;
	}


	Ptr<Packet>
		BEgressQueue::DoDequeueRR(bool paused[]) //this is for switch only
	{
		NS_LOG_FUNCTION(this);

		if (m_bytesInQueueTotal == 0)
		{
			NS_LOG_LOGIC("Queue empty");
			return 0;
		}
		bool found = false;
		uint32_t qIndex;

		if (m_queues[qCnt - 1]->GetNPackets() > 0) //7 is the highest priority
		{
			found = true;
			qIndex = qCnt - 1;
		}
		else
		{
			for (qIndex = qCnt - 2; qIndex < qCnt; qIndex--) //strict policy
			{
				if (m_bwsatisfied[qIndex].GetTimeStep() < Simulator::Now().GetTimeStep() && m_queues[qIndex]->GetNPackets() > 0)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				for (qIndex = 1; qIndex <= qCnt; qIndex++)
				{
					if (paused[(qIndex + m_rrlast) % qCnt] && m_queues[(qIndex + m_rrlast) % qCnt]->GetNPackets() > 0)  //round robin
					{
						stat_pfc_block++;
						PfcExperienceTag pet;
						Ptr<Packet> p = ConstCast<Packet, const Packet>(m_queues[(qIndex + m_rrlast) % qCnt]->Peek());
						if (p->PeekPacketTag(pet)) {
							// std::cout << "PAUSED : " << (int)pet.m_socketId << std::endl;
							int32_t socketId = (int32_t)pet.m_socketId;
							if(PausedTimeStart.find(socketId) == PausedTimeStart.end()) {
								PausedTimeStart[socketId] = Simulator::Now();
							}
						}
						else
						{
							std::cerr << "PAUSED : -1" << std::endl;
						}
					}
					if (!paused[(qIndex + m_rrlast) % qCnt] && m_queues[(qIndex + m_rrlast) % qCnt]->GetNPackets() > 0)  //round robin
					{
						stat_pfc_pass++;
						found = true;
						break;
					}
				}
				qIndex = (qIndex + m_rrlast) % qCnt;
			}
		}
		if (found)
		{
			Ptr<Packet> p = m_queues[qIndex]->Dequeue();
			{
				// Check if the flow has been blocked by PFC
				PfcExperienceTag pet;
				if(p->RemovePacketTag(pet)) {
					int32_t socketId = (int32_t)pet.m_socketId;
					if(PausedTimeStart.find(socketId) != PausedTimeStart.end()) {
						PausedTime[socketId] = PausedTime[socketId] + (Simulator::Now() - PausedTimeStart[socketId]);
						PausedTimeStart.erase(socketId);
					}

					p->AddPacketTag(pet);
				}
			}
			m_bytesInQueueTotal -= p->GetSize();
			m_bytesInQueue[qIndex] -= p->GetSize();
			m_bwsatisfied[qIndex] = m_bwsatisfied[qIndex] + (m_minBW[qIndex].CalculateBytesTxTime(p->GetSize()));
			if (Simulator::Now().GetTimeStep() > m_bwsatisfied[qIndex])
				m_bwsatisfied[qIndex] = Simulator::Now();
			if (qIndex != qCnt - 1)
			{
				m_rrlast = qIndex;
			}
			m_qlast = qIndex;
			NS_LOG_LOGIC("Popped " << p);
			NS_LOG_LOGIC("Number bytes " << m_bytesInQueueTotal);
			return p;
		}
		NS_LOG_LOGIC("Nothing can be sent");
		return 0;
	}

	Ptr<Packet>
		BEgressQueue::DoDequeueQCN(bool paused[], Time avail[], uint32_t m_findex_qindex_map[])
	{
		NS_LOG_FUNCTION(this);
		if (m_bytesInQueueTotal == 0)
		{
			NS_LOG_LOGIC("Queue empty");
			return 0;
		}
		bool found = false;
		uint32_t qIndex;
		if (m_queues[0]->GetNPackets() > 0)  //priority 0 is the highest priority in qcn
		{
			found = true;
			qIndex = 0;
		}
		else
		{
			for (qIndex = 1; qIndex <= m_fcount; qIndex++)
			{
				if (!paused[m_findex_qindex_map[(qIndex + m_rrlast) % m_fcount]] && m_queues[(qIndex + m_rrlast) % m_fcount]->GetNPackets() > 0)
				{
					if (avail[(qIndex + m_rrlast) % m_fcount].GetTimeStep() > Simulator::Now().GetTimeStep()) //not available now
						continue;
					found = true;
					break;
				}
			}
			qIndex = (qIndex + m_rrlast) % m_fcount;
		}
		if (found)
		{
			Ptr<Packet> p = m_queues[qIndex]->Dequeue();
			m_bytesInQueueTotal -= p->GetSize();
			m_bytesInQueue[qIndex] -= p->GetSize();
			if (qIndex != 0)
			{
				m_rrlast = qIndex;
			}
			m_qlast = qIndex;
			NS_LOG_LOGIC("Popped " << p);
			NS_LOG_LOGIC("Number bytes " << m_bytesInQueueTotal);
			return p;
		}
		NS_LOG_LOGIC("Nothing can be sent");
		return 0;
	}

	uint32_t
		BEgressQueue::bdpfcIRN(bool paused[], uint32_t m_findex_qindex_map[], uint32_t m_irn_maxSeq[], uint32_t m_irn_maxAck[], uint32_t m_irn_bdp) {
		NS_LOG_FUNCTION(this);
		if (m_bytesInQueueTotal == 0) {
			NS_LOG_LOGIC("Queue empty");
			return 0;
		}
		bool found = false;
		uint32_t qIndex;
		if (m_queues[0]->GetNPackets() > 0)  //priority 0 is the highest priority in qcn
		{
			//for now we have only 2 priority available
			found = true;
			qIndex = 0;
		} else {
			for (qIndex = 1; qIndex <= m_fcount; qIndex++) {
				if (!paused[m_findex_qindex_map[(qIndex + m_rrlast) % m_fcount]] && m_queues[(qIndex + m_rrlast) % m_fcount]->GetNPackets() > 0 && m_irn_maxSeq[(qIndex + m_rrlast) % m_fcount] <= m_irn_maxAck[(qIndex + m_rrlast) % m_fcount] + m_irn_bdp) {
					found = true;
					break;
				}
			}
			qIndex = (qIndex + m_rrlast) % m_fcount;
		}
		uint32_t result = 0;
		if (found) {
			if (qIndex != 0) {
				m_rrlast = qIndex - 1;
			}
			result = qIndex + 1;
		}
		return result;
	}

	Ptr<Packet>
		BEgressQueue::DoDequeueIRN(bool paused[], uint32_t m_findex_qindex_map[]) {
		NS_LOG_FUNCTION(this);
		if (m_bytesInQueueTotal == 0) {
			NS_LOG_LOGIC("Queue empty");
			return 0;
		}
		bool found = false;
		uint32_t qIndex;
		if (m_queues[0]->GetNPackets() > 0)  //priority 0 is the highest priority in qcn
		{
			//for now we have only 2 priority available
			found = true;
			qIndex = 0;
		} else {
			for (qIndex = 1; qIndex <= m_fcount; qIndex++) {
				if (!paused[m_findex_qindex_map[(qIndex + m_rrlast) % m_fcount]] && m_queues[(qIndex + m_rrlast) % m_fcount]->GetNPackets() > 0) {
					found = true;
					break;
				}
			}
			qIndex = (qIndex + m_rrlast) % m_fcount;
		}
		if (found) {
			Ptr<Packet> p = m_queues[qIndex]->Dequeue();
			m_bytesInQueueTotal -= p->GetSize();
			m_bytesInQueue[qIndex] -= p->GetSize();
			if (qIndex != 0) {
				m_rrlast = qIndex;
			}
			m_qlast = qIndex;
			NS_LOG_LOGIC("Popped " << p);
			NS_LOG_LOGIC("Number bytes " << m_bytesInQueueTotal);
			return p;

		}
		NS_LOG_LOGIC("Nothing can be sent");
		return 0;
	}

	Ptr<Packet>
		BEgressQueue::DoDequeuePFC(bool paused[], uint32_t m_findex_qindex_map[]) {
		NS_LOG_FUNCTION(this);
		if (m_bytesInQueueTotal == 0) {
			NS_LOG_LOGIC("Queue empty");
			return 0;
		}
		bool found = false;
		uint32_t qIndex;
		if (m_queues[0]->GetNPackets() > 0)  //priority 0 is the highest priority in qcn
		{
			found = true;
			qIndex = 0;
		} else {
			for (qIndex = 1; qIndex <= m_fcount; qIndex++) {
				if (!paused[m_findex_qindex_map[(qIndex + m_rrlast) % m_fcount]] && m_queues[(qIndex + m_rrlast) % m_fcount]->GetNPackets() > 0) {
					found = true;
					break;
				} else {
				}
			}
			qIndex = (qIndex + m_rrlast) % m_fcount;
		}
		if (found) {
			Ptr<Packet> p = m_queues[qIndex]->Dequeue();
			m_bytesInQueueTotal -= p->GetSize();
			m_bytesInQueue[qIndex] -= p->GetSize();
			if (qIndex != 0) {
				m_rrlast = qIndex;
			}
			m_qlast = qIndex;
			NS_LOG_LOGIC("Popped " << p);
			NS_LOG_LOGIC("Number bytes " << m_bytesInQueueTotal);
			return p;
		}
		NS_LOG_LOGIC("Nothing can be sent");
		return 0;
	}

	bool
		BEgressQueue::Enqueue(Ptr<Packet> p, uint32_t qIndex)
	{
		NS_LOG_FUNCTION(this << p);
		//
		// If DoEnqueue fails, Queue::Drop is called by the subclass
		//
		bool retval = DoEnqueue(p, qIndex);
		/*if (retval)
		{
			NS_LOG_LOGIC("m_traceEnqueue (p)");
			m_traceEnqueue(p);

			uint32_t size = p->GetSize();
			m_nBytes += size;
			m_nTotalReceivedBytes += size;

			m_nPackets++;
			m_nTotalReceivedPackets++;
		}*/
		return retval;
	}

	Ptr<Packet>
		BEgressQueue::Dequeue(bool paused[])
	{
		NS_LOG_FUNCTION(this);
		Ptr<Packet> packet = DoDequeue(paused);
		/*if (packet != 0)
		{
			NS_ASSERT(m_nBytes >= packet->GetSize());
			NS_ASSERT(m_nPackets > 0);
			m_nBytes -= packet->GetSize();
			m_nPackets--;
			NS_LOG_LOGIC("m_traceDequeue (packet)");
			m_traceDequeue(packet);
		}*/
		return packet;
	}


	Ptr<Packet>
		BEgressQueue::DequeueNIC(bool paused[])
	{
		NS_LOG_FUNCTION(this);
		Ptr<Packet> packet = DoDequeueNIC(paused);
		/* if (packet != 0)
		{
			NS_ASSERT(m_nBytes >= packet->GetSize());
			NS_ASSERT(m_nPackets > 0);
			m_nBytes -= packet->GetSize();
			m_nPackets--;
			NS_LOG_LOGIC("m_traceDequeue (packet)");
			m_traceDequeue(packet);
		}*/
		return packet;
	}

	Ptr<Packet>
		BEgressQueue::DequeueRR(bool paused[])
	{
		NS_LOG_FUNCTION(this);
		Ptr<Packet> packet = DoDequeueRR(paused);
		/* if (packet != 0)
		{
			NS_ASSERT(m_nBytes >= packet->GetSize());
			NS_ASSERT(m_nPackets > 0);
			m_nBytes -= packet->GetSize();
			m_nPackets--;
			NS_LOG_LOGIC("m_traceDequeue (packet)");
			m_traceDequeue(packet);
		}*/
		return packet;
	}

	Ptr<Packet>
		BEgressQueue::DequeueQCN(bool paused[], Time avail[], uint32_t m_findex_qindex_map[])  //do all DequeueNIC does, plus QCN
	{
		NS_LOG_FUNCTION(this);
		Ptr<Packet> packet = DoDequeueQCN(paused, avail, m_findex_qindex_map);
		/*if (packet != 0)
		{
			NS_ASSERT(m_nBytes >= packet->GetSize());
			NS_ASSERT(m_nPackets > 0);
			m_nBytes -= packet->GetSize();
			m_nPackets--;
			NS_LOG_LOGIC("m_traceDequeue (packet)");
			m_traceDequeue(packet);
		}*/
		return packet;
	}

	Ptr<Packet>
		BEgressQueue::DequeueIRN(bool paused[], uint32_t m_findex_qindex_map[])  //do all DequeueNIC does, plus QCN
	{
		NS_LOG_FUNCTION(this);
		Ptr<Packet> packet = DoDequeueIRN(paused, m_findex_qindex_map);
		/* if (packet != 0) {
			NS_ASSERT(m_nBytes >= packet->GetSize());
			NS_ASSERT(m_nPackets > 0);
			m_nBytes -= packet->GetSize();
			m_nPackets--;
			NS_LOG_LOGIC("m_traceDequeue (packet)");
			m_traceDequeue(packet);
		}*/
		return packet;
	}
	Ptr<Packet>
		BEgressQueue::DequeuePFC(bool paused[], uint32_t m_findex_qindex_map[])  //do all DequeueNIC does, plus QCN
	{
		NS_LOG_FUNCTION(this);
		Ptr<Packet> packet = DoDequeuePFC(paused, m_findex_qindex_map);
		/* if (packet != 0) {
			NS_ASSERT(m_nBytes >= packet->GetSize());
			NS_ASSERT(m_nPackets > 0);
			m_nBytes -= packet->GetSize();
			m_nPackets--;
			NS_LOG_LOGIC("m_traceDequeue (packet)");
			m_traceDequeue(packet);
		}*/
		return packet;
	}

	bool
		BEgressQueue::DoEnqueue(Ptr<Packet> p)	//for compatiability
	{
		std::cout << "Warning: Call Broadcom queues without priority\n";
		uint32_t qIndex = 0;
		NS_LOG_FUNCTION(this << p);
		if (m_bytesInQueueTotal + p->GetSize() < m_maxBytes)
		{
			m_queues[qIndex]->Enqueue(p);
			m_bytesInQueueTotal += p->GetSize();
			m_bytesInQueue[qIndex] += p->GetSize();
		}
		else
		{
			return false;

		}
		return true;
	}


	Ptr<Packet>
		BEgressQueue::DoDequeue(void)
	{
		std::cout << "Warning: Call Broadcom queues without priority\n";
		uint32_t paused[qCnt] = { 0 };
		NS_LOG_FUNCTION(this);

		if (m_bytesInQueueTotal == 0)
		{
			NS_LOG_LOGIC("Queue empty");
			return 0;
		}

		bool found = false;
		uint32_t qIndex;
		for (qIndex = 1; qIndex <= qCnt; qIndex++)
		{
			if (paused[(qIndex + m_rrlast) % qCnt] == 0 && m_queues[qIndex]->GetNPackets() > 0)
			{
				found = true;
				break;
			}
		}
		if (found)
		{
			Ptr<Packet> p = m_queues[qIndex]->Dequeue();
			m_bytesInQueueTotal -= p->GetSize();
			m_bytesInQueue[qIndex] -= p->GetSize();
			m_rrlast = qIndex;
			NS_LOG_LOGIC("Popped " << p);
			NS_LOG_LOGIC("Number bytes " << m_bytesInQueueTotal);
			return p;
		}
		NS_LOG_LOGIC("Nothing can be sent");
		return 0;
	}


	Ptr<const Packet>
		BEgressQueue::DoPeek(void) const	//DoPeek doesn't work for multiple queues!!
	{
		std::cout << "Warning: Call Broadcom queues without priority\n";
		NS_LOG_FUNCTION(this);
		if (m_bytesInQueueTotal == 0)
		{
			NS_LOG_LOGIC("Queue empty");
			return 0;
		}
		NS_LOG_LOGIC("Number packets " << m_packets.size());
		NS_LOG_LOGIC("Number bytes " << m_bytesInQueue);
		return m_queues[0]->Peek();
	}

	uint32_t
		BEgressQueue::GetNBytes(uint32_t qIndex) const
	{
		return m_bytesInQueue[qIndex];
	}


	uint32_t
		BEgressQueue::GetNBytesTotal() const
	{
		return m_bytesInQueueTotal;
	}

	uint32_t
		BEgressQueue::GetLastQueue()
	{
		return m_qlast;
	}

	Ptr<const Packet>
		BEgressQueue::PeekQueue(uint32_t i) 
	{
		if (m_queues[i]->IsEmpty())
			return nullptr;
		return m_queues[i]->Peek();
	}

	void
		BEgressQueue::RecoverQueue(Ptr<DropTailQueue<Packet>> buffer, uint32_t i)
	{
		Ptr<Packet> packet;
		Ptr<DropTailQueue<Packet>> tmp = CreateObject<DropTailQueue<Packet>>();
		//clear orignial queue
		while (!m_queues[i]->IsEmpty())
		{
			packet = m_queues[i]->Dequeue();
			m_bytesInQueue[i] -= packet->GetSize();
			m_bytesInQueueTotal -= packet->GetSize();
		}
		//recover queue and preserve buffer
		while (!buffer->IsEmpty())
		{
			packet = buffer->Dequeue();
			tmp->Enqueue(packet->Copy());
			m_queues[i]->Enqueue(packet->Copy());
			m_bytesInQueue[i] += packet->GetSize();
			m_bytesInQueueTotal += packet->GetSize();
		}
		//restore buffer
		while (!tmp->IsEmpty())
		{
			buffer->Enqueue(tmp->Dequeue()->Copy());
		}

	}

	
	bool
	BEgressQueue::Enqueue (Ptr<Packet> item) {
		std::cerr << "BEgressQueue must not be called through this method"  << std::endl;
		return Enqueue(item, 1);
	}
	

	Ptr<Packet>
	BEgressQueue::Dequeue (void) {
		bool paused[qCnt];
		std::cerr << "BEgressQueue must not be called through this method"  << std::endl;
		return Dequeue(paused);
	}

	Ptr<Packet>  
	BEgressQueue::Remove (void) {
		std::cerr << "BEgressQueue must not be called through this method"  << std::endl;
		return nullptr;
	}

	Ptr<const Packet> 
	BEgressQueue::Peek (void) const {
		std::cerr << "BEgressQueue must not be called through this method"  << std::endl;
		return DoPeek();
	}


}