/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2007, 2008 University of Washington
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
* Author: Yibo Zhu <yibzh@microsoft.com>, Hwijoon Lim <wjuni@kaist.ac.kr>
*/

#define __STDC_LIMIT_MACROS 1
#include <stdint.h>
#include <stdio.h>
#include "qbb-net-device.h"
#include "ns3/net-device-queue-interface.h"
#include "ns3/log.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/double.h"
#include "ns3/data-rate.h"
#include "ns3/object-vector.h"
#include "pause-header.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/assert.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-header.h"
#include "ns3/simulator.h"
#include "ns3/point-to-point-channel.h"
#include "qbb-channel.h"
#include "ns3/random-variable-stream.h"
#include "ns3/flow-id-tag.h"
#include "qbb-header.h"
#include "ns3/error-model.h"
#include "cn-header.h"
#include "ns3/ppp-header.h"
#include "ns3/udp-header.h"
#include "ns3/tcp-header.h"
#include "ns3/seq-ts-header.h"
#include "ns3/tlt-tag.h"
#include "irn-header.h"
#include "ns3/flow-stat-tag.h"
#include <iostream>

#define DEBUG_PRINT 0
namespace ns3 {


	static uint64_t txUimpBytes = 0;
	static uint64_t txImpBytes = 0;
	static uint64_t txImpEBytes = 0;
	static uint64_t txTltDropBytes = 0;
	static uint64_t pause_cnt = 0;
	static uint64_t txImpCount = 0;
	static uint64_t txImpECount = 0;
	static uint64_t txUimpCount = 0;
	static double last_recv = 0;
	
	static uint64_t importantDropBytes = 0;
	static uint64_t importantDropCnt = 0;
	static uint64_t unimportantDropCnt = 0;
	static Time totalPauseDuration = Time(0);
	static bool stat_print = true;

	NS_LOG_COMPONENT_DEFINE("QbbNetDevice");

	NS_OBJECT_ENSURE_REGISTERED(QbbNetDevice);

	TypeId
		QbbNetDevice::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::QbbNetDevice")
			.SetParent<PointToPointNetDevice>()
			.AddConstructor<QbbNetDevice>()
			.AddAttribute("QbbEnabled",
				"Enable the generation of PAUSE packet.",
				BooleanValue(true),
				MakeBooleanAccessor(&QbbNetDevice::m_qbbEnabled),
				MakeBooleanChecker())
			.AddAttribute("QcnEnabled",
				"Enable the generation of PAUSE packet.",
				BooleanValue(false),
				MakeBooleanAccessor(&QbbNetDevice::m_qcnEnabled),
				MakeBooleanChecker())
			.AddAttribute("DynamicThreshold",
				"Enable dynamic threshold.",
				BooleanValue(false),
				MakeBooleanAccessor(&QbbNetDevice::m_dynamicth),
				MakeBooleanChecker())
			.AddAttribute("ClampTargetRate",
				"Clamp target rate.",
				BooleanValue(false),
				MakeBooleanAccessor(&QbbNetDevice::m_EcnClampTgtRate),
				MakeBooleanChecker())
			.AddAttribute("ClampTargetRateAfterTimeInc",
				"Clamp target rate after timer increase.",
				BooleanValue(false),
				MakeBooleanAccessor(&QbbNetDevice::m_EcnClampTgtRateAfterTimeInc),
				MakeBooleanChecker())
			.AddAttribute("PauseTime",
				"Number of microseconds to pause upon congestion",
				UintegerValue(5),
				MakeUintegerAccessor(&QbbNetDevice::m_pausetime),
				MakeUintegerChecker<uint32_t>())
			.AddAttribute("CNPInterval",
				"The interval of generating CNP",
				DoubleValue(50.0),
				MakeDoubleAccessor(&QbbNetDevice::m_qcn_interval),
				MakeDoubleChecker<double>())
			.AddAttribute("AlphaResumInterval",
				"The interval of resuming alpha",
				DoubleValue(55.0),
				MakeDoubleAccessor(&QbbNetDevice::m_alpha_resume_interval),
				MakeDoubleChecker<double>())
			.AddAttribute("RPTimer",
				"The rate increase timer at RP in microseconds",
				DoubleValue(1500.0),
				MakeDoubleAccessor(&QbbNetDevice::m_rpgTimeReset),
				MakeDoubleChecker<double>())
			.AddAttribute("FastRecoveryTimes",
				"The rate increase timer at RP",
				UintegerValue(5),
				MakeUintegerAccessor(&QbbNetDevice::m_rpgThreshold),
				MakeUintegerChecker<uint32_t>())
			.AddAttribute("DCTCPGain",
				"Control gain parameter which determines the level of rate decrease",
				DoubleValue(1.0 / 16),
				MakeDoubleAccessor(&QbbNetDevice::m_g),
				MakeDoubleChecker<double>())
			.AddAttribute("MinRate",
				"Minimum rate of a throttled flow",
				DataRateValue(DataRate("100Mb/s")),
				MakeDataRateAccessor(&QbbNetDevice::m_minRate),
				MakeDataRateChecker())
			.AddAttribute("ByteCounter",
				"Byte counter constant for increment process.",
				UintegerValue(150000),
				MakeUintegerAccessor(&QbbNetDevice::m_bc),
				MakeUintegerChecker<uint32_t>())
			.AddAttribute("RateAI",
				"Rate increment unit in AI period",
				DataRateValue(DataRate("5Mb/s")),
				MakeDataRateAccessor(&QbbNetDevice::m_rai),
				MakeDataRateChecker())
			.AddAttribute("RateHAI",
				"Rate increment unit in hyperactive AI period",
				DataRateValue(DataRate("50Mb/s")),
				MakeDataRateAccessor(&QbbNetDevice::m_rhai),
				MakeDataRateChecker())
			.AddAttribute("NPSamplingInterval",
				"The QCN NP sampling interval",
				DoubleValue(0.0),
				MakeDoubleAccessor(&QbbNetDevice::m_qcn_np_sampling_interval),
				MakeDoubleChecker<double>())
			.AddAttribute("NackGenerationInterval",
				"The NACK Generation interval",
				DoubleValue(10),
				MakeDoubleAccessor(&QbbNetDevice::m_nack_interval),
				MakeDoubleChecker<double>())
			.AddAttribute("L2BackToZero",
				"Layer 2 go back to zero transmission.",
				BooleanValue(false),
				MakeBooleanAccessor(&QbbNetDevice::m_backto0),
				MakeBooleanChecker())
			.AddAttribute("L2TestRead",
				"Layer 2 test read go back 0 but NACK from n.",
				BooleanValue(false),
				MakeBooleanAccessor(&QbbNetDevice::m_testRead),
				MakeBooleanChecker())
			.AddAttribute("L2ChunkSize",
				"Layer 2 chunk size. Disable chunk mode if equals to 0.",
				UintegerValue(0),
				MakeUintegerAccessor(&QbbNetDevice::m_chunk),
				MakeUintegerChecker<uint32_t>())
			.AddAttribute("L2AckInterval",
				"Layer 2 Ack intervals. Disable ack if equals to 0.",
				UintegerValue(0),
				MakeUintegerAccessor(&QbbNetDevice::m_ack_interval),
				MakeUintegerChecker<uint32_t>())
			.AddAttribute("L2WaitForAck",
				"Wait for Ack before sending out next message.",
				BooleanValue(false),
				MakeBooleanAccessor(&QbbNetDevice::m_waitAck),
				MakeBooleanChecker())
			.AddAttribute("L2WaitForAckTimer",
				"Sender's timer of waiting for the ack",
				DoubleValue(4000.0),
				MakeDoubleAccessor(&QbbNetDevice::m_waitAckTimer),
				MakeDoubleChecker<double>())
			.AddAttribute("EnableTLT",
				"Enable TLT",
				BooleanValue(false),
				MakeBooleanAccessor(&QbbNetDevice::m_tltEnabled),
				MakeBooleanChecker())
			.AddAttribute("EnableIRN",
				"Enable IRN",
				BooleanValue(false),
				MakeBooleanAccessor(&QbbNetDevice::m_irn),
				MakeBooleanChecker())
			.AddAttribute("RtoLow",
				"Low RTO for IRN",
				UintegerValue(100),
				MakeUintegerAccessor(&QbbNetDevice::m_irn_rtoLow),
				MakeUintegerChecker<uint32_t>())
			.AddAttribute("RtoHigh",
				"High RTO for IRN",
				UintegerValue(320),
				MakeUintegerAccessor(&QbbNetDevice::m_irn_rtoHigh),
				MakeUintegerChecker<uint32_t>())
			.AddAttribute("BDP",
				"Bandwidth-delay product for IRN",
				UintegerValue(20),
				MakeUintegerAccessor(&QbbNetDevice::m_irn_bdp),
				MakeUintegerChecker<uint32_t>())
			.AddAttribute("usequeue_flownum",
							"Use queue number as much as flow",
							BooleanValue(true),
							MakeBooleanAccessor(&QbbNetDevice::m_uselotofqueue),
							MakeBooleanChecker())
			.AddAttribute("TLT_IMPORTANT_PKT_INTERVAL",
							"Important packet interval",
							UintegerValue(48),
							MakeUintegerAccessor(&QbbNetDevice::TLT_IMPORTANT_PKT_INTERVAL),
							MakeUintegerChecker<uint32_t>())
			.AddAttribute("PFC_ENABLED",
							"ENABLE PFC",
							BooleanValue(true),
							MakeBooleanAccessor(&QbbNetDevice::m_PFCenabled),
							MakeBooleanChecker())
			;

		return tid;
	}

	QbbNetDevice::QbbNetDevice()
	{
		NS_LOG_FUNCTION(this);
		m_ecn_source = new std::vector<ECNAccount>;
		m_tltPacketCount = 0;
		for (uint32_t i = 0; i < qCnt; i++)
		{
			m_paused[i] = false;
			m_pauseStart[i] = Time(0);
			m_pauseDuration[i] = Time(0);
		}
		m_qcn_np_sampling = 0;
		for (uint32_t i = 0; i < fCnt; i++)
		{
			m_credits[i] = 0;
			m_nextAvail[i] = Time(0);
			m_findex_udpport_map[i] = 0;
			m_findex_qindex_map[i] = 0;
			m_waitingAck[i] = false;
			for (uint32_t j = 0; j < maxHop; j++)
			{
				m_txBytes[i][j] = m_bc;				//we don't need this at the beginning, so it doesn't matter what value it has
				m_rpWhile[i][j] = m_rpgTimeReset;	//we don't need this at the beginning, so it doesn't matter what value it has
				m_rpByteStage[i][j] = 0;
				m_rpTimeStage[i][j] = 0;
				m_alpha[i][j] = 0.5;
				m_rpStage[i][j] = 0; //not in any qcn stage
			}

			m_irn_maxAck[i] = 0;
			m_irn_maxSeq[i] = 0;
		}
		for (uint32_t i = 0; i < pCnt; i++)
		{
			m_ECNState[i] = 0;
			m_ECNIngressCount[i] = 0;
			m_ECNEgressCount[i] = 0;
		}

		m_uniform_random_var.SetStream(0);
	}

	QbbNetDevice::~QbbNetDevice()
	{
		NS_LOG_FUNCTION(this);
	}

	void
		QbbNetDevice::DoDispose()
	{
		NS_LOG_FUNCTION(this);
		// Cancel all the Qbb events
		for (uint32_t i = 0; i < qCnt; i++)
		{
			Simulator::Cancel(m_resumeEvt[i]);
		}

		for (uint32_t i = 0; i < fCnt; i++)
		{
			Simulator::Cancel(m_rateIncrease[i]);
		}

		for (uint32_t i = 0; i < pCnt; i++)
			for (uint32_t j = 0; j < qCnt; j++)
				Simulator::Cancel(m_recheckEvt[i][j]);
		PointToPointNetDevice::DoDispose();
		


		if(stat_print) {
  		printf("%.8lf\tIMP_STAT\t%p\t%lu\n", Simulator::Now().GetSeconds(), this, txImpBytes);
  		printf("%.8lf\tIMPE_STAT\t%p\t%lu\n", Simulator::Now().GetSeconds(), this, txImpEBytes);
  		printf("%.8lf\tUIMP_STAT\t%p\t%lu\n", Simulator::Now().GetSeconds(), this, txUimpBytes);
  		printf("%.8lf\tTLT_DROP\t%p\t%lu\n", Simulator::Now().GetSeconds(), this, txTltDropBytes);
  		printf("%.8lf\tPAUSE\t%p\t%lu\n", Simulator::Now().GetSeconds(), this, pause_cnt);
  		printf("%.8lf\tMAX_FCT\t%p\t%.8lf\n", Simulator::Now().GetSeconds(), this, last_recv); 
  		printf("%.8lf\tIMP_DROP_BYTES\t%p\t%lu\n", Simulator::Now().GetSeconds(), this, importantDropBytes); 
  		printf("%.8lf\tIMP_DROP_CNT\t%p\t%lu\n", Simulator::Now().GetSeconds(), this, importantDropCnt); 
  		printf("%.8lf\tUIMP_DROP_CNT\t%p\t%lu\n", Simulator::Now().GetSeconds(), this, unimportantDropCnt); 
  		printf("%.8lf\tIMP_CNT\t%p\t%lu\n", Simulator::Now().GetSeconds(), this, txImpCount); 
  		printf("%.8lf\tIMPE_CNT\t%p\t%lu\n", Simulator::Now().GetSeconds(), this, txImpECount); 
  		printf("%.8lf\tUIMP_CNT\t%p\t%lu\n", Simulator::Now().GetSeconds(), this, txUimpCount); 
		printf("%.8lf\tPFC_TIME_TOTAL\t%p\t%.8lf\n", Simulator::Now().GetSeconds(), this, totalPauseDuration.GetSeconds()); 
		stat_print = false;
		}

		// Time pd = Time(0);
		// for (unsigned i = 0; i < qCnt; i++)
		// 	pd = pd + m_pauseDuration[i];

		// printf("%.8lf\tPFC_TIME\t%p\t%.8lf\n", Simulator::Now().GetSeconds(), this, pd.GetSeconds()); 
	}

	void
		QbbNetDevice::TransmitComplete(void)
	{
		NS_LOG_FUNCTION(this);
		NS_ASSERT_MSG(m_txMachineState == BUSY, "Must be BUSY if transmitting");
		m_txMachineState = READY;
		NS_ASSERT_MSG(m_currentPkt != 0, "QbbNetDevice::TransmitComplete(): m_currentPkt zero");
		m_phyTxEndTrace(m_currentPkt);
		m_currentPkt = 0;
		DequeueAndTransmit();
	}


	uint32_t QbbNetDevice::GetPacketsInflight(uint32_t i) {
		uint32_t t;
		if (m_irn_maxAck[i] > m_irn_maxSeq[i]) {
			//std::cout << "erre\n";
			t = 0;
		} else {
			t = m_irn_maxSeq[i] - m_irn_maxAck[i];
		}
		return t;
	}
	void
		QbbNetDevice::DequeueAndTransmit(void) {
		//NS_LOG_FUNCTION(this);
		//uint32_t nodeid = m_node->GetId();
		if (m_txMachineState == BUSY) return;	// Quit if channel busy
		Ptr<Packet> p = 0;

		bool irnBdp = false;
		uint16_t protocol_;
		// IRN BDP-FC implementation
		uint32_t flowid = 0;
		if (m_irn && m_node->GetNodeType() == 0) {
			Ptr<Packet> packet;

			for (uint32_t i = 1; i < m_queue->m_fcount; i++)	//distribute credits
			{
				if (m_queue->PeekQueue(i) == NULL) {
					continue;
				}
				packet = m_queue->PeekQueue(i)->Copy();

				Ipv4Header h;
				//uint16_t protocol = 0;
				ProcessHeader(packet, protocol_);
				packet->RemoveHeader(h);
				UdpHeader udph;
				packet->RemoveHeader(udph);
				SeqTsHeader sth;
				packet->RemoveHeader(sth);
				m_irn_maxSeq[i] = sth.GetSeq();

			}
			//this sees if their is queue which is not empty, and returns the flow index of queue which will be next dequeued
			flowid = m_queue->bdpfcIRN(m_paused, m_findex_qindex_map, m_irn_maxSeq, m_irn_maxAck, m_irn_bdp);
			//if (flowid != 0) {//nack ack will be filtered by protocol number
			//	packet = m_queue->PeekQueue(flowid - 1)->Copy();
			//	Ipv4Header h;
			//	//uint16_t protocol = 0;
			//	ProcessHeader(packet, protocol_);
			//	packet->RemoveHeader(h);
			//	UdpHeader udph;
			//	packet->RemoveHeader(udph);
			//	SeqTsHeader sth;
			//	packet->RemoveHeader(sth);
			//	protocol_ = h.GetProtocol();
			//	//flow id 1 change, check if there is queue which should be sent, not just ending dequetransmit
			//}

			//I used flowid==0 as error code here
			// flowid==1 means high priority, m_queue[0]
			if (flowid == 0) {
				return; //if bdpfc says stop don't send more packets.
			}
		}
		//if flow id is n
		if ((m_irn  && m_node->GetNodeType() == 0) && (flowid > 1) && (flowid != 0 && GetPacketsInflight(flowid - 1) > m_irn_bdp)) {
			irnBdp = true;
			//m_irn_bdp += 1; // Actual packet is one less 
		} else {

			if (m_node->GetNodeType() == 0 && m_qcnEnabled) //QCN enable NIC    
			{
				p = m_queue->DequeueQCN(m_paused, m_nextAvail, m_findex_qindex_map);
			} else if (m_node->GetNodeType() == 0 && m_irn) //irn enable NIC    
			{
				p = m_queue->DequeueIRN(m_paused, m_findex_qindex_map);
			} else if (m_node->GetNodeType() == 0 && m_uselotofqueue) //QCN disable NIC
			{
				//p = m_queue->DequeueNIC(m_paused);//use 8 queue
				p = m_queue->DequeuePFC(m_paused, m_findex_qindex_map);
			} else if (m_node->GetNodeType() == 0 && !m_uselotofqueue) {
				p = m_queue->DequeueNIC(m_paused);
			} else   //switch, doesn't care about qcn, just send
			{
				//p = m_queue->Dequeue(m_paused);		//this is strict priority
				p = m_queue->DequeueRR(m_paused);		//this is round-robin
			}
		}

		if (p != 0) {
			
        #if DEBUG_PRINT
			TltTag tlt;
			int32_t socketId = tlt.debug_socketId;
			if (m_tltEnabled && p->PeekPacketTag(tlt)) {
				socketId = tlt.debug_socketId;
				if(socketId>=0) 
					std::cerr<< "Flow " << (int)tlt.debug_socketId << " : Deque/Transmit Target Found - " << (int)tlt.GetType() << std::endl; 
			}
		#endif

			m_snifferTrace(p);
			/*if (nodeid == 3) {// check if dequeueand transmit is sending out correctly
			std::cout << Simulator::Now();
			}*/
			m_promiscSnifferTrace(p);
			Ipv4Header h;
			Ptr<Packet> packet = p->Copy();
			uint16_t protocol = 0;
			ProcessHeader(packet, protocol);
			packet->RemoveHeader(h);
			FlowIdTag t;
			//uint32_t pg_v;
			//pg_v = GetPriority(p->Copy());
			if (m_node->GetNodeType() == 0) //I am a NIC, do QCN
			{
				uint32_t fIndex = m_queue->GetLastQueue();
				if (m_qcnEnabled) {
					if (m_rate[fIndex] == 0)			//late initialization	
					{
						m_rate[fIndex] = m_bps;
						for (uint32_t j = 0; j < maxHop; j++) {
							m_rateAll[fIndex][j] = m_bps;
							m_targetRate[fIndex][j] = m_bps;
						}
					}
					double creditsDue = std::max(0.0, m_bps.GetBitRate() / m_rate[fIndex].GetBitRate() * (p->GetSize() - m_credits[fIndex]));
					Time nextSend = m_tInterframeGap + (m_bps.CalculateBytesTxTime(creditsDue));
					m_nextAvail[fIndex] = Simulator::Now() + nextSend;
					for (uint32_t i = 0; i < m_queue->m_fcount; i++)	//distribute credits
					{
						if (m_nextAvail[i].GetTimeStep() <= Simulator::Now().GetTimeStep())
							m_credits[i] += m_rate[i].GetBitRate() / m_bps.GetBitRate()*creditsDue;
					}
					m_credits[fIndex] = 0;	//reset credits
					for (uint32_t i = 0; i < 1; i++) {
						if (m_rpStage[fIndex][i] > 0)
							m_txBytes[fIndex][i] -= p->GetSize();
						else
							m_txBytes[fIndex][i] = m_bc;
						if (m_txBytes[fIndex][i] < 0) {
							if (m_rpStage[fIndex][i] == 1) {
								rpr_fast_byte(fIndex, i);
							} else if (m_rpStage[fIndex][i] == 2) {
								rpr_active_byte(fIndex, i);
							} else if (m_rpStage[fIndex][i] == 3) {
								rpr_hyper_byte(fIndex, i);
							}
						}
					}
				} else {
					Time nextSend = m_tInterframeGap;
					m_nextAvail[fIndex] = Simulator::Now() + nextSend;
				}

				//not send in chunks
				if (h.GetProtocol() == 17 && m_waitAck && !m_irn) //if it's udp, check wait_for_ack
				{
					UdpHeader udph;
					packet->RemoveHeader(udph);
					SeqTsHeader sth;
					packet->RemoveHeader(sth);
					unsigned int i;
					uint32_t port = udph.GetSourcePort();
					if (!m_uselotofqueue) {
						for (i = 1; i < m_queue->m_fcount; i++) {
							if (m_findex_udpport_map[i] == port && m_findex_qindex_map[i] == fIndex) {
								break;
							}
						}
						if (i == m_queue->m_fcount) {
							std::cout << "ERROR: NACK NIC cannot find the flow\n";
							return;
						}
					} else {
						i = fIndex;
					}
					if (sth.GetSeq() >= m_milestone_tx[i]) {
						//m_nextAvail[fIndex] = Simulator::Now() + Seconds(32767); //stop sending this flow until acked
						if (!m_waitingAck[i]) {
							//std::cout << "Waiting the ACK of the message of flow " << fIndex << " " << sth.GetSeq() << ".\n";
							//fflush(stdout);

							m_retransmit[i] = Simulator::Schedule(MicroSeconds(m_waitAckTimer), &QbbNetDevice::Retransmit, this, i);
							m_waitingAck[i] = true;
						} else {
							/*Simulator::Cancel(m_retransmit[i]);
							m_retransmit[i] = Simulator::Schedule(MicroSeconds(m_waitAckTimer), &QbbNetDevice::Retransmit, this, i);
							m_waitingAck[i] = true;*/
						}

					} else {
						if (m_uselotofqueue) {
							//NS_ASSERT_MSG(0,"SHOULD THIS HAPPEN, even if I erase all buffer before milestontx");
							//std::cout << "SHOULD THIS HAPPEN, even if I erase all buffer before milestontx";
						}
						//skip packet which is already sent
						DequeueAndTransmit();
						return;
					}
				}

				p->RemovePacketTag(t);

				// Check here for TLT

				if (m_tltEnabled) {
					TltTag tlt;
					FlowStatTag fst;
					// only do this if RDMA
					if (h.GetProtocol() == 17) {
						if (m_tltPacketCount % TLT_IMPORTANT_PKT_INTERVAL == TLT_IMPORTANT_PKT_INTERVAL - 1 || h.GetProtocol() != 17 || (packet->PeekPacketTag(fst) && fst.GetType() == FlowStatTag::FLOW_END) || (m_irn &&  m_irn_bdp - 1 == GetPacketsInflight(fIndex))) {
							//std::cout << "Important" << std::endl;
							tlt.SetType(TltTag::PACKET_IMPORTANT);
						} else {
							//std::cout << "Nonimportant" << std::endl;
							tlt.SetType(TltTag::PACKET_NOT_IMPORTANT);
						}
					}
					//Only update when it was normal RDMA packets, except for last packet of flow, and which is ACK, NACK, ECN ... etc
					if (h.GetProtocol() == 17 && !(fst.GetType() == FlowStatTag::FLOW_END) && !(m_irn && m_irn_bdp - 1 == GetPacketsInflight(fIndex))) {
						m_tltPacketCount = (m_tltPacketCount + 1) % TLT_IMPORTANT_PKT_INTERVAL;
					}

					// only do this if RDMA
					if (h.GetProtocol() == 17)
						p->AddPacketTag(tlt);
				}

				TransmitStart(p);
			} else //I am a switch, do ECN if this is not a pause
			{
				if (m_queue->GetLastQueue() == qCnt - 1)//this is a pause or cnp, send it immediately!
				{

					//NS_LOG_INFO("QCN or CNP FRAME SENT TO NODE ");
					Ptr<Packet> pp = p->Copy();
					PppHeader ppp;
					pp->RemoveHeader(ppp);
					p->RemovePacketTag(t);
					TransmitStart(p);
				} else {
					//switch ECN
					p->PeekPacketTag(t);
					uint32_t inDev = t.GetFlowId();
					/*m_node->m_broadcom->RemoveFromIngressAdmission(inDev, m_queue->GetLastQueue(), p->GetSize());
					m_node->m_broadcom->RemoveFromEgressAdmission(m_ifIndex, m_queue->GetLastQueue(), p->GetSize());*/
					//if (m_qcnEnabled) {
						PppHeader ppp;
						p->RemoveHeader(ppp);
						p->RemoveHeader(h);
						bool egressCongested = ShouldSendCN(inDev, m_ifIndex, m_queue->GetLastQueue());
						if (egressCongested) {
							h.SetEcn((Ipv4Header::EcnType)0x03);
						}
						p->AddHeader(h);
						p->AddHeader(ppp);
					//}
					p->RemovePacketTag(t);
					if (TransmitStart(p)) {
						m_node->m_broadcom->RemoveFromIngressAdmission(inDev, m_queue->GetLastQueue(), p->GetSize());
						m_node->m_broadcom->RemoveFromEgressAdmission(m_ifIndex, m_queue->GetLastQueue(), p->GetSize());
						TltTag tlt;
						if (m_tltEnabled && p->PeekPacketTag(tlt) && tlt.GetType() == TltTag::PACKET_NOT_IMPORTANT) {
							m_node->m_broadcom->RemoveFromIngressTLT(inDev, m_queue->GetLastQueue(), p->GetSize());
							m_node->m_broadcom->RemoveFromEgressTLT(m_ifIndex, m_queue->GetLastQueue(), p->GetSize());
						}
					} else {
						//
					}

				}
			}
			return;
		} else //No queue can deliver any packet
		{
			if (irnBdp) {
				NS_LOG_INFO("IRN BDP-FC prohibits send at node " << m_node->GetId());
			} else {
				//NS_LOG_INFO("PAUSE prohibits send at node " << m_node->GetId());
			}

			//NS_LOG_INFO("PAUSE prohibits send at node " << m_node->GetId());
			//Check if this is necessary for irnBdp
			if (m_node->GetNodeType() == 0 && (m_qcnEnabled)) //nothing to send, possibly due to qcn flow control, if so reschedule sending
			{
				Time t = Simulator::GetMaximumSimulationTime();
				for (uint32_t i = 0; i < m_queue->m_fcount; i++) {
					if (m_queue->GetNBytes(i) == 0)
						continue;
					t = Min(m_nextAvail[i], t);
				}
				if (m_nextSend.IsExpired() &&
					t < Simulator::GetMaximumSimulationTime() &&
					t.GetTimeStep() > Simulator::Now().GetTimeStep()) {
					NS_LOG_LOGIC("Next DequeueAndTransmit at " << t << " or " << (t - Simulator::Now()) << " later");
					NS_ASSERT(t > Simulator::Now());
					m_nextSend = Simulator::Schedule(t - Simulator::Now(), &QbbNetDevice::DequeueAndTransmit, this);
				}
			}
			/*else {
			Simulator::Schedule(MicroSeconds(1), &QbbNetDevice::DequeueAndTransmit, this);
			}*/
		}
		return;
	}

	void
		QbbNetDevice::Retransmit(uint32_t findex) {
		uint32_t nodeid = m_node->GetId();
		std::cout << "TLT: " << m_tltEnabled << " Resending the message of flow " << findex << " node: " << nodeid << " \n";
		fflush(stdout);
		NS_LOG_INFO("TLT: " << m_tltEnabled << " RETRANSMISSION OCCURED " << nodeid << " \n");
		if (m_qcnEnabled || m_uselotofqueue) {
			m_queue->RecoverQueue(m_sendingBuffer[findex], findex);
		} else {//if not qcn, all flow uses same queue 
			uint32_t pg_temp;
			Ptr<DropTailQueue<Packet>> tmp = CreateObject<DropTailQueue<Packet>>();
			Ptr<Packet> packet;
			//recover queue and preserve buffer
			while (!m_sendingBuffer[findex]->IsEmpty()) {
				packet = m_sendingBuffer[findex]->Dequeue();
				pg_temp = m_findex_qindex_map[findex];
				tmp->Enqueue(packet->Copy());
				m_queue->Enqueue(packet, pg_temp);
			}
			//restore buffer
			while (!tmp->IsEmpty()) {
				m_sendingBuffer[findex]->Enqueue(tmp->Dequeue()->Copy());
			}
		}

		//youngmok
		//m_nextAvail[findex] = Simulator::Now();
		m_waitingAck[findex] = false;
		//
		if (m_sendingBuffer[findex]->IsEmpty()) {

		} else {
			DequeueAndTransmit();
		}
	}



	void
		QbbNetDevice::Resume(unsigned qIndex)
	{
		NS_LOG_FUNCTION(this << qIndex);
		NS_ASSERT_MSG(m_paused[qIndex], "Must be PAUSEd");
		m_paused[qIndex] = false;
		Time diff = Simulator::Now() - m_pauseStart[qIndex];
		m_pauseDuration[qIndex] = m_pauseDuration[qIndex] + diff;
		totalPauseDuration = totalPauseDuration + diff;
		m_pauseStart[qIndex] = Seconds(0);
		NS_LOG_INFO("Node " << m_node->GetId() << " dev " << m_ifIndex << " queue " << qIndex << " resumed at " << Simulator::Now().GetSeconds());
		DequeueAndTransmit();
	}

	void
		QbbNetDevice::PauseFinish(unsigned qIndex)
	{
		Resume(qIndex);
	}


	void
		QbbNetDevice::RetransmitIRN(uint32_t seq, uint32_t i) {
		//uint32_t nodeid = m_node->GetId();
		if (m_sendingBuffer[i]->Peek() && seq < GetSeq(m_sendingBuffer[i]->Peek()->Copy())) {
			return;
		}

		// check if it is still not acked
		bool acked = true;
		for (std::deque<Ptr<Packet>>::iterator it = m_irn_retransmitted[i].begin(); it != m_irn_retransmitted[i].end(); ++it) {
			if (GetSeq((*it)->Copy()) == seq) {
				acked = false ;
				break;
			}
		}
		if (acked) return;

		//std::cout << "[SEND] RETRANSMITTED PACKET DROPPED AGAIN DETECTED BY TIMEOUT" << std::endl;
		m_irn_bdp += 1; // Actual Packets are 1 lesser
		uint32_t buffer_seq_ifnoretrans = 0;
		if (m_queue->PeekQueue(i)) {
			buffer_seq_ifnoretrans = GetSeq(m_queue->PeekQueue(i)->Copy());
		}
		// originally, buffer_seq_ifnoretrans must hold "not transmitted packets" only, but the front one might be untransmitted retransmission Packet (If NACK comes too early)
		// so check here if buffer_seq_ifnotretrans is real "not transmitted packets", or "untransmitted retransmission"
		std::deque<Ptr<Packet>> tmpq = m_irn_retransmitted[i];
		bool doRetransmission = true;
		while (!tmpq.empty()) {
			if (GetSeq(tmpq.front()->Copy()) == buffer_seq_ifnoretrans) {
				doRetransmission = false;
				break;
			}
			tmpq.pop_front();
		}
		if (!doRetransmission) {
			// NACKs Seq has not been sent... schedule RetransmitIRN again and retry
			Simulator::Schedule(MicroSeconds(100), &QbbNetDevice::RetransmitIRN, this, seq, i);
			return;
		}

		// update bitmap to set selective ack.
		std::queue<Ptr<Packet>> tmp;
		while (!m_sendingBuffer[i]->IsEmpty())
			tmp.push(m_sendingBuffer[i]->Dequeue());

		if (tmp.empty()) {
			return;
		}
		//uint32_t tmp_sack_target = (GetSeq(tmp.front()->Copy()));
		//std::cout << "[SEND] Pending to send " << tmp_sack_target << std::endl;

		Ptr<DropTailQueue<Packet>> transmit_q = CreateObject<DropTailQueue<Packet>>();
		while (!tmp.empty()) {
			uint32_t curseq = GetSeq(tmp.front()->Copy());
			//seq: what scheduled retransmission is goingto transmit
			//buffer_seq_ifnoretrans: currently sending packet
			if (curseq < seq) {
			m_sendingBuffer[i]->Enqueue(tmp.front());
			} else if (curseq == seq) {
				m_sendingBuffer[i]->Enqueue(tmp.front());
				//std::cout << "[SEND] TIMEOUT - Retransmit Seq #" << curseq << std::endl;
				m_irn_retransmitted[i].push_back(tmp.front()->Copy());
				transmit_q->Enqueue(tmp.front()->Copy());
				//m_irn_bdp -= 1;
				//std::cout << "[SEND] Retransmit Count = " << (++m_irn_retransmission_cnt) << std::endl;
				Simulator::Schedule(MicroSeconds(m_irn_rtoLow), &QbbNetDevice::RetransmitIRN, this, curseq, i);
			} else if (curseq < buffer_seq_ifnoretrans || buffer_seq_ifnoretrans == 0) {
				// sent but not nack/acked packet
				// put sendingbuffer, but not at transmit_q
				m_sendingBuffer[i]->Enqueue(tmp.front());
			}
			else {
				NS_ASSERT_MSG(curseq >= seq, "All packets smaller than acc. ack should have been removed");
				// packet that has not been sent
				// put both to sending buffer, and transmit_q
				m_sendingBuffer[i]->Enqueue(tmp.front());
				if (curseq == seq) {
					std::cout << "[SEND] TIMEOUT - Retransmit Seq #" << curseq << std::endl;
					m_irn_retransmitted[i].push_back(tmp.front()->Copy());
					transmit_q->Enqueue(tmp.front()->Copy());
					m_irn_bdp -= 1; // Actual packets are 1 more
					std::cout << "[SEND] Retransmit Count = " << (++m_irn_retransmission_cnt) << std::endl;
					Simulator::Schedule(MicroSeconds(m_irn_rtoLow), &QbbNetDevice::RetransmitIRN, this, GetSeq(tmp.front()->Copy()), i);
				}
				else {
					transmit_q->Enqueue(tmp.front()->Copy());
				}
			}
			tmp.pop();
		}

		// until here, transmit_q will have packet that has not been sent + packet on seq
		// fill m_queue
		m_queue->RecoverQueue(transmit_q, i);

		// we need to stop timer and force retransmit
		m_nextAvail[i] = Simulator::Now();
		Simulator::Cancel(m_retransmit[i]);
		DequeueAndTransmit();
	}

	void
		QbbNetDevice::Receive(Ptr<Packet> packet)
	{
		NS_LOG_FUNCTION(this << packet);
		last_recv = Simulator::Now().GetSeconds();

		if (m_receiveErrorModel && m_receiveErrorModel->IsCorrupt(packet))
		{
			// 
			// If we have an error model and it indicates that it is time to lose a
			// corrupted packet, don't forward this packet up, let it go.
			//
			m_phyRxDropTrace(packet);
			NS_ABORT_MSG("Unexpected Packet Drop");
			return;
		}

		uint16_t protocol = 0;

		Ptr<Packet> p = packet->Copy();
		ProcessHeader(p, protocol);

        #if DEBUG_PRINT
		TltTag tlt;
		int32_t socketId = tlt.debug_socketId;
		if (m_tltEnabled && p->PeekPacketTag(tlt)) {
			socketId = tlt.debug_socketId;
			if(socketId>=0) 
				std::cerr<< "Flow " << socketId << " : Recv from Device (" << (int)tlt.GetType() << ") : ";
				p->Print(std::cerr);
				std::cerr << std::endl; 
		}
		#endif
		
		Ipv4Header ipv4h;
		p->RemoveHeader(ipv4h);

		if ((ipv4h.GetProtocol() != 0xFF && ipv4h.GetProtocol() != 0xFD && ipv4h.GetProtocol() != 0xFC) || m_node->GetNodeType() > 0)
		{ //This is not QCN feedback, not NACK, or I am a switch so I don't care
			if (ipv4h.GetProtocol() != 0xFE) //not PFC
			{
				packet->AddPacketTag(FlowIdTag(GetIfIndex()));
				if (GetNode()->GetNodeType() == 0) //NIC
				{
					if (ipv4h.GetProtocol() == 17)	//look at udp only
					{
						uint16_t ecnbits = ipv4h.GetEcn();

						UdpHeader udph;
						p->RemoveHeader(udph);
						SeqTsHeader sth;
						p->PeekHeader(sth);

						p->AddHeader(udph);

						bool found = false;
						uint32_t i, key = 0;

						for (i = 0; i < m_ecn_source->size(); ++i)
						{
							if ((*m_ecn_source)[i].source == ipv4h.GetSource() && (*m_ecn_source)[i].qIndex == GetPriority(p->Copy()) && (*m_ecn_source)[i].port == udph.GetSourcePort())
							{
								found = true;
								if (ecnbits != 0 && Simulator::Now().GetMicroSeconds() > m_qcn_np_sampling)
								{
									(*m_ecn_source)[i].ecnbits |= ecnbits;
									(*m_ecn_source)[i].qfb++;
								}
								(*m_ecn_source)[i].total++;
								key = i;
							}
						}
						if (!found)
						{
							ECNAccount tmp;
							tmp.qIndex = GetPriority(p->Copy());
							tmp.source = ipv4h.GetSource();
							if (ecnbits != 0 && Simulator::Now().GetMicroSeconds() > m_qcn_np_sampling && tmp.qIndex != 1) //dctcp
							{
								tmp.ecnbits |= ecnbits;
								tmp.qfb = 1;
							}
							else
							{
								tmp.ecnbits = 0;
								tmp.qfb = 0;
							}
							tmp.total = 1;
							tmp.port = udph.GetSourcePort();
							ReceiverNextExpectedSeq[m_ecn_source->size()] = 0;
							m_nackTimer[m_ecn_source->size()] = Time(0);
							//YOUNGMOK
							m_milestone_rx[m_ecn_source->size()] = 0;
							m_lastNACK[m_ecn_source->size()] = -1;
							key = m_ecn_source->size();
							m_ecn_source->push_back(tmp);
							CheckandSendQCN(tmp.source, tmp.qIndex, tmp.port);
						}
						int x = ReceiverCheckSeq(sth.GetSeq(), key);
						NS_LOG_INFO("[RECV] SEQ = " << sth.GetSeq() << ", mode = " << x);
						
						if (x == 2 || x == 6) //generate NACK
						{
							Ptr<Packet> newp = Create<Packet>(0);
							if (m_irn) {
								irnHeader seqh;
								seqh.SetSeq(ReceiverNextExpectedSeq[key]);
								seqh.SetPG(GetPriority(p->Copy()));
								seqh.SetPort(udph.GetSourcePort());
								if(x == 2)
									seqh.SetSeqNack(sth.GetSeq());
								else {
									seqh.SetSeqNack(0); // NACK without ackSyndrome (ACK) in loss recovery mode
								}

								NS_LOG_INFO("[RECV] NACK by SEQ = " << seqh.GetSeq() << ", NACK=" << seqh.GetSeqNACK());
								newp->AddHeader(seqh);
							}
							else 
							{
								qbbHeader seqh;
								seqh.SetSeq(ReceiverNextExpectedSeq[key]);
								seqh.SetPG(GetPriority(p->Copy()));
								seqh.SetPort(udph.GetSourcePort());
								newp->AddHeader(seqh);
							}
							Ipv4Header head;	// Prepare IPv4 header
							head.SetDestination(ipv4h.GetSource());
							Ipv4Address myAddr = m_node->GetObject<Ipv4>()->GetAddress(m_ifIndex, 0).GetLocal();
							head.SetSource(myAddr);
							head.SetProtocol(0xFD); //nack=0xFD
							head.SetTtl(64);
							head.SetPayloadSize(newp->GetSize());
							head.SetIdentification(m_uniform_random_var.GetInteger(0, 65535));
							newp->AddHeader(head);
							uint32_t protocolNumber = 2048;
							AddHeader(newp, protocolNumber);	// Attach PPP header
							if (m_qcnEnabled || m_irn || m_uselotofqueue) {
								if (!m_queue->Enqueue(newp, 0)) {
									NS_LOG_INFO("NACK ENQUEUE FAIL");
									std::cout << "NACK ENQUEUE FAIL";
								} 
							} else if (!m_queue->Enqueue(newp, qCnt - 1)) {
								NS_LOG_INFO("NACK ENQUEUE FAIL");
								std::cout << "NACK ENQUEUE FAIL";
							}
							DequeueAndTransmit();
						}
						else if (x == 1 && !m_irn) //generate ACK
						{
							Ptr<Packet> newp = Create<Packet>(0);
							qbbHeader seqh;
							seqh.SetSeq(ReceiverNextExpectedSeq[key]);
							seqh.SetPG(GetPriority(p->Copy()));
							seqh.SetPort(udph.GetSourcePort());
							newp->AddHeader(seqh);
							Ipv4Header head;	// Prepare IPv4 header
							head.SetDestination(ipv4h.GetSource());
							Ipv4Address myAddr = m_node->GetObject<Ipv4>()->GetAddress(m_ifIndex, 0).GetLocal();
							head.SetSource(myAddr);
							head.SetProtocol(0xFC); //ack=0xFC
							head.SetTtl(64);
							head.SetPayloadSize(newp->GetSize());
							head.SetIdentification(m_uniform_random_var.GetInteger(0, 65535));
							newp->AddHeader(head);
							uint32_t protocolNumber = 2048;
							AddHeader(newp, protocolNumber);	// Attach PPP header
							if (m_qcnEnabled || m_irn || m_uselotofqueue) {
								if (!m_queue->Enqueue(newp, 0)) {
									NS_LOG_INFO("ACK ENQUEUE FAIL");
									std::cout << "ACK ENQUEUE FAIL";
								}
							} else {
								if (!m_queue->Enqueue(newp, 0)) {
									NS_LOG_INFO("ACK ENQUEUE FAIL");
									std::cout << "ACK ENQUEUE FAIL";
								}
							}
							DequeueAndTransmit();
						}
					}
				}
				PointToPointReceive(packet);
			}
			else // If this is a Pause, stop the corresponding queue
			{
				if (!m_qbbEnabled) return;
				//std::cout << "Received PFC" << std::endl;
				PauseHeader pauseh;
				p->RemoveHeader(pauseh);
				unsigned qIndex = pauseh.GetQIndex();
				m_paused[qIndex] = true;
				if(!m_pauseStart[qIndex].GetNanoSeconds()) {
					m_pauseStart[qIndex] = Simulator::Now();
				}
				if (pauseh.GetTime() > 0)
				{
					Simulator::Cancel(m_resumeEvt[qIndex]);
					m_resumeEvt[qIndex] = Simulator::Schedule(MicroSeconds(pauseh.GetTime()), &QbbNetDevice::PauseFinish, this, qIndex);
					if (m_node->GetNodeType() > 0) {
						NS_LOG_INFO("Node " << m_node->GetId() << " queue 0" << " paused at " << Simulator::Now().GetMicroSeconds());
					}
				}
				else
				{
					Simulator::Cancel(m_resumeEvt[qIndex]);
					PauseFinish(qIndex);
				}
			}
		}
		else if (ipv4h.GetProtocol() == 0xFF)
		{
			// QCN on NIC
			// This is a Congestion signal
			// Then, extract data from the congestion packet.
			// We assume, without verify, the packet is destinated to me
			CnHeader cnHead;
			p->RemoveHeader(cnHead);
			uint32_t qIndex = cnHead.GetQindex();
			if (qIndex == 1)		//DCTCP
			{
				std::cout << "TCP--ignore\n";
				return;
			}
			uint32_t udpport = cnHead.GetFlow();
			uint16_t ecnbits = cnHead.GetECNBits();
			uint16_t qfb = cnHead.GetQfb();
			uint16_t total = cnHead.GetTotal();

			uint32_t i;
			for (i = 1; i < m_queue->m_fcount; i++)
			{
				if (m_findex_udpport_map[i] == udpport && m_findex_qindex_map[i] == qIndex)
					break;
			}
			if (i == m_queue->m_fcount)
				std::cout << "ERROR: QCN NIC cannot find the flow\n";

			if (qfb == 0)
			{
				std::cout << "ERROR: Unuseful QCN\n";
				return;	// Unuseful CN
			}
			if (m_rate[i] == 0)			//lazy initialization	
			{
				m_rate[i] = m_bps;
				for (uint32_t j = 0; j < maxHop; j++)
				{
					m_rateAll[i][j] = m_bps;
					m_targetRate[i][j] = m_bps;	//targetrate remembers the last rate
				}
			}
			if (ecnbits == 0x03)
			{
				rpr_cnm_received(i, 0, qfb*1.0 / (total + 1));
			}
			m_rate[i] = m_bps;
			for (uint32_t j = 0; j < maxHop; j++)
				m_rate[i] = std::min(m_rate[i], m_rateAll[i][j]);
			PointToPointReceive(packet);
		}

		else if (ipv4h.GetProtocol() == 0xFD)//NACK on NIC
		{
			
			if (m_irn) {
				//uint32_t sack_target = 0;
				irnHeader qbbh;
				p->Copy()->RemoveHeader(qbbh);

				uint32_t qIndex = qbbh.GetPG();
				uint32_t seq = qbbh.GetSeq(); //cumulative ack
				uint32_t port = qbbh.GetPort();
				uint32_t seqNack = qbbh.GetSeqNACK(); //selective ack ooo packet's seq
				unsigned int i;
				for (i = 1; i < m_queue->m_fcount; i++)
				{
					if (m_findex_udpport_map[i] == port && m_findex_qindex_map[i] == qIndex)
					{
						break;
					}
				}
				if (i == m_queue->m_fcount)
				{
					std::cout << "ERROR: NACK NIC cannot find the flow\n";
					return;
				}

				if ((seq - 1) > m_irn_maxAck[i]) {
					m_irn_maxAck[i] = seq - 1;
				}
				//if previously dequeueandtransmit was stopped by bdp-fc, we should restart it here 
				if (GetPacketsInflight(i) <= m_irn_bdp) {
					DequeueAndTransmit();
				}
				if (m_sendingBuffer[i]->IsEmpty()) { //empty or asking to retransmit from lower than already sent
					PointToPointReceive(packet);
					return;
				}

				//uint32_t buffer_seq = GetSeq(m_sendingBuffer[i]->Peek()->Copy());
				//bool newAck = false;
				if (!m_backto0)
				{
					// update next sequence to send, if cumulative ack is higher.
					// check if any new packets are cumulatively acked and update last ack value and the bitmap.
					// In this implementation, I used queue instead of bitmap
					uint32_t buffer_seq_ifnoretrans = 0;
					if (m_queue->PeekQueue(i)) {
						buffer_seq_ifnoretrans = GetSeq(m_queue->PeekQueue(i)->Copy());
					}
					// originally, buffer_seq_ifnoretrans must hold "not transmitted packets" only, but the front one might be untransmitted retransmission Packet (If NACK comes too early)
					// so check here if buffer_seq_ifnotretrans is real "not transmitted packets", or "untransmitted retransmission"
					std::deque<Ptr<Packet>> tmpq = m_irn_retransmitted[i];
					bool doRetransmission = true; // whether to generate extra retransmission
					while (!tmpq.empty()) {
						if (GetSeq(tmpq.front()->Copy()) == buffer_seq_ifnoretrans) {
							// this is untransmitted retransmission
							//should we send multiple time?
							NS_LOG_WARN("There is pending retransmission, not generating extra retransmission");
							doRetransmission = false;
							break;
						}
						tmpq.pop_front();
					}

					//buffer_seq : the unacked sent packet's sequence number
					uint32_t buffer_seq = GetSeq(m_sendingBuffer[i]->Peek()->Copy());
					//if buffer_seq is smaller it must be an ack 
					if (buffer_seq < seq) {
						doRetransmission = false;
					}

					// update m_sendingBuffer : remove packets < accumulated ack
					while (seq > buffer_seq)
					{
						// newAck = true; // removed to remove newAck
						//std::cout << "[SEND] Confirmed the Reception of " << buffer_seq << std::endl;
						m_sendingBuffer[i]->Dequeue();
						if (!m_sendingBuffer[i]->GetNPackets()) {
							buffer_seq = 0;
							break;
						}
						buffer_seq = GetSeq(m_sendingBuffer[i]->Peek()->Copy());
					}
					
					// update m_irn_retransmitted[i] : remove packets < accumulated ack
					uint32_t buffer_retx_seq = 0;
					if (m_irn_retransmitted[i].size() > 0) {
						buffer_retx_seq = GetSeq(m_irn_retransmitted[i].front()->Copy());
						while (seq > buffer_retx_seq)
						{
							//std::cout << "[SEND] Confirmed the Reception of " << buffer_retx_seq << ", which was retransmitted" << std::endl;
							// m_irn_bdp += 1; // Actual packet is one less
							m_irn_retransmitted[i].pop_front();
							if (!m_irn_retransmitted[i].size())
								break;
							buffer_retx_seq = GetSeq(m_irn_retransmitted[i].front()->Copy());
						}
						if (!m_irn_retransmitted[i].size()) buffer_retx_seq = 0;
					}


					// update MaxSeq
					if (doRetransmission && buffer_seq_ifnoretrans > m_irn_maxSeq[i])
						m_irn_maxSeq[i] = buffer_seq_ifnoretrans;
					// update MaxAck
					if (seqNack > m_irn_maxAck[i])
						m_irn_maxAck[i] = seqNack;
					if (seq - 1 > m_irn_maxAck[i])
						m_irn_maxAck[i] = seq - 1;


					//if a nack is received,
					//doretransmission:: only false if currently it was in NACK retransmission.
					//we should make it false even if ack has not came for all
					//if (seqNack > seq && doRetransmission) // equivalent to ackSyndrome
					if (doRetransmission || buffer_seq_ifnoretrans == 0)
					{
						/*
						If not retransmitted, in which it should have been,
						packet tobe retransmitted exists in m_queue do retransmission is false
						buffer_seq_ifnoretrans: not sent packets sequecne
						Sendingbuffer
						1.  [seq--------buffer_seq_ifnoretrans---] Sending packets, lost happened in middle
						- seqnack can be 0 or not
						- buffer_seq_ifnoretrans must be bigger than seq, seqnack
						2.  [seq-----]  Sent all and only lost packets exists
						-seqnack should be 0
						-buffer_seq_ifnoretrans should be 0

						2-1. [seq----] BDP-FC has made m_queue empty, need to send more sending buffer
						3. seq---[sending buffer] eliminated by buffer_seq == seq
						*/
						// update bitmap to set selective ack.
						std::queue<Ptr<Packet>> tmp;
						while (!m_sendingBuffer[i]->IsEmpty())
							tmp.push(m_sendingBuffer[i]->Dequeue());

						//uint32_t tmp_sack_target = (GetSeq(tmp.front()->Copy()));
						//std::cout << "[SEND] Pending to send " << tmp_sack_target << std::endl;
						
						Ptr<DropTailQueue<Packet>> transmit_q = CreateObject<DropTailQueue<Packet>>();
						while (!tmp.empty()) {
							uint32_t curseq = GetSeq(tmp.front()->Copy());
							if (curseq == seq) {
								m_sendingBuffer[i]->Enqueue(tmp.front());
								bool alreadySent = false;
								for (std::deque<Ptr<Packet>>::iterator it = m_irn_retransmitted[i].begin(); it != m_irn_retransmitted[i].end(); ++it) {
									if (GetSeq((*it)->Copy()) == seq) {
										//std::cout << "[SEND] We already sent Seq #" << curseq << std::endl;
										alreadySent = true;
										break;
									}
								}
								//Only do one retransmission on multiple NACK
								if (!alreadySent) {
									//NS_LOG_INFO("[SEND] Retransmit Seq #" << curseq);
									m_irn_retransmitted[i].push_back(tmp.front()->Copy());
									transmit_q->Enqueue(tmp.front()->Copy());
									//m_irn_bdp -= 1;
									//Simulator::Cancel(m_retransmit[i]);
									//NS_LOG_INFO("[SEND] Retransmit Count = " << (++m_irn_retransmission_cnt));
									Simulator::Schedule(MicroSeconds(m_irn_rtoLow), &QbbNetDevice::RetransmitIRN, this, curseq, i);
								}
							} else if (curseq == seqNack) {
								// nacked packet : no need to retransmit, or put at sendingbuffer
								//std::cout << "[SEND] Seq #" << curseq << " has been NACKed." << std::endl;
							}
							else if (curseq < buffer_seq_ifnoretrans && buffer_seq_ifnoretrans != 0) { // || (buffer_seq_ifnoretrans==0 && seqNack!=0  )) {
								// sent but not nack/acked packet
								// put sendingbuffer, but not at transmit_q
								m_sendingBuffer[i]->Enqueue(tmp.front());
							}
							else if (curseq >= buffer_seq_ifnoretrans && buffer_seq_ifnoretrans != 0) {
								NS_ASSERT_MSG(curseq >= seq, "All packets smaller than acc. ack should have been removed");
								// packet that has not been sent
								// put both to sending buffer, and transmit_q
								m_sendingBuffer[i]->Enqueue(tmp.front());
								transmit_q->Enqueue(tmp.front()->Copy());
							} else {
								//buffer_seq_ifnoretrans ==0
								m_sendingBuffer[i]->Enqueue(tmp.front());
							}
							tmp.pop();
						}

						// until here, transmit_q will have packet that has not been sent + packet on seq
						// fill m_queue
						m_queue->RecoverQueue(transmit_q, i);

						DequeueAndTransmit();
					}
				}
				else
				{
					NS_FATAL_ERROR("NOT IMPLEMENTED IF GOBACK=1");
					uint32_t goback_seq = seq / m_chunk*m_chunk;
					//buffer_seq : the unacked sent packet's sequence number
					uint32_t buffer_seq = GetSeq(m_sendingBuffer[i]->Peek()->Copy());
					if (buffer_seq > goback_seq)
					{
						std::cout << "ERROR: Sendingbuffer miss!\n";
					}
					while (goback_seq > buffer_seq)
					{
						m_sendingBuffer[i]->Dequeue();
						buffer_seq = GetSeq(m_sendingBuffer[i]->Peek()->Copy());
					}

					m_queue->RecoverQueue(m_sendingBuffer[i], i);
				}

				PointToPointReceive(packet);
			}
			else {// not m_irn
				qbbHeader qbbh;
				p->Copy()->RemoveHeader(qbbh);

				uint32_t qIndex = qbbh.GetPG();
				uint32_t seq = qbbh.GetSeq();
				uint32_t port = qbbh.GetPort();
				uint32_t i;
				for (i = 1; i < m_queue->m_fcount; i++)
				{
					if (m_findex_udpport_map[i] == port && m_findex_qindex_map[i] == qIndex)
					{
						break;
					}
				}
				if (i == m_queue->m_fcount)
				{
					std::cout << "ERROR: NACK NIC cannot find the flow\n";
					return;
				}
				//youngmok
				if (seq < m_milestone_tx[i]) {
					//if already received before shouldn't get the packet
					return;
				}
				// get the front packet and check seq, checking queue contains packets which need to be retransmitted
				uint32_t buffer_seq = GetSeq(m_sendingBuffer[i]->Peek()->Copy());
				if (!m_backto0)
				{
					if (buffer_seq > seq)
					{
						std::cout << "ERROR: Sendingbuffer miss!\n";
						NS_LOG_INFO("ERROR: SENDINGBUFFER MISS");
					}
					while (seq > buffer_seq)
					{
						m_sendingBuffer[i]->Dequeue();

						if (m_sendingBuffer[i]->IsEmpty()) {
							break;
						}
						buffer_seq = GetSeq(m_sendingBuffer[i]->Peek()->Copy());
					}
				}
				else
				{
					uint32_t goback_seq = seq / m_chunk*m_chunk;
					if (buffer_seq > goback_seq)
					{
						std::cout << "ERROR: Sendingbuffer miss!\n";
					}
					while (goback_seq > buffer_seq)
					{
						m_sendingBuffer[i]->Dequeue();
						buffer_seq = GetSeq(m_sendingBuffer[i]->Peek()->Copy());
					}

				}

				//recover from NACKED
				if (m_qcnEnabled || m_uselotofqueue) {
					m_queue->RecoverQueue(m_sendingBuffer[i], i);
				} else {
					Ptr<DropTailQueue<Packet>> tmp = CreateObject<DropTailQueue<Packet>>();
					Ptr<Packet> packet;
					//recover queue and preserve buffer
					while (!m_sendingBuffer[i]->IsEmpty()) {
						packet = m_sendingBuffer[i]->Dequeue();
						tmp->Enqueue(packet->Copy());
						m_queue->Enqueue(packet, qIndex);
					}
					//restore buffer
					while (!tmp->IsEmpty()) {
						m_sendingBuffer[i]->Enqueue(tmp->Dequeue()->Copy());
					}
				}

				if (m_waitAck && m_waitingAck[i]) {
					m_milestone_tx[i] = std::max(seq, m_milestone_tx[i]);
					//youngmok
					//m_nextAvail[i] = Simulator::Now();

					Simulator::Cancel(m_retransmit[i]);
					//m_waitingAck[i] = false;
					m_retransmit[i] = Simulator::Schedule(MicroSeconds(m_waitAckTimer), &QbbNetDevice::Retransmit, this, i);
					m_waitingAck[i] = true;
					DequeueAndTransmit();
				} else {
					//youngmok
					//why should we check waitingAck is true or not should erase this code
					DequeueAndTransmit();
					//
				}

				PointToPointReceive(packet);
			}
		}
		else if (ipv4h.GetProtocol() == 0xFC)//ACK on NIC
		{
			if (m_irn) {
				NS_ASSERT("SHOULDN'T RECEIVE ACK WHEN IRN ");
				std::cout << "SHOULDN'T RECEIVE ACK WHEN IRN ";
			}
			qbbHeader qbbh;
			p->Copy()->RemoveHeader(qbbh);

			uint32_t qIndex = qbbh.GetPG();
			uint32_t seq = qbbh.GetSeq();
			uint32_t port = qbbh.GetPort();
			unsigned int i;
			for (i = 1; i < m_queue->m_fcount; i++)
			{
				if (m_findex_udpport_map[i] == port && m_findex_qindex_map[i] == qIndex)
				{
					break;
				}
			}
			if (i == m_queue->m_fcount)
			{
				std::cout << "ERROR: ACK NIC cannot find the flow\n";
				return;
			}

			uint32_t buffer_seq = GetSeq(m_sendingBuffer[i]->Peek()->Copy());

			if (m_ack_interval == 0)
			{
				std::cout << "ERROR: shouldn't receive ack\n";
			}
			else
			{
				if (!m_backto0)
				{
					while (seq > buffer_seq)
					{
						m_sendingBuffer[i]->Dequeue();
						if (m_sendingBuffer[i]->IsEmpty())
						{
							break;
						}
						buffer_seq = GetSeq(m_sendingBuffer[i]->Peek()->Copy());
					}
				}
				else
				{
					uint32_t goback_seq = seq / m_chunk*m_chunk;
					while (goback_seq > buffer_seq)
					{
						m_sendingBuffer[i]->Dequeue();
						if (m_sendingBuffer[i]->IsEmpty())
						{
							break;
						}
						buffer_seq = GetSeq(m_sendingBuffer[i]->Peek()->Copy());
					}
				}
			}

			//not send_in_chunks
			if (m_waitAck && seq > m_milestone_tx[i] && !m_sendingBuffer[i]->IsEmpty()) {
				//Got ACK, resume sending
				//m_nextAvail[i] = Simulator::Now();
				Simulator::Cancel(m_retransmit[i]);
				//m_waitingAck[i] = false;

				m_retransmit[i] = Simulator::Schedule(MicroSeconds(m_waitAckTimer), &QbbNetDevice::Retransmit, this, i);
				m_waitingAck[i] = true;
				m_milestone_tx[i] = seq;
				//why should we dequeueand transmit after getting ack
				//DequeueAndTransmit();

			} else if (m_sendingBuffer[i]->IsEmpty()) {
				Simulator::Cancel(m_retransmit[i]);
				m_waitingAck[i] = false;
				m_milestone_tx[i] = seq;
			}

			PointToPointReceive(packet);
		}
	}

	void
		QbbNetDevice::PointToPointReceive(Ptr<Packet> packet)
	{
		NS_LOG_FUNCTION(this << packet);
		//NS_LOG_WARN("Qbb (Node: " << m_node->GetNodeType() << ") Received This packet " << *packet);
		DoMpiReceive(packet);
		/*
		uint16_t protocol = 0;

		if (m_receiveErrorModel && m_receiveErrorModel->IsCorrupt(packet))
		{
			// 
			// If we have an error model and it indicates that it is time to lose a
			// corrupted packet, don't forward this packet up, let it go.
			//
			m_phyRxDropTrace(packet);
		}
		else
		{
			// 
			// Hit the trace hooks.  All of these hooks are in the same place in this 
			// device becuase it is so simple, but this is not usually the case in 
			// more complicated devices.
			//
			m_snifferTrace(packet);
			m_promiscSnifferTrace(packet);
			m_phyRxEndTrace(packet);
			//
			// Strip off the point-to-point protocol header and forward this packet
			// up the protocol stack.  Since this is a simple point-to-point link,
			// there is no difference in what the promisc callback sees and what the
			// normal receive callback sees.
			//
			ProcessHeader(packet, protocol);

			if (!m_promiscCallback.IsNull())
			{
				m_macPromiscRxTrace(packet);
				m_promiscCallback(this, packet, protocol, GetRemote(), GetAddress(), NetDevice::PACKET_HOST);
			}

			m_macRxTrace(packet);
			m_rxCallback(this, packet, protocol, GetRemote());
		} */
	}

	uint32_t
		QbbNetDevice::GetPriority(Ptr<Packet> p) //Pay attention this function modifies the packet!!! Copy the packet before passing in.
	{
		UdpHeader udph;
		p->RemoveHeader(udph);
		SeqTsHeader seqh;
		p->RemoveHeader(seqh);
		return seqh.GetPG();
	}

	uint32_t
		QbbNetDevice::GetSeq(Ptr<Packet> p) //Pay attention this function modifies the packet!!! Copy the packet before passing in.
	{
		uint16_t protocol;
		ProcessHeader(p, protocol);
		Ipv4Header ipv4h;
		p->RemoveHeader(ipv4h);
		UdpHeader udph;
		p->RemoveHeader(udph);
		SeqTsHeader seqh;
		p->RemoveHeader(seqh);
		return seqh.GetSeq();
	}

	bool
		QbbNetDevice::Send(Ptr<Packet> packet, const Address &dest, uint16_t protocolNumber)
	{
		NS_LOG_FUNCTION(this << packet << dest << protocolNumber);
		//NS_LOG_WARN("[" << Simulator::Now() << "] " << m_node << " Sending packet " << *packet);
		NS_LOG_LOGIC("UID is " << packet->GetUid());
		if (IsLinkUp() == false) {
			m_macTxDropTrace(packet);
			return false;
		}

		Ipv4Header h;
		packet->PeekHeader(h);
		unsigned qIndex;
		if (h.GetProtocol() == 0xFF || h.GetProtocol() == 0xFE || h.GetProtocol() == 0xFD || h.GetProtocol() == 0xFC)  //QCN or PFC or NACK, go highest priority
		{
			qIndex = qCnt - 1;
		}
		else
		{
			Ptr<Packet> p = packet->Copy();
			p->RemoveHeader(h);

			if (h.GetProtocol() == 17)
				qIndex = GetPriority(p);
			else
				qIndex = 1; //dctcp
		}

		Ptr<Packet> p = packet->Copy();
		AddHeader(packet, protocolNumber);

		if (m_node->GetNodeType() == 0)
		{
			if ((m_qcnEnabled || m_uselotofqueue || m_irn) && qIndex == qCnt - 1) //if using lots of queue use 0 as highest priority like qcn
			{
				m_queue->Enqueue(packet, 0); //QCN uses 0 as the highest priority on NIC
			}
			else
			{
				if (h.GetProtocol() == 17) {
					// UDP - RDMA
					Ipv4Header ipv4h;
					p->RemoveHeader(ipv4h);
					UdpHeader udph;
					p->RemoveHeader(udph);
					uint32_t port = udph.GetSourcePort();
					uint32_t i;
					for (i = 1; i < fCnt; i++)
					{
						if (m_findex_udpport_map[i] == 0)
						{
							m_queue->m_fcount = i + 1;
							m_findex_udpport_map[i] = port;
							m_findex_qindex_map[i] = qIndex;
							m_sendingBuffer[i] = CreateObject<DropTailQueue<Packet>>();
							if (m_waitAck)
							{
								//send_in_chunks
								if (false) {
									m_milestone_tx[i] = m_chunk;
								} else {
									m_milestone_tx[i] = 0;
								}
							}
							break;
						}
						if (m_findex_udpport_map[i] == port && m_findex_qindex_map[i] == qIndex)
							break;
					}
					if (m_sendingBuffer[i]->GetNPackets() == 8000)
					{
						m_sendingBuffer[i]->Dequeue();
					}
					m_sendingBuffer[i]->Enqueue(packet->Copy());
					if (m_qcnEnabled || m_irn || m_uselotofqueue)
					{
						m_queue->Enqueue(packet, i);
					}
					else
					{
						m_queue->Enqueue(packet, qIndex);
					}
				} else {
					// dctcp, tcp, etc.
        			#if DEBUG_PRINT
					TltTag tlt;
					int32_t socketId = tlt.debug_socketId;
					if (packet->PeekPacketTag(tlt)) {
						socketId = tlt.debug_socketId;
						if(socketId>=0) 
							std::cerr<< "Flow " << socketId << " : Ingress " << (int)(tlt.GetType()) << " to NIC" << std::endl; 
					}
					#endif
					m_queue->Enqueue(packet, qIndex);
				}
			}

			DequeueAndTransmit();
		}
		else //switch
		{
			NS_ASSERT(m_ifIndex < m_node->m_broadcom->pCnt);
			//AddHeader(packet, protocolNumber);
			if (qIndex != qCnt - 1)			//not pause frame
			{
				FlowIdTag t;
				packet->PeekPacketTag(t);
				uint32_t inDev = t.GetFlowId();
				TltTag tlt;
				#if DEBUG_PRINT
				int32_t socketId = tlt.debug_socketId;
				#endif		
				bool tlt_uip_flag = true;
				if(packet->PeekPacketTag(tlt)) {
					#if DEBUG_PRINT
					socketId = tlt.debug_socketId;
					#endif	
				}
				if (m_tltEnabled && packet->PeekPacketTag(tlt)) {
					if (tlt.GetType() == TltTag::PACKET_NOT_IMPORTANT) {
						if (m_node->m_broadcom->CheckEgressTLT(m_ifIndex, qIndex, packet->GetSize())) {
							//&& m_node->m_broadcom->CheckEgressTLT(m_ifIndex, qIndex, packet->GetSize())
							//m_node->m_broadcom->CheckIngressTLT(inDev, qIndex, packet->GetSize())
						} else {
							tlt_uip_flag = false;
							txTltDropBytes += packet->GetSize();
							unimportantDropCnt += 1;

							//std::cout << "Dropping Uimp Packet" << *packet << std::endl;
						}
					} else {
						//tlt.Print(std::cout);
						//std::cout << std::endl;
						
        				#if DEBUG_PRINT
						if(socketId>=0) 
							std::cerr<< "Flow " << socketId << " : Ingress Important" << (int)(tlt.GetType()) << std::endl; 
						#endif		
					}
				}
				uint32_t nodeid = m_node->GetId();
				if (m_uniform_random_var.GetValue(0, 1) < 0.01) {
					NS_LOG_INFO(Simulator::Now().GetMicroSeconds() << " Used Queue (Kb)" << m_node->m_broadcom->GetUsedBufferTotal() / 1000 << " Switch: " << nodeid);
				}
				if (tlt_uip_flag && m_node->m_broadcom->CheckIngressAdmission(inDev, qIndex, packet->GetSize()) && m_node->m_broadcom->CheckEgressAdmission(m_ifIndex, qIndex, packet->GetSize()) && m_queue->Enqueue(packet, qIndex))			// Admission control
				{
					m_node->m_broadcom->UpdateIngressAdmission(inDev, qIndex, packet->GetSize());
					m_node->m_broadcom->UpdateEgressAdmission(m_ifIndex, qIndex, packet->GetSize());
					//update tlt
					if (m_tltEnabled && packet->PeekPacketTag(tlt) && tlt.GetType() == TltTag::PACKET_NOT_IMPORTANT) {
						m_node->m_broadcom->UpdateIngressTLT(inDev, qIndex, packet->GetSize());
						m_node->m_broadcom->UpdateEgressTLT(m_ifIndex, qIndex, packet->GetSize());
						
				        #if DEBUG_PRINT
						if(socketId>=0) {
							std::cerr<< "Flow " << socketId << " : Ingress Unimportant : ";			
							packet->Print(std::cerr);
							std::cerr << std::endl; 
						}
						#endif
					} else {
				        #if DEBUG_PRINT
						if(socketId>=0) {
							std::cerr<< "Flow " << socketId << " : Ingress Important -> Successful (" << (int)(tlt.GetType()) << ") : ";			
							packet->Print(std::cerr);
							std::cerr << std::endl; 
						}
						#endif
					}

					if (m_tltEnabled && packet->PeekPacketTag(tlt)) {
						if (tlt.GetType() == TltTag::PACKET_IMPORTANT || tlt.GetType() == TltTag::PACKET_IMPORTANT_FORCE) {
							txImpBytes += packet->GetSize();
							txImpCount++;
						}
						else if (tlt.GetType() == TltTag::PACKET_IMPORTANT_ECHO || tlt.GetType() == TltTag::PACKET_IMPORTANT_ECHO_FORCE)
						{
							txImpEBytes += packet->GetSize();
							txImpECount++;
						}
						else
						{
							txUimpBytes += packet->GetSize();
							txUimpCount++;
						}
					} else {
						txUimpBytes += packet->GetSize();
					}
					m_macTxTrace(packet);
					//youngmok moved to if
					//m_queue->Enqueue(packet, qIndex); // go into MMU and queues
				} else if (tlt_uip_flag) {
					if(m_tltEnabled && tlt.GetType() != TltTag::PACKET_NOT_IMPORTANT) {
						importantDropBytes += packet->GetSize();
						importantDropCnt += 1;
					} else {
						unimportantDropCnt += 1;
					}
#if DEBUG_PRINT
						if(socketId>=0) 
							std::cerr<< "Flow " << socketId << " : Ingress Admission Fail" << (int)(tlt.GetType()) << std::endl; 
					#endif
				}
				if (tlt_uip_flag) {
        			#if DEBUG_PRINT
					if(socketId>=0) 
						std::cerr<< "Flow " << socketId << " : Deque/Transmit "  << (int)(tlt.GetType())  << std::endl; 
					#endif
					DequeueAndTransmit();
				}
				if (m_node->GetNodeType() == 1)
				{
					CheckQueueFull(inDev, qIndex); //check queue full
				}
			}
			else			//pause or cnp, doesn't need admission control, just go
			{
				if (!m_queue->Enqueue(packet, qIndex)) {
					NS_LOG_INFO("NACK CNP or ACK DROPPED");
				}
				DequeueAndTransmit();
			}

		}
		return true;
	}

	void
		QbbNetDevice::CheckQueueFull(uint32_t inDev, uint32_t qIndex)
	{
		if (m_PFCenabled) {

		} else {
			return;
		}
		NS_LOG_FUNCTION(this);
		//uint32_t nodeid = m_node->GetId();
		Ptr<Ipv4> m_ipv4 = m_node->GetObject<Ipv4>();
		bool pClasses[qCnt] = { 0 };
		m_node->m_broadcom->GetPauseClasses(inDev, qIndex, pClasses);
		Ptr<NetDevice> device = m_ipv4->GetNetDevice(inDev);
		for (uint32_t j = 0; j < qCnt; j++)
		{
			if (pClasses[j])			// Create the PAUSE packet
			{
				Ptr<Packet> p = Create<Packet>(0);
				PauseHeader pauseh(m_pausetime, m_queue->GetNBytes(j), j);
				p->AddHeader(pauseh);
				Ipv4Header ipv4h;  // Prepare IPv4 header
				ipv4h.SetProtocol(0xFE);
				ipv4h.SetSource(m_node->GetObject<Ipv4>()->GetAddress(m_ifIndex, 0).GetLocal());
				ipv4h.SetDestination(Ipv4Address("255.255.255.255"));
				ipv4h.SetPayloadSize(p->GetSize());
				ipv4h.SetTtl(1);
				ipv4h.SetIdentification(m_uniform_random_var.GetInteger(0, 65535));
				p->AddHeader(ipv4h);
				device->Send(p, Mac48Address("ff:ff:ff:ff:ff:ff"), 0x0800);
				//std::cout << "Sent PAUSE packet" << std::endl;
  				//printf("%.8lf\tPAUSE\t%p\t%u\n", Simulator::Now().GetSeconds(), this, ++pause_cnt);
				  ++pause_cnt;
				m_node->m_broadcom->m_pause_remote[inDev][qIndex] = true;
				Simulator::Cancel(m_recheckEvt[inDev][qIndex]);
				m_recheckEvt[inDev][qIndex] = Simulator::Schedule(MicroSeconds(m_pausetime / 2), &QbbNetDevice::CheckQueueFull, this, inDev, qIndex);
			}
		}

		//ON-OFF
		for (uint32_t j = 0; j < qCnt; j++)
		{
			if (!m_node->m_broadcom->m_pause_remote[inDev][qIndex])
				continue;
			if (m_node->m_broadcom->GetResumeClasses(inDev, qIndex))  // Create the PAUSE packet
			{
				Ptr<Packet> p = Create<Packet>(0);
				PauseHeader pauseh(0, m_queue->GetNBytes(j), j); //resume
				p->AddHeader(pauseh);
				Ipv4Header ipv4h;  // Prepare IPv4 header
				ipv4h.SetProtocol(0xFE);
				ipv4h.SetSource(m_node->GetObject<Ipv4>()->GetAddress(m_ifIndex, 0).GetLocal());
				ipv4h.SetDestination(Ipv4Address("255.255.255.255"));
				ipv4h.SetPayloadSize(p->GetSize());
				ipv4h.SetTtl(1);
				ipv4h.SetIdentification(m_uniform_random_var.GetInteger(0, 65535));
				p->AddHeader(ipv4h);
				device->Send(p, Mac48Address("ff:ff:ff:ff:ff:ff"), 0x0800);
				m_node->m_broadcom->m_pause_remote[inDev][qIndex] = false;
				Simulator::Cancel(m_recheckEvt[inDev][qIndex]);
			}
		}
	}

	bool
		QbbNetDevice::IsLocal(const Ipv4Address& addr) const
	{
		Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4>();
		for (unsigned i = 0; i < ipv4->GetNInterfaces(); i++) {
			for (unsigned j = 0; j < ipv4->GetNAddresses(i); j++) {
				if (ipv4->GetAddress(i, j).GetLocal() == addr) {
					return true;
				};
			};
		};
		return false;
	}

	void
		QbbNetDevice::ConnectWithoutContext(const CallbackBase& callback)
	{
		NS_LOG_FUNCTION(this);
		m_sendCb.ConnectWithoutContext(callback);
	}

	void
		QbbNetDevice::DisconnectWithoutContext(const CallbackBase& callback)
	{
		NS_LOG_FUNCTION(this);
		m_sendCb.DisconnectWithoutContext(callback);
	}

	int32_t
		QbbNetDevice::PrintStatus(std::ostream& os)
	{
		os << "Size:";
		uint32_t sum = 0;
		for (unsigned i = 0; i < qCnt; ++i) {
			os << " " << (m_paused[i] ? "q" : "Q") << "[" << i << "]=" << m_queue->GetNBytes(i);
			sum += m_queue->GetNBytes(i);
		};
		os << " sum=" << sum << std::endl;
		return sum;
	};

	bool
		QbbNetDevice::Attach(Ptr<QbbChannel> ch)
	{
		NS_LOG_FUNCTION(this << &ch);
		m_channel = ch;
		m_channel->Attach(this);
		NotifyLinkUp();
		return true;
	}

	bool
		QbbNetDevice::TransmitStart(Ptr<Packet> p)
	{
		NS_LOG_FUNCTION(this << p);
		NS_LOG_LOGIC("UID is " << p->GetUid() << ")");
		//
		// This function is called to start the process of transmitting a packet.
		// We need to tell the channel that we've started wiggling the wire and
		// schedule an event that will be executed when the transmission is complete.
		//
		NS_ASSERT_MSG(m_txMachineState == READY, "Must be READY to transmit");
		m_txMachineState = BUSY;
		m_currentPkt = p;
		m_phyTxBeginTrace(m_currentPkt);
		Time txTime = m_bps.CalculateBytesTxTime(p->GetSize());
		Time txCompleteTime = txTime + m_tInterframeGap;
		NS_LOG_LOGIC("Schedule TransmitCompleteEvent in " << txCompleteTime.GetSeconds() << "sec");
		Simulator::Schedule(txCompleteTime, &QbbNetDevice::TransmitComplete, this);
		
        #if DEBUG_PRINT
		TltTag tlt;
		int32_t socketId = tlt.debug_socketId;
		if (m_tltEnabled && p->PeekPacketTag(tlt)) {
			socketId = tlt.debug_socketId;
			if(socketId>=0) {
				std::cerr<< "Flow " << socketId << " : Transmit from Device Important=" << (int)tlt.debug_socketId << " : ";			
				p->Print(std::cerr);
				std::cerr << std::endl;  
			}
		}
		#endif
		bool result = m_channel->TransmitStart(p, this, txTime);
		if (result == false)
		{
			m_phyTxDropTrace(p);
		}
		return result;
	}

	Address
		QbbNetDevice::GetRemote(void) const
	{
		NS_ASSERT(m_channel->GetNDevices() == 2);
		for (uint32_t i = 0; i < m_channel->GetNDevices(); ++i)
		{
			Ptr<NetDevice> tmp = m_channel->GetDevice(i);
			if (tmp != this)
			{
				return tmp->GetAddress();
			}
		}
		NS_ASSERT(false);
		return Address();  // quiet compiler.
	}

	bool
		QbbNetDevice::ShouldSendCN(uint32_t indev, uint32_t ifindex, uint32_t qIndex)
	{
		return m_node->m_broadcom->ShouldSendCN(indev, ifindex, qIndex);
	}

	void
		QbbNetDevice::CheckandSendQCN(Ipv4Address source, uint32_t qIndex, uint32_t port) //port is udp port
	{
		if (m_node->GetNodeType() > 0)
			return;
		if (!m_qcnEnabled)
			return;
		for (uint32_t i = 0; i < m_ecn_source->size(); ++i)
		{
			ECNAccount info = (*m_ecn_source)[i];
			if (info.source == source && info.qIndex == qIndex && info.port == port)
			{
				if (info.ecnbits == 0x03)
				{
					Ptr<Packet> p = Create<Packet>(0);
					CnHeader cn(port, qIndex, info.ecnbits, info.qfb, info.total);	// Prepare CN header
					p->AddHeader(cn);
					Ipv4Header head;	// Prepare IPv4 header
					head.SetDestination(source);
					Ipv4Address myAddr = m_node->GetObject<Ipv4>()->GetAddress(m_ifIndex, 0).GetLocal();
					head.SetSource(myAddr);
					head.SetProtocol(0xFF);
					head.SetTtl(64);
					head.SetPayloadSize(p->GetSize());
					head.SetIdentification(m_uniform_random_var.GetInteger(0, 65535));
					p->AddHeader(head);
					uint32_t protocolNumber = 2048;
					AddHeader(p, protocolNumber);	// Attach PPP header
					if (m_qcnEnabled)
						m_queue->Enqueue(p, 0);
					else
						m_queue->Enqueue(p, qCnt - 1);
					((*m_ecn_source)[i]).ecnbits = 0;
					((*m_ecn_source)[i]).qfb = 0;
					((*m_ecn_source)[i]).total = 0;
					DequeueAndTransmit();
					Simulator::Schedule(MicroSeconds(m_qcn_interval), &QbbNetDevice::CheckandSendQCN, this, source, qIndex, port);

				}
				else
				{
					((*m_ecn_source)[i]).ecnbits = 0;
					((*m_ecn_source)[i]).qfb = 0;
					((*m_ecn_source)[i]).total = 0;
					Simulator::Schedule(MicroSeconds(m_qcn_interval), &QbbNetDevice::CheckandSendQCN, this, source, qIndex, port);
				}
				break;
			}
			else
			{
				continue;
			}
		}
		return;
	}

	void
		QbbNetDevice::SetBroadcomParams(
			uint32_t pausetime,
			double qcn_interval,
			double qcn_resume_interval,
			double g,
			DataRate minRate,
			DataRate rai,
			uint32_t fastrecover_times
		)
	{
		m_pausetime = pausetime;
		m_qcn_interval = qcn_interval;
		m_rpgTimeReset = qcn_resume_interval;
		m_g = g;
		m_minRate = m_minRate;
		m_rai = rai;
		m_rpgThreshold = fastrecover_times;
	}

	Ptr<Channel>
		QbbNetDevice::GetChannel(void) const
	{
		return m_channel;
	}

	uint32_t
		QbbNetDevice::GetUsedBuffer(uint32_t port, uint32_t qIndex)
	{
		uint32_t i;
		if (m_qcnEnabled || m_uselotofqueue || m_irn)
		{
			for (i = 1; i < m_queue->m_fcount; i++)
			{
				if (m_findex_qindex_map[i] == qIndex && m_findex_udpport_map[i] == port)
					break;
			}
			return m_queue->GetNBytes(i);
		}
		else
		{
			return m_queue->GetNBytes(qIndex);
		}
	}


	void
		QbbNetDevice::SetQueue(Ptr<BEgressQueue> q)
	{
		NS_LOG_FUNCTION(this << q);
		NS_ASSERT(q);
		m_queue = q;
	}

	Ptr<BEgressQueue>
		QbbNetDevice::GetQueue()
	{
		return m_queue;
	}


	void
		QbbNetDevice::ResumeECNState(uint32_t inDev)
	{
		m_ECNState[inDev] = 0;
	}

	void
		QbbNetDevice::ResumeECNIngressState(uint32_t inDev)
	{
		m_ECNIngressCount[inDev] = 0;
	}


	void
		QbbNetDevice::ResumeECNEgressState(uint32_t inDev)
	{
		m_ECNEgressCount[inDev] = 0;
	}


	void
		QbbNetDevice::rpr_adjust_rates(uint32_t fIndex, uint32_t hop)
	{
		AdjustRates(fIndex, hop, DataRate("0bps"));
		rpr_fast_recovery(fIndex, hop);
		return;
	}

	void
		QbbNetDevice::rpr_fast_recovery(uint32_t fIndex, uint32_t hop)
	{
		m_rpStage[fIndex][hop] = 1;
		return;
	}


	void
		QbbNetDevice::rpr_active_increase(uint32_t fIndex, uint32_t hop)
	{
		AdjustRates(fIndex, hop, m_rai);
		m_rpStage[fIndex][hop] = 2;
	}


	void
		QbbNetDevice::rpr_active_byte(uint32_t fIndex, uint32_t hop)
	{
		m_rpByteStage[fIndex][hop]++;
		m_txBytes[fIndex][hop] = m_bc;
		rpr_active_increase(fIndex, hop);
	}

	void
		QbbNetDevice::rpr_active_time(uint32_t fIndex, uint32_t hop)
	{
		m_rpTimeStage[fIndex][hop]++;
		m_rpWhile[fIndex][hop] = m_rpgTimeReset;
		Simulator::Cancel(m_rptimer[fIndex][hop]);
		m_rptimer[fIndex][hop] = Simulator::Schedule(MicroSeconds(m_rpWhile[fIndex][hop]), &QbbNetDevice::rpr_timer_wrapper, this, fIndex, hop);
		rpr_active_select(fIndex, hop);
	}


	void
		QbbNetDevice::rpr_fast_byte(uint32_t fIndex, uint32_t hop)
	{
		m_rpByteStage[fIndex][hop]++;
		m_txBytes[fIndex][hop] = m_bc;
		if (m_rpByteStage[fIndex][hop] < m_rpgThreshold)
		{
			rpr_adjust_rates(fIndex, hop);
		}
		else
		{
			rpr_active_select(fIndex, hop);
		}
			
		return;
	}

	void
		QbbNetDevice::rpr_fast_time(uint32_t fIndex, uint32_t hop)
	{
		m_rpTimeStage[fIndex][hop]++;
		m_rpWhile[fIndex][hop] = m_rpgTimeReset;
		Simulator::Cancel(m_rptimer[fIndex][hop]);
		m_rptimer[fIndex][hop] = Simulator::Schedule(MicroSeconds(m_rpWhile[fIndex][hop]), &QbbNetDevice::rpr_timer_wrapper, this, fIndex, hop);
		if (m_rpTimeStage[fIndex][hop] < m_rpgThreshold)
			rpr_adjust_rates(fIndex, hop);
		else
			rpr_active_select(fIndex, hop);
		return;
	}


	void
		QbbNetDevice::rpr_hyper_byte(uint32_t fIndex, uint32_t hop)
	{
		m_rpByteStage[fIndex][hop]++;
		m_txBytes[fIndex][hop] = m_bc / 2;
		rpr_hyper_increase(fIndex, hop);
	}


	void
		QbbNetDevice::rpr_hyper_time(uint32_t fIndex, uint32_t hop)
	{
		m_rpTimeStage[fIndex][hop]++;
		m_rpWhile[fIndex][hop] = m_rpgTimeReset / 2;
		Simulator::Cancel(m_rptimer[fIndex][hop]);
		m_rptimer[fIndex][hop] = Simulator::Schedule(MicroSeconds(m_rpWhile[fIndex][hop]), &QbbNetDevice::rpr_timer_wrapper, this, fIndex, hop);
		rpr_hyper_increase(fIndex, hop);
	}


	void
		QbbNetDevice::rpr_active_select(uint32_t fIndex, uint32_t hop)
	{
		if (m_rpByteStage[fIndex][hop] < m_rpgThreshold || m_rpTimeStage[fIndex][hop] < m_rpgThreshold)
			rpr_active_increase(fIndex, hop);
		else
			rpr_hyper_increase(fIndex, hop);
		return;
	}


	void
		QbbNetDevice::rpr_hyper_increase(uint32_t fIndex, uint32_t hop)
	{
		AdjustRates(fIndex, hop, m_rhai.GetBitRate()*(std::min(m_rpByteStage[fIndex][hop], m_rpTimeStage[fIndex][hop]) - m_rpgThreshold + 1));
		m_rpStage[fIndex][hop] = 3;
		return;
	}

	void
		QbbNetDevice::AdjustRates(uint32_t fIndex, uint32_t hop, DataRate increase)
	{
		if (((m_rpByteStage[fIndex][hop] == 1) || (m_rpTimeStage[fIndex][hop] == 1)) && (m_targetRate[fIndex][hop] > 10 * m_rateAll[fIndex][hop]))
			m_targetRate[fIndex][hop] /= 8;
		else
			m_targetRate[fIndex][hop] += increase;

		m_rateAll[fIndex][hop] = (m_rateAll[fIndex][hop] / 2) + (m_targetRate[fIndex][hop] / 2);

		if (m_rateAll[fIndex][hop] > m_bps)
			m_rateAll[fIndex][hop] = m_bps;

		m_rate[fIndex] = m_bps;
		for (uint32_t j = 0; j < maxHop; j++)
			m_rate[fIndex] = std::min(m_rate[fIndex], m_rateAll[fIndex][j]);
		return;
	}

	void
		QbbNetDevice::rpr_cnm_received(uint32_t findex, uint32_t hop, double fraction)
	{
		if (!m_EcnClampTgtRateAfterTimeInc && !m_EcnClampTgtRate)
		{
			if (m_rpByteStage[findex][hop] != 0)
			{
				m_targetRate[findex][hop] = m_rateAll[findex][hop];
				m_txBytes[findex][hop] = m_bc;
			}
		}
		else if (m_EcnClampTgtRate)
		{
			m_targetRate[findex][hop] = m_rateAll[findex][hop];
			m_txBytes[findex][hop] = m_bc; //for fluid model, QCN standard doesn't have this.
		}
		else
		{
			if (m_rpByteStage[findex][hop] != 0 || m_rpTimeStage[findex][hop] != 0)
			{
				m_targetRate[findex][hop] = m_rateAll[findex][hop];
				m_txBytes[findex][hop] = m_bc;
			}
		}
		m_rpByteStage[findex][hop] = 0;
		m_rpTimeStage[findex][hop] = 0;
		//m_alpha[findex][hop] = (1-m_g)*m_alpha[findex][hop] + m_g*fraction;
		m_alpha[findex][hop] = (1 - m_g)*m_alpha[findex][hop] + m_g; 	//binary feedback
		m_rateAll[findex][hop] = std::max(m_minRate, m_rateAll[findex][hop] * (1 - m_alpha[findex][hop] / 2));
		Simulator::Cancel(m_resumeAlpha[findex][hop]);
		m_resumeAlpha[findex][hop] = Simulator::Schedule(MicroSeconds(m_alpha_resume_interval), &QbbNetDevice::ResumeAlpha, this, findex, hop);
		m_rpWhile[findex][hop] = m_rpgTimeReset;
		Simulator::Cancel(m_rptimer[findex][hop]);
		m_rptimer[findex][hop] = Simulator::Schedule(MicroSeconds(m_rpWhile[findex][hop]), &QbbNetDevice::rpr_timer_wrapper, this, findex, hop);
		rpr_fast_recovery(findex, hop);
	}

	void
		QbbNetDevice::rpr_timer_wrapper(uint32_t fIndex, uint32_t hop)
	{
		if (m_rpStage[fIndex][hop] == 1)
		{
			rpr_fast_time(fIndex, hop);
		}
		else if (m_rpStage[fIndex][hop] == 2)
		{
			rpr_active_time(fIndex, hop);
		}
		else if (m_rpStage[fIndex][hop] == 3)
		{
			rpr_hyper_time(fIndex, hop);
		}
		return;
	}

	void
		QbbNetDevice::ResumeAlpha(uint32_t fIndex, uint32_t hop)
	{
		m_alpha[fIndex][hop] = (1 - m_g)*m_alpha[fIndex][hop];
		Simulator::Cancel(m_resumeAlpha[fIndex][hop]);
		m_resumeAlpha[fIndex][hop] = Simulator::Schedule(MicroSeconds(m_alpha_resume_interval), &QbbNetDevice::ResumeAlpha, this, fIndex, hop);
	}


	int
		QbbNetDevice::ReceiverCheckSeq(uint32_t seq, uint32_t key) 
	{
		uint32_t expected = ReceiverNextExpectedSeq[key];
		uint32_t nodeid = m_node->GetId();
		if (nodeid == 1 && key == 40) {
			//std::cout << Simulator::Now() <<" expected "<< expected<<" seq came " << seq<<"\n";
		}
		if (seq == expected) {
			if (m_irn) {
				//m_irn_recv_sack[key] >>= 1;
				m_irn_recv_sack[key][0] = 1;
				size_t acc_ack = 0;
				for (acc_ack = 0; acc_ack < m_irn_recv_sack[key].size(); acc_ack++) {
					if (!m_irn_recv_sack[key][acc_ack])
						break;
				}
				ReceiverNextExpectedSeq[key] += acc_ack;
				m_irn_recv_sack[key] >>= acc_ack;
				if (acc_ack > 0) {
					return 6; // Generate NACK, GoBack to 0
				}

			}

			ReceiverNextExpectedSeq[key] = ReceiverNextExpectedSeq[key] + 1;


			if (ReceiverNextExpectedSeq[key] > m_milestone_rx[key] && !m_irn) {
				m_milestone_rx[key] += m_ack_interval;
				return 1; //Generate ACK
			} else {
				return 5;
			}

		} else if (seq > expected) {
			if (m_irn) {
				// Generate NACK
				if (seq - expected < m_irn_recv_sack[key].size()) {
					m_irn_recv_sack[key][seq - expected] = 1;
					size_t acc_ack = 0;
					for (acc_ack = 0; acc_ack < m_irn_recv_sack[key].size(); acc_ack++) {
						if (!m_irn_recv_sack[key][acc_ack])
							break;
					}
					ReceiverNextExpectedSeq[key] += acc_ack;
					m_irn_recv_sack[key] >>= acc_ack;
					if (ReceiverNextExpectedSeq[key] == seq) {
						if (acc_ack > 0) {
							return 6; // Generate ACK
						}
						return 5;
						//if (ReceiverNextExpectedSeq[key] > m_milestone_rx[key])
						//{
						//	m_milestone_rx[key] += m_ack_interval;
						//	return 1; //Generate ACK
						//}
						//else if (ReceiverNextExpectedSeq[key] % m_chunk == 0)
						//{
						//	return 1;
						//}
						//else
						//{
						//	return 5;
						//}
					}
					//m_nackTimer[key] = Simulator::Now() + MicroSeconds(m_nack_interval);
					//m_lastNACK[key] = expected;
					/*if (m_backto0 && !m_testRead)
					{
					ReceiverNextExpectedSeq[key] = ReceiverNextExpectedSeq[key] / m_chunk*m_chunk;
					}*/
					return 2;
				} else {
					//m_nackTimer[key] = Simulator::Now() + MicroSeconds(m_nack_interval);
					//m_lastNACK[key] = expected;
					/*if (m_backto0 && !m_testRead)
					{
					ReceiverNextExpectedSeq[key] = ReceiverNextExpectedSeq[key] / m_chunk*m_chunk;
					}*/
					return 2;
				}
			} else {
				// Generate NACK
				if (Simulator::Now() > m_nackTimer[key] || m_lastNACK[key] != expected) {
					m_nackTimer[key] = Simulator::Now() + MicroSeconds(m_nack_interval);
					m_lastNACK[key] = expected;
					if (m_backto0 && !m_testRead) {
						ReceiverNextExpectedSeq[key] = ReceiverNextExpectedSeq[key] / m_chunk*m_chunk;
					}
					return 2;
				} else
					return 4;
			}

		} else {
			// Duplicate. 
			return 3;
		}
	}


	void
	QbbNetDevice::DoInitialize (void)
	{
		if (m_queueInterface)
		{
			NS_ASSERT_MSG (m_queue != 0, "A Queue object has not been attached to the device");

			// connect the traced callbacks of m_queue to the static methods provided by
			// the NetDeviceQueue class to support flow control and dynamic queue limits.
			// This could not be done in NotifyNewAggregate because at that time we are
			// not guaranteed that a queue has been attached to the netdevice
			m_queueInterface->ConnectQueueTraces<Packet> (m_queue, 0);
		}

		NetDevice::DoInitialize ();
	}

} // namespace ns3

