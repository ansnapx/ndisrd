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

//
// Macroses
//

#define PRINT_IP_ADDR(addr) \
	((UCHAR *)&(addr))[0], ((UCHAR *)&(addr))[1], ((UCHAR *)&(addr))[2], ((UCHAR *)&(addr))[3]


//
// Global variables
// 

static PFW_INSTANCE g_pFwInstance;
static BOOL g_bTerminateThread;
static KEVENT g_Event;

static LIST_ENTRY g_FwRequestList;
static KSPIN_LOCK g_FwRequestLock;

static PVOID g_pThread;

//
// Declarations
//

NTSTATUS RequestCallback( IN PFW_REQUEST pRequest );
VOID ProcessRequestThread( PVOID pContext );
void PrintRequest( const PFW_REQUEST pRequest );

//
// Definitions
//

NTSTATUS RegisterCallback()
{
	HANDLE hThread;
	NTSTATUS Status;

	InitializeListHead( &g_FwRequestList );
	KeInitializeSpinLock( &g_FwRequestLock );

	Status = FWRegisterRequestCallback( &g_pFwInstance, RequestCallback, 0 );
	if( ! NT_SUCCESS(Status) )
	{
		return Status;
	}
	
	KeInitializeEvent( &g_Event, SynchronizationEvent, FALSE );
	PsCreateSystemThread( &hThread, 0, NULL, NULL, NULL, ProcessRequestThread, NULL );
	ObReferenceObjectByHandle( hThread, THREAD_ALL_ACCESS, NULL, KernelMode, &g_pThread, NULL );
	ZwClose( hThread );
	
	return Status;
}

VOID DeregisterCallback()
{
	g_bTerminateThread = TRUE;
	KeSetEvent( &g_Event, IO_NO_INCREMENT, FALSE );
	KeWaitForSingleObject( g_pThread, Executive, KernelMode, FALSE, NULL );

	FWDeregisterRequestCallback( g_pFwInstance );
}

NTSTATUS RequestCallback(
	IN PFW_REQUEST pRequest
	)
{
	KIRQL OldIrql;

#ifdef DECISION_ALLOW

	return STATUS_SUCCESS;

#elif DECISION_BLOCK

	return STATUS_UNSUCCESSFUL;

#else // more processing required

	PrintRequest( pRequest );

	FWCloneRequest( pRequest, &pRequest );

	// We may use internal LIST_ENTRY
	KeAcquireSpinLock( &g_FwRequestLock, &OldIrql );
	InsertTailList( &g_FwRequestList, &pRequest->m_qLink );
	KeReleaseSpinLock( &g_FwRequestLock, OldIrql );

	KeSetEvent( &g_Event, IO_NO_INCREMENT, FALSE );

	return STATUS_UNSUCCESSFUL;	

#endif
}

VOID ProcessRequestThread( PVOID pContext )
{
	KIRQL OldIrql;

	while( ! g_bTerminateThread )
	{	
		KeWaitForSingleObject( &g_Event, Executive, KernelMode, FALSE, NULL );

		KeAcquireSpinLock( &g_FwRequestLock, &OldIrql );
		while( ! IsListEmpty(&g_FwRequestList) )
		{
			PFW_REQUEST pFwRequest = CONTAINING_RECORD( RemoveHeadList( &g_FwRequestList ), FW_REQUEST, m_qLink );
			KeReleaseSpinLock( &g_FwRequestLock, OldIrql );

			FWInjectRequest( g_pFwInstance, pFwRequest );
			FWFreeCloneRequest( pFwRequest );

			KeAcquireSpinLock( &g_FwRequestLock, &OldIrql );
		}
		KeReleaseSpinLock( &g_FwRequestLock, OldIrql );
	}

	PsTerminateSystemThread(0);
}

void PrintRequest( const PFW_REQUEST pRequest )
{
	char *lpszIpProto; 
	char *lpszDirection;

	switch( pRequest->m_IpProto )
	{	
	case 0:
		lpszIpProto = "IP";
		break;
	case 1:
		lpszIpProto = "ICMP";
		break;
	case 2:
		lpszIpProto = "IGMP";
		break;
	case 3:
		lpszIpProto = "GGP";
		break;
	case 6:
		lpszIpProto = "TCP";
		break;
	case 12:
		lpszIpProto = "PUP";
		break;
	case 17:
		lpszIpProto = "UDP";
		break;
	case 22:
		lpszIpProto = "IDP";
		break;
	case 77:
		lpszIpProto = "ND";
		break;
	case 255:
		lpszIpProto = "RAW";
		break;
	default:
		lpszIpProto = "Unknown";
	}

	switch( pRequest->m_pFltPacket->m_Direction )
	{	
	case 1:
		lpszDirection = "OUT";
		break;
	case 2:
		lpszDirection = "IN";
		break;
	default:
		lpszDirection = "Unknown";
	}

	if( pRequest->m_IpProto==1 )
	{
		DbgPrint( "[" DRIVER_NAME "] Pid = %4d, IpProto = %4s, SourceAddr = %3d.%3d.%3d.%3d, DestAddr = %3d.%3d.%3d.%3d, Type = %5d, Code = %5d, Direction = %3s, ReqFlags =%s%s%s%s%s%s%s%s%s%s%s, pBuffer = %p, BufferSize = %d\n",
			(ULONG)pRequest->m_Pid,
			lpszIpProto, 
			PRINT_IP_ADDR(pRequest->m_SourceAddr), PRINT_IP_ADDR(pRequest->m_DestAddr), 		
			pRequest->m_Type, pRequest->m_Code,		
			lpszDirection, 			
			(pRequest->m_ReqFlags&FL_REQUEST_STOP_PROCESSING?" FL_REQUEST_STOP_PROCESSING":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_NOT_ETHERNET?" FL_REQUEST_NOT_ETHERNET":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_MALFORMED_IP?" FL_REQUEST_MALFORMED_IP":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_FRAGMENTED_IP?" FL_REQUEST_FRAGMENTED_IP":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_MALFORMED_UDP?" FL_REQUEST_MALFORMED_UDP":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_MALFORMED_TCP?" FL_REQUEST_MALFORMED_TCP":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_INVALID_TCP_FLAGS?" FL_REQUEST_INVALID_TCP_FLAGS":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_MALFORMED_ICMP?" FL_REQUEST_MALFORMED_ICMP":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_ROUTED?" FL_REQUEST_ROUTED":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_ARP?" FL_REQUEST_ARP":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_LOOPBACK?" FL_REQUEST_LOOPBACK":""),
			pRequest->m_pFltPacket->m_pBuffer, pRequest->m_pFltPacket->m_BufferSize );
	}
	else
	{
		DbgPrint( "[" DRIVER_NAME "] Pid = %4d, IpProto = %4s, SourceAddr = %3d.%3d.%3d.%3d, DestAddr = %3d.%3d.%3d.%3d, SourcePort = %5d, DestPort = %5d, Direction = %3s, ReqFlags =%s%s%s%s%s%s%s%s%s%s%s, pBuffer = %p, BufferSize = %d\n",
			(ULONG)pRequest->m_Pid,
			lpszIpProto, 
			PRINT_IP_ADDR(pRequest->m_SourceAddr), PRINT_IP_ADDR(pRequest->m_DestAddr), 		
			RtlUshortByteSwap(pRequest->m_SourcePort), RtlUshortByteSwap(pRequest->m_DestPort), 			
			lpszDirection, 			
			(pRequest->m_ReqFlags&FL_REQUEST_STOP_PROCESSING?" FL_REQUEST_STOP_PROCESSING":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_NOT_ETHERNET?" FL_REQUEST_NOT_ETHERNET":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_MALFORMED_IP?" FL_REQUEST_MALFORMED_IP":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_FRAGMENTED_IP?" FL_REQUEST_FRAGMENTED_IP":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_MALFORMED_UDP?" FL_REQUEST_MALFORMED_UDP":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_MALFORMED_TCP?" FL_REQUEST_MALFORMED_TCP":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_INVALID_TCP_FLAGS?" FL_REQUEST_INVALID_TCP_FLAGS":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_MALFORMED_ICMP?" FL_REQUEST_MALFORMED_ICMP":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_ROUTED?" FL_REQUEST_ROUTED":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_ARP?" FL_REQUEST_ARP":""), 
			(pRequest->m_ReqFlags&FL_REQUEST_LOOPBACK?" FL_REQUEST_LOOPBACK":""),
			pRequest->m_pFltPacket->m_pBuffer, pRequest->m_pFltPacket->m_BufferSize );
	}
}