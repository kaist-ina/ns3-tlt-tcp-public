/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Author: Hwijoon Lim <wjuni@kaist.ac.kr>
*/
#ifndef PFC_EXPERIENCE_TAG_H
#define PFC_EXPERIENCE_TAG_H

#include "ns3/tag.h"

namespace ns3 {
	/**
	* \ingroup tlt
	* \brief The packet header for an TLT packet
	*/
	class PfcExperienceTag : public Tag
	{
	public:
		PfcExperienceTag();
		static TypeId GetTypeId(void);
		virtual TypeId GetInstanceTypeId(void) const;
		virtual void Print(std::ostream &os) const;
		virtual uint32_t GetSerializedSize(void) const;
		virtual void Serialize(TagBuffer i) const;
		virtual void Deserialize(TagBuffer i);
		
        
		uint64_t m_accumulate {0};
		uint64_t m_start {0};
		uint32_t m_socketId {0};

	private:
	};

} // namespace ns3

#endif /* TLT_TAG_H */