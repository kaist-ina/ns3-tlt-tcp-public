/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Author: Hwijoon Lim <wjuni@kaist.ac.kr>
*/

#include "tlt-tag.h"

namespace ns3 {
NS_OBJECT_ENSURE_REGISTERED(TltTag);
TltTag::TltTag() :
	Tag(),
m_type(PACKET_NOT_IMPORTANT)
{
}


TypeId
TltTag::GetTypeId(void)
{
	static TypeId tid = TypeId("ns3::TltTag")
		.SetParent<Tag>()
		.AddConstructor<TltTag>()
		;
	return tid;
}
TypeId
TltTag::GetInstanceTypeId(void) const
{
	return GetTypeId();
}

uint32_t
TltTag::GetSerializedSize(void) const
{
	return sizeof(m_type)+sizeof(debug_socketId);
}

void
TltTag::Serialize(TagBuffer i) const
{
	i.WriteU8(m_type);
	i.WriteU32(debug_socketId);
}

void
TltTag::Deserialize(TagBuffer i)
{
	uint8_t t = i.ReadU8();
	NS_ASSERT(t == PACKET_IMPORTANT || t == PACKET_NOT_IMPORTANT || t == PACKET_IMPORTANT_ECHO ||
	 t == PACKET_IMPORTANT_FORCE  || t == PACKET_IMPORTANT_ECHO_FORCE || t == PACKET_IMPORTANT_FAST_RETRANS || t == PACKET_IMPORTANT_CONTROL);
	m_type = t;
	debug_socketId = i.ReadU32();
}

void TltTag::SetType(uint8_t ttl)
{
	NS_ASSERT(ttl == PACKET_IMPORTANT || ttl == PACKET_NOT_IMPORTANT || ttl == PACKET_IMPORTANT_ECHO
	|| ttl == PACKET_IMPORTANT_FORCE  || ttl == PACKET_IMPORTANT_ECHO_FORCE
	|| ttl == PACKET_IMPORTANT_FAST_RETRANS || ttl == PACKET_IMPORTANT_CONTROL);
	m_type = ttl;
}

uint8_t TltTag::GetType()
{
	return m_type;
}

void TltTag::Print(std::ostream & os) const
{
	os << "TLT Important: " << ((m_type == PACKET_IMPORTANT) ? "Yes " : (
		(m_type == PACKET_IMPORTANT_ECHO) ? "Echo " : (
			(m_type == PACKET_IMPORTANT_FORCE) ? "Force " : (
				(m_type == PACKET_IMPORTANT_ECHO_FORCE) ? "EchoForce " : (
					(m_type == PACKET_IMPORTANT_FAST_RETRANS) ? "EchoForce " : "No " )))));
}

} // namespace ns3

