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

typedef struct _CONNECTION 
{
	LIST_ENTRY	  m_qLink;

	UINT64		  m_Pid;
	DWORD		  m_LocalAddr;
	WORD		  m_LocalPort;
	DWORD		  m_RemoteAddr;
	WORD		  m_RemotePort;
	WORD		  m_IpProto;

}CONNECTION, *PCONNECTION;

static KSPIN_LOCK g_ConnectionsLock;
static LIST_ENTRY g_ConnectionsList;
static NPAGED_LOOKASIDE_LIST g_LookasideList;

//
// Functions definitons
//

VOID InitializeConnections( )
{
	KeInitializeSpinLock( &g_ConnectionsLock );
	InitializeListHead(&g_ConnectionsList);

	ExInitializeNPagedLookasideList( &g_LookasideList, NULL, NULL, 0, sizeof(CONNECTION), DRIVER_TAG, 0 );
}

VOID UninitializeConnections( )
{
	KIRQL oldIrql;

	KeAcquireSpinLock( &g_ConnectionsLock, &oldIrql );

	while ( !IsListEmpty(&g_ConnectionsList) ) 
	{ 
		PCONNECTION pConnections = CONTAINING_RECORD( RemoveHeadList(&g_ConnectionsList), CONNECTION, m_qLink );
		ExFreeToNPagedLookasideList( &g_LookasideList, pConnections );
	} 

	KeReleaseSpinLock( &g_ConnectionsLock, oldIrql );

	ExDeleteNPagedLookasideList( &g_LookasideList );
}

PVOID AddConnection( UINT64 pid, WORD IpProto, DWORD LocalAddr, WORD LocalPort, DWORD RemoteAddr, WORD RemotePort )
{
	KIRQL oldIrql;
	PLIST_ENTRY pListEntry;
	PCONNECTION pConnection = NULL;

	if( pid == 0 )
	{
		DbgPrint( "Alarma, dude! pid==0 for IpProto=%d, RemoteAddr==%3d.%3d.%3d.%3d", IpProto, PRINT_IP_ADDR(RemoteAddr) );
	}

	KeAcquireSpinLock( &g_ConnectionsLock, &oldIrql );

	// If connection is already exist, then just update it
	pListEntry = g_ConnectionsList.Flink;
	while( pListEntry != &g_ConnectionsList )
	{
		pConnection = CONTAINING_RECORD( pListEntry, CONNECTION, m_qLink );
		pListEntry = pListEntry->Flink;

		if( (pConnection->m_IpProto == IpProto) &&
			(pConnection->m_LocalPort == LocalPort) && 
			(pConnection->m_LocalAddr==0||(pConnection->m_LocalAddr == LocalAddr))	&&		
			(pConnection->m_RemotePort==0) &&
			(pConnection->m_RemoteAddr==0) )
		{	
			dprintf( "Connection: changed Pid = %4d, IpProto = %4d, LocalAddr = %3d.%3d.%3d.%3d, RemoteAddr = %3d.%3d.%3d.%3d, LocalPort = %5d, RemotePort = %5d\n",
				(ULONG)pid, IpProto, PRINT_IP_ADDR(LocalAddr), PRINT_IP_ADDR(RemoteAddr), RtlUshortByteSwap(LocalPort), RtlUshortByteSwap(RemotePort) );

			pConnection->m_RemoteAddr = RemoteAddr;
			pConnection->m_RemotePort = RemotePort;
			pConnection->m_Pid = pid;
			KeReleaseSpinLock( &g_ConnectionsLock, oldIrql );
			return pConnection;
		}
	}

	pConnection = ExAllocateFromNPagedLookasideList( &g_LookasideList );
	if( pConnection )
	{ 		 
		pConnection->m_Pid			= pid;
		pConnection->m_IpProto		= IpProto;
		pConnection->m_LocalAddr	= LocalAddr;
		pConnection->m_LocalPort	= LocalPort;
		pConnection->m_RemoteAddr	= RemoteAddr;
		pConnection->m_RemotePort	= RemotePort;		

		InsertHeadList(&g_ConnectionsList, &pConnection->m_qLink);
	} 

	dprintf( "Connection: added Pid = %4d, IpProto = %4d, LocalAddr = %3d.%3d.%3d.%3d, RemoteAddr = %3d.%3d.%3d.%3d, LocalPort = %5d, RemotePort = %5d\n",
		(ULONG)pid, IpProto, PRINT_IP_ADDR(LocalAddr), PRINT_IP_ADDR(RemoteAddr), RtlUshortByteSwap(LocalPort), RtlUshortByteSwap(RemotePort) );

	KeReleaseSpinLock( &g_ConnectionsLock, oldIrql );

	return pConnection;
}

PVOID GetConnection( WORD IpProto, DWORD LocalAddr, WORD LocalPort, DWORD RemoteAddr, WORD RemotePort )
{
	KIRQL oldIrql;
	PLIST_ENTRY pListEntry;	

	KeAcquireSpinLock( &g_ConnectionsLock, &oldIrql );

	pListEntry = g_ConnectionsList.Flink;
	while( pListEntry != &g_ConnectionsList )
	{
		PCONNECTION pConnection = CONTAINING_RECORD( pListEntry, CONNECTION, m_qLink );
		pListEntry = pListEntry->Flink;

		if( (pConnection->m_IpProto == IpProto) &&
			(pConnection->m_LocalPort == LocalPort) && 
			(pConnection->m_LocalAddr == LocalAddr) &&
			(pConnection->m_RemotePort == RemotePort) &&
			(pConnection->m_RemoteAddr == RemoteAddr) )
		{
			dprintf( "Connection: get Pid = %4d, IpProto = %4d, LocalAddr = %3d.%3d.%3d.%3d, RemoteAddr = %3d.%3d.%3d.%3d, LocalPort = %5d, RemotePort = %5d\n",
				(ULONG)pConnection->m_Pid, IpProto, PRINT_IP_ADDR(LocalAddr), PRINT_IP_ADDR(RemoteAddr), RtlUshortByteSwap(LocalPort), RtlUshortByteSwap(RemotePort) );

			KeReleaseSpinLock( &g_ConnectionsLock, oldIrql );
			return pConnection;
		}
	}

	//pListEntry = g_ConnectionsList.Flink;
	//while( pListEntry != &g_ConnectionsList )
	//{
	//	PCONNECTION pConnection = CONTAINING_RECORD( pListEntry, CONNECTION, m_qLink );
	//	pListEntry = pListEntry->Flink;

	//	if( (pConnection->m_IpProto == IpProto) &&
	//		(pConnection->m_LocalPort == LocalPort) && 
	//		(pConnection->m_LocalAddr == LocalAddr) &&
	//		(pConnection->m_RemotePort==0) &&
	//		(pConnection->m_RemoteAddr==0) )
	//	{
	//		KeReleaseSpinLock( &g_ConnectionsLock, oldIrql );
	//		return pConnection;
	//	}
	//}

	KeReleaseSpinLock( &g_ConnectionsLock, oldIrql );

	return NULL;
}


VOID RemoveConnection( PVOID pRemovingConnection )
{
	KIRQL oldIrql;
	PLIST_ENTRY pListEntry;

	dprintf( "RemoveConnection. pRemovingConnection = %p\n", pRemovingConnection );

	KeAcquireSpinLock( &g_ConnectionsLock, &oldIrql );	

	pListEntry = g_ConnectionsList.Flink;
	while( pListEntry != &g_ConnectionsList )
	{
		PCONNECTION pConnection = CONTAINING_RECORD( pListEntry, CONNECTION, m_qLink );
		pListEntry = pListEntry->Flink;

		if( pConnection == pRemovingConnection )
		{
			RemoveEntryList( &pConnection->m_qLink );
			KeReleaseSpinLock( &g_ConnectionsLock, oldIrql );
			ExFreeToNPagedLookasideList( &g_LookasideList, pConnection );
			return;
		}
	}

	KeReleaseSpinLock( &g_ConnectionsLock, oldIrql );
}

VOID GetPidFromLocalInfo( PFW_PID_REQUEST request )
{
	KIRQL oldIrql;
	PLIST_ENTRY pListEntry;	
	request->m_Pid = 0;

	KeAcquireSpinLock( &g_ConnectionsLock, &oldIrql );

	if((request->m_IpProto == IPPROTO_TCP) || (request->m_IpProto == IPPROTO_UDP))
	{
		pListEntry = g_ConnectionsList.Flink;
		while( pListEntry != &g_ConnectionsList )
		{
			PCONNECTION pConnection = CONTAINING_RECORD( pListEntry, CONNECTION, m_qLink );
			pListEntry = pListEntry->Flink;

			if( (pConnection->m_LocalPort == request->m_LocalPort ) && 
				(pConnection->m_IpProto == request->m_IpProto ) &&
				(pConnection->m_LocalAddr == request->m_LocalAddr) &&
				(pConnection->m_RemotePort == request->m_RemotePort ) &&
				(pConnection->m_RemoteAddr == request->m_RemoteAddr) )
			{
				dprintf( "Connection: getpid(1) Pid = %4d, IpProto = %4d, LocalAddr = %3d.%3d.%3d.%3d, RemoteAddr = %3d.%3d.%3d.%3d, LocalPort = %5d, RemotePort = %5d\n",
					(ULONG)pConnection->m_Pid, pConnection->m_IpProto, PRINT_IP_ADDR(pConnection->m_LocalAddr), PRINT_IP_ADDR(pConnection->m_RemoteAddr), RtlUshortByteSwap(pConnection->m_LocalPort), RtlUshortByteSwap(pConnection->m_RemotePort) );

				request->m_Pid = pConnection->m_Pid;
				KeReleaseSpinLock( &g_ConnectionsLock, oldIrql );
				return;
			}
		}

		pListEntry = g_ConnectionsList.Flink;
		while( pListEntry != &g_ConnectionsList )
		{
			PCONNECTION pConnection = CONTAINING_RECORD( pListEntry, CONNECTION, m_qLink );
			pListEntry = pListEntry->Flink;

			if( (pConnection->m_LocalPort == request->m_LocalPort ) && 
				(pConnection->m_IpProto == request->m_IpProto ) &&
				((pConnection->m_LocalAddr == 0) || (pConnection->m_LocalAddr == request->m_LocalAddr )) &&
				(pConnection->m_RemotePort == request->m_RemotePort) &&
				(pConnection->m_RemoteAddr == request->m_RemoteAddr) )
			{
				dprintf( "Connection: getpid(2) Pid = %4d, IpProto = %4d, LocalAddr = %3d.%3d.%3d.%3d, RemoteAddr = %3d.%3d.%3d.%3d, LocalPort = %5d, RemotePort = %5d\n",
					(ULONG)pConnection->m_Pid, pConnection->m_IpProto, PRINT_IP_ADDR(pConnection->m_LocalAddr), PRINT_IP_ADDR(pConnection->m_RemoteAddr), RtlUshortByteSwap(pConnection->m_LocalPort), RtlUshortByteSwap(pConnection->m_RemotePort) );

				request->m_Pid = pConnection->m_Pid;
				KeReleaseSpinLock( &g_ConnectionsLock, oldIrql );
				return;
			}
		}

		pListEntry = g_ConnectionsList.Flink;
		while( pListEntry != &g_ConnectionsList )
		{
			PCONNECTION pConnection = CONTAINING_RECORD( pListEntry, CONNECTION, m_qLink );
			pListEntry = pListEntry->Flink;

			if( (pConnection->m_LocalPort == request->m_LocalPort ) && 
				(pConnection->m_IpProto == request->m_IpProto ) &&
				(pConnection->m_LocalAddr == request->m_LocalAddr ) &&
				(pConnection->m_RemotePort==0) &&
				(pConnection->m_RemoteAddr==0) )
			{
				dprintf( "Connection: getpid(3) Pid = %4d, IpProto = %4d, LocalAddr = %3d.%3d.%3d.%3d, RemoteAddr = %3d.%3d.%3d.%3d, LocalPort = %5d, RemotePort = %5d\n",
					(ULONG)pConnection->m_Pid, pConnection->m_IpProto, PRINT_IP_ADDR(pConnection->m_LocalAddr), PRINT_IP_ADDR(pConnection->m_RemoteAddr), RtlUshortByteSwap(pConnection->m_LocalPort), RtlUshortByteSwap(pConnection->m_RemotePort) );

				request->m_Pid = pConnection->m_Pid;
				KeReleaseSpinLock( &g_ConnectionsLock, oldIrql );
				return;
			}
		}

		pListEntry = g_ConnectionsList.Flink;
		while( pListEntry != &g_ConnectionsList )
		{
			PCONNECTION pConnection = CONTAINING_RECORD( pListEntry, CONNECTION, m_qLink );
			pListEntry = pListEntry->Flink;

			if( (pConnection->m_LocalPort == request->m_LocalPort ) && 
				(pConnection->m_IpProto == request->m_IpProto ) &&
				((pConnection->m_LocalAddr == 0) || (pConnection->m_LocalAddr == request->m_LocalAddr )) &&
				(pConnection->m_RemotePort==0) &&
				(pConnection->m_RemoteAddr==0) )
			{
				dprintf( "Connection: getpid(4) Pid = %4d, IpProto = %4d, LocalAddr = %3d.%3d.%3d.%3d, RemoteAddr = %3d.%3d.%3d.%3d, LocalPort = %5d, RemotePort = %5d\n",
					(ULONG)pConnection->m_Pid, pConnection->m_IpProto, PRINT_IP_ADDR(pConnection->m_LocalAddr), PRINT_IP_ADDR(pConnection->m_RemoteAddr), RtlUshortByteSwap(pConnection->m_LocalPort), RtlUshortByteSwap(pConnection->m_RemotePort) );

				request->m_Pid = pConnection->m_Pid;
				KeReleaseSpinLock( &g_ConnectionsLock, oldIrql );
				return;
			}
		}
	}
	else
	{
		pListEntry = g_ConnectionsList.Flink;
		while( pListEntry != &g_ConnectionsList )
		{
			PCONNECTION pConnection = CONTAINING_RECORD( pListEntry, CONNECTION, m_qLink );
			pListEntry = pListEntry->Flink;

			if( (pConnection->m_IpProto == request->m_IpProto ) &&
				(pConnection->m_RemoteAddr == request->m_RemoteAddr) )
			{
				request->m_Pid = pConnection->m_Pid;
				KeReleaseSpinLock( &g_ConnectionsLock, oldIrql );
				return;
			}
		}

		pListEntry = g_ConnectionsList.Flink;
		while( pListEntry != &g_ConnectionsList )
		{
			PCONNECTION pConnection = CONTAINING_RECORD( pListEntry, CONNECTION, m_qLink );
			pListEntry = pListEntry->Flink;

			if( (pConnection->m_IpProto == 255 ) &&
				(pConnection->m_RemoteAddr == request->m_RemoteAddr) )
			{
				request->m_Pid = pConnection->m_Pid;
				KeReleaseSpinLock( &g_ConnectionsLock, oldIrql );
				return;
			}
		}
	}

    KeReleaseSpinLock( &g_ConnectionsLock, oldIrql );
}
