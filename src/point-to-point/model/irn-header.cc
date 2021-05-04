#include <stdint.h>
#include <iostream>
#include "irn-header.h"
#include "ns3/buffer.h"
#include "ns3/address-utils.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE("irnHeader");

namespace ns3 {

	NS_OBJECT_ENSURE_REGISTERED(irnHeader);

	irnHeader::irnHeader(uint16_t pg)
		: m_pg(pg)
	{
	}

	irnHeader::irnHeader()
		: m_pg(0)
	{}

	irnHeader::~irnHeader()
	{}

	void irnHeader::SetPG(uint16_t pg)
	{
		m_pg = pg;
	}

	void irnHeader::SetSeq(uint32_t seq)
	{
		m_seq = seq;
	}

	void irnHeader::SetPort(uint16_t port)
	{
		m_port = port;
	}

	void irnHeader::SetSeqNack(uint32_t seq)
	{
		m_seq_NACK = seq;
	}

	uint16_t irnHeader::GetPG() const
	{
		return m_pg;
	}

	uint32_t irnHeader::GetSeq() const
	{
		return m_seq;
	}

	uint16_t irnHeader::GetPort() const
	{
		return m_port;
	}

	uint16_t irnHeader::GetSeqNACK() const
	{
		return m_seq_NACK;
	}

	TypeId
		irnHeader::GetTypeId(void)
	{
		static TypeId tid = TypeId("ns3::irnHeader")
			.SetParent<Header>()
			.AddConstructor<irnHeader>()
			;
		return tid;
	}
	TypeId
		irnHeader::GetInstanceTypeId(void) const
	{
		return GetTypeId();
	}
	void irnHeader::Print(std::ostream &os) const
	{
		os << "IRN NACK:" << "pg=" << m_pg << ",seq=" << m_seq << ",seqNACK=" << m_seq_NACK;
	}
	uint32_t irnHeader::GetSerializedSize(void)  const
	{
		return sizeof(m_pg) + sizeof(m_seq) + sizeof(m_port)+ +sizeof(m_seq_NACK);
	}
	void irnHeader::Serialize(Buffer::Iterator start)  const
	{
		Buffer::Iterator i = start;
		i.WriteU16(m_pg);
		i.WriteU32(m_seq);
		i.WriteU16(m_port);
		i.WriteU32(m_seq_NACK);
	}

	uint32_t irnHeader::Deserialize(Buffer::Iterator start)
	{
		Buffer::Iterator i = start;
		m_pg = i.ReadU16();
		m_seq = i.ReadU32();
		m_port = i.ReadU16();
		m_seq_NACK = i.ReadU32();
		return GetSerializedSize();
	}


}; // namespace ns3
