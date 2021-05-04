/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Author: Hwijoon Lim <wjuni@kaist.ac.kr>
*/

#include "tcp-flow-id-tag.h"

namespace ns3 {
NS_OBJECT_ENSURE_REGISTERED(TcpFlowIdTag);
TcpFlowIdTag::TcpFlowIdTag() :
	Tag()
{
}


TypeId
TcpFlowIdTag::GetTypeId(void)
{
	static TypeId tid = TypeId("ns3::TcpFlowIdTag")
		.SetParent<Tag>()
		.AddConstructor<TcpFlowIdTag>()
		;
	return tid;
}
TypeId
TcpFlowIdTag::GetInstanceTypeId(void) const
{
	return GetTypeId();
}

uint32_t
TcpFlowIdTag::GetSerializedSize(void) const
{
	return sizeof(m_socketId) + sizeof(m_remoteSocketId);
}

void
TcpFlowIdTag::Serialize(TagBuffer i) const
{
	i.WriteU32(m_socketId);
	i.WriteU32(m_remoteSocketId);
}

void
TcpFlowIdTag::Deserialize(TagBuffer i)
{
	m_socketId = i.ReadU32();
	m_remoteSocketId = i.ReadU32();
}

void TcpFlowIdTag::Print(std::ostream & os) const
{
    os << "TcpFlowIdTag=" << m_socketId  << " (Remote=" << m_remoteSocketId << ")";
}

} // namespace ns3

