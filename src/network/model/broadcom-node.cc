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
#include <fstream>
#include "broadcom-node.h"
#include "node.h"
#include "node-list.h"
#include "net-device.h"
#include "application.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/object-vector.h"
#include "ns3/uinteger.h"
#include "ns3/log.h"
#include "ns3/assert.h"
#include "ns3/global-value.h"
#include "ns3/boolean.h"
#include "ns3/simulator.h"
#include "ns3/random-variable-stream.h"
#include "ns3/double.h"

NS_LOG_COMPONENT_DEFINE("BroadcomNode");
#define EGRESS_STAT 0 
#define MTU 1500


namespace ns3
{
#define SCHEDULE_QUEUELEN_STAT() \
	{                            \
		if (!stat_queuestat_scheduled) {Simulator::Schedule(MilliSeconds(1), &BroadcomNode::PeriodicPrintStat, this); stat_queuestat_scheduled = true;} \
	}

#define UPDATE_MAX_PARAM()                                                                                                                                                        \
	{                                                                                                                                                                             \
		stat_max_usedEgressPortBytes[port] = m_usedEgressPortBytes[port] > stat_max_usedEgressPortBytes[port] ? m_usedEgressPortBytes[port] : stat_max_usedEgressPortBytes[port]; \
		stat_max_bytesEgressTltUip[port] = m_bytesEgressTltUip[port] > stat_max_bytesEgressTltUip[port] ? m_bytesEgressTltUip[port] : stat_max_bytesEgressTltUip[port];           \
		stat_sum_usedEgressPortBytes[port] += m_usedEgressPortBytes[port];                                                                                                        \
		stat_sum_usedEgressTltUip[port] += m_bytesEgressTltUip[port];                                                                                                           \
		stat_sum_usedEgressPortAccCount[port] += 1;                                                                                                                               \
		stat_sum_queueDirty[port] = true; \
		SCHEDULE_QUEUELEN_STAT();  \
	} // namespace ns3

	NS_OBJECT_ENSURE_REGISTERED(BroadcomNode);
	static uint64_t bytesDropped = 0;
	static uint64_t packetsDropped = 0;
	static bool print_stat = true;

	TypeId
	BroadcomNode::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::BroadcomNode")
			.SetParent<Object>()
			.AddConstructor<BroadcomNode>()
			.AddAttribute("DctcpThresh", " ",
						  UintegerValue(40 * MTU),
						  MakeUintegerAccessor(&BroadcomNode::m_dctcp_threshold),
						  MakeUintegerChecker<uint32_t>())
			.AddAttribute("DctcpMaxThresh", "Threshold for fast retransmit",
						  UintegerValue(400 * MTU),
						  MakeUintegerAccessor(&BroadcomNode::m_dctcp_threshold_max),
						  MakeUintegerChecker<uint32_t>())
			.AddAttribute("PFC_ENABLED",
						  "Enable the generation of PAUSE packet.",
						  BooleanValue(true),
						  MakeBooleanAccessor(&BroadcomNode::m_PFCenabled),
						  MakeBooleanChecker())
			.AddAttribute("MaxBytesTltUip",
						  "The maximum number of bytes unimportant packets (TLT)",
						  DoubleValue(0),
						  MakeDoubleAccessor(&BroadcomNode::m_maxBytes_TltUip),
						  MakeDoubleChecker<double>())
			.AddAttribute("IngressAlpha",
						  "Broadcom Ingress alpha",
						  DoubleValue(1./128.),
						  MakeDoubleAccessor(&BroadcomNode::m_pg_shared_alpha_cell),
						  MakeDoubleChecker<double>())
			.AddAttribute("EgressAlpha",
						  "Broadcom Egress alpha",
						  DoubleValue(1./4.),
						  MakeDoubleAccessor(&BroadcomNode::m_pg_shared_alpha_cell_egress),
						  MakeDoubleChecker<double>())
			;
		return tid;
	}


	BroadcomNode::BroadcomNode()
	{
		m_maxBufferBytes = 4500 * 1000; //Originally: 9MB Current:4.5MB
		m_usedTotalBytes = 0;
		m_totaluipbytes = 0;

		for (uint32_t i = 0; i < pCnt; i++) // port 0 is not used
		{
			m_usedIngressPortBytes[i] = 0;
			m_usedEgressPortBytes[i] = 0;
			m_bytesIngressTltUip[i] = 0;
			m_bytesEgressTltUip[i] = 0;
			stat_max_bytesEgressTltUip[i] = 0;
			stat_max_usedEgressPortBytes[i] = 0;
			stat_sum_usedEgressPortBytes[i] = 0;
			stat_sum_usedEgressTltUip[i] = 0;
			stat_sum_usedEgressPortAccCount[i] = 0;
			stat_sum_queueDirty[i] = false;
			for (uint32_t j = 0; j < qCnt; j++)
			{
				m_usedIngressPGBytes[i][j] = 0;
				m_usedIngressPGHeadroomBytes[i][j] = 0;
				m_usedEgressQMinBytes[i][j] = 0;
				m_usedEgressQSharedBytes[i][j] = 0;
				m_pause_remote[i][j] = false;
			}
		}
		for (int i = 0; i < 4; i++)
		{
			m_usedIngressSPBytes[i] = 0;
			m_usedEgressSPBytes[i] = 0;
		}
		//ingress params
		m_buffer_cell_limit_sp = 4000 * MTU; //ingress sp buffer threshold
		//m_buffer_cell_limit_sp_shared=4000*MTU; //ingress sp buffer shared threshold, nonshare -> share
		m_pg_min_cell = MTU; //ingress pg guarantee
		m_port_min_cell = MTU; //ingress port guarantee
		m_pg_shared_limit_cell = 20 * MTU; //max buffer for an ingress pg
		m_port_max_shared_cell = 4800 * MTU; //max buffer for an ingress port
		m_pg_hdrm_limit = 103000; //2*10us*40Gbps+2*1.5kB //106 * MTU; //ingress pg headroom
		m_port_max_pkt_size = 100 * MTU; //ingress global headroom
		m_buffer_cell_limit_sp = m_maxBufferBytes - (pCnt - 1)*m_pg_hdrm_limit - (pCnt - 1)*std::max(qCnt*m_pg_min_cell, m_port_min_cell);//12000 * MTU; //ingress sp buffer threshold
		//still needs reset limits..
		m_port_min_cell_off = 4700 * MTU;
		m_pg_shared_limit_cell_off = m_pg_shared_limit_cell - 2 * MTU;
		//egress params
		m_op_buffer_shared_limit_cell = m_maxBufferBytes - (pCnt - 1)*std::max(qCnt*m_pg_min_cell, m_port_min_cell);//m_maxBufferBytes; //per egress sp limit
		m_op_uc_port_config_cell = m_maxBufferBytes; //per egress port limit
		m_q_min_cell = 1 + MTU;
		m_op_uc_port_config1_cell = m_maxBufferBytes;
		//qcn
		m_pg_qcn_threshold = 60 * MTU;
		m_pg_qcn_threshold_max = 60 * MTU;
		m_pg_qcn_maxp = 0.1;
		//dynamic threshold
		m_dynamicth = false;
		//pfc and dctcp
		m_enable_pfc_on_dctcp = 1;
		m_dctcp_threshold = 40 * MTU;
		m_dctcp_threshold_max = 400 * MTU;
		// these are set as attribute
		m_pg_shared_alpha_cell = 0;
		m_pg_shared_alpha_cell_egress = 0;
		m_port_shared_alpha_cell = 128;   //not used for now. not sure whether this is used on switches
		m_pg_shared_alpha_cell_off_diff = 16;
		m_port_shared_alpha_cell_off_diff = 16;
		m_log_start = 2.1;
		m_log_end = 2.2;
		m_log_step = 0.00001;

		m_uniform_random_var.SetStream(0);
		Simulator::Schedule(Seconds(998), &BroadcomNode::PrintStat, this);

		SCHEDULE_QUEUELEN_STAT();
	
	}

	BroadcomNode::~BroadcomNode()
	{}

	void
	BroadcomNode::PeriodicPrintStat() {
		stat_queuestat_scheduled = false;
		bool dirty = false;

		for (unsigned int i = 0; i < pCnt; i++) {
			if (stat_sum_queueDirty[i]) {
				stat_sum_queueDirty[i] = false;
				dirty = true;
			}
			fprintf(stdout, "%.8lf\tPERPORT_Q\t%d\t%u\t%u\t%u\n", Simulator::Now().GetSeconds(), switchId, i,
				m_usedEgressPortBytes[i],
				m_bytesEgressTltUip[i]);
			
		}

		if (dirty) {
			SCHEDULE_QUEUELEN_STAT();
		}
	}

	void
	BroadcomNode::PrintStat() {
		if(print_stat) {
			print_stat = false;
			fprintf(stdout, "%.8lf\tBCMNODE_DROP_BYTES\t%lu\n", Simulator::Now().GetSeconds(), bytesDropped);
			fprintf(stdout, "%.8lf\tBCMNODE_DROP_PKTS\t%lu\n", Simulator::Now().GetSeconds(), packetsDropped);
		}
		for (int port = 1; port < (int)pCnt; port++) {
			fprintf(stdout, "%.8lf\tMAX_PERPORT_QUEUE\t%d\t%u\t%u\t%u\n", Simulator::Now().GetSeconds(), switchId, port,
					stat_max_usedEgressPortBytes[port],
					stat_max_bytesEgressTltUip[port]);
		}
		for (int port = 1; port < (int)pCnt; port++) {
			fprintf(stdout, "%.8lf\tAVG_PERPORT_QUEUE\t%d\t%u\t%lu\t%lu\n", Simulator::Now().GetSeconds(), switchId, port,
					stat_sum_usedEgressPortAccCount[port] ? stat_sum_usedEgressPortBytes[port]/stat_sum_usedEgressPortAccCount[port] : 0,
					stat_sum_usedEgressPortAccCount[port] ? stat_sum_usedEgressTltUip[port]/stat_sum_usedEgressPortAccCount[port] : 0);
		}

		fflush(stdout);
	}

	bool
		BroadcomNode::CheckIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize)
	{
		
		NS_ASSERT(m_pg_shared_alpha_cell > 0);

		if (m_usedTotalBytes + psize > m_maxBufferBytes)  //buffer full, usually should not reach here.
		{
			std::cout << "WARNING: Drop because ingress buffer full\n";
			bytesDropped += psize;
			packetsDropped += 1;
			return false;
		}
		if (m_usedIngressPGBytes[port][qIndex] + psize > m_pg_min_cell && m_usedIngressPortBytes[port] + psize > m_port_min_cell) // exceed guaranteed, use share buffer
		{
			if (m_usedIngressSPBytes[GetIngressSP(port, qIndex)] > m_buffer_cell_limit_sp) // check if headroom is already being used
			{
				if (m_usedIngressPGHeadroomBytes[port][qIndex] + psize > m_pg_hdrm_limit) // exceed headroom space
				{
					// if(m_PFCenabled)
						std::cout << "WARNING: Drop because ingress headroom full:" << m_usedIngressPGHeadroomBytes[port][qIndex] << "\t" << m_pg_hdrm_limit << " tlt uip " << m_maxBytes_TltUip << "\n"; 
					
					bytesDropped += psize;
					packetsDropped += 1;
					return false;
				}
			}
		}
		return true;
	}




	bool
		BroadcomNode::CheckEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize)
	{
		NS_ASSERT(m_pg_shared_alpha_cell_egress > 0);

		//PFC OFF Nothing
		bool threshold = true;
		if (m_usedEgressSPBytes[GetEgressSP(port, qIndex)] + psize > m_op_buffer_shared_limit_cell)  //exceed the sp limit
		{
			std::cout << "WARNING: Drop because egress SP buffer full\n";
			bytesDropped += psize;
			packetsDropped += 1;
			return false;
		}
		if (m_usedEgressPortBytes[port] + psize > m_op_uc_port_config_cell)	//exceed the port limit
		{
			std::cout << "WARNING: Drop because egress Port buffer full\n";
			bytesDropped += psize;
			packetsDropped += 1;
			return false;
		}
		if (m_usedEgressQSharedBytes[port][qIndex] + psize > m_op_uc_port_config1_cell) //exceed the queue limit
		{
			std::cout << "WARNING: Drop because egress Q buffer full\n";
			bytesDropped += psize;
			packetsDropped += 1;
			return false;
		}
		//PFC ON return true here
		if (m_PFCenabled) {
			return true;
		}
		//std::cout << "Comparing " << ((double)m_usedEgressQSharedBytes[port][qIndex] + psize ) << " > " << ( m_pg_shared_alpha_cell_egress*((double)m_op_buffer_shared_limit_cell - m_usedEgressSPBytes[GetEgressSP(port, qIndex)])) << std::endl;
		if ((double)m_usedEgressQSharedBytes[port][qIndex] + psize  > m_pg_shared_alpha_cell_egress*((double)m_op_buffer_shared_limit_cell - m_usedEgressSPBytes[GetEgressSP(port, qIndex)])) {
			// std::cout << "WARNING: Drop because egress DT threshold exceed for port: " << port << " queue " << qIndex;
			threshold = false;
			bytesDropped += psize;
			packetsDropped += 1;
			//drop because it exceeds threshold
		}
		return threshold;
	}


	void
		BroadcomNode::UpdateIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize)
	{
		m_usedTotalBytes += psize; //count total buffer usage
		m_usedIngressSPBytes[GetIngressSP(port, qIndex)] += psize;
		m_usedIngressPortBytes[port] += psize;
		m_usedIngressPGBytes[port][qIndex] += psize;
		if (m_usedIngressSPBytes[GetIngressSP(port, qIndex)] > m_buffer_cell_limit_sp)	//begin to use headroom buffer
		{
			m_usedIngressPGHeadroomBytes[port][qIndex] += psize;
		}
		return;
	}

	bool
		BroadcomNode::CheckIngressTLT(uint32_t port, uint32_t qIndex, uint32_t psize) {
		return true;
	}

	bool
		BroadcomNode::CheckEgressTLT(uint32_t port, uint32_t qIndex, uint32_t psize) {
		// if m_maxBytes_TltUip is 0, regard TLT is disabled
		if (m_maxBytes_TltUip && m_bytesEgressTltUip[port] + psize >= m_maxBytes_TltUip) {
			return false;
		}
		return true;
	}

	void
		BroadcomNode::UpdateIngressTLT(uint32_t port, uint32_t qIndex, uint32_t psize) {

		m_bytesIngressTltUip[port] += psize;
		m_totaluipbytes += psize;
		return;
	}
	void
		BroadcomNode::UpdateEgressTLT(uint32_t port, uint32_t qIndex, uint32_t psize) {

		m_bytesEgressTltUip[port] += psize;
		UPDATE_MAX_PARAM();
		return;
	}
	void
		BroadcomNode::RemoveFromIngressTLT(uint32_t port, uint32_t qIndex, uint32_t psize) {
		m_bytesIngressTltUip[port] -= psize;
		m_totaluipbytes -= psize;
	}

	void
		BroadcomNode::RemoveFromEgressTLT(uint32_t port, uint32_t qIndex, uint32_t psize) {
		m_bytesEgressTltUip[port] -= psize;
		UPDATE_MAX_PARAM();

	}

	void
		BroadcomNode::UpdateEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize) {
		if (true) {


			if (m_usedEgressQMinBytes[port][qIndex] + psize < m_q_min_cell)    //guaranteed
			{
				m_usedEgressQMinBytes[port][qIndex] += psize;
				m_usedEgressPortBytes[port] = m_usedEgressPortBytes[port] + psize;
				UPDATE_MAX_PARAM();
#if EGRESS_STAT
				if(printQueueLen)
		  				fprintf(stderr, "%.8lf\tEGRESS_QUEUE\t%d\t%u\t%u\t%u\n", Simulator::Now().GetSeconds(), switchId, port, 
						  std::max(m_usedEgressQMinBytes[port][qIndex], m_usedEgressPortBytes[port]),
						m_bytesEgressTltUip[port]);
				#endif
				return;
			} else {
				/*
				2 case
				First, when there is left space in q_min_cell, and we should use remaining space in q_min_cell and add rest to the shared_pool
				Second, just adding to shared pool
				*/
				if (m_usedEgressQMinBytes[port][qIndex] != m_q_min_cell) {
					m_usedEgressQSharedBytes[port][qIndex] = m_usedEgressQSharedBytes[port][qIndex] + psize + m_usedEgressQMinBytes[port][qIndex] - m_q_min_cell;
					m_usedEgressPortBytes[port] = m_usedEgressPortBytes[port] + psize; //+ m_usedEgressQMinBytes[port][qIndex] - m_q_min_cell ;
					m_usedEgressSPBytes[GetEgressSP(port, qIndex)] = m_usedEgressSPBytes[GetEgressSP(port, qIndex)] + psize + m_usedEgressQMinBytes[port][qIndex] - m_q_min_cell;
					m_usedEgressQMinBytes[port][qIndex] = m_q_min_cell;

				} else {
					m_usedEgressQSharedBytes[port][qIndex] += psize;
					m_usedEgressPortBytes[port] += psize;
					m_usedEgressSPBytes[GetEgressSP(port, qIndex)] += psize;

				}
				UPDATE_MAX_PARAM();
#if EGRESS_STAT
				if(printQueueLen)
		  				fprintf(stderr, "%.8lf\tEGRESS_QUEUE\t%d\t%u\t%u\t%u\n", Simulator::Now().GetSeconds(), switchId, port, 
						  std::max(m_usedEgressQMinBytes[port][qIndex], m_usedEgressPortBytes[port]),
						  m_bytesEgressTltUip[port]);
				#endif

			}
			return;
		} else {
 
		}
	}

	void
		BroadcomNode::RemoveFromIngressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize)
	{
		if (m_usedTotalBytes < psize) {
			m_usedTotalBytes = psize;
			std::cout << "Warning : Illegal Remove" << std::endl;
		}
		if (m_usedIngressSPBytes[GetIngressSP(port, qIndex)] < psize) {
			m_usedIngressSPBytes[GetIngressSP(port, qIndex)] = psize;
			std::cout << "Warning : Illegal Remove" << std::endl;
		}
		if (m_usedIngressSPBytes[GetIngressSP(port, qIndex)] < psize) {
			m_usedIngressSPBytes[GetIngressSP(port, qIndex)] = psize;
			std::cout << "Warning : Illegal Remove" << std::endl;
		}
		if (m_usedIngressPortBytes[port] < psize) {
			m_usedIngressPortBytes[port] = psize;
			std::cout << "Warning : Illegal Remove" << std::endl;
		}
		if (m_usedIngressPGBytes[port][qIndex] < psize) {
			m_usedIngressPGBytes[port][qIndex] = psize;
			std::cout << "Warning : Illegal Remove" << std::endl;
		}
		m_usedTotalBytes -= psize;
		m_usedIngressSPBytes[GetIngressSP(port, qIndex)] -= psize;
		m_usedIngressPortBytes[port] -= psize;
		m_usedIngressPGBytes[port][qIndex] -= psize;
		if ((double)m_usedIngressPGHeadroomBytes[port][qIndex] - psize > 0)
			m_usedIngressPGHeadroomBytes[port][qIndex] -= psize;
		else
			m_usedIngressPGHeadroomBytes[port][qIndex] = 0;
			
		#if EGRESS_STAT
				if(printQueueLen)
		  				fprintf(stderr, "%.8lf\tEGRESS_QUEUE\t%d\t%u\t%u\t%u\n", Simulator::Now().GetSeconds(), switchId, port, 
						   std::max(m_usedEgressQMinBytes[port][qIndex], m_usedEgressPortBytes[port]),
						   m_bytesEgressTltUip[port]);
		#endif
		return;
	}

	void
		BroadcomNode::RemoveFromEgressAdmission(uint32_t port, uint32_t qIndex, uint32_t psize) {

		if (true) {

			if (m_usedEgressQMinBytes[port][qIndex] < m_q_min_cell)    //guaranteed
			{
				if (m_usedEgressQMinBytes[port][qIndex]<psize) {
					std::cout << "STOP overflow\n";
				}
				m_usedEgressQMinBytes[port][qIndex] -= psize;
				m_usedEgressPortBytes[port] -= psize;
				UPDATE_MAX_PARAM();
				return;
			} else {
				/*
				2 case
				First, when packet was using both qminbytes and qsharedbytes we should substract from each one
				Second, just subtracting shared pool
				*/

				//first case
				if (m_usedEgressQMinBytes[port][qIndex] == m_q_min_cell  &&m_usedEgressQSharedBytes[port][qIndex] < psize) {

					m_usedEgressQMinBytes[port][qIndex] = m_usedEgressQMinBytes[port][qIndex] + m_usedEgressQSharedBytes[port][qIndex] - psize;
					m_usedEgressSPBytes[GetEgressSP(port, qIndex)] = m_usedEgressSPBytes[GetEgressSP(port, qIndex)] - m_usedEgressQSharedBytes[port][qIndex];
					m_usedEgressQSharedBytes[port][qIndex] = 0;
					if (m_usedEgressPortBytes[port]<psize) {
						std::cout << "STOP overflow\n";
					}
					m_usedEgressPortBytes[port] -= psize;


				} else {
					if (m_usedEgressQSharedBytes[port][qIndex]<psize || m_usedEgressPortBytes[port]<psize || m_usedEgressSPBytes[GetEgressSP(port, qIndex)]<psize) {
						std::cout << "STOP overflow\n";
					}
					m_usedEgressQSharedBytes[port][qIndex] -= psize;
					m_usedEgressPortBytes[port] -= psize;
					m_usedEgressSPBytes[GetEgressSP(port, qIndex)] -= psize;

				}
				UPDATE_MAX_PARAM();
				return;
			}

		} else {

		}
	}

	void
		BroadcomNode::GetPauseClasses(uint32_t port, uint32_t qIndex, bool pClasses[])
	{
		if (port >= pCnt) {
			std::cout << "ERROR: port is " << port << std::endl;
		}
		if (m_dynamicth)
		{
			for (uint32_t i = 0; i < qCnt; i++)
			{
				pClasses[i] = false;
				if (m_usedIngressPGBytes[port][i] <= m_pg_min_cell + m_port_min_cell)
					continue;
				if (i == 1 && !m_enable_pfc_on_dctcp)			//dctcp
					continue;

				//std::cout << "BCM : Used=" << m_usedIngressPGBytes[port][i] << ", thresh=" << m_pg_shared_alpha_cell*((double)m_buffer_cell_limit_sp - m_usedIngressSPBytes[GetIngressSP(port, qIndex)]) + m_pg_min_cell+m_port_min_cell << std::endl;

				if ((double)m_usedIngressPGBytes[port][i] - m_pg_min_cell - m_port_min_cell > m_pg_shared_alpha_cell*((double)m_buffer_cell_limit_sp - m_usedIngressSPBytes[GetIngressSP(port, qIndex)]) || m_usedIngressPGHeadroomBytes[port][qIndex] != 0)
				{

					pClasses[i] = true;
				}
			}
		}
		else
		{
			if (m_usedIngressPortBytes[port] > m_port_max_shared_cell)					//pause the whole port
			{
				for (uint32_t i = 0; i < qCnt; i++)
				{
					if (i == 1 && !m_enable_pfc_on_dctcp)	//dctcp
						pClasses[i] = false;

					pClasses[i] = true;
				}
				return;
			}
			else
			{
				for (uint32_t i = 0; i < qCnt; i++)
				{
					pClasses[i] = false;
				}
			}
			if (m_usedIngressPGBytes[port][qIndex] > m_pg_shared_limit_cell)
			{
				if (qIndex == 1 && !m_enable_pfc_on_dctcp)
					return;

				pClasses[qIndex] = true;
			}
		}
		return;
	}


	bool
		BroadcomNode::GetResumeClasses(uint32_t port, uint32_t qIndex)
	{
		if (m_dynamicth)
		{
			if ((double)m_usedIngressPGBytes[port][qIndex] - m_pg_min_cell - m_port_min_cell < m_pg_shared_alpha_cell*((double)m_buffer_cell_limit_sp - m_usedIngressSPBytes[GetIngressSP(port, qIndex)] - m_pg_shared_alpha_cell_off_diff) && m_usedIngressPGHeadroomBytes[port][qIndex] == 0)
			{
				return true;
			}
		}
		else
		{
			if (m_usedIngressPGBytes[port][qIndex] < m_pg_shared_limit_cell_off
				&& m_usedIngressPortBytes[port] < m_port_min_cell_off)
			{
				return true;
			}
		}
		return false;
	}

	uint32_t
		BroadcomNode::GetIngressSP(uint32_t port, uint32_t pgIndex)
	{
		if (pgIndex == 1)
			return 1;
		else
			return 0;
	}

	uint32_t
		BroadcomNode::GetEgressSP(uint32_t port, uint32_t qIndex)
	{
		if (qIndex == 1)
			return 1;
		else
			return 0;
	}

	bool
		BroadcomNode::ShouldSendCN(uint32_t indev, uint32_t ifindex, uint32_t qIndex)
	{
		if (qIndex == qCnt - 1)
			return false;
		if (qIndex == 1)	//dctcp
		{
			if (m_usedEgressQSharedBytes[ifindex][qIndex] > m_dctcp_threshold_max)
			{
				//std::cerr << "Sending CN : " << m_usedEgressQSharedBytes[ifindex][qIndex] << " > " << m_dctcp_threshold_max << std::endl;
				return true;
			}
			else
			{
				if (m_usedEgressQSharedBytes[ifindex][qIndex] > m_dctcp_threshold && m_dctcp_threshold != m_dctcp_threshold_max)
				{
					double p = 1.0 * (m_usedEgressQSharedBytes[ifindex][qIndex] - m_dctcp_threshold) / (m_dctcp_threshold_max - m_dctcp_threshold);
					if (m_uniform_random_var.GetValue(0, 1) < p)
						return true;
				}
			}
			return false;
		}
		else
		{
			if (m_usedEgressQSharedBytes[ifindex][qIndex] > m_pg_qcn_threshold_max)
			{
				return true;
			}
			else if (m_usedEgressQSharedBytes[ifindex][qIndex] > m_pg_qcn_threshold && m_pg_qcn_threshold != m_pg_qcn_threshold_max)
			{
				double p = 1.0 * (m_usedEgressQSharedBytes[ifindex][qIndex] - m_pg_qcn_threshold) / (m_pg_qcn_threshold_max - m_pg_qcn_threshold) * m_pg_qcn_maxp;
				if (m_uniform_random_var.GetValue(0, 1) < p)
					return true;
			}
			return false;
		}
	}


	void
		BroadcomNode::SetBroadcomParams(
			uint32_t buffer_cell_limit_sp, //ingress sp buffer threshold p.120
			uint32_t buffer_cell_limit_sp_shared, //ingress sp buffer shared threshold p.120, nonshare -> share
			uint32_t pg_min_cell, //ingress pg guarantee p.121					---1
			uint32_t port_min_cell, //ingress port guarantee						---2
			uint32_t pg_shared_limit_cell, //max buffer for an ingress pg			---3	PAUSE
			uint32_t port_max_shared_cell, //max buffer for an ingress port		---4	PAUSE
			uint32_t pg_hdrm_limit, //ingress pg headroom
			uint32_t port_max_pkt_size, //ingress global headroom
			uint32_t q_min_cell,	//egress queue guaranteed buffer
			uint32_t op_uc_port_config1_cell, //egress queue threshold
			uint32_t op_uc_port_config_cell, //egress port threshold
			uint32_t op_buffer_shared_limit_cell, //egress sp threshold
			uint32_t q_shared_alpha_cell,
			uint32_t port_share_alpha_cell,
			uint32_t pg_qcn_threshold)
	{
		m_buffer_cell_limit_sp = buffer_cell_limit_sp;
		m_buffer_cell_limit_sp_shared = buffer_cell_limit_sp_shared;
		m_pg_min_cell = pg_min_cell;
		m_port_min_cell = port_min_cell;
		m_pg_shared_limit_cell = pg_shared_limit_cell;
		m_port_max_shared_cell = port_max_shared_cell;
		m_pg_hdrm_limit = pg_hdrm_limit;
		m_port_max_pkt_size = port_max_pkt_size;
		m_q_min_cell = q_min_cell;
		m_op_uc_port_config1_cell = op_uc_port_config1_cell;
		m_op_uc_port_config_cell = op_uc_port_config_cell;
		m_op_buffer_shared_limit_cell = op_buffer_shared_limit_cell;
		m_pg_shared_alpha_cell = q_shared_alpha_cell;
		m_port_shared_alpha_cell = port_share_alpha_cell;
		m_pg_qcn_threshold = pg_qcn_threshold;
	}

	uint32_t
		BroadcomNode::GetUsedBufferTotal()
	{
		return m_usedTotalBytes;
	}

	void
		BroadcomNode::SetDynamicThreshold()
	{
		m_dynamicth = true;
		m_pg_shared_limit_cell = m_maxBufferBytes;	//using dynamic threshold, we don't respect the static thresholds anymore
		m_port_max_shared_cell = m_maxBufferBytes;
		return;
	}

	void
		BroadcomNode::SetMarkingThreshold(uint32_t kmin, uint32_t kmax, double pmax)
	{
		m_pg_qcn_threshold = kmin * 1030;
		m_pg_qcn_threshold_max = kmax * 1030;
		m_pg_qcn_maxp = pmax;
	}

	void
		BroadcomNode::SetTCPMarkingThreshold(uint32_t kmin, uint32_t kmax)
	{
		m_dctcp_threshold = kmin * 1500;
		m_dctcp_threshold_max = kmax * 1500;
	}

} // namespace ns3
