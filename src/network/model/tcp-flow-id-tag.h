/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
* Author: Hwijoon Lim <wjuni@kaist.ac.kr>
*/
#ifndef TCP_FLOW_ID_TAG_H
#define TCP_FLOW_ID_TAG_H

#include "ns3/tag.h"

namespace ns3 {
	/**
	* \ingroup tlt
	* \brief The packet header for an tcp flow id packet
	*/
	class TcpFlowIdTag : public Tag
	{
	public:
		TcpFlowIdTag();
		static TypeId GetTypeId(void);
		virtual TypeId GetInstanceTypeId(void) const;
		virtual void Print(std::ostream &os) const;
		virtual uint32_t GetSerializedSize(void) const;
		virtual void Serialize(TagBuffer i) const;
		virtual void Deserialize(TagBuffer i);
		
		int32_t m_socketId {-1};
		int32_t m_remoteSocketId {-1};

	private:
	};

} // namespace ns3

#endif /* TCP_FLOW_ID_TAG_H */