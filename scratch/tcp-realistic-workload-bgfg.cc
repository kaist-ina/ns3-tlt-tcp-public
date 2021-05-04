/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
*/

#undef PGO_TRAINING
#define PATH_TO_PGO_CONFIG "path_to_pgo_config"
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define SIMULATION_TCP 1
#define SIMULATION_ROCE 2

#define SIMULATION_TYPE SIMULATION_TCP

#include <iostream>
#include <fstream>
#include <vector>
#include <time.h> 
#include "ns3/core-module.h"
#include "ns3/qbb-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/broadcom-node.h"
#include "ns3/packet.h"
#include "ns3/error-model.h"
#include "ns3/tcp-dctcp.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/netanim-module.h"
#include <unordered_map>
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("GENERIC_SIMULATION");

unsigned int FOREGROUND_INCAST_FLOW_PER_HOST = 4;
int use_static_rto = 0;

bool enable_qcn = true, use_dynamic_pfc_threshold = true, packet_level_ecmp = false, flow_level_ecmp = false, enable_dctcp = false, red_setbit = false, red_use_mark_p = false;
bool m_PFCenabled, enable_pcap_output = true, enable_ascii_output = true;
bool enable_tlt = false; bool enable_irn = false; bool usequeue_flownum = false;
double min_rto = 1, load=0.6;
uint32_t packet_payload_size = 1000, l2_chunk_size = 0, l2_ack_interval = 0; uint32_t packet_num_per_flow;
double pause_time = 5, simulator_stop_time = 3.01, app_start_time = 1.0, app_stop_time = 9.0, red_mark_p = 0.1;
double tlt_maxbytes_uip = 0.; double TLT_IMP_PKT_INTV; // , queue_maxBytes=0;//youngmok
std::string data_rate, link_delay, topology_file, flow_file, tcp_flow_file, trace_file, trace_output_file, EXPOUTPUT;
bool used_port[65536] = { 0 };
bool jumbo_packet = false;
int tcp_flow_per_node = 1, tcp_flow_total= 0;
double foreground_flow_ratio = 0.01; 
bool enable_tlp = false;
int tcp_initial_cwnd = 10;
int random_seed = 1;
double cnp_interval = 50, alpha_resume_interval = 55, rp_timer, dctcp_gain = 1 / 16, np_sampling_interval = 0, pmax = 1;
uint32_t byte_counter, fast_recovery_times = 5, kmax = 60, kmin = 60;
std::string rate_ai, rate_hai;
double bdp_val;
bool clamp_target_rate = false, clamp_target_rate_after_timer = false, send_in_chunks = true, l2_wait_for_ack = false, l2_back_to_zero = false, l2_test_read = false;
double error_rate_per_link = 0.0;
double broadcom_ingress_alpha = 1./128., broadcom_egress_alpha = 1./4.;
uint32_t optimization_bit = 0;
#if SIMULATION_TYPE == SIMULATION_TCP

int dctcp_sender_cnt = 48;
static std::vector<uint32_t> currentTxBytes;
// The number of bytes to send in this simulation.
static std::vector<uint32_t> targetTxBytes;
static uint32_t dctcp_tx_bytes = 64000; // not used here
// Perform series of 1040 byte writes (this is a multiple of 26 since
// we want to detect data splicing in the output stream)
static const uint32_t writeSize = 1040;
uint8_t data[writeSize];

uint32_t num_background_flow = 0;
// These are for starting the writing process, and handling the sending
// socket's notification upcalls (events).  These two together more or less
// implement a sending "Application", although not a proper ns3::Application
// subclass.

std::vector<std::pair<Ptr<Socket>, uint32_t>> sockets;
void StartFlow(Ptr<Node> clientNode, uint32_t flowId, std::pair<uint32_t, uint32_t> sendRecvHosts, Ipv4Address servAddress, uint16_t servPort, uint32_t targetLen);
void StartFlowForeground(Ptr<Node> clientNode, uint32_t flowId, std::pair<uint32_t, uint32_t> sendRecvHosts, Ipv4Address servAddress, uint16_t servPort, uint32_t targetLen);
void WriteUntilBufferFull(Ptr<Socket>, uint32_t);

/*static void
CwndTracer(uint32_t oldval, uint32_t newval) {
	NS_LOG_INFO("Moving cwnd from " << oldval << " to " << newval);
}*/

#endif

bool load_workload(const char *workload_file, uint32_t **workload_cdf);
void printStatistics();
void simpleTest() {
	// std::cout << "Simple Test !!" << std::endl;
}
namespace ns3 {
	extern char *argv1;
}
int main(int argc, char *argv[])
{
	//LogComponentEnable("GlobalRouteManagerImpl", LOG_LEVEL_LOGIC);
	// LogComponentEnable("SelectivePacketQueue", LOG_LEVEL_LOGIC);
	//LogComponentEnable("GlobalRouteManagerImpl", LOG_LEVEL_LOGIC);
	LogComponentEnable("QbbNetDevice", LOG_LEVEL_WARN);
	uint32_t *workload_cdf = nullptr;
	argv1 = (argv[1]);
	if (argc > 1)
	{
		//Read the configuration file
		std::ifstream conf;
		conf.open(argv[1]);
		if (!conf.is_open()) {
			std::cerr<< "Failed to open config file " << argv[1] << std::endl;
			return 1;
		}
		while (!conf.eof()) {
			std::string key;
			conf >> key;

			//std::cerr << conf.cur << "\n";

			if (key.compare("ENABLE_QCN") == 0) {
				uint32_t v;
				conf >> v;
				enable_qcn = v;
				if (enable_qcn)
					std::cerr << "ENABLE_QCN\t\t\t" << "Yes" << "\n";
				else
					std::cerr << "ENABLE_QCN\t\t\t" << "No" << "\n";
			} else if (key.compare("ENABLE_DCTCP") == 0) {
				uint32_t v;
				conf >> v;
				enable_dctcp = v;
				if (enable_dctcp)
					std::cerr << "ENABLE_DCTCP\t\t\t" << "Yes" << "\n";
				else
					std::cerr << "ENABLE_DCTCP\t\t\t" << "No" << "\n";
			} else if (key.compare("ENABLE_TLT") == 0) {
				uint32_t v;
				conf >> v;
				enable_tlt = v;
				if (enable_tlt)
					std::cerr << "ENABLE_TLT\t\t\t" << "Yes" << "\n";
				else
					std::cerr << "ENABLE_TLT\t\t\t" << "No" << "\n";
			} else if (key.compare("RED_SETBIT") == 0) {
				uint32_t v;
				conf >> v;
				red_setbit = v;
				if (red_setbit)
					std::cerr << "RED_SETBIT\t\t\t" << "Yes" << "\n";
				else
					std::cerr << "RED_SETBIT\t\t\t" << "No" << "\n";
			} else if (key.compare("RED_USE_MARK_P") == 0) {
				uint32_t v;
				conf >> v;
				red_use_mark_p = v;
				if (red_use_mark_p)
					std::cerr << "RED_USE_MARK_P\t\t\t" << "Yes" << "\n";
				else
					std::cerr << "RED_USE_MARK_P\t\t\t" << "No" << "\n";
			} else if (key.compare("RED_MARK_P") == 0) {
				double v;
				conf >> v;
				red_mark_p = v;
				std::cerr << "RED_MARK_P\t\t\t" << red_mark_p << "\n";
			} else if (key.compare("USE_DYNAMIC_PFC_THRESHOLD") == 0) {
				uint32_t v;
				conf >> v;
				use_dynamic_pfc_threshold = v;
				if (use_dynamic_pfc_threshold)
					std::cerr << "USE_DYNAMIC_PFC_THRESHOLD\t" << "Yes" << "\n";
				else
					std::cerr << "USE_DYNAMIC_PFC_THRESHOLD\t" << "No" << "\n";
			} else if (key.compare("CLAMP_TARGET_RATE") == 0) {
				uint32_t v;
				conf >> v;
				clamp_target_rate = v;
				if (clamp_target_rate)
					std::cerr << "CLAMP_TARGET_RATE\t\t" << "Yes" << "\n";
				else
					std::cerr << "CLAMP_TARGET_RATE\t\t" << "No" << "\n";
			} else if (key.compare("CLAMP_TARGET_RATE_AFTER_TIMER") == 0) {
				uint32_t v;
				conf >> v;
				clamp_target_rate_after_timer = v;
				if (clamp_target_rate_after_timer)
					std::cerr << "CLAMP_TARGET_RATE_AFTER_TIMER\t" << "Yes" << "\n";
				else
					std::cerr << "CLAMP_TARGET_RATE_AFTER_TIMER\t" << "No" << "\n";
			} else if (key.compare("PACKET_LEVEL_ECMP") == 0) {
				uint32_t v;
				conf >> v;
				packet_level_ecmp = v;
				if (packet_level_ecmp)
					std::cerr << "PACKET_LEVEL_ECMP\t\t" << "Yes" << "\n";
				else
					std::cerr << "PACKET_LEVEL_ECMP\t\t" << "No" << "\n";
			} else if (key.compare("FLOW_LEVEL_ECMP") == 0) {
				uint32_t v;
				conf >> v;
				flow_level_ecmp = v;
				if (flow_level_ecmp)
					std::cerr << "FLOW_LEVEL_ECMP\t\t\t" << "Yes" << "\n";
				else
					std::cerr << "FLOW_LEVEL_ECMP\t\t\t" << "No" << "\n";
			} else if (key.compare("PAUSE_TIME") == 0) {
				double v;
				conf >> v;
				pause_time = v;
				std::cerr << "PAUSE_TIME\t\t\t" << pause_time << "\n";
			} else if (key.compare("DATA_RATE") == 0) {
				std::string v;
				conf >> v;
				data_rate = v;
				std::cerr << "DATA_RATE\t\t\t" << data_rate << "\n";
			} else if (key.compare("LINK_DELAY") == 0) {
				std::string v;
				conf >> v;
				link_delay = v;
				std::cerr << "LINK_DELAY\t\t\t" << link_delay << "\n";
			} else if (key.compare("PACKET_PAYLOAD_SIZE") == 0) {
				uint32_t v;
				conf >> v;
				packet_payload_size = v;
				std::cerr << "PACKET_PAYLOAD_SIZE\t\t" << packet_payload_size << "\n";
			} else if (key.compare("L2_CHUNK_SIZE") == 0) {
				uint32_t v;
				conf >> v;
				l2_chunk_size = v;
				std::cerr << "L2_CHUNK_SIZE\t\t\t" << l2_chunk_size << "\n";
			} else if (key.compare("L2_ACK_INTERVAL") == 0) {
				uint32_t v;
				conf >> v;
				l2_ack_interval = v;
				std::cerr << "L2_ACK_INTERVAL\t\t\t" << l2_ack_interval << "\n";
			} else if (key.compare("L2_WAIT_FOR_ACK") == 0) {
				uint32_t v;
				conf >> v;
				l2_wait_for_ack = v;
				if (l2_wait_for_ack)
					std::cerr << "L2_WAIT_FOR_ACK\t\t\t" << "Yes" << "\n";
				else
					std::cerr << "L2_WAIT_FOR_ACK\t\t\t" << "No" << "\n";
			} else if (key.compare("L2_BACK_TO_ZERO") == 0) {
				uint32_t v;
				conf >> v;
				l2_back_to_zero = v;
				if (l2_back_to_zero)
					std::cerr << "L2_BACK_TO_ZERO\t\t\t" << "Yes" << "\n";
				else
					std::cerr << "L2_BACK_TO_ZERO\t\t\t" << "No" << "\n";
			} else if (key.compare("L2_TEST_READ") == 0) {
				uint32_t v;
				conf >> v;
				l2_test_read = v;
				if (l2_test_read)
					std::cerr << "L2_TEST_READ\t\t\t" << "Yes" << "\n";
				else
					std::cerr << "L2_TEST_READ\t\t\t" << "No" << "\n";
			} else if (key.compare("TOPOLOGY_FILE") == 0) {
				std::string v;
				conf >> v;
				topology_file = v;
				std::cerr << "TOPOLOGY_FILE\t\t\t" << topology_file << "\n";
			} else if (key.compare("FLOW_FILE") == 0) {
				std::string v;
				conf >> v;
				flow_file = v;
				std::cerr << "FLOW_FILE\t\t\t" << flow_file << "\n";
			} else if (key.compare("TCP_FLOW_FILE") == 0) {
				std::string v;
				conf >> v;
				tcp_flow_file = v;
				std::cerr << "TCP_FLOW_FILE\t\t\t" << tcp_flow_file << "\n";
			} else if (key.compare("TRACE_FILE") == 0) {
				std::string v;
				conf >> v;
				trace_file = v;
				std::cerr << "TRACE_FILE\t\t\t" << trace_file << "\n";
			} else if (key.compare("TRACE_OUTPUT_FILE") == 0) {
				std::string v;
				conf >> v;
				trace_output_file = v;
				if (argc > 2) {
					trace_output_file = trace_output_file + std::string(argv[2]);
				}
				std::cerr << "TRACE_OUTPUT_FILE\t\t" << trace_output_file << "\n";
			} else if (key.compare("APP_START_TIME") == 0) {
				double v;
				conf >> v;
				app_start_time = v;
				std::cerr << "SINK_START_TIME\t\t\t" << app_start_time << "\n";
			} else if (key.compare("APP_STOP_TIME") == 0) {
				double v;
				conf >> v;
				app_stop_time = v;
				std::cerr << "SINK_STOP_TIME\t\t\t" << app_stop_time << "\n";
			} else if (key.compare("SIMULATOR_STOP_TIME") == 0) {
				double v;
				conf >> v;
				simulator_stop_time = v;
				std::cerr << "SIMULATOR_STOP_TIME\t\t" << simulator_stop_time << "\n";
			} else if (key.compare("CNP_INTERVAL") == 0) {
				double v;
				conf >> v;
				cnp_interval = v;
				std::cerr << "CNP_INTERVAL\t\t\t" << cnp_interval << "\n";
			} else if (key.compare("ALPHA_RESUME_INTERVAL") == 0) {
				double v;
				conf >> v;
				alpha_resume_interval = v;
				std::cerr << "ALPHA_RESUME_INTERVAL\t\t" << alpha_resume_interval << "\n";
			} else if (key.compare("RP_TIMER") == 0) {
				double v;
				conf >> v;
				rp_timer = v;
				std::cerr << "RP_TIMER\t\t\t" << rp_timer << "\n";
			} else if (key.compare("BYTE_COUNTER") == 0) {
				uint32_t v;
				conf >> v;
				byte_counter = v;
				std::cerr << "BYTE_COUNTER\t\t\t" << byte_counter << "\n";
			} else if (key.compare("KMAX") == 0) {
				uint32_t v;
				conf >> v;
				kmax = v;
				std::cerr << "KMAX\t\t\t\t" << kmax << "\n";
			} else if (key.compare("KMIN") == 0) {
				uint32_t v;
				conf >> v;
				kmin = v;
				std::cerr << "KMIN\t\t\t\t" << kmin << "\n";
			} else if (key.compare("PMAX") == 0) {
				double v;
				conf >> v;
				pmax = v;
				std::cerr << "PMAX\t\t\t\t" << pmax << "\n";
			} else if (key.compare("DCTCP_GAIN") == 0) {
				double v;
				conf >> v;
				dctcp_gain = v;
				std::cerr << "DCTCP_GAIN\t\t\t" << dctcp_gain << "\n";
			} else if (key.compare("FAST_RECOVERY_TIMES") == 0) {
				uint32_t v;
				conf >> v;
				fast_recovery_times = v;
				std::cerr << "FAST_RECOVERY_TIMES\t\t" << fast_recovery_times << "\n";
			} else if (key.compare("RATE_AI") == 0) {
				std::string v;
				conf >> v;
				rate_ai = v;
				std::cerr << "RATE_AI\t\t\t\t" << rate_ai << "\n";
			} else if (key.compare("RATE_HAI") == 0) {
				std::string v;
				conf >> v;
				rate_hai = v;
				std::cerr << "RATE_HAI\t\t\t" << rate_hai << "\n";
			} else if (key.compare("NP_SAMPLING_INTERVAL") == 0) {
				double v;
				conf >> v;
				np_sampling_interval = v;
				std::cerr << "NP_SAMPLING_INTERVAL\t\t" << np_sampling_interval << "\n";
			} else if (key.compare("SEND_IN_CHUNKS") == 0) {
				uint32_t v;
				conf >> v;
				send_in_chunks = v;
				if (send_in_chunks) {
					std::cerr << "SEND_IN_CHUNKS\t\t\t" << "Yes" << "\n";
					std::cerr << "WARNING: deprecated and not tested. Please consider using L2_WAIT_FOR_ACK";
				} else
					std::cerr << "SEND_IN_CHUNKS\t\t\t" << "No" << "\n";
			} else if (key.compare("ERROR_RATE_PER_LINK") == 0) {
				double v;
				conf >> v;
				error_rate_per_link = v;
				std::cerr << "ERROR_RATE_PER_LINK\t\t" << error_rate_per_link << "\n";
			} else if (key.compare("TLT_MAXBYTES_UIP") == 0) {
				double v;
				conf >> v;
				tlt_maxbytes_uip = v;
				std::cerr << "TLT_MAXBYTES_UIP\t\t" << tlt_maxbytes_uip << "\n";
			} else if (key.compare("MIN_RTO") == 0) {
				double v;
				conf >> v;
				min_rto = v;
				std::cerr << "MIN_RTO\t\t\t\t" << min_rto << "ms\n";
			} else if (key.compare("ENABLE_IRN") == 0) {
				uint32_t v;
				conf >> v;
				enable_irn = v;
				if (enable_irn)
					std::cerr << "ENABLE_IRN\t\t\t" << "Yes" << "\n";
				else
					std::cerr << "ENABLE_IRN\t\t\t" << "No" << "\n";
			} else if (key.compare("DCTCP_SENDER_CNT") == 0) {
				double v;
				conf >> v;
				dctcp_sender_cnt = v;
				std::cerr << "DCTCP_SENDER_CNT\t\t" << dctcp_sender_cnt << "\n";
			} else if (key.compare("DCTCP_INCAST_SIZE") == 0) {
				double v;
				conf >> v;
				dctcp_tx_bytes = v;
				std::cerr << "DCTCP_INCAST_SIZE\t\t" << dctcp_tx_bytes << "\n";
			} else if (key.compare("ENABLE_PFC") == 0) {
				uint32_t v;
				conf >> v;
				m_PFCenabled = v;
				if (m_PFCenabled)
					std::cerr << "ENABLE_PFC\t\t\t" << "Yes" << "\n";
				else
					std::cerr << "ENABLE_PFC\t\t\t" << "No" << "\n";
			} else if (key.compare("ENABLE_USEQUEUE_FLOWNUM") == 0) {
				double v;
				conf >> v;
				usequeue_flownum = v;
				std::cerr << "ENABLE_USEQUEUE_FLOWNUM\t\t" << usequeue_flownum << "\n";
			} else if (key.compare("TLT_IMP_PKT_INTV") == 0) {
				uint32_t v;
				conf >> v;
				TLT_IMP_PKT_INTV = v;
				std::cerr << "TLT_IMP_PKT_INTV\t\t" << TLT_IMP_PKT_INTV << "\n";
			} else if (key.compare("EXPOUTPUT") == 0) {
				std::string v;
				conf >> v;
				EXPOUTPUT = v;
				std::cerr << "EXPOUTPUT\t\t" << EXPOUTPUT << "\n";
			} else if (key.compare("BDP_VALUE") == 0) {
				double v;
				conf >> v;
				bdp_val = v;
				std::cerr << "BDP_VALUE\t\t" << bdp_val << "\n";
			} else if (key.compare("JUMBO_PACKET") == 0) {
				uint32_t v;
				conf >> v;
				jumbo_packet = v;
				if (jumbo_packet)
					std::cerr << "JUMBO_PACKET\t\t\t" << "Yes" << "\n";
				else
					std::cerr << "JUMBO_PACKET\t\t\t" << "No" << "\n";
			} else if (key.compare("TCP_FLOW_PER_NODE") == 0) {
				uint32_t v;
				conf >> v;
				tcp_flow_per_node = v;
				std::cerr << "TCP_FLOW_PER_NODE\t\t" << tcp_flow_per_node << "\n";
			} else if (key.compare("TCP_FLOW_TOTAL") == 0) {
				uint32_t v;
				conf >> v;
				tcp_flow_total = v;
				std::cerr << "TCP_FLOW_TOTAL\t\t" << tcp_flow_total << "\n";
			} else if (key.compare("ENABLE_PCAP_OUTPUT") == 0) {
				uint32_t v;
				conf >> v;
				enable_pcap_output = v;
				std::cerr << "ENABLE_PCAP_OUTPUT\t\t" << enable_pcap_output << "\n";
			} else if (key.compare("ENABLE_ASCII_OUTPUT") == 0) {
				uint32_t v;
				conf >> v;
				enable_ascii_output = v;
				std::cerr << "ENABLE_ASCII_OUTPUT\t\t" << enable_ascii_output << "\n";
			} else if (key.compare("ENABLE_TLP") == 0) {
				uint32_t v;
				conf >> v;
				enable_tlp = (bool)v;
				std::cerr << "ENABLE_TLP\t\t" << enable_tlp << "\n";
			} else if (key.compare("OPTIMIZE") == 0) {
				uint32_t v;
				conf >> v;
				optimization_bit = v;
				std::cerr << "OPTIMIZE\t\t" << optimization_bit << "\n";
			} else if (key.compare("FOREGROUND_RATIO") == 0) {
				double v;
				conf >> v;
				foreground_flow_ratio = v;
				std::cerr << "FOREGROUND_RATIO\t\t" << foreground_flow_ratio << "\n";
			} else if (key.compare("BROADCOM_INGRESS_ALPHA") == 0) {
				double v;
				conf >> v;
				broadcom_ingress_alpha = v;
				std::cerr << "BROADCOM_INGRESS_ALPHA\t\t" << broadcom_ingress_alpha << "\n";
			} else if (key.compare("BROADCOM_EGRESS_ALPHA") == 0) {
				double v;
				conf >> v;
				broadcom_egress_alpha = v;
				std::cerr << "BROADCOM_EGRESS_ALPHA\t\t" << broadcom_egress_alpha << "\n";
			//} else if (key.compare("BROADCOM_SHARED_BUFFER") == 0) {
			//	uint32_t v;
			//	conf >> v;
			//	broadcom_shared_buffer = v;
			//	std::cerr << "BROADCOM_SHARED_BUFFER\t\t" << broadcom_egress_alpha << "\n";
			} else if (key.compare("FOREGROUND_INCAST_FLOW_PER_HOST") == 0) {
				int v;
				conf >> v;
				FOREGROUND_INCAST_FLOW_PER_HOST = v;
				
				std::cerr << "FOREGROUND_INCAST_FLOW_PER_HOST\t\t" << FOREGROUND_INCAST_FLOW_PER_HOST << "\n";
			} else if (key.compare("USE_STATIC_RTO") == 0) {
				int v;
				conf >> v;
				use_static_rto = v;
				std::cerr << "USE_STATIC_RTO\t\t\t" << use_static_rto << "\n";
			}else if (key.compare("LOAD") == 0) {
				double v;
				conf >> v;
				load = v;
				std::cerr << "LOAD\t\t\t" << load << "\n";
			} else if (key.compare("TCP_INITIAL_CWND") == 0) {
				int v;
				conf >> v;
				tcp_initial_cwnd = v;
				std::cerr << "TCP_INITIAL_CWND\t\t\t" << tcp_initial_cwnd << "\n";
			} else if (key.compare("RANDOM_SEED") == 0) {
				int v;
				conf >> v;
				random_seed = v;
				std::cerr << "RANDOM_SEED\t\t\t" << random_seed << "\n";
			}
			
			fflush(stdout);
		}
		conf.close();

		if(!load_workload(tcp_flow_file.c_str(), &workload_cdf)){
			std::cerr<< "Failed to open workload file " << tcp_flow_file.c_str() << std::endl;
			return 1;
		}
	} else {
		std::cerr << "Error: require a config and workload file\n";
		fflush(stdout);
		return 1;
	}

	bool dynamicth = use_dynamic_pfc_threshold;
	NS_ASSERT(packet_level_ecmp + flow_level_ecmp < 2); //packet level ecmp and flow level ecmp are exclusive
	Config::SetDefault("ns3::Ipv4GlobalRouting::RandomEcmpRouting", BooleanValue(packet_level_ecmp));
	Config::SetDefault("ns3::Ipv4GlobalRouting::FlowEcmpRouting", BooleanValue(flow_level_ecmp));
	Config::SetDefault("ns3::QbbNetDevice::PauseTime", UintegerValue(pause_time));
	Config::SetDefault("ns3::QbbNetDevice::QcnEnabled", BooleanValue(enable_qcn));
	Config::SetDefault("ns3::QbbNetDevice::DynamicThreshold", BooleanValue(dynamicth));
	Config::SetDefault("ns3::QbbNetDevice::ClampTargetRate", BooleanValue(clamp_target_rate));
	Config::SetDefault("ns3::QbbNetDevice::ClampTargetRateAfterTimeInc", BooleanValue(clamp_target_rate_after_timer));
	Config::SetDefault("ns3::QbbNetDevice::CNPInterval", DoubleValue(cnp_interval));
	Config::SetDefault("ns3::QbbNetDevice::NPSamplingInterval", DoubleValue(np_sampling_interval));
	Config::SetDefault("ns3::QbbNetDevice::AlphaResumInterval", DoubleValue(alpha_resume_interval));
	Config::SetDefault("ns3::QbbNetDevice::RPTimer", DoubleValue(rp_timer));
	Config::SetDefault("ns3::QbbNetDevice::ByteCounter", UintegerValue(byte_counter));
	Config::SetDefault("ns3::QbbNetDevice::FastRecoveryTimes", UintegerValue(fast_recovery_times));
	Config::SetDefault("ns3::QbbNetDevice::DCTCPGain", DoubleValue(dctcp_gain));
	Config::SetDefault("ns3::QbbNetDevice::RateAI", DataRateValue(DataRate(rate_ai)));
	Config::SetDefault("ns3::QbbNetDevice::RateHAI", DataRateValue(DataRate(rate_hai)));
	Config::SetDefault("ns3::QbbNetDevice::L2BackToZero", BooleanValue(l2_back_to_zero));
	Config::SetDefault("ns3::QbbNetDevice::L2TestRead", BooleanValue(l2_test_read));
	Config::SetDefault("ns3::QbbNetDevice::L2ChunkSize", UintegerValue(l2_chunk_size));
	Config::SetDefault("ns3::QbbNetDevice::L2AckInterval", UintegerValue(l2_ack_interval));
	Config::SetDefault("ns3::QbbNetDevice::L2WaitForAck", BooleanValue(l2_wait_for_ack));
	Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(MicroSeconds(min_rto*1000.)));
	Config::SetDefault("ns3::RttEstimator::InitialEstimation", TimeValue(MicroSeconds(MAX(min_rto*1000., 100.))));
	
	if (min_rto >= 1.) {
		// 1ms
		//ClockGranularity
		Config::SetDefault("ns3::TcpSocketBase::ClockGranularity", TimeValue (MilliSeconds (1)));

	} else {
		Config::SetDefault("ns3::TcpSocketBase::ClockGranularity", TimeValue (MicroSeconds (1)));
	}
	
	Config::SetDefault("ns3::TcpSocketBase::Optimization", UintegerValue(optimization_bit));
	if (enable_tlt) {
		Config::SetDefault("ns3::BroadcomNode::MaxBytesTltUip", DoubleValue(tlt_maxbytes_uip));
	} else {
		Config::SetDefault("ns3::BroadcomNode::MaxBytesTltUip", DoubleValue(0));
	}

	Config::SetDefault("ns3::TcpSocketBase::UseStaticRTO", BooleanValue(use_static_rto));
	Config::SetDefault("ns3::QbbNetDevice::usequeue_flownum", BooleanValue(usequeue_flownum));
	Config::SetDefault("ns3::QbbNetDevice::TLT_IMPORTANT_PKT_INTERVAL", UintegerValue(TLT_IMP_PKT_INTV));
	Config::SetDefault("ns3::BroadcomNode::PFC_ENABLED", BooleanValue(m_PFCenabled));
	Config::SetDefault("ns3::QbbNetDevice::PFC_ENABLED", BooleanValue(m_PFCenabled));
	Config::SetDefault("ns3::QbbNetDevice::BDP", UintegerValue(bdp_val));

	Config::SetDefault("ns3::BroadcomNode::IngressAlpha", DoubleValue(broadcom_ingress_alpha));
	Config::SetDefault("ns3::BroadcomNode::EgressAlpha", DoubleValue(broadcom_egress_alpha));
#if SIMULATION_TYPE == SIMULATION_ROCE
	Config::SetDefault("ns3::QbbNetDevice::EnableTLT", BooleanValue(enable_tlt));
	Config::SetDefault("ns3::QbbNetDevice::EnableIRN", BooleanValue(enable_irn));
	Config::SetDefault("ns3::TcpSocketBase::DCTCP", BooleanValue(false));
#elif SIMULATION_TYPE == SIMULATION_TCP
	Config::SetDefault("ns3::QbbNetDevice::EnableTLT", BooleanValue(false));
	Config::SetDefault("ns3::QbbNetDevice::EnableIRN", BooleanValue(false));
	Config::SetDefault("ns3::TcpSocketBase::EcnMode", EnumValue (TcpSocketBase::EcnMode_t::NoEcn));
	
	if (enable_dctcp) {
		Config::SetDefault("ns3::TcpSocketBase::EcnMode", EnumValue (TcpSocketBase::EcnMode_t::DCTCP));
		Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpDCTCP::GetTypeId ()));
		Config::SetDefault("ns3::TcpDCTCP::g", DoubleValue (dctcp_gain));
	}
	if (enable_tlt) {
		Config::SetDefault("ns3::TcpL4Protocol::TLT", BooleanValue(true));
		Config::SetDefault("ns3::TcpSocketBase::TLT", BooleanValue(true));
		Config::SetDefault("ns3::TcpSocketBase::ReTxThreshold", UintegerValue(1));
		Config::SetDefault("ns3::QbbNetDevice::EnableTLT", BooleanValue(true));
	} else {
		Config::SetDefault("ns3::TcpL4Protocol::TLT", BooleanValue(false));
		Config::SetDefault("ns3::TcpSocketBase::TLT", BooleanValue(false));
		Config::SetDefault("ns3::TcpSocketBase::ReTxThreshold", UintegerValue(1));
	}

	if (enable_tlp) {
		Config::SetDefault("ns3::TcpSocketBase::TLP", BooleanValue(true));
	}

	if (jumbo_packet) {
		Config::SetDefault("ns3::PointToPointNetDevice::Mtu", UintegerValue(9000));
		Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(8940));
		
	}

	Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(tcp_initial_cwnd));
	
	Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue (MicroSeconds ( min_rto*1000.)));
	Config::SetDefault("ns3::RttEstimator::InitialEstimation", TimeValue(MicroSeconds(MAX(min_rto*1000., 100.))));
	if (min_rto >= 1.) {
		// 1ms
		//ClockGranularity
		Config::SetDefault("ns3::TcpSocketBase::ClockGranularity", TimeValue (MilliSeconds (1)));

	} else {
		Config::SetDefault("ns3::TcpSocketBase::ClockGranularity", TimeValue (MicroSeconds (1)));
	}

	//Config::SetDefault("ns3::FlowIdTag::FlowPerDevice", UintegerValue(tcp_flow_per_node));

#endif
	SeedManager::SetSeed(random_seed);

#if SIMULATION_TYPE == SIMULATION_ROCE
	std::ifstream topof, flowf, tracef, tcpflowf;
	topof.open(topology_file.c_str());
	if (!topof.is_open()) {
		std::cerr << "Cannot open topology file" << std::endl;
		return 1;
	}
	flowf.open(flow_file.c_str());
	if (!flowf.is_open()) {
		std::cerr << "Cannot open flow file" << std::endl;
		return 1;
	}
	tracef.open(trace_file.c_str());
	if (!tracef.is_open()) {
		std::cerr << "Cannot open trace file" << std::endl;
		return 1;
	}
	tcpflowf.open(tcp_flow_file.c_str());
	if (!tcpflowf.is_open()) {
		std::cerr << "Cannot open tcp flow file" << std::endl;
		return 1;
	}
	uint32_t node_num, switch_num, link_num, flow_num, trace_num, tcp_flow_num;
	topof >> node_num >> switch_num >> link_num;
	flowf >> flow_num;
	tracef >> trace_num;
	tcpflowf >> tcp_flow_num;


	NodeContainer n;
	n.Create(node_num);
	for (uint32_t i = 0; i < switch_num; i++) {
		uint32_t sid;
		topof >> sid;
		n.Get(sid)->SetNodeType(1, dynamicth); //broadcom switch
		n.Get(sid)->m_broadcom->SetMarkingThreshold(kmin, kmax, pmax);
	}


	NS_LOG_INFO("Create nodes.");

	InternetStackHelper internet;
	internet.Install(n);

	NS_LOG_INFO("Create channels.");

	//
	// Explicitly create the channels required by the topology.
	//

	Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
	Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
	rem->SetRandomVariable(uv);
	uv->SetStream(50);
	rem->SetAttribute("ErrorRate", DoubleValue(error_rate_per_link));
	rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));

	QbbHelper qbb;
	Ipv4AddressHelper ipv4;
	for (uint32_t i = 0; i < link_num; i++) {
		uint32_t src, dst;
		std::string data_rate, link_delay;
		double error_rate;
		topof >> src >> dst >> data_rate >> link_delay >> error_rate;

		qbb.SetDeviceAttribute("DataRate", StringValue(data_rate));
		qbb.SetChannelAttribute("Delay", StringValue(link_delay));

		if (error_rate > 0) {
			Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
			Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
			rem->SetRandomVariable(uv);
			uv->SetStream(50);
			rem->SetAttribute("ErrorRate", DoubleValue(error_rate));
			rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
			qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
		} else {
			qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
		}

		fflush(stdout);
		NetDeviceContainer d = qbb.Install(n.Get(src), n.Get(dst), src, dst);

		char ipstring[24];
		sprintf(ipstring, "10.%d.%d.0", i / 254 + 1, i % 254 + 1);
		ipv4.SetBase(ipstring, "255.255.255.0");
		ipv4.Assign(d);
	}


	NodeContainer trace_nodes;
	for (uint32_t i = 0; i < trace_num; i++) {
		uint32_t nid;
		tracef >> nid;
		trace_nodes = NodeContainer(trace_nodes, n.Get(nid));
	}
	AsciiTraceHelper ascii;
	qbb.EnableAscii(ascii.CreateFileStream(trace_output_file), trace_nodes);

	Ipv4GlobalRoutingHelper::PopulateRoutingTables();

	NS_LOG_INFO("Create Applications.");

	uint32_t packetSize = packet_payload_size;
	Time interPacketInterval = Seconds(0.0000005 / 2);

	for (uint32_t i = 0; i < flow_num; i++) {
		uint32_t src, dst, pg, maxPacketCount, port;
		double start_time, stop_time;
		while (used_port[port = int(UniformVariable(0, 1).GetValue() * 40000)])
			continue;
		used_port[port] = true;
		flowf >> src >> dst >> pg >> maxPacketCount >> start_time >> stop_time;
		NS_ASSERT(n.Get(src)->GetNodeType() == 0 && n.Get(dst)->GetNodeType() == 0);
		Ptr<Ipv4> ipv4 = n.Get(dst)->GetObject<Ipv4>();
		Ipv4Address serverAddress = ipv4->GetAddress(1, 0).GetLocal(); //GetAddress(0,0) is the loopback 127.0.0.1

		if (send_in_chunks) {
			UdpEchoServerHelper server0(port, pg); //Add Priority
			ApplicationContainer apps0s = server0.Install(n.Get(dst));
			apps0s.Start(Seconds(app_start_time));
			apps0s.Stop(Seconds(app_stop_time));
			UdpEchoClientHelper client0(serverAddress, port, pg); //Add Priority
			client0.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
			client0.SetAttribute("Interval", TimeValue(interPacketInterval));
			client0.SetAttribute("PacketSize", UintegerValue(packetSize));
			ApplicationContainer apps0c = client0.Install(n.Get(src));
			apps0c.Start(Seconds(start_time));
			apps0c.Stop(Seconds(stop_time));
		} else {
			UdpServerHelper server0(port);
			ApplicationContainer apps0s = server0.Install(n.Get(dst));
			apps0s.Start(Seconds(app_start_time));
			apps0s.Stop(Seconds(app_stop_time));
			UdpClientHelper client0(serverAddress, port, pg); //Add Priority
			client0.SetAttribute("MaxPackets", UintegerValue(maxPacketCount));
			client0.SetAttribute("Interval", TimeValue(interPacketInterval));
			client0.SetAttribute("PacketSize", UintegerValue(packetSize));
			ApplicationContainer apps0c = client0.Install(n.Get(src));
			apps0c.Start(Seconds(start_time));
			apps0c.Stop(Seconds(stop_time));
		}

	}


	for (uint32_t i = 0; i < tcp_flow_num; i++) {
		uint32_t src, dst, pg, maxPacketCount, port;
		double start_time, stop_time;
		while (used_port[port = int(UniformVariable(0, 1).GetValue() * 40000)])
			continue;
		used_port[port] = true;
		tcpflowf >> src >> dst >> pg >> maxPacketCount >> start_time >> stop_time;
		NS_ASSERT(n.Get(src)->GetNodeType() == 0 && n.Get(dst)->GetNodeType() == 0);
		Ptr<Ipv4> ipv4 = n.Get(dst)->GetObject<Ipv4>();
		Ipv4Address serverAddress = ipv4->GetAddress(1, 0).GetLocal();

		Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
		PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);

		ApplicationContainer sinkApp = sinkHelper.Install(n.Get(dst));
		sinkApp.Start(Seconds(app_start_time));
		sinkApp.Stop(Seconds(app_stop_time));

		BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(serverAddress, port));
		// Set the amount of data to send in bytes.  Zero is unlimited.
		source.SetAttribute("MaxBytes", UintegerValue(0));
		ApplicationContainer sourceApps = source.Install(n.Get(src));
		sourceApps.Start(Seconds(start_time));
		sourceApps.Stop(Seconds(stop_time));
	}


	topof.close();
	flowf.close();
	tracef.close();
	tcpflowf.close();

	//
	// Now, do the actual simulation.
	//
	std::cerr << "Running Simulation.\n";
	fflush(stdout);
	NS_LOG_INFO("Run Simulation.");
	Simulator::Stop(Seconds(simulator_stop_time));
	Simulator::Run();
	Simulator::Destroy();
	NS_LOG_INFO("Done.");

#elif SIMULATION_TYPE == SIMULATION_TCP
//
// Network topology
//
//           10Gb/s, 10us       10Gb/s, 10us
//       n0-----------------n1-----------------n2
//
//
// - Tracing of queues and packet receptions to file 
//   "tcp-large-transfer.tr"
// - pcap traces also generated in the following files
//   "tcp-large-transfer-$n-$i.pcap" where n and i represent node and interface
// numbers respectively
//  Usage (e.g.): ./waf --run tcp-large-transfer

	NS_LOG_COMPONENT_DEFINE("TcpRealisticWorkload");

	CommandLine cmd;
	cmd.Parse(argc, argv);

	// initialize the tx buffer.
	for (uint32_t i = 0; i < writeSize; ++i) {
		char m = toascii(97 + i % 26);
		data[i] = m;
	}
	
	std::ifstream topof;
	topof.open(topology_file.c_str());
	if (!topof.is_open()) {
		std::cerr << "Cannot open topology file" << std::endl;
		return 1;
	}
	uint32_t node_num, switch_num, link_num;
	topof >> node_num >> switch_num >> link_num;
	
	NodeContainer n;
	n.Create(node_num);
	for (uint32_t i = 0; i < switch_num; i++) {
		uint32_t sid;
		topof >> sid;
		n.Get(sid)->SetNodeType(1, dynamicth); //broadcom switch
		n.Get(sid)->m_broadcom->SetTCPMarkingThreshold(kmin, kmax); // BugFix!
		if(i < 12) {
			// Egress log of switches
			n.Get(sid)->m_broadcom->printQueueLen = true;
			n.Get(sid)->m_broadcom->switchId = i;
		}
	}
	
	NS_LOG_INFO("Create nodes.");

	InternetStackHelper internet;
	internet.Install(n);

	NS_LOG_INFO("Create channels.");
	//
	// Explicitly create the channels required by the topology.
	//

	Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
	Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
	rem->SetRandomVariable(uv);
	uv->SetStream(50);
	rem->SetAttribute("ErrorRate", DoubleValue(error_rate_per_link));
	rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));

	QbbHelper qbb;
	Ipv4AddressHelper ipv4;
	std::map <uint32_t, Ipv4InterfaceContainer> ifcs;

	for (uint32_t i = 0; i < link_num; i++) {
		uint32_t src, dst;
		std::string data_rate, link_delay;
		double error_rate;
		topof >> src >> dst >> data_rate >> link_delay >> error_rate;

		qbb.SetDeviceAttribute("DataRate", StringValue(data_rate));
		qbb.SetChannelAttribute("Delay", StringValue(link_delay));

		if (error_rate > 0) {
			Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
			Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
			rem->SetRandomVariable(uv);
			uv->SetStream(50);
			rem->SetAttribute("ErrorRate", DoubleValue(error_rate));
			NS_ABORT_IF(error_rate != 0);
			rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
			qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
		} else {
			qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
		}

		fflush(stdout);
		NetDeviceContainer d = qbb.Install(n.Get(src), n.Get(dst), src, dst);

		char ipstring[24];
		sprintf(ipstring, "10.%d.%d.0", i / 254 + 1, i % 254 + 1);
		ipv4.SetBase(ipstring, "255.255.255.0");
		ifcs.insert(std::pair<uint32_t, Ipv4InterfaceContainer>(dst, ipv4.Assign(d)));
	}

	Ipv4GlobalRoutingHelper::PopulateRoutingTables();
	topof.close();

	//Define Connection
	uint16_t servPort = 50000;

	// Create a packet sink to receive these packets on n2...
	for(unsigned i=switch_num; i< node_num; i++) {
		PacketSinkHelper sink("ns3::TcpSocketFactory",
								InetSocketAddress(Ipv4Address::GetAny(), servPort));
		ApplicationContainer apps = sink.Install(n.Get(i));
		apps.Start(Seconds(0.0));
		apps.Stop(Seconds(1000.0));
	}
	
	// Create a source to send packets from n0.  Instead of a full Application
	// and the helper APIs you might see in other example files, this example
	// will use sockets directly and register some socket callbacks as a sending
	// "Application".

	// Create and bind the socket...
	Ptr<ExponentialRandomVariable> exp_rv = CreateObject<ExponentialRandomVariable> ();
	Ptr<UniformRandomVariable> uniform_rv = CreateObject<UniformRandomVariable> ();
	exp_rv->SetStream(0); // deterministic
	uniform_rv->SetStream(0); // deterministic

	const double LINK_RATE_IN_GBPS = 40.;
	const double oversubscription_ratio = 2.0; // 4 uplinks, 8 downlinks
	const uint32_t MTU_SIZE = 1538;
	const uint32_t MSS_SIZE = 1480;
	double avg_flow_size = 0.;
	for (int i=0; i<=1000; i++)
		avg_flow_size += workload_cdf[i];
	avg_flow_size = avg_flow_size / 1001.;
	std::cerr << "Link Rate : " << LINK_RATE_IN_GBPS << "Gbps, Load : " << load << ", AvgFlowSize: " << avg_flow_size << ", workload: " << tcp_flow_file << std::endl;
	int host_num = node_num - switch_num;
	if(foreground_flow_ratio > 1 || foreground_flow_ratio < 0) 
		NS_ABORT_MSG("Invalid foreground flow ratio");

	double lambda = (double) LINK_RATE_IN_GBPS * 1e9 * load / (8. *avg_flow_size * ((double)MTU_SIZE)/MSS_SIZE) / oversubscription_ratio * host_num;
	double fg_lambda = (double) LINK_RATE_IN_GBPS * 1e9 * load * foreground_flow_ratio / (8. * FOREGROUND_INCAST_FLOW_PER_HOST * dctcp_tx_bytes * ((double)MTU_SIZE)/MSS_SIZE) / oversubscription_ratio;
	std::cerr << "Total Lambda : " << lambda << " Hz, Foreground Lambda : " << fg_lambda << " Hz, ";
	lambda = lambda * (1-foreground_flow_ratio);
	std::cerr << "Background Lambda : " << lambda << " Hz, ";
	if(lambda > 0)
 		exp_rv->SetAttribute("Mean", DoubleValue(1./lambda));
	double current_time = 0.;

	int remaining_flows = tcp_flow_total;
	const Time large_foreground_interval = fg_lambda > 0 ? Seconds(1/fg_lambda) : MilliSeconds(0);
	std::cerr << "Foreground Interval : " << (large_foreground_interval.GetMicroSeconds()/1000.) << "ms" << std::endl;
	
	uint32_t flow_id = 0;
	for (int i=0; i< remaining_flows; i++) {
		uint32_t send_host = static_cast<uint32_t>(uniform_rv->GetInteger(0, node_num - switch_num -1));
		uint32_t recv_host = static_cast<uint32_t>(uniform_rv->GetInteger(0, node_num - switch_num -2));
		if(recv_host >= send_host) recv_host++;
		//std::cout << "From host " << send_host << "(" << (send_host + switch_num) << ") to " << recv_host << "(" << (recv_host+switch_num) << ")" << std::endl;
		//std::cout << "From host " << n.Get(switch_num + send_host)->GetDevice(1)->GetAddress() << " to " << n.Get(switch_num + recv_host)->GetDevice(1)->GetAddress() << std::endl;

		uint32_t target_len = workload_cdf[uniform_rv->GetInteger(0, 1000)];
		if(target_len == 0) target_len = 1;
		targetTxBytes.push_back(target_len);
		currentTxBytes.push_back(0);
		auto it = ifcs.find(switch_num+recv_host);
		NS_ASSERT(it != ifcs.end());
		
    	std::cerr << "Flow " << flow_id << " : Total length : " << target_len << std::endl;
		Simulator::Schedule(Seconds(current_time), &StartFlow, n.Get(switch_num+send_host), flow_id, std::pair<uint32_t, uint32_t>(send_host, recv_host), it->second.GetAddress(1), servPort, target_len);
		flow_id++;
		current_time += exp_rv->GetValue();
	}
	// 100ms, 1ms 10kflow -> 100
	//flow_id = remaining_flows + 10000000;
	if(foreground_flow_ratio != 0 && foreground_flow_ratio != 1 && large_foreground_interval.GetMicroSeconds() < 100 && FOREGROUND_INCAST_FLOW_PER_HOST != 0) {
		NS_ABORT_MSG("Error: Foreground flow might end earlier than background flow");
	}
	num_background_flow = flow_id;
	current_time = large_foreground_interval.GetSeconds();
	if(current_time > 0) {
		for (int i=0; i< tcp_flow_total/5; i++) {
			uint32_t recv_host = static_cast<uint32_t>(uniform_rv->GetInteger(0, node_num - switch_num -1));
			for(unsigned int j=0; j< node_num - switch_num ; j++) {
				if(j == recv_host) continue;
				auto it = ifcs.find(switch_num+recv_host);
				NS_ASSERT(it != ifcs.end());
				
				for(unsigned int k =0; k < FOREGROUND_INCAST_FLOW_PER_HOST; k++) {
					targetTxBytes.push_back(dctcp_tx_bytes);
					currentTxBytes.push_back(0);
					Simulator::Schedule(Seconds(current_time), &StartFlowForeground, n.Get(switch_num+j), flow_id, std::pair<uint32_t, uint32_t>(j, recv_host), it->second.GetAddress(1), servPort, dctcp_tx_bytes);
					flow_id++;
				}
					
			}
			current_time += large_foreground_interval.GetSeconds(); 
		}  
	}
    


	// One can toggle the comment for the following line on or off to see the
	// effects of finite send buffer modelling.  One can also change the size of
	// said buffer.

	//localSocket->SetAttribute("SndBufSize", UintegerValue(4096));

	//Ask for ASCII and pcap traces of network traffic
	char sbuf[128] = { 0, };
	if (enable_tlt)
		sprintf(sbuf, "tcp-realistic-flow%d-%d-tlt%dk.tr", dctcp_sender_cnt, tcp_flow_total, (int)(tlt_maxbytes_uip/1000));
	else
		sprintf(sbuf, "tcp-realistic-flow%d-%d-tltoff.tr", dctcp_sender_cnt, tcp_flow_total);
	if(enable_ascii_output) {
		AsciiTraceHelper ascii;
		qbb.EnableAsciiAll(ascii.CreateFileStream(sbuf));
	}
	if(enable_pcap_output)
		qbb.EnablePcapAll(sbuf); 

	// std::vector<FILE *> swtich_trace;
	// for(unsigned int i=0; i< switch_num; i++) {
	// 	sprintf(sbuf, "switch_trace_%d_%lf_%d_%d_%d_%d_sw%u.tr", tcp_flow_total, load, enable_dctcp, m_PFCenabled, enable_tlt, tlt_maxbytes_uip, i);
	// 	FILE * fs = fopen(sbuf, "w");

	// }

	// Finally, set up the simulator to run.  The 1000 second hard limit is a
	// failsafe in case some change above causes the simulation to never end
	Simulator::Stop(Seconds(1000));
	Simulator::Schedule(Seconds(999), &printStatistics);
	Simulator::Schedule(Seconds(900), &simpleTest);
	Simulator::Run();
	Simulator::Destroy();
#endif
}
namespace ns3 {
	extern std::unordered_map<int32_t, Time> PausedTime; 
}


void printStatistics() {
	
	unsigned long totalPayload = 0;
	for(auto it = sockets.begin(); it != sockets.end(); it++) {
		totalPayload += it->second;
	}
	printf("%.8lf\tTOTAL_PAYLOAD\t0\t%lu\n", Simulator::Now().GetSeconds(), totalPayload);
	printf("Flow#\tsrc\tdst\tstart\tend\tduration\tsize\tpaused\n");
	// fprintf(stderr, "Flow#\tsrc\tdst\tstart\tend\tduration\tsize\tack\tcompleted\n");
	for(auto it = sockets.begin(); it != sockets.end(); it++) {
		printf("%d\t%d\t%d\t%.9lf\t%.9lf\t%.9lf\t%u\t%.9lf\n", it->first->socketId, it->first->host_src, it->first->host_dst, it->first->firstUsedTcp.GetSeconds(), it->first->lastUsedTcp.GetSeconds(), 
		  it->first->lastUsedTcp.GetSeconds() - it->first->firstUsedTcp.GetSeconds(), it->second, PausedTime[it->first->socketId].GetSeconds());
		// fprintf(stderr, "%d\t%d\t%d\t%.9lf\t%.9lf\t%.9lf\t%u\t%u\t%s\n", it->first->socketId, it->first->host_src, it->first->host_dst, it->first->firstUsedTcp.GetSeconds(), it->first->lastUsedTcp.GetSeconds(), 
		//   it->first->lastUsedTcp.GetSeconds() - it->first->firstUsedTcp.GetSeconds(), it->second, it->first->lastAckTcp.GetValue(), (it->first->lastAckTcp.GetValue() > it->second ? "COMPLETE": "INCOMP"));
		totalPayload += it->second;
	}

	fflush(stdout);
	fflush(stderr);

}

#if SIMULATION_TYPE == SIMULATION_TCP


//-----------------------------------------------------------it->first->------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//begin implementation of sending "Application"

static volatile bool enable_foreground = true;

void StartFlow(Ptr<Node> clientNode, uint32_t flowId, std::pair<uint32_t, uint32_t> sendRecvHosts, Ipv4Address servAddress, uint16_t servPort, uint32_t targetLen) {
	Ptr<Socket> localSocket = Socket::CreateSocket(clientNode, TcpSocketFactory::GetTypeId());
	sockets.push_back(std::pair<Ptr<Socket>, uint32_t>(localSocket,targetLen));
	localSocket->Bind();
	localSocket->socketId = flowId;
	localSocket->host_src = sendRecvHosts.first;
	localSocket->host_dst = sendRecvHosts.second;
	localSocket->targetLen = targetLen;
	localSocket->num_background_flow = (int32_t) num_background_flow;
	localSocket->socket_flow_type = flowId < num_background_flow ? BACKGROUND_FLOW : FOREGROUND_FLOW;
	std::cerr << "Starting flow " << localSocket->socketId << " at time " << Simulator::Now().GetSeconds() << " to " << servAddress << std::endl;
	if(flowId == (uint32_t)(tcp_flow_total-1)) {
		enable_foreground = false;
	}
	localSocket->Connect(InetSocketAddress(servAddress, servPort)); //connect

																	// tell the tcp implementation to call WriteUntilBufferFull again
																	// if we blocked and new tx buffer space becomes available
	localSocket->SetSendCallback(MakeCallback(&WriteUntilBufferFull));
	localSocket->firstUsed = Simulator::Now();
	WriteUntilBufferFull(localSocket, localSocket->GetTxAvailable());
}
void StartFlowForeground(Ptr<Node> clientNode, uint32_t flowId, std::pair<uint32_t, uint32_t> sendRecvHosts, Ipv4Address servAddress, uint16_t servPort, uint32_t targetLen) {
	if(enable_foreground) {
		StartFlow(clientNode, flowId, sendRecvHosts, servAddress, servPort, targetLen);
	}
}

void WriteUntilBufferFull(Ptr<Socket> localSocket, uint32_t txSpace) {
	int i = localSocket->socketId;
	while (currentTxBytes[i] < targetTxBytes[i]) {
		uint32_t left = targetTxBytes[i] - currentTxBytes[i];
		uint32_t dataOffset = currentTxBytes[i] % writeSize;
		uint32_t toWrite = writeSize - dataOffset;
		uint32_t txAvail = localSocket->GetTxAvailable();
		if (txAvail == 0) {
			// we will be called again when new tx space becomes available.
			return;
		}
		toWrite = std::min(toWrite, left);
		toWrite = std::min(toWrite, localSocket->GetTxAvailable());
		int amountSent = localSocket->Send(&data[dataOffset], toWrite, 0);
		if (amountSent < 0) {
			// we will be called again when new tx space becomes available.
			return;
		}
		currentTxBytes[i] += amountSent;
	}
	localSocket->Close();
	//std::cerr << "Closing flow " << localSocket->socketId << " at time " << Simulator::Now().GetSeconds() << std::endl;
}

#endif

bool load_workload(const char *workload_file, uint32_t **workload_cdf) {
  FILE *f = fopen(workload_file, "r");
  if(!f) return false;
  *workload_cdf = new uint32_t[1001];
  int flow_size;
  double value, prev_fs = 0.;
  int cursor = 0;
  while(fscanf(f, "%d %lf", &flow_size, &value) == 2) {
    int current = (int)(value*1000.);
    for(int i=cursor; i < current; i++) {
      (*workload_cdf)[i] = prev_fs + (flow_size-prev_fs)*(i-cursor)/((double)(current-cursor));
    }
    cursor = current;
    prev_fs = flow_size;
  }
  for (int i=cursor; i< 1001; i++){
    (*workload_cdf)[i] = flow_size;
  }
  fclose(f);
  return true;
}

