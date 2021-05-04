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

#ifndef BROADCOM_EGRESS_H
#define BROADCOM_EGRESS_H

#include <queue>
#include "ns3/packet.h"
#include "ns3/queue.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/event-id.h"

#define RECORD_QUEUE_LEN 1


namespace ns3 {

	class TraceContainer;

	class BEgressQueue : public Queue<Packet> {
	public:
		static TypeId GetTypeId(void);
		static const unsigned fCnt = 128; //max number of queues, 128 for NICs
		static const unsigned qCnt = 8; //max number of queues, 8 for switches
		BEgressQueue();
		virtual ~BEgressQueue();
		bool Enqueue(Ptr<Packet> p, uint32_t qIndex);
		Ptr<Packet> Dequeue(bool paused[]);
		Ptr<Packet> DequeueRR(bool paused[]);
		Ptr<Packet> DequeueNIC(bool paused[]);//QCN disable NIC
		Ptr<Packet> DequeueQCN(bool paused[], Time avail[], uint32_t m_findex_qindex_map[]);//QCN enable NIC
		Ptr<Packet> DequeueIRN(bool paused[], uint32_t m_findex_qindex_map[]);
		Ptr<Packet> DequeuePFC(bool paused[], uint32_t m_findex_qindex_map[]);
		uint32_t bdpfcIRN(bool paused[], uint32_t m_findex_qindex_map[], uint32_t m_irn_maxSeq[], uint32_t m_irn_maxAck[], uint32_t m_irn_bdp);
		uint32_t GetNBytes(uint32_t qIndex) const;
		uint32_t GetNBytesTotal() const;
		uint32_t GetLastQueue();
		uint32_t m_fcount;
		void RecoverQueue(Ptr<DropTailQueue<Packet>> buffer, uint32_t i);
		Ptr<const Packet> PeekQueue(uint32_t i) ;
		virtual bool Enqueue (Ptr<Packet> item);
		virtual Ptr<Packet> Dequeue (void);
		virtual Ptr<Packet>  Remove (void);
		virtual Ptr<const Packet> Peek (void) const;
		int nodeId{-1};
		uint32_t stat_pfc_pass{0};
		uint32_t stat_pfc_block{0};
		void PrintStat();

	private:
		bool DoEnqueue(Ptr<Packet> p, uint32_t qIndex);
		Ptr<Packet> DoDequeue(bool paused[]);
		Ptr<Packet> DoDequeueNIC(bool paused[]);
		Ptr<Packet> DoDequeueRR(bool paused[]);
		Ptr<Packet> DoDequeueQCN(bool paused[], Time avail[], uint32_t m_findex_qindex_map[]);
		Ptr<Packet> DoDequeueIRN(bool paused[], uint32_t m_findex_qindex_map[]);
		Ptr<Packet> DoDequeuePFC(bool paused[], uint32_t m_findex_qindex_map[]);
		//for compatibility
		virtual bool DoEnqueue(Ptr<Packet> p);
		virtual Ptr<Packet> DoDequeue(void);
		virtual Ptr<const Packet> DoPeek(void) const;
		std::queue<Ptr<Packet> > m_packets;
		uint32_t m_maxPackets;
		double m_maxBytes; //total bytes limit
		uint32_t m_qmincell; //guaranteed see page 126
		uint32_t m_queuelimit; //limit for each queue
		uint32_t m_shareused; //used bytes by sharing
		uint32_t m_bytesInQueue[fCnt];
		uint32_t m_bytesInQueueTotal;
		uint32_t m_rrlast;
		uint32_t m_qlast;
		std::vector<Ptr<Queue<Packet>>> m_queues; // uc queues
		//For strict priority
		Time m_bwsatisfied[qCnt];
		DataRate m_minBW[qCnt];

#if RECORD_QUEUE_LEN
		std::vector<std::pair<uint32_t, uint32_t>> queuelen_log;
#endif
	};

} // namespace ns3

#endif /* DROPTAIL_H */
