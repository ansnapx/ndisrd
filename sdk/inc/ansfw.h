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

#pragma once

#include "ansfltr.h"
#include "iphlp.h"

enum FW_REQUEST_FLAGS 
{ 
	FL_REQUEST_STOP_PROCESSING = 0x00000001,
	FL_REQUEST_NOT_ETHERNET = 0x00000002,
	FL_REQUEST_MALFORMED_IP = 0x00000004,
	FL_REQUEST_XSUM_IP = 0x00000008,
	FL_REQUEST_FRAGMENTED_IP = 0x00000010,
	FL_REQUEST_MALFORMED_UDP = 0x00000020,
	FL_REQUEST_XSUM_UDP = 0x00000040,
	FL_REQUEST_MALFORMED_TCP = 0x00000080,
	FL_REQUEST_XSUM_TCP = 0x00000100,
	FL_REQUEST_INVALID_TCP_FLAGS = 0x00000200,
	FL_REQUEST_MALFORMED_ICMP = 0x00000400,
	FL_REQUEST_XSUM_ICMP = 0x00000800,
	FL_REQUEST_ASSURED = 0x00001000,        
	FL_REQUEST_UNKNOWN_NAME = 0x00002000,    
	FL_REQUEST_SENT_TO_APP = 0x00004000,
	FL_REQUEST_SAFE_APP = 0x00010000,
	FL_REQUEST_ALLOW = 0x00040000,
	FL_REQUEST_BLOCK = 0x00080000,
	FL_REQUEST_LOG = 0x00100000,
	FL_REQUEST_ASK = 0x00200000,
	FL_REQUEST_TDI_DOWN = 0x00800000,	
	FL_REQUEST_LEARN = 0x01000000,
	FL_REQUEST_ACE_MATCH = 0x02000000,
	FL_REQUEST_ROUTED = 0x04000000,
	FL_REQUEST_ARP = 0x08000000,
	FL_REQUEST_LOOPBACK = 0x10000000,
	FL_REQUEST_LOG_WHEN_ALLOWED = 0x20000000,
	FL_REQUEST_REASSEMBLED = 0x40000000
};

typedef struct _FW_REQUEST {

	LIST_ENTRY	m_qLink;
	LONG		m_References;

	PFLT_PACKET	m_pFltPacket;
	UINT64		m_Pid;
	DWORD		m_ReqFlags;
	
	// Protocol fields
	BYTE		m_IpProto;
	DWORD		m_SourceAddr;
	DWORD		m_DestAddr;
	union
	{
		WORD		m_SourcePort;
		BYTE		m_Type;
	};
	union 
	{
		WORD		m_DestPort;
		BYTE		m_Code;
	};

	// Packet fields pointers
	ether_header_ptr	m_pEthernet;
	ether_arp_ptr		m_pArp;
	iphdr_ptr			m_pIp;
	tcphdr_ptr			m_pTcp;
	udphdr_ptr			m_pUdp;
	icmphdr_ptr         m_pIcmp;

} FW_REQUEST, *PFW_REQUEST;

typedef 
NTSTATUS (*FW_REQUEST_CALLBACK)(
	IN PFW_REQUEST Request
	);

typedef struct _FW_INSTANCE {
	LIST_ENTRY			m_qLink;
	FW_REQUEST_CALLBACK	m_pFwRequestCallback;
} FW_INSTANCE, *PFW_INSTANCE;

NTSTATUS FWRegisterRequestCallback(
	OUT PFW_INSTANCE * Instance,
	IN  FW_REQUEST_CALLBACK Callback,
	IN  DWORD Flags
	);

NTSTATUS FWDeregisterRequestCallback(
	IN PFW_INSTANCE Instance
	);

NTSTATUS FWInjectRequest(
	IN PFW_INSTANCE  Instance OPTIONAL,
	IN PFW_REQUEST Request
	);

NTSTATUS FWCloneRequest(
	IN PFW_REQUEST Request,
	OUT PFW_REQUEST *ClonedRequest
	);

VOID FWFreeCloneRequest(
	IN PFW_REQUEST ClonedRequest
	);