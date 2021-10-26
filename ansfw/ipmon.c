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
#include <tdikrnl.h>

typedef struct _NET_ADDRESS 
{
	LIST_ENTRY	m_qLink;
	DWORD		m_dwAddr;
} NET_ADDRESS, *PNET_ADDRESS;

static HANDLE g_TdiPNPHandler;
static LIST_ENTRY g_NetAddressesList;
static KSPIN_LOCK g_NetAddressesLock;
static NPAGED_LOOKASIDE_LIST g_NetAddressesLookasideList;

BOOL IsLocalAddress( DWORD dwAddr )
{
	BOOL bResult;
	KIRQL OldIrql;
	PLIST_ENTRY pListEntry;

	// Loopback address
	bResult = ((((DWORD)(dwAddr) & 0x000000ff) == 0x0000007f));
	if( bResult )
		return bResult;

	// Local interfaces
	KeAcquireSpinLock( &g_NetAddressesLock, &OldIrql );

	pListEntry = g_NetAddressesList.Flink;
	while( pListEntry != &g_NetAddressesList )
	{
		PNET_ADDRESS pNetAddress = CONTAINING_RECORD( pListEntry, NET_ADDRESS, m_qLink );
		pListEntry = pListEntry->Flink;

		if( pNetAddress->m_dwAddr == dwAddr )
		{
			bResult = TRUE;
			break;
		}
	}

	KeReleaseSpinLock( &g_NetAddressesLock, OldIrql );

	return bResult;
}

VOID
ClientPnPAddNetAddress(
    IN PTA_ADDRESS  Address,
    IN PUNICODE_STRING  DeviceName,
    IN PTDI_PNP_CONTEXT  Context
    )
{
	if((Address->AddressType == TDI_ADDRESS_TYPE_IP ) &&
	   (Address->AddressLength == 14) && 
	   DeviceName)
	{
		PNET_ADDRESS pNetAddress = ExAllocateFromNPagedLookasideList( &g_NetAddressesLookasideList );
		if( pNetAddress )
		{
			KIRQL OldIrql;
			pNetAddress->m_dwAddr = *(DWORD *)&Address->Address[2];

			KeAcquireSpinLock( &g_NetAddressesLock, &OldIrql );
			InsertHeadList( &g_NetAddressesList, &pNetAddress->m_qLink );
			KeReleaseSpinLock( &g_NetAddressesLock, OldIrql );
		}

		//UNICODE_STRING GUID;
		//WCHAR szGUID[MAX_PATH] = L"";
		//DWORD dwAddr = *(DWORD *)&Address->Address[2];

		//GUID.Buffer = szGUID;
		//GUID.Length = 0;
		//GUID.MaximumLength = MAX_PATH * sizeof(WCHAR);

		//RtlCopyUnicodeString(&GUID, DeviceName);

		//dprintf("ClientPnPAddNetAddress(%d.%d.%d.%d) %S", 
		//	PRINT_IP_ADDR(dwAddr),
		//	szGUID);

		//MF_SetAdapterIPByTCPDeviceName(&GUID, dwAddr);
	}
}

VOID
ClientPnPDelNetAddress(
    IN PTA_ADDRESS  Address,
    IN PUNICODE_STRING  DeviceName,
    IN PTDI_PNP_CONTEXT  Context
    )
{
	if((Address->AddressType == TDI_ADDRESS_TYPE_IP ) &&
	   (Address->AddressLength == 14) &&
	   DeviceName)
	{
		KIRQL OldIrql;
		PLIST_ENTRY pListEntry;

		KeAcquireSpinLock( &g_NetAddressesLock, &OldIrql );

		pListEntry = g_NetAddressesList.Flink;
		while( pListEntry != &g_NetAddressesList )
		{
			PNET_ADDRESS pNetAddress = CONTAINING_RECORD( pListEntry, NET_ADDRESS, m_qLink );

			if( pNetAddress->m_dwAddr == *(DWORD *)&Address->Address[2] )
			{
				RemoveEntryList( pListEntry );	
				pListEntry = g_NetAddressesList.Flink;
			}
			else
			{
				pListEntry = pListEntry->Flink;
			}
		}

		KeReleaseSpinLock( &g_NetAddressesLock, OldIrql );

		//UNICODE_STRING GUID;
		//WCHAR szGUID[MAX_PATH] = L"";
		//DWORD dwAddr = *(DWORD *)&Address->Address[2];

		//GUID.Buffer = szGUID;
		//GUID.Length = 0;
		//GUID.MaximumLength = MAX_PATH * sizeof(WCHAR);

		//RtlCopyUnicodeString(&GUID, DeviceName);

		//dprintf("ClientPnPDelNetAddress(%d.%d.%d.%d) %S", 
		//	PRINT_IP_ADDR(dwAddr),
		//	szGUID);

		//MF_SetAdapterIPByTCPDeviceName(&GUID, 0);
	}
}

NTSTATUS RegisterIpMonitor( )
{
	NTSTATUS status;
	TDI_CLIENT_INTERFACE_INFO tdiInterfaceInfo;

	ExInitializeNPagedLookasideList( &g_NetAddressesLookasideList, NULL, NULL, 0, sizeof(NET_ADDRESS), DRIVER_TAG, 0 );
	InitializeListHead( &g_NetAddressesList );
	KeInitializeSpinLock( &g_NetAddressesLock );

	RtlZeroMemory(&tdiInterfaceInfo, sizeof(TDI_CLIENT_INTERFACE_INFO));
	tdiInterfaceInfo.MajorTdiVersion = TDI_CURRENT_MAJOR_VERSION;
	tdiInterfaceInfo.MinorTdiVersion = TDI_CURRENT_MINOR_VERSION;
	tdiInterfaceInfo.AddAddressHandlerV2 = ClientPnPAddNetAddress;
	tdiInterfaceInfo.DelAddressHandlerV2 = ClientPnPDelNetAddress;

	status = TdiRegisterPnPHandlers( &tdiInterfaceInfo, sizeof(TDI_CLIENT_INTERFACE_INFO), &g_TdiPNPHandler );
	if( ! NT_SUCCESS(status) )
	{
		ExDeleteNPagedLookasideList( &g_NetAddressesLookasideList );
	}
	
	return status;
}

VOID DeregisterIpMonitor( )
{
	if(g_TdiPNPHandler)
	{
		NTSTATUS status = TdiDeregisterPnPHandlers( g_TdiPNPHandler );

		if( !NT_SUCCESS(status) )
		{
			dprintf("ruleDeregisterTDIPNPHandlers failed. status = %X", status);
		}

		g_TdiPNPHandler = NULL;
	}

	ExDeleteNPagedLookasideList( &g_NetAddressesLookasideList );
}