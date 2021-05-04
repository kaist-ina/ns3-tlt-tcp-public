/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Author: Youngmok Jung <tom418@kaist.ac.kr>
*/

#include "ns3/tag.h"

namespace ns3 {
	/**
	* \ingroup tlt
	* \brief The packet header for an TLT packet
	*/
	class FlowIDNUMTag : public Tag
	{
	public:
		FlowIDNUMTag();
		static TypeId GetTypeId(void);
		virtual TypeId GetInstanceTypeId(void) const;
		virtual void Print(std::ostream &os) const;
		virtual uint32_t GetSerializedSize(void) const;
		virtual void Serialize(TagBuffer i) const;
		virtual void Deserialize(TagBuffer i);
		void SetType(uint16_t ttl);
		uint16_t GetType();
		uint16_t Getflowid();
		
		
	private:
		uint16_t flow_stat;
		static uint16_t global_FLOWID_counter;

	};

} // namespace ns3

