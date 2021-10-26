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

typedef struct _FLT_MY_PACKET
{
	LIST_ENTRY m_qLink;
	PFLT_PACKET m_pPacket;
} FLT_MY_PACKET, *PFLT_MY_PACKET;


//
// Prototypes
//

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
VOID ProcessRequestThread( PVOID pContext );


//
// Global variables
// 

static KEVENT g_Event;
static PVOID g_pThread;
static BOOL g_bTerminateThread;

static LIST_ENTRY g_MyRequestsList;
static KSPIN_LOCK g_MyRequestsLock;

static LONG g_bFltConnected;
static FLT_FUNCTIONS g_FltFunctions;
static PFLT_INSTANCE g_pFltInstance;


//
// Implementation
//

NTSTATUS RegisterCallbacks()
{
	NTSTATUS status;
	HANDLE hThread;

	InitializeListHead( &g_MyRequestsList );
	KeInitializeSpinLock( &g_MyRequestsLock );

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
	}

	if( NT_SUCCESS(status) )
	{
		KeInitializeEvent( &g_Event, SynchronizationEvent, FALSE );
		PsCreateSystemThread( &hThread, 0, NULL, NULL, NULL, ProcessRequestThread, NULL );
		ObReferenceObjectByHandle( hThread, THREAD_ALL_ACCESS, NULL, KernelMode, &g_pThread, NULL );
		ZwClose( hThread );

		InterlockedExchange( &g_bFltConnected, TRUE );
	}

	return status;
}

VOID DeregisterCallbacks()
{
	KIRQL oldIrql;
	NTSTATUS status;

	g_bTerminateThread = TRUE;
	KeSetEvent( &g_Event, IO_NO_INCREMENT, FALSE );
	KeWaitForSingleObject( g_pThread, Executive, KernelMode, FALSE, NULL );

	if( InterlockedExchange(&g_bFltConnected, FALSE) && g_FltFunctions.m_pFltDeregisterCallbacks )
		status = g_FltFunctions.m_pFltDeregisterCallbacks( g_pFltInstance );

	RtlZeroMemory( &g_FltFunctions, sizeof(FLT_FUNCTIONS) );
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
		KdPrint(( "Failed IoGetDeviceObjectPointer.\n"));
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
		KdPrint(( "Failed IoBuildDeviceIoControlRequest.\n"));
		ObDereferenceObject( pFileObject );
		return STATUS_UNSUCCESSFUL;
	}

	status = IoCallDriver( pDeviceObject, pIrp );
	if( ! NT_SUCCESS(status) )
	{
		KdPrint(( "Failed IoCallDriver.\n"));
	}

	ObDereferenceObject( pFileObject );
	return status;
}


NTSTATUS RequestCallback(
	IN PFLT_PACKET pFltPacket
	)
{
	NTSTATUS status;
	PFLT_MY_PACKET pMyRequest;
	KIRQL OldIrql;

	KdPrint(( "RequestCallback processing\n" ));

#ifdef DECISION_ALLOW
	return STATUS_SUCCESS;
#elif DECISION_BLOCK
	return STATUS_UNSUCCESSFUL;
#else // more processing required

	if( InterlockedCompareExchange(&g_bFltConnected, 0, 0) && g_FltFunctions.m_pFltCloneRequest )
	{
		g_FltFunctions.m_pFltCloneRequest( pFltPacket, &pFltPacket );

		pMyRequest = ExAllocatePoolWithTag( NonPagedPool, sizeof(FLT_MY_PACKET), DRIVER_TAG );
		if( ! pMyRequest )
		{
			if( InterlockedCompareExchange(&g_bFltConnected, 0, 0) && g_FltFunctions.m_pFltFreeCloneRequest )
				g_FltFunctions.m_pFltFreeCloneRequest( pFltPacket );

			return STATUS_UNSUCCESSFUL;	
		}

		pMyRequest->m_pPacket = pFltPacket;

		KeAcquireSpinLock( &g_MyRequestsLock, &OldIrql );
		InsertTailList( &g_MyRequestsList, &pMyRequest->m_qLink );
		KeReleaseSpinLock( &g_MyRequestsLock, OldIrql );

		KeSetEvent( &g_Event, IO_NO_INCREMENT, FALSE );
	}

	return STATUS_UNSUCCESSFUL;	

#endif
}

VOID OpenAdapterCallback(
	IN PVOID pAdapter,
	IN PNDIS_STRING  BindingName)
{
	KdPrint(( "===> OpenAdapterCallback\n" ));
}

VOID CloseAdapterCallback(
	IN PVOID pAdapter)
{
	KdPrint(( "===> CloseAdapterCallback\n" ));
}

VOID UnloadCallback( )
{
	InterlockedExchange(&g_bFltConnected, FALSE);
	RtlZeroMemory( &g_FltFunctions, sizeof(FLT_FUNCTIONS) );
}

VOID ProcessRequestThread( PVOID pContext )
{
	KIRQL oldIrql;

	while( ! g_bTerminateThread )
	{	
		PFLT_MY_PACKET pMyRequest;

		KeWaitForSingleObject( &g_Event, Executive, KernelMode, FALSE, NULL );

		KeAcquireSpinLock( &g_MyRequestsLock, &oldIrql );
		while( ! IsListEmpty(&g_MyRequestsList) )
		{
			pMyRequest = CONTAINING_RECORD( RemoveHeadList( &g_MyRequestsList ), FLT_MY_PACKET, m_qLink );
			KeReleaseSpinLock( &g_MyRequestsLock, oldIrql );

			if( InterlockedCompareExchange(&g_bFltConnected, 0, 0) && g_FltFunctions.m_pFltInjectRequest )
				g_FltFunctions.m_pFltInjectRequest( g_pFltInstance, pMyRequest->m_pPacket );

			if( InterlockedCompareExchange(&g_bFltConnected, 0, 0) && g_FltFunctions.m_pFltFreeCloneRequest )
				g_FltFunctions.m_pFltFreeCloneRequest( pMyRequest->m_pPacket );

			ExFreePool( pMyRequest );

			KeAcquireSpinLock( &g_MyRequestsLock, &oldIrql );
		}
		KeReleaseSpinLock( &g_MyRequestsLock, oldIrql );
	}

	PsTerminateSystemThread(0);
}