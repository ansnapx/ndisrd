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

#ifndef __PROC_H__
#define __PROC_H__

#define INT_BUF_POOL_SIZE 500

extern HANDLE			g_LineupEvent;
extern HANDLE			g_AdapterEvent;

typedef struct _INTERMEDIATE_BUFFER
{
	LIST_ENTRY			m_qLink;
	ULONG				m_Length;
	ULONG				m_Flags;
	ULONG				m_dwDeviceFlags;
	BYTE				m_FlowBits;
	BYTE				m_Direction;
	// Protocol field
	UCHAR				m_IpProto; // Specifies one of IP protocols

	// Packet fields pointers
	ether_header_ptr	m_pEthernet;
	ether_arp_ptr		m_pArp;
	iphdr_ptr			m_pIp;
	tcphdr_ptr			m_pTcp;
	udphdr_ptr			m_pUdp;
	icmphdr_ptr         m_pIcmp;
	
	UCHAR				m_IBuffer [MAX_ETHER_SIZE];
	
} INTERMEDIATE_BUFFER, *PINTERMEDIATE_BUFFER;
// structure for reading packets
typedef
struct _NDISRD_ETH_Packet
{
	PINTERMEDIATE_BUFFER		Buffer;
}
NDISRD_ETH_Packet, *PNDISRD_ETH_Packet;

typedef
struct _ETH_REQUEST
{
	HANDLE				hAdapterHandle;
	NDISRD_ETH_Packet	EthPacket;
}	
ETH_REQUEST, *PETH_REQUEST;

// Structure for setting adapter mode
typedef
struct _ADAPTER_MODE
{
	HANDLE			hAdapterHandle;
	unsigned long	dwFlags;
}
ADAPTER_MODE, *PADAPTER_MODE;

typedef
struct _ADAPTER_EVENT
{
	HANDLE			hAdapterHandle;
	HANDLE			hEvent;

}ADAPTER_EVENT, *PADAPTER_EVENT;

typedef
struct _PACKET_OID_DATA 
{
	HANDLE			hAdapterHandle;
    ULONG			Oid;
    ULONG			Length;
    UCHAR			Data[1];

}PACKET_OID_DATA, *PPACKET_OID_DATA; 

//NTSTATUS				IB_AllocateIntermediateBufferPool ();
//VOID					IB_DeallocateIntermediateBufferPool ();
//
//PFLT_PACKET	IB_AllocateIntermediateBuffer ();

VOID FreeNdisData( PFLT_PACKET pClonePacket );

//
//VOID					IB_FreeIntermediateBuffer (
//							PINTERMEDIATE_BUFFER pBuffer
//							);
//
//VOID					FLT_FilterReceivedPacket ( 
//							NDIS_HANDLE NdisBindingHandle,
//							PFLT_PACKET pBuffer 
//							);
//
//VOID					FLT_FilterSendPacket ( 
//							NDIS_HANDLE NdisBindingHandle,
//							PFLT_PACKET pBuffer 
//							);

#endif // __PROCESSING_H__