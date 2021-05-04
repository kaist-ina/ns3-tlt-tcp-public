#include "tcp-dctcp.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "tcp-socket-base.h"
#include "ns3/tcp-header.h"

NS_LOG_COMPONENT_DEFINE ("TcpDCTCP");

namespace ns3 {
#define DCTCP_STAT 0
NS_OBJECT_ENSURE_REGISTERED (TcpDCTCP);

TypeId
TcpDCTCP::GetTypeId (void)
{
  static TypeId tid = TypeId("ns3::TcpDCTCP")
    .SetParent<TcpNewReno> ()
    .SetGroupName ("Internet")
    .AddConstructor<TcpDCTCP> ()
    .AddAttribute("g", "The g in the DCTCP",
                  DoubleValue (0.0625),
                  MakeDoubleAccessor (&TcpDCTCP::m_g),
                  MakeDoubleChecker<double> (0));

  return tid;
}

TcpDCTCP::TcpDCTCP (void) :
  TcpNewReno (),
  m_g (0.0625),
  m_alpha (1),
  m_isCE (false),
  m_hasDelayedACK (false),
  m_bytesAcked (0),
  m_ecnBytesAcked (0),
  m_highTxMark (0)
{
  NS_LOG_FUNCTION (this);
}

TcpDCTCP::TcpDCTCP (const TcpDCTCP &sock) :
    TcpNewReno (sock),
  m_g (sock.m_g),
  m_alpha (sock.m_alpha),
  m_isCE (false),
  m_hasDelayedACK (false),
  m_bytesAcked (sock.m_bytesAcked),
  m_ecnBytesAcked (sock.m_ecnBytesAcked),
  m_highTxMark (sock.m_highTxMark)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_LOGIC ("Invoked the copy constructor");
}

TcpDCTCP::~TcpDCTCP (void)
{
}

std::string
TcpDCTCP::GetName () const
{
  return "TcpDCTCP";
}

void
TcpDCTCP::PktsAcked (Ptr<TcpSocketState> tcb, uint32_t segmentsAcked,
        const Time &rtt, bool withECE, SequenceNumber32 highTxMark, SequenceNumber32 ackNumber)
{
  NS_LOG_FUNCTION (this << tcb << segmentsAcked << rtt << withECE << highTxMark << ackNumber);
  m_bytesAcked += segmentsAcked * tcb->m_segmentSize;
  if (withECE) {
    m_ecnBytesAcked += segmentsAcked * tcb->m_segmentSize;
  }
  //std::cout << "PktsAcked - AckNumber=" << ackNumber << ", highTxMark=" << highTxMark << ", m_highTxMark=" << m_highTxMark << std::endl;
  if (ackNumber >= m_highTxMark)
  {
    m_highTxMark = highTxMark;
    TcpDCTCP::UpdateAlpha ();
  }
}

void
TcpDCTCP::UpdateAlpha()
{
  double f;
  if (m_bytesAcked == 0) {
      f = 0.0;
  } else {
      f = static_cast<double>(m_ecnBytesAcked) / static_cast<double>(m_bytesAcked);
  }
  m_alpha = (1 - m_g) * m_alpha + m_g * f;
  NS_LOG_LOGIC (this << Simulator::Now () << " alpha updated: " << m_alpha << " and f: " << f << " bytes: " << m_bytesAcked << " ece bytes: " << m_ecnBytesAcked);
  m_bytesAcked = 0;
  m_ecnBytesAcked = 0;
  #if DCTCP_STAT
  printf("%.8lf\tDCTCP_ALPHA\t%p\t%lf\n", Simulator::Now().GetSeconds(), this, m_alpha.Get());
  #endif
}

void
TcpDCTCP::CwndEvent(Ptr<TcpSocketState> tcb, TcpSocketState::TcpCAEvent_t ev, Ptr<TcpSocketBase> socket)
{
  if (!socket) {
      std::cout << "ERROR : CWND EVENT NULL SOCKET" << std::endl;
      return;
  }
  if (ev == TcpSocketState::CA_EVENT_ECN_IS_CE && m_isCE == false) // No CE -> CE
  {
    NS_LOG_LOGIC (this << Simulator::Now () << " No CE -> CE ");
    // Note, since the event occurs before writing the data into the buffer,
    // the AckNumber would be the old one, which satisfies our state machine
    if (m_hasDelayedACK)
    {
      NS_LOG_DEBUG ("Delayed ACK exists, sending ACK");
      SendEmptyPacket(socket, TcpHeader::ACK);
    }
    tcb->m_demandCWR = true;
    m_isCE = true;
  }
  else if (ev == TcpSocketState::CA_EVENT_ECN_NO_CE && m_isCE == true) // CE -> No CE
  {
    NS_LOG_LOGIC (this << " CE -> No CE ");
    if (m_hasDelayedACK)
    {
      NS_LOG_DEBUG ("Delayed ACK exists, sending ACK | ECE");
      SendEmptyPacket(socket, TcpHeader::ACK | TcpHeader::ECE);
    }
    tcb->m_demandCWR = false;
    m_isCE = false;
  }
  else if (ev == TcpSocketState::CA_EVENT_DELAYED_ACK)
  {
    m_hasDelayedACK = true;
    NS_LOG_LOGIC (this << " Reserve delay ACK ");
  }
  else if (ev == TcpSocketState::CA_EVENT_NON_DELAYED_ACK)
  {
    m_hasDelayedACK = false;
    NS_LOG_LOGIC (this << " Cancel delay ACK ");
  }
}

void
TcpDCTCP::IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
  // In CA_CWR, the DCTCP keeps the windows size
  if (tcb->m_congState != TcpSocketState::CA_CWR)
  {
    //auto prev = tcb->m_cWnd;
    TcpNewReno::IncreaseWindow(tcb, segmentsAcked);
    //NS_LOG_WARN("Increasing Window from " << prev << " to " << tcb->m_cWnd );
    
  }
}

uint32_t
TcpDCTCP::GetSsThresh(Ptr<TcpSocketState> tcb, uint32_t bytesInFlight)
{
  NS_LOG_FUNCTION (this << tcb << bytesInFlight);
  if (tcb->m_congState == TcpSocketState::CA_RECOVERY)
  {
      return TcpNewReno::GetSsThresh (tcb, bytesInFlight);
  }
  return std::max(std::max (static_cast<uint32_t>((1 - m_alpha / 2) * tcb->m_cWnd), bytesInFlight / 2), 2 * tcb->m_segmentSize);
}

uint32_t
TcpDCTCP::GetCwnd(Ptr<TcpSocketState> tcb)
{
  NS_LOG_FUNCTION (this << tcb);
  
  //NS_LOG_WARN("Decreasing Window from " << tcb->m_cWnd << " to " << static_cast<uint32_t>((1 - m_alpha / 2) * tcb->m_cWnd ));
  return std::max(static_cast<uint32_t>((1 - m_alpha / 2) * tcb->m_cWnd ), tcb->m_segmentSize);
}

Ptr<TcpCongestionOps>
TcpDCTCP::Fork ()
{
  return CreateObject<TcpDCTCP> (*this);
}

}