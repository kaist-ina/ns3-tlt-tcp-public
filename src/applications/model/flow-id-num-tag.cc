/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Author: Youngmok Jung <tom418@kaist.ac.kr>
*/

#include "flow-id-num-tag.h"

namespace ns3 {
	NS_OBJECT_ENSURE_REGISTERED(FlowIDNUMTag);


	FlowIDNUMTag::FlowIDNUMTag() :
		Tag(),
		flow_stat(0)
	{
	}


	TypeId
		FlowIDNUMTag::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::FlowIDNUMTag")
			.SetParent<Tag>()
			.AddConstructor<FlowIDNUMTag>()
			;
		return tid;
	}
	TypeId
		FlowIDNUMTag::GetInstanceTypeId(void) const
	{
		return GetTypeId();
	}

	uint32_t
		FlowIDNUMTag::GetSerializedSize(void) const
	{
		return sizeof(flow_stat);
	}

	void
		FlowIDNUMTag::Serialize(TagBuffer i) const
	{
		i.WriteU16(flow_stat);
	}

	void
		FlowIDNUMTag::Deserialize(TagBuffer i)
	{
		flow_stat = i.ReadU16();
	}

	void FlowIDNUMTag::SetType(uint16_t ttl)
	{
		
		flow_stat = ttl;
	}

	uint16_t FlowIDNUMTag::GetType()
	{
		return flow_stat;
	}

	uint16_t FlowIDNUMTag::Getflowid()
	{
		static uint32_t nextFlowId = 1;
		flow_stat = nextFlowId++;
		return flow_stat;
	}

	void FlowIDNUMTag::Print(std::ostream & os) const
	{
		os << flow_stat;
	}

} // namespace ns3

