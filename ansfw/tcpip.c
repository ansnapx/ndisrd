/*
 * Copyright 2001-2021 ansnap@sina.com 
 *
 * This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "precomp.h"

#if (NTDDI_VERSION < NTDDI_LONGHORN)
#include "..\ansfltr\nt\extens.h"
#include "..\ansfltr\nt\ndisrd.h" // we need PADAPT. 
#endif

//
// Declaration
//

static VOID ruleValidateIP(PFW_REQUEST pFwPacket);
static VOID ruleValidateUDP( PFW_REQUEST pFwPacket );
static VOID ruleValidateTCP( PFW_REQUEST pFwPacket );
static VOID ruleValidateICMP( PFW_REQUEST pFwPacket );
static VOID ruleValidateArp( PFW_REQUEST pFwPacket );
static BOOLEAN UF_ValidTCPFlags(USHORT TcpFlags);


VOID InitPacketHeaders( PFW_REQUEST pFwPacket ) 
{ 
	//UINT isRouted = 0; 
 //
	//PINTERMEDIATE_BUFFER  pBuffer = (PINTERMEDIATE_BUFFER)rule->m_pBuffer; 
 
	if((pFwPacket->m_pFltPacket->m_BufferSize < sizeof(ether_header)) || (pFwPacket->m_pFltPacket->m_BufferSize > MAX_ETHER_SIZE)) 
	{ 
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_NOT_ETHERNET; 
		return; 
	} 
 
	pFwPacket->m_pEthernet = (ether_header_ptr)pFwPacket->m_pFltPacket->m_pBuffer; 
	
	//NdisMoveMemory(pFwPacket->m_SourceMAC,
	//			  pFwPacket->m_pEthernet->h_source,
	//			  6);

	//NdisMoveMemory(pFwPacket->m_DestMAC,
	//			  pFwPacket->m_pEthernet->h_dest,
	//			  6);

//#if (NTDDI_VERSION < NTDDI_LONGHORN)
//	if ( ((PADAPT)pFwPacket->m_pFltPacket->m_pAdapter) -> m_Medium == NdisMediumWan) 
//	{ 
//		ETH_COMPARE_NETWORK_ADDRESSES( 
//			pFwPacket->m_pFltPacket->m_Direction == PHYSICAL_DIRECTION_IN ? pFwPacket->m_pEthernet->h_dest : pFwPacket->m_pEthernet->h_source, 
//			((PADAPT)pFwPacket->m_pFltPacket->m_pAdapter) -> m_LocalAddress, 
//			&isRouted 
//			); 
//	} 
//	else 
//	{ 
//		// 802.3 
//		ETH_COMPARE_NETWORK_ADDRESSES( 
//			pFwPacket->m_pFltPacket->m_Direction == PHYSICAL_DIRECTION_IN ? pFwPacket->m_pEthernet->h_dest : pFwPacket->m_pEthernet->h_source, 
//			((PADAPT)pFwPacket->m_pFltPacket->m_pAdapter) -> m_CurrentAddress, 
//			&isRouted 
//			); 
//	} 
//#else
//	ETH_COMPARE_NETWORK_ADDRESSES( 
//		pFwPacket->m_pFltPacket->m_Direction == PHYSICAL_DIRECTION_IN ? pFwPacket->m_pEthernet->h_dest : pFwPacket->m_pEthernet->h_source, 
//		pFwPacket->m_pFltPacket->m_pAdapter->m_AttachParameters.CurrentMacAddress, 
//		&isRouted 
//		); 
//#endif // (NTDDI_VERSION < NTDDI_LONGHORN)
//
//	if(isRouted) 
//		pFwPacket->m_ReqFlags |= FL_REQUEST_ROUTED; 
 
	if ( pFwPacket->m_pEthernet->h_proto == NETH_P_IP) 
	{ 
		ruleValidateIP(pFwPacket); 

		if(pFwPacket->m_ReqFlags & FL_REQUEST_STOP_PROCESSING) 
			return; 

		if(pFwPacket->m_pFltPacket->m_Direction == PHYSICAL_DIRECTION_IN )
		{		
			if( ! IsLocalAddress(pFwPacket->m_pIp->ip_dst.S_un.S_addr) )
				pFwPacket->m_ReqFlags |= FL_REQUEST_ROUTED; 
		}

//#if (NTDDI_VERSION < NTDDI_LONGHORN)
//		if(/*(isRouted == 0) &&*/ (pFwPacket->m_pFltPacket->m_Direction == PHYSICAL_DIRECTION_IN) && (((PADAPT)pFwPacket->m_pFltPacket->m_pAdapter)->m_LastIndicatedIP!=0))
//		{			
//			if(((PADAPT)pFwPacket->m_pFltPacket->m_pAdapter)->m_LastIndicatedIP != pFwPacket->m_pIp->ip_dst.S_un.S_addr )
//				pFwPacket->m_ReqFlags |= FL_REQUEST_ROUTED; 
//		}
//#else // (NTDDI_VERSION < NTDDI_LONGHORN)
//		if(/*(isRouted == 0) &&*/ (pFwPacket->m_pFltPacket->m_Direction == PHYSICAL_DIRECTION_IN) && (pFwPacket->m_pFltPacket->m_pAdapter->m_LastIndicatedIP!=0) )
//		{			
//			if(pFwPacket->m_pFltPacket->m_pAdapter->m_LastIndicatedIP != pFwPacket->m_pIp->ip_dst.S_un.S_addr )
//				pFwPacket->m_ReqFlags |= FL_REQUEST_ROUTED; 
//		}
//#endif  // (NTDDI_VERSION < NTDDI_LONGHORN)



		switch(pFwPacket->m_pIp->ip_p)
		{ 
			case IPPROTO_UDP: 
			{ 
				ruleValidateUDP(pFwPacket); 
				break; 
			} 
 
			case IPPROTO_TCP: 
			{ 
				ruleValidateTCP(pFwPacket); 
				break; 
			} 
 
			case IPPROTO_ICMP: 
			{ 
				ruleValidateICMP(pFwPacket); 
				break; 
			} 
		}			 
	} 
	else if ( pFwPacket->m_pEthernet->h_proto == NETH_P_ARP)
	{
		ruleValidateArp(pFwPacket);
	}
	else
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_NOT_ETHERNET; 
} 

VOID ruleValidateIP(PFW_REQUEST pFwPacket) 
{ 
	DWORD nIpLen; 
	DWORD nIpHdrLen = 0; 
	//WORD nOldCrc = 0; 
	//PINTERMEDIATE_BUFFER  pBuffer = (PINTERMEDIATE_BUFFER)rule->m_pBuffer; 
	//PADAPTER_ENTRY pAdapter = (PADAPTER_ENTRY)rule->m_pAdapter;
 
	nIpLen = pFwPacket->m_pFltPacket->m_BufferSize; 
	 
	if(nIpLen < (sizeof(iphdr)+(DWORD)MHdrSize)) 
	{ 
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_MALFORMED_IP; 
		return; 
	} 
 
	pFwPacket->m_pIp = (iphdr_ptr)(((PUCHAR)pFwPacket->m_pFltPacket->m_pBuffer) + MHdrSize); 
 
	//if(nIpLen < ntohs(pFwPacket->m_pIp->ip_len)) 
	//{ 
	//	pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_MALFORMED_IP; 
	//	return; 
	//} 

	if((nIpLen < (ntohs(pFwPacket->m_pIp->ip_len) + (DWORD)MHdrSize))) 
	{ 
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_MALFORMED_IP; 
		return; 
	} 

	nIpHdrLen = pFwPacket->m_pIp->ip_hl*4; 
 
	nIpLen = ntohs(pFwPacket->m_pIp->ip_len); 
 
	if(nIpHdrLen < sizeof(iphdr)) 
	{ 
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_MALFORMED_IP; 
		return; 
	} 
 
	if(pFwPacket->m_pIp->ip_p >= IPPROTO_MAX) 
	{ 
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_MALFORMED_IP; 
		return; 
	} 
	 
	pFwPacket->m_IpProto = pFwPacket->m_pIp->ip_p; 
 
	//if((pFwPacket->m_pFltPacket->m_Direction == PHYSICAL_DIRECTION_OUT) && (g_fwPacketAnalysisMode & FL_IDS_DO_XSUM_OUT)) 
	//{ 
	//	if(UF_in_cksum((PBYTE)pBuffer->m_pIp,pBuffer->m_pIp->ip_hl*4)) 
	//	{ 
	//		rule->m_Flags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_XSUM_IP; 
	//		return; 
	//	} 
	//} 
 
	//if((rule->m_Direction == PHYSICAL_DIRECTION_IN) && (g_fwPacketAnalysisMode & FL_IDS_DO_XSUM_IN)) 
	//{ 
	//	if(UF_in_cksum((PBYTE)pBuffer->m_pIp,pBuffer->m_pIp->ip_hl*4)) 
	//	{ 
	//		rule->m_Flags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_XSUM_IP; 
	//		return; 
	//	} 
	//} 
 
	pFwPacket->m_SourceAddr = pFwPacket->m_pIp->ip_src.S_un.S_addr; 
	pFwPacket->m_DestAddr = pFwPacket->m_pIp->ip_dst.S_un.S_addr; 
 
	if(ntohs(pFwPacket->m_pIp->ip_off) & IP_OFFMASK) 
	{ 
		pFwPacket->m_ReqFlags |= /*FL_REQUEST_STOP_PROCESSING |*/ FL_REQUEST_FRAGMENTED_IP;
 		return; 
	} 
} 

VOID ruleValidateUDP( PFW_REQUEST pFwPacket ) 
{ 
	WORD nUdpLen = 0; 
	///PINTERMEDIATE_BUFFER  pBuffer = (PINTERMEDIATE_BUFFER)rule->m_pBuffer; 
 
	nUdpLen = ntohs(pFwPacket->m_pIp->ip_len) - (pFwPacket->m_pIp->ip_hl*4); 
 
	if(nUdpLen < sizeof(udphdr)) 
	{ 
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_MALFORMED_UDP; 
		return; 
	} 
	 
	pFwPacket->m_pUdp = (udphdr_ptr)(((PUCHAR)pFwPacket->m_pIp) + pFwPacket->m_pIp->ip_hl*4); 
	 
	if((nUdpLen < ntohs(pFwPacket->m_pUdp->length)) || (ntohs(pFwPacket->m_pUdp->length) < sizeof(udphdr))) 
	{ 
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_MALFORMED_UDP; 
		return; 
	} 
 
	//if((rule->m_Direction == PHYSICAL_DIRECTION_OUT) && (g_fwPacketAnalysisMode & FL_IDS_DO_XSUM_OUT)) 
	//{ 
	//	if(UF_CalcTUDCPChecksum(IPPROTO_UDP, 
	//						(PUSHORT)pBuffer->m_pUdp, 
	//						(PUSHORT)&pBuffer->m_pIp->ip_src.S_un.S_addr, 
	//						(PUSHORT)&pBuffer->m_pIp->ip_dst.S_un.S_addr, 
	//						ntohs(pBuffer->m_pUdp->length))) 
	//	{ 
	//		rule->m_Flags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_XSUM_UDP; 
	//		return; 
	//	} 
	//} 
	// 
	//if((rule->m_Direction == PHYSICAL_DIRECTION_IN) && (g_fwPacketAnalysisMode & FL_IDS_DO_XSUM_IN)) 
	//{ 
	//	if(UF_CalcTUDCPChecksum(IPPROTO_UDP, 
	//						(PUSHORT)pBuffer->m_pUdp, 
	//						(PUSHORT)&pBuffer->m_pIp->ip_src.S_un.S_addr, 
	//						(PUSHORT)&pBuffer->m_pIp->ip_dst.S_un.S_addr, 
	//						ntohs(pBuffer->m_pUdp->length))) 
	//	{ 
	//		rule->m_Flags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_XSUM_UDP; 
	//		return; 
	//	} 
	//} 
	 
 
	pFwPacket->m_SourcePort = pFwPacket->m_pUdp->th_sport; 
	pFwPacket->m_DestPort = pFwPacket->m_pUdp->th_dport; 
} 

VOID ruleValidateTCP( PFW_REQUEST pFwPacket ) 
{ 
	WORD nTcpLen = 0; 
	WORD TcpFlags = 0; 
	//PINTERMEDIATE_BUFFER  pBuffer = (PINTERMEDIATE_BUFFER)rule->m_pBuffer; 
 
	nTcpLen = ntohs(pFwPacket->m_pIp->ip_len) - (pFwPacket->m_pIp->ip_hl*4); 
 
	if(nTcpLen < sizeof(tcphdr)) 
	{ 
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_MALFORMED_TCP; 
		return; 
	} 
 
	pFwPacket->m_pTcp = (tcphdr_ptr)(((PUCHAR)pFwPacket->m_pIp) + pFwPacket->m_pIp->ip_hl*4); 
 
	if (((pFwPacket->m_pTcp->th_off*4) < sizeof(tcphdr))||  
		 (nTcpLen <(pFwPacket->m_pTcp->th_off*4)) || 
		 ((pFwPacket->m_pTcp->th_off*4) > MAX_TCP_HEADER_SIZE)) 
	{ 
	 
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_MALFORMED_TCP; 
		return; 
	} 
 
	TcpFlags = ((pFwPacket->m_pTcp->th_flags) & ~(TH_ECE|TH_CWR));  
	 
	if(!UF_ValidTCPFlags(TcpFlags)) 
	{ 
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_INVALID_TCP_FLAGS; 
		return; 
	} 
 
	//if((rule->m_Direction == PHYSICAL_DIRECTION_OUT) && (g_fwPacketAnalysisMode & FL_IDS_DO_XSUM_OUT)) 
	//{ 
	//	if(UF_CalcTUDCPChecksum(IPPROTO_TCP, 
	//						(PUSHORT)pBuffer->m_pTcp, 
	//						(PUSHORT)&pBuffer->m_pIp->ip_src.S_un.S_addr, 
	//						(PUSHORT)&pBuffer->m_pIp->ip_dst.S_un.S_addr, 
	//						nTcpLen)) 
	//	{ 
	//		rule->m_Flags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_XSUM_TCP; 
	//		return; 
	//	} 
	//} 
 //
	//if((rule->m_Direction == PHYSICAL_DIRECTION_IN) && (g_fwPacketAnalysisMode & FL_IDS_DO_XSUM_IN)) 
	//{ 
	//	if(UF_CalcTUDCPChecksum(IPPROTO_TCP, 
	//						(PUSHORT)pBuffer->m_pTcp, 
	//						(PUSHORT)&pBuffer->m_pIp->ip_src.S_un.S_addr, 
	//						(PUSHORT)&pBuffer->m_pIp->ip_dst.S_un.S_addr, 
	//						nTcpLen)) 
	//	{ 
	//		rule->m_Flags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_XSUM_TCP; 
	//		return; 
	//	} 
	//} 
 //
	pFwPacket->m_SourcePort = pFwPacket->m_pTcp->th_sport; 
	pFwPacket->m_DestPort = pFwPacket->m_pTcp->th_dport; 
} 

VOID ruleValidateICMP( PFW_REQUEST pFwPacket ) 
{ 
	WORD nIcmpLen = 0; 
	//PINTERMEDIATE_BUFFER  pBuffer = (PINTERMEDIATE_BUFFER)rule->m_pBuffer; 
 
	nIcmpLen = ntohs(pFwPacket->m_pIp->ip_len) - (pFwPacket->m_pIp->ip_hl*4); 
 
	if(nIcmpLen < sizeof(icmphdr)) 
	{ 
		dprintf("ruleValidateICMP len = %d", nIcmpLen);
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_MALFORMED_ICMP; 
		return; 
	} 
 
	pFwPacket->m_pIcmp = (icmphdr_ptr)(((PUCHAR)pFwPacket->m_pIp) + (pFwPacket->m_pIp->ip_hl*4)); 
	 
	if((nIcmpLen > MAX_ICMP_PACKET_SIZE) && (pFwPacket->m_pFltPacket->m_Direction & PHYSICAL_DIRECTION_IN)) 
	{ 
		dprintf("ruleValidateICMP MAX_ICMP_PACKET_SIZE = %d", MAX_ICMP_PACKET_SIZE);
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_MALFORMED_ICMP; 
		return; 
	} 
 
	//if((rule->m_Direction == PHYSICAL_DIRECTION_OUT) && (g_fwPacketAnalysisMode & FL_IDS_DO_XSUM_OUT)) 
	//{ 
	//	if(UF_in_cksum((PBYTE)pBuffer->m_pIcmp,nIcmpLen)) 
	//	{ 
	//		rule->m_Flags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_XSUM_ICMP; 
	//		return; 
	//	}	 
	//} 
 //
	//if((rule->m_Direction == PHYSICAL_DIRECTION_IN) && (g_fwPacketAnalysisMode & FL_IDS_DO_XSUM_IN)) 
	//{ 
	//	if(UF_in_cksum((PBYTE)pBuffer->m_pIcmp,nIcmpLen)) 
	//	{ 
	//		rule->m_Flags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_XSUM_ICMP; 
	//		return; 
	//	}	 
	//} 
 //
	pFwPacket->m_Type = pFwPacket->m_pIcmp->icmp_type; 
	pFwPacket->m_Code = pFwPacket->m_pIcmp->icmp_code; 

	//dprintf("ruleValidateICMP Valid");

} 

VOID ruleValidateArp( PFW_REQUEST pFwPacket )
{
	DWORD nArpLen = 0;
	//PINTERMEDIATE_BUFFER  pBuffer = (PINTERMEDIATE_BUFFER)rule->m_pBuffer; 
	
	nArpLen = pFwPacket->m_pFltPacket->m_BufferSize;

	if(nArpLen < sizeof(arphdr) + MHdrSize) 
	{ 
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_MALFORMED_IP; 
		return; 
	} 
 
	if(nArpLen < sizeof(ether_arp) + MHdrSize) 
	{
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_NOT_ETHERNET; 
		return;
	}

	pFwPacket->m_pArp = (ether_arp_ptr)(((PUCHAR)pFwPacket->m_pFltPacket->m_pBuffer) + MHdrSize); 

	if ((pFwPacket->m_pArp->arp_hrd != htons(ARPHRD_ETHER))&& 
		(pFwPacket->m_pArp->arp_hrd != htons(ARPHRD_IEEE802)))
	{
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_NOT_ETHERNET; 
		return;
	}

	if ((pFwPacket->m_pArp->arp_pro != NETH_P_IP)&&
		(pFwPacket->m_pArp->arp_pro != NETH_P_TRAIL)) 
	{
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_NOT_ETHERNET; 
		return;
	}

	if ((pFwPacket->m_pArp->arp_op != htons(ARPOP_REQUEST))&&
	    (pFwPacket->m_pArp->arp_op != htons(ARPOP_REPLY)))
	{
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_NOT_ETHERNET; 
		return;
	}

	if ((pFwPacket->m_pArp->arp_hln != 6) || (pFwPacket->m_pArp->arp_pln != 4))
	{
		pFwPacket->m_ReqFlags |= FL_REQUEST_STOP_PROCESSING | FL_REQUEST_NOT_ETHERNET; 
		return;
	}

	pFwPacket->m_ReqFlags |= FL_REQUEST_ARP;
}

BOOLEAN UF_ValidTCPFlags(USHORT TcpFlags)
{
	return ((TcpFlags == (TH_SYN)) ||
	(TcpFlags == (TH_SYN|TH_ACK)) ||
	(TcpFlags == (TH_SYN|TH_ACK|TH_PUSH)) ||
	(TcpFlags == (TH_RST)) ||
	(TcpFlags == (TH_RST|TH_ACK)) ||
	(TcpFlags == (TH_RST|TH_ACK|TH_PUSH)) ||
	(TcpFlags == (TH_FIN|TH_ACK)) ||
	(TcpFlags == (TH_ACK)) ||
	(TcpFlags == (TH_ACK|TH_PUSH)) ||
	(TcpFlags == (TH_ACK|TH_URG|TH_PUSH)) ||
	(TcpFlags == (TH_ACK|TH_URG)) ||
	(TcpFlags == (TH_FIN|TH_ACK|TH_PUSH)) ||
	(TcpFlags == (TH_FIN|TH_ACK|TH_URG)) ||
	(TcpFlags == (TH_FIN|TH_ACK|TH_URG|TH_PUSH)));
}


//***********************************************************************************
// Name: htons
//
// Description: Function converts a USHORT from host to TCP/IP network byte order
// (which is big-endian). 
//
// Return value: value in TCP/IP network byte order
//
// Parameters: 
// hostshort	- 16-bit number in host byte order
//
// NOTE: None
// **********************************************************************************

USHORT htons( USHORT hostshort )
{
	PUCHAR	pBuffer;
	USHORT	nResult;

	nResult = 0;
	pBuffer = (PUCHAR )&hostshort;

	nResult = ( (pBuffer[ 0 ] << 8) & 0xFF00 )
		| ( pBuffer[ 1 ] & 0x00FF );

	return( nResult );
}

//***********************************************************************************
// Name: ntohs
//
// Description: Function converts a USHORT from TCP/IP network byte order to host
// (which is little-endian). 
//
// Return value: value in host byte order
//
// Parameters: 
// netshort	- 16-bit number in TCP/IP network byte order
//
// NOTE: None
// **********************************************************************************

USHORT ntohs( USHORT netshort )
{
	PUCHAR	pBuffer;
	USHORT	nResult;

	nResult = 0;
	pBuffer = (PUCHAR )&netshort;

	nResult = ( (pBuffer[ 0 ] << 8) & 0xFF00 )
		| ( pBuffer[ 1 ] & 0x00FF );

	return( nResult );
}

//***********************************************************************************
// Name: htonl
//
// Description: Function converts a ULONG from host to TCP/IP network byte order
// (which is big-endian). 
//
// Return value: value in TCP/IP network byte order
//
// Parameters: 
// hostlong	- 32-bit number in host byte order
//
// NOTE: None
// **********************************************************************************

ULONG htonl( ULONG hostlong )
{
	ULONG    nResult = hostlong >> 16;
	USHORT	upper = (USHORT) nResult & 0x0000FFFF;
	USHORT	lower = (USHORT) hostlong & 0x0000FFFF;

	upper = htons( upper );
	lower = htons( lower );

    nResult = 0x10000 * lower + upper;
	return( nResult );
}

//***********************************************************************************
// Name: ntohl
//
// Description: Function converts a ULONG from TCP/IP network byte order to host
// (which is little-endian). 
//
// Return value: value in host byte order
//
// Parameters: 
// netlong	- 32-bit number in TCP/IP network byte order
//
// NOTE: None
// **********************************************************************************

ULONG ntohl( ULONG netlong )
{
	ULONG    nResult = netlong >> 16;
	USHORT   upper = (USHORT) nResult & 0x0000FFFF;
	USHORT	lower = (USHORT) netlong & 0x0000FFFF;

	upper = htons( upper );
	lower = htons( lower );

    nResult = 0x10000 * lower + upper;
	return( nResult );
}

BOOLEAN isLoopback(DWORD ip)
{
	return ((((DWORD)(ip) & 0x000000ff) == 0x0000007f));
}