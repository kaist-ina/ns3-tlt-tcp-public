/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Author: Hwijoon Lim <wjuni@kaist.ac.kr>
*/

#include "pfc-experience-tag.h"

namespace ns3 {
NS_OBJECT_ENSURE_REGISTERED(PfcExperienceTag);
PfcExperienceTag::PfcExperienceTag() :
	Tag()
{
}


TypeId
PfcExperienceTag::GetTypeId(void)
{
	static TypeId tid = TypeId("ns3::PfcExperienceTag")
		.SetParent<Tag>()
		.AddConstructor<PfcExperienceTag>()
		;
	return tid;
}
TypeId
PfcExperienceTag::GetInstanceTypeId(void) const
{
	return GetTypeId();
}

uint32_t
PfcExperienceTag::GetSerializedSize(void) const
{
	return sizeof(m_accumulate) + sizeof(m_start) + sizeof(m_socketId);
}

void
PfcExperienceTag::Serialize(TagBuffer i) const
{
	i.WriteU64(m_accumulate);
    i.WriteU64(m_start);
	i.WriteU32(m_socketId);
}

void
PfcExperienceTag::Deserialize(TagBuffer i)
{
	m_accumulate = i.ReadU64();
	m_start = i.ReadU64();
	m_socketId = i.ReadU32();
}

void PfcExperienceTag::Print(std::ostream & os) const
{
    os << "PFC_Experienced=" << m_accumulate << " (current block started at " << m_start << ")";
}

} // namespace ns3

