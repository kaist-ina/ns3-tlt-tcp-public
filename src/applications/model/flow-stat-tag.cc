/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Author: Youngmok Jung <tom418@kaist.ac.kr>
*/

#include "flow-stat-tag.h"

namespace ns3 {
	FlowStatTag::FlowStatTag() :
		flow_stat(FLOW_NOTEND)
	{
	}


	TypeId
		FlowStatTag::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::FlowStatTag")
			.SetParent<Tag>()
			.AddConstructor<FlowStatTag>()
			;
		return tid;
	}
	TypeId
		FlowStatTag::GetInstanceTypeId(void) const
	{
		return GetTypeId();
	}

	uint32_t
		FlowStatTag::GetSerializedSize(void) const
	{
		return sizeof(flow_stat);
	}

	void
		FlowStatTag::Serialize(TagBuffer i) const
	{
		i.WriteU8(flow_stat);
	}

	void
		FlowStatTag::Deserialize(TagBuffer i)
	{
		uint8_t t = i.ReadU8();
		NS_ASSERT(t == FLOW_END || t == FLOW_NOTEND || t == FLOW_START);
		flow_stat = t;
	}

	void FlowStatTag::SetType(uint8_t ttl)
	{
		NS_ASSERT(ttl == FLOW_END || ttl == FLOW_NOTEND || ttl == FLOW_START);
		flow_stat = ttl;
	}

	uint8_t FlowStatTag::GetType()
	{
		return flow_stat;
	}

	void FlowStatTag::Print(std::ostream & os) const
	{
		if (flow_stat != FLOW_NOTEND) {
			os << "Flow ended: " << ((flow_stat == FLOW_END) ? "Yes " : "No ");
		}
		else {
			os << "Flow Continuing: Yes";
		}
	}

} // namespace ns3

