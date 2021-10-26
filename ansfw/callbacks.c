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

NTSTATUS RequestCallback(
	IN PFLT_PACKET Packet
	);

VOID OpenAdapterCallback(
	IN PVOID pAdapter,
	IN PNDIS_STRING  BindingName
	);

VOID CloseAdapterCallback(
	IN PVOID pAdapter
	);

VOID UnloadCallback( );

NTSTATUS ObtainFilterFunctions( PFLT_FUNCTIONS pFltFunctions );

VOID GetPacketPid( PFW_REQUEST pFwPacket );

NPAGED_LOOKASIDE_LIST g_FwRequestsLookasideList;

static FLT_FUNCTIONS g_FltFunctions;
static PFLT_INSTANCE g_pFltInstance;
static LONG g_bFltConnected;

static LIST_ENTRY g_FwInstancesList;
static KSPIN_LOCK g_FwInstancesLock;


NTSTATUS RegisterFilterCallbacks()
{
	NTSTATUS status;

	status = ObtainFilterFunctions( &g_FltFunctions );
	if( NT_SUCCESS(status) && g_FltFunctions.m_pFltRegisterCallbacks )
	{
		FLT_REGISTRATION_CONTEXT regContext; 
		regContext.m_pRequestCallback = RequestCallback;
		regContext.m_pOpenAdapterCallback = OpenAdapterCallback;
		regContext.m_pCloseAdapterCallback = CloseAdapterCallback;
		regContext.m_pUnloadCallback = UnloadCallback;

		status = g_FltFunctions.m_pFltRegisterCallbacks(
			&g_pFltInstance,
			&regContext );

		if( NT_SUCCESS(status) )
		{
			InterlockedExchange( &g_bFltConnected, TRUE );

			ExInitializeNPagedLookasideList( &g_FwRequestsLookasideList, NULL, NULL, 0, sizeof(FW_REQUEST)+sizeof(FLT_PACKET), DRIVER_TAG, 0 );
			InitializeListHead( &g_FwInstancesList );
			KeInitializeSpinLock( &g_FwInstancesLock );
		}
	}

	return status;
}

NTSTATUS DeregisterFilterCallbacks()
{
	NTSTATUS status = STATUS_SUCCESS;

	if( InterlockedExchange(&g_bFltConnected, FALSE) && g_FltFunctions.m_pFltDeregisterCallbacks )
		status = g_FltFunctions.m_pFltDeregisterCallbacks( g_pFltInstance );

	RtlZeroMemory( &g_FltFunctions, sizeof(FLT_FUNCTIONS) );

	ExDeleteNPagedLookasideList( &g_FwRequestsLookasideList );

	return status;
}

NTSTATUS ObtainFilterFunctions( PFLT_FUNCTIONS pFltFunctions )
{
	NTSTATUS status;
	KIRQL oldIrql;

	UNICODE_STRING usFltDeviceName;
	PDEVICE_OBJECT pDeviceObject;
	PFILE_OBJECT pFileObject;
	
	PIRP pIrp;
	IO_STATUS_BLOCK iosb;

	RtlInitUnicodeString( &usFltDeviceName, FLT_DEVICE_NAME );
	status = IoGetDeviceObjectPointer( &usFltDeviceName, FILE_ALL_ACCESS, &pFileObject, &pDeviceObject );
	if( ! NT_SUCCESS(status) )
	{		
		dprintf( "Failed IoGetDeviceObjectPointer.\n");
		return status;
	}

	pIrp = IoBuildDeviceIoControlRequest( 
		IOCTL_OBTAIN_FILTER_FUNCTIONS, 
		pDeviceObject, 
		NULL, 
		0,
		pFltFunctions, 
		sizeof(FLT_FUNCTIONS), 
		TRUE, 
		NULL, 
		&iosb );
	if( pIrp == NULL )
	{
		dprintf( "Failed IoBuildDeviceIoControlRequest.\n");
		ObDereferenceObject( pFileObject );
		return STATUS_UNSUCCESSFUL;
	}

	status = IoCallDriver( pDeviceObject, pIrp );
	if( ! NT_SUCCESS(status) )
	{
		dprintf( "Failed IoCallDriver.\n");
	}

	ObDereferenceObject( pFileObject );
	return status;
}

BOOLEAN IsClientConnected()
{
	BOOLEAN result;
	KIRQL oldIrql;

	KeAcquireSpinLock( &g_FwInstancesLock, &oldIrql );
	result = (IsListEmpty(&g_FwInstancesList) ? FALSE : TRUE);
	KeReleaseSpinLock( &g_FwInstancesLock, oldIrql );

	return result;
}

PFW_REQUEST AllocateRequest( PFLT_PACKET pFltPacket )
{
	PFW_REQUEST pFwPacket = ExAllocateFromNPagedLookasideList( &g_FwRequestsLookasideList );

	if( pFwPacket )
	{
		if( InterlockedCompareExchange(&g_bFltConnected, 0, 0) && g_FltFunctions.m_pFltCloneRequest )
			g_FltFunctions.m_pFltCloneRequest( pFltPacket, &pFltPacket );

		RtlZeroMemory( pFwPacket, sizeof(FW_REQUEST) );
		pFwPacket->m_pFltPacket = pFltPacket;
		pFwPacket->m_References = 1;
	}

	return pFwPacket;
}

NTSTATUS RequestCallback(
	IN PFLT_PACKET pFltPacket
	)
{	
	NTSTATUS Status;
	PFW_REQUEST pFwPacket;

	if( !IsClientConnected() )
		return STATUS_SUCCESS;

	pFwPacket = AllocateRequest( pFltPacket );
	if( ! pFwPacket )
		return STATUS_INSUFFICIENT_RESOURCES;

	InitPacketHeaders( pFwPacket );
	GetPacketPid( pFwPacket );

	if((pFwPacket->m_ReqFlags & FL_REQUEST_FRAGMENTED_IP) && pFwPacket->m_pFltPacket->m_Direction==PHYSICAL_DIRECTION_IN)
	{		
		dprintf( "Calling ReassemblePacket\n" );
		ReassemblePacket( pFwPacket );
		return STATUS_UNSUCCESSFUL;
	}

	Status = ProcessRequest( NULL, pFwPacket );

	FWFreeCloneRequest( pFwPacket );

	return Status;
}

NTSTATUS ProcessRequest(
	PFW_INSTANCE pFwInstance,
	PFW_REQUEST pFwRequest 
	)
{
	KIRQL oldIrql;
	NTSTATUS status = STATUS_SUCCESS;
	PLIST_ENTRY pListEntry;

	KeAcquireSpinLock( &g_FwInstancesLock, &oldIrql );

	if( pFwInstance )
		pListEntry = pFwInstance->m_qLink.Flink;
	else
		pListEntry = g_FwInstancesList.Flink;

	while( pListEntry != &g_FwInstancesList )
	{
		PFW_INSTANCE pCurrFwInstance = CONTAINING_RECORD( pListEntry, FW_INSTANCE, m_qLink );

		if( pCurrFwInstance->m_pFwRequestCallback )
			status = pCurrFwInstance->m_pFwRequestCallback( pFwRequest );

		if( status != STATUS_SUCCESS )
			break;

		pListEntry = pListEntry->Flink;
	}

	KeReleaseSpinLock( &g_FwInstancesLock, oldIrql );

	return status;
}

VOID OpenAdapterCallback(
	IN PVOID pAdapter,
	IN PNDIS_STRING  BindingName)
{

}

VOID CloseAdapterCallback(
	IN PVOID pAdapter)
{

}

VOID UnloadCallback( )
{
	InterlockedExchange(&g_bFltConnected, FALSE);
	RtlZeroMemory( &g_FltFunctions, sizeof(FLT_FUNCTIONS) );
}

NTSTATUS FWRegisterRequestCallback(
	OUT PFW_INSTANCE * ppInstance,
	IN  FW_REQUEST_CALLBACK FwRequestCallback,
	IN  DWORD Flags
	)
{
	KIRQL oldIrql;
	NTSTATUS status;

	if(ppInstance==NULL || FwRequestCallback==NULL)
		return STATUS_INVALID_PARAMETER;

	*ppInstance = ExAllocatePoolWithTag( NonPagedPool, sizeof(FW_INSTANCE), DRIVER_TAG );
	if( *ppInstance == NULL )
		return STATUS_INSUFFICIENT_RESOURCES;

	(*ppInstance)->m_pFwRequestCallback = FwRequestCallback;

	KeAcquireSpinLock( &g_FwInstancesLock, &oldIrql );

	InsertTailList( &g_FwInstancesList, &(*ppInstance)->m_qLink );

	KeReleaseSpinLock( &g_FwInstancesLock, oldIrql );

	return STATUS_SUCCESS;
}

NTSTATUS FWDeregisterRequestCallback(
	IN PFW_INSTANCE pFwInstance
	)
{
	KIRQL oldIrql;

	KeAcquireSpinLock( &g_FwInstancesLock, &oldIrql );

	RemoveEntryList( &pFwInstance->m_qLink );

	KeReleaseSpinLock( &g_FwInstancesLock, oldIrql );

	ExFreePool( pFwInstance );

	return STATUS_SUCCESS;
}

NTSTATUS FWInjectRequest(
						 IN PFW_INSTANCE  pInstance OPTIONAL,
						 IN PFW_REQUEST pRequest
						 )
{
	NTSTATUS status;

	status = ProcessRequest( pInstance, pRequest );
	if( NT_SUCCESS(status) )
	{
		if( pRequest->m_ReqFlags & FL_REQUEST_LOOPBACK )
		{
			InjectLoopbackRequest(pRequest);
		}
		else if( pRequest->m_ReqFlags & FL_REQUEST_REASSEMBLED )
		{
			if( InterlockedCompareExchange(&g_bFltConnected, 0, 0) && g_FltFunctions.m_pFltInjectRequest )
			{
				PDATA_LIST pDataList = CONTAINING_RECORD( pRequest, DATA_LIST, m_FwRequest );
				PLIST_ENTRY pListEntry = pDataList->m_FragmentsList.Flink;
				while( pListEntry != &pDataList->m_FragmentsList )
				{
					PFW_REQUEST pFragment = CONTAINING_RECORD( pListEntry, FW_REQUEST, m_qLink );
					pListEntry = pListEntry->Flink;
					status = g_FltFunctions.m_pFltInjectRequest( g_pFltInstance, pFragment->m_pFltPacket );
				}	
			}
		}
		else
		{
			if( InterlockedCompareExchange(&g_bFltConnected, 0, 0) && g_FltFunctions.m_pFltInjectRequest )
				status = g_FltFunctions.m_pFltInjectRequest( g_pFltInstance, pRequest->m_pFltPacket );
			else
				status = STATUS_UNSUCCESSFUL;
		}
	}

	return status;
}

NTSTATUS FWCloneRequest(
	IN PFW_REQUEST pRequest,
	OUT PFW_REQUEST *ppClonedRequest
	)
{	
	InterlockedIncrement( &pRequest->m_References );
	*ppClonedRequest = pRequest;

	return STATUS_SUCCESS;
}

VOID FWFreeCloneRequest(
	IN PFW_REQUEST pCloneRequest
	)
{
	if( InterlockedDecrement( &pCloneRequest->m_References )==0 )
	{
		if( pCloneRequest->m_ReqFlags & FL_REQUEST_LOOPBACK )
		{
			FreeLoopbackRequest(pCloneRequest);
		}
		else if( pCloneRequest->m_ReqFlags & FL_REQUEST_REASSEMBLED )
		{
			PDATA_LIST pDataList = CONTAINING_RECORD( pCloneRequest, DATA_LIST, m_FwRequest );
			PLIST_ENTRY pListEntry = pDataList->m_FragmentsList.Flink;
			while( pListEntry != &pDataList->m_FragmentsList )
			{
				PFW_REQUEST pFragment = CONTAINING_RECORD( pListEntry, FW_REQUEST, m_qLink );
				pListEntry = pListEntry->Flink;
				FWFreeCloneRequest( pFragment );
			}	
			ruleRecycleDatagramWorkBuffer(pDataList);	
		}
		else
		{
			if( InterlockedCompareExchange(&g_bFltConnected, 0, 0) && g_FltFunctions.m_pFltInjectRequest )
				g_FltFunctions.m_pFltFreeCloneRequest( pCloneRequest->m_pFltPacket );

			ExFreeToNPagedLookasideList( &g_FwRequestsLookasideList, pCloneRequest );
		}
	}
}

VOID GetPacketPid( PFW_REQUEST pFwPacket )
{
	FW_PID_REQUEST pidRequest;

	pidRequest.m_Pid = 0;
	pidRequest.m_Flags = pFwPacket->m_pFltPacket->m_Direction;
	pidRequest.m_IpProto = pFwPacket->m_IpProto;

	if((pFwPacket->m_IpProto != IPPROTO_TCP) && 
		(pFwPacket->m_IpProto != IPPROTO_UDP))
	{
		pidRequest.m_LocalAddr = pFwPacket->m_SourceAddr;
		pidRequest.m_RemoteAddr = pFwPacket->m_DestAddr;
		pidRequest.m_LocalPort = 0;
		pidRequest.m_RemotePort = 0;
	}
	else
	{
		if(pFwPacket->m_pFltPacket->m_Direction & PHYSICAL_DIRECTION_OUT)
		{
			pidRequest.m_LocalAddr = pFwPacket->m_SourceAddr;
			pidRequest.m_LocalPort = pFwPacket->m_SourcePort;
			pidRequest.m_RemoteAddr = pFwPacket->m_DestAddr;
			pidRequest.m_RemotePort = pFwPacket->m_DestPort;
		}
		else
		{
			pidRequest.m_LocalAddr = pFwPacket->m_DestAddr;
			pidRequest.m_LocalPort = pFwPacket->m_DestPort;
			pidRequest.m_RemoteAddr = pFwPacket->m_SourceAddr;
			pidRequest.m_RemotePort = pFwPacket->m_SourcePort;
		}
	}

	GetPidFromLocalInfo( &pidRequest );

	pFwPacket->m_Pid = pidRequest.m_Pid;
}
