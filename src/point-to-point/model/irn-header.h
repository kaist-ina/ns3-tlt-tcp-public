#ifndef IRN_HEADER_H
#define IRN_HEADER_H

#include <stdint.h>
#include "ns3/header.h"
#include "ns3/buffer.h"

namespace ns3 {

	/**
	* \ingroup Pause
	* \brief Header for the Congestion Notification Message
	*
	* This header must be used only if IRN = 1
	*/

	class irnHeader : public Header
	{
	public:

		irnHeader(uint16_t pg);
		irnHeader();
		virtual ~irnHeader();

		//Setters
		/**
		* \param pg The PG
		*/
		void SetPG(uint16_t pg);
		void SetSeq(uint32_t seq);
		void SetPort(uint16_t port);
		void SetSeqNack(uint32_t seq);

		//Getters
		/**
		* \return The pg
		*/
		uint16_t GetPG() const;
		uint32_t GetSeq() const;
		uint16_t GetPort() const;
		uint16_t GetSeqNACK() const;

		static TypeId GetTypeId(void);
		virtual TypeId GetInstanceTypeId(void) const;
		virtual void Print(std::ostream &os) const;
		virtual uint32_t GetSerializedSize(void) const;
		virtual void Serialize(Buffer::Iterator start) const;
		virtual uint32_t Deserialize(Buffer::Iterator start);

	private:
		uint16_t m_pg;
		uint32_t m_seq; // the qbb sequence number.
		uint16_t m_port; //udp port to identify flows...
		uint32_t m_seq_NACK; //seq no of packet that triggered NACK

		// send expected seq, send currentSeq which triggered NACK
		// =========++++++++++++++++++=
		//	        |                 |
		//        m_seq           m_seq_NACK
	};

}; // namespace ns3

#endif /* IRN_HEADER_H */
