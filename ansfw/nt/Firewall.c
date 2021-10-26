/*++

Copyright (c)2009 Andrew SONG, All Rights Reserved

Module Name:

	firewall.c

Environment:

	Kernel mode

Revision History:

	May-09 : Created 

--*/

#include "precomp.h"

LIST_ENTRY g_PendingRequestList;
NPROT_LOCK g_PendingRequestListLock;

LIST_ENTRY g_FreeRequestList;
NPROT_LOCK g_FreeRequestListLock;

BOOL	   g_DoFwThread; 
PVOID	   g_FwThreadObj; 
KEVENT	   g_FwThreadEvent;

VOID fwThread( PVOID data );
NTSTATUS ProcessLoopbackRequest(PTDI_CONN_REQUEST request);

//NTSTATUS (NTAPI *ProcessLoopbackRequest)(PVOID request);

//NTSTATUS isConnectionOk(PTDI_CONN_REQUEST request)
//{
//	NTSTATUS Status = 0;
//
//	PAGED_CODE();
//
//	if((request->m_SourcePort == request->m_DestPort) &&
//	   ((request->m_SourceAddr == request->m_DestAddr) ||
//	   (request->m_SourceAddr == 0)))
//	   return STATUS_INVALID_PARAMETER;
//
//	return ProcessLoopbackRequest(request);
//
//	//__try
//	//{
//	//	if(InterlockedCompareExchangePointer((PVOID *)&ProcessLoopbackRequest, (PVOID)ProcessLoopbackRequest, NULL) == NULL)
//	//	{
//	//		dprintf("isConnectionOk() NDIS is down 0x%p\n", ProcessLoopbackRequest);
//	//		return STATUS_DEVICE_NOT_READY;
//	//	}
//
//	//	return ProcessLoopbackRequest(request);
//	//}
//	//__except(EXCEPTION_EXECUTE_HANDLER)
//	//{
//	//	Status = GetExceptionCode();
//	//	dprintf("isConnectionOk() exception 0x%x\n", Status);
//	//}
//
//	//return Status;
//}

VOID tdiInitFirewall( )
{
	HANDLE 	hdl;
	NPROT_INIT_LOCK(&g_PendingRequestListLock);
	NPROT_INIT_LOCK(&g_FreeRequestListLock);

	InitializeListHead(&g_PendingRequestList);
	InitializeListHead(&g_FreeRequestList);
	
	KeInitializeEvent(&g_FwThreadEvent, SynchronizationEvent, FALSE);

	g_DoFwThread = TRUE;
	
	if (PsCreateSystemThread( &hdl, 0, NULL, NULL, NULL, fwThread, NULL ) != STATUS_SUCCESS) 
	{
		if (ObReferenceObjectByHandle( hdl, THREAD_ALL_ACCESS, NULL, KernelMode, &g_FwThreadObj, NULL ) != STATUS_SUCCESS)
			g_FwThreadObj = NULL;
		else
			ZwClose( hdl );
	}
}

VOID tdiCloseFirewall( )
{
	g_DoFwThread = FALSE;

	KeSetEvent( &g_FwThreadEvent, IO_NO_INCREMENT, g_FwThreadObj != NULL );

	if (g_FwThreadObj != NULL) 
	{
		KeWaitForSingleObject( g_FwThreadObj, Executive, KernelMode, FALSE, NULL );
		ObDereferenceObject( g_FwThreadObj );
		g_FwThreadObj = NULL;
	}
}

VOID tdiRecycleRequest(PTDI_CONN_REQUEST request)
{
	NPROT_ACQUIRE_LOCK( &g_FreeRequestListLock);
	InsertTailList(&g_FreeRequestList, &request->m_qLink);
	NPROT_RELEASE_LOCK( &g_FreeRequestListLock);
}

PTDI_CONN_REQUEST tdiAllocateRequest( )
{
	PTDI_CONN_REQUEST request = NULL; 
 
	NPROT_ACQUIRE_LOCK( &g_FreeRequestListLock);
 
	if ( !IsListEmpty ( &g_FreeRequestList ) ) 
	{ 
		request = (PTDI_CONN_REQUEST) RemoveHeadList ( &g_FreeRequestList ); 
		NPROT_RELEASE_LOCK( &g_FreeRequestListLock);
		NdisZeroMemory ( request, sizeof(TDI_CONN_REQUEST) ); 
		return request; 
	} 
 
	NPROT_RELEASE_LOCK( &g_FreeRequestListLock);
 
	if(NdisAllocateMemoryWithTag ( 
			&request, 
			sizeof(TDI_CONN_REQUEST), 
			'rtpR'
			) == NDIS_STATUS_SUCCESS) 
	{ 
		 
		NdisZeroMemory ( request, sizeof(TDI_CONN_REQUEST) ); 
		return request; 
	} 
 
	return NULL;
}

NTSTATUS tdiChainIrp(PTDI_CONN_REQUEST request)
{
	dprintf("tdiChainIrp (Irp = 0x%x)\n", request->m_pIrp);

	if(request->m_CompletionFunc)
	{
		IoMarkIrpPending( request->m_pIrp );

		IoCopyCurrentIrpStackLocationToNext( request->m_pIrp );

		IoSetCompletionRoutine(
			request->m_pIrp,
			request->m_CompletionFunc,
			request->m_pContext,
			TRUE,
			TRUE,
			TRUE
			);

		IoCallDriver(request->m_pDevExt->LowerDeviceObject, request->m_pIrp);
		return STATUS_PENDING;
	}
	else
	{
		IoSkipCurrentIrpStackLocation( request->m_pIrp );
	}

	return IoCallDriver(request->m_pDevExt->LowerDeviceObject, request->m_pIrp);
}

NTSTATUS tdiFailIrp(PTDI_CONN_REQUEST request)
{
	request->m_pIrp->IoStatus.Status = request->m_StatusFail;
	request->m_pIrp->IoStatus.Information = 0;
	IoCompleteRequest( request->m_pIrp, IO_NO_INCREMENT );
	return request->m_StatusFail;
}

VOID tdiCancelIrp (IN PDEVICE_OBJECT pDevObj, IN PIRP irp) 
{
	PTDI_CONN_REQUEST reqOn = NULL;

	dprintf("tdiCancelIrp - cancel irp = %x\n", irp );

	IoReleaseCancelSpinLock( irp->CancelIrql );

	NPROT_ACQUIRE_LOCK( &g_PendingRequestListLock);

	reqOn = (PTDI_CONN_REQUEST)g_PendingRequestList.Flink;

	while(reqOn != (PTDI_CONN_REQUEST)&g_PendingRequestList)
	{
		if(reqOn->m_pIrp == irp)
		{
			dprintf("tdiCancelIrp - remove request\n");
			RemoveEntryList(&reqOn->m_qLink);
			tdiRecycleRequest(reqOn);
			reqOn = (PTDI_CONN_REQUEST)g_PendingRequestList.Flink;
			continue;
		}

		reqOn = (PTDI_CONN_REQUEST)reqOn->m_qLink.Flink;
	}

	NPROT_RELEASE_LOCK( &g_PendingRequestListLock);

	irp->IoStatus.Status = STATUS_CANCELLED;
	irp->IoStatus.Information = 0;
	IoCompleteRequest( irp, IO_NO_INCREMENT );

	KeSetEvent( &g_FwThreadEvent, IO_NO_INCREMENT, FALSE );
}

NTSTATUS tdiPendIrp(PTDI_CONN_REQUEST request)
{
	PDRIVER_CANCEL	oldCancelRoutine;
	NTSTATUS		status = STATUS_PENDING;
	PIRP			irp = NULL;
	
	dprintf("tdiPendIrp (Irp = 0x%x)\n", request->m_pIrp);

	NPROT_ACQUIRE_LOCK( &g_PendingRequestListLock);
	InsertTailList(&g_PendingRequestList, &request->m_qLink);

	IoMarkIrpPending(request->m_pIrp);

	// Must set a cancel routine before checking the Cancel flag.
	oldCancelRoutine = IoSetCancelRoutine ( request->m_pIrp, tdiCancelIrp );

	irp = request->m_pIrp;

	// The IRP was cancelled.  Check whether or not our cancel routine was called.
	if (irp->Cancel) 
	{
		dprintf("tdiPendIrp(our cancel routine is called)\n");
		oldCancelRoutine = IoSetCancelRoutine( irp, NULL );
		// The cancel routine was NOT called 
		if (oldCancelRoutine)
		{
			dprintf("tdiPendIrp(our cancel routine is not called)\n");
			status = irp->IoStatus.Status = STATUS_CANCELLED;
			RemoveEntryList( &request->m_qLink );
			tdiRecycleRequest( request);
		}
	}

	NPROT_RELEASE_LOCK( &g_PendingRequestListLock);

	if (status != STATUS_PENDING)
		IoCompleteRequest( irp, IO_NO_INCREMENT );

	KeSetEvent( &g_FwThreadEvent, IO_NO_INCREMENT, FALSE );
	dprintf("tdiPendIrp Status = 0x%x(Irp = 0x%x)\n",status, request->m_pIrp);
	return status;
}

PTDI_CONN_REQUEST tdiGetRequest(PTDI_CONN_REQUEST request)
{
	PDRIVER_CANCEL	oldCancelRoutine;

	oldCancelRoutine = IoSetCancelRoutine( request->m_pIrp, NULL );

	if (oldCancelRoutine) 
	{
		RemoveEntryList( &request->m_qLink );
		return request;
	}

	return NULL;
}

NTSTATUS tdiConnectionOk(PTDI_CONN_REQUEST request)
{
	PTDI_CONN_REQUEST reqOn = NULL;
	
	dprintf("tdiConnectionOk(Irp = 0x%x)\n", request->m_pIrp);
	reqOn = tdiAllocateRequest( );

	if(reqOn == NULL)
	{
		dprintf("tdiConnectionOk allocation failed(Irp = 0x%x)\n", request->m_pIrp);
		return tdiChainIrp(request);
	}

	RtlCopyMemory(reqOn, request, sizeof(TDI_CONN_REQUEST));

	return tdiPendIrp(reqOn);
}

VOID fwThread( PVOID data )
{
	PTDI_CONN_REQUEST reqOn = NULL;
	PTDI_CONN_REQUEST request = NULL;
	
	KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);

	while(g_DoFwThread)
	{
		KeWaitForSingleObject( &g_FwThreadEvent, Executive, KernelMode, FALSE, NULL );

		if(!g_DoFwThread)
			break;

		NPROT_ACQUIRE_LOCK( &g_PendingRequestListLock);

		reqOn = (PTDI_CONN_REQUEST)g_PendingRequestList.Flink;

		while(reqOn != (PTDI_CONN_REQUEST)&g_PendingRequestList)
		{
			if(request = tdiGetRequest(reqOn))
			{
				//NTSTATUS Status = 0;
				PFW_REQUEST pFwRequest;
				NPROT_RELEASE_LOCK( &g_PendingRequestListLock);

				//Status = isConnectionOk(request);

				//if(Status != STATUS_SUCCESS)
				//{
				//	dprintf("isConnectionOk Error Status = 0x%x\n", Status);
				//	tdiChainIrp(request);
				//}

				if( IsClientConnected() )
				{

					pFwRequest = ExAllocateFromNPagedLookasideList( &g_FwRequestsLookasideList );
					if( pFwRequest )
					{
						pFwRequest->m_pFltPacket = (PVOID)((ULONG_PTR)pFwRequest + sizeof(FW_REQUEST));

						pFwRequest->m_ReqFlags = FL_REQUEST_LOOPBACK;
						//pFwRequest->m_pFltPacket->m_Direction = PHYSICAL_DIRECTION_OUT;
						pFwRequest->m_Pid = request->m_Pid;
						pFwRequest->m_IpProto = (BYTE)request->m_IpProto;
						pFwRequest->m_SourceAddr = request->m_SourceAddr;
						pFwRequest->m_DestAddr = request->m_DestAddr;
						pFwRequest->m_SourcePort = request->m_SourcePort;
						pFwRequest->m_DestPort = request->m_DestPort;
						
						pFwRequest->m_pFltPacket->m_Direction = PHYSICAL_DIRECTION_OUT;
						pFwRequest->m_pFltPacket->m_pBuffer = request;
						pFwRequest->m_pFltPacket->m_BufferSize = 0;
						pFwRequest->m_References = 1;
						//pFwRequest->m_pFltPacket->m_pBuffer = request;
						//pFwRequest->m_pFltPacket->m_BufferSize = sizeof(TDI_CONN_REQUEST);
						//((PTDI_CONN_REQUEST)pFwRequest->m_pFltPacket->m_pBuffer)->m_References = 1;


						FWInjectRequest( NULL, pFwRequest );

						FWFreeCloneRequest( pFwRequest );
						////if( ProcessLoopbackRequest(request) != STATUS_SUCCESS )
						////{
						////	tdiChainIrp(request);
						////}

					}
					else
					{
						tdiChainIrp(request);
						tdiRecycleRequest(request);
					}
				}
				else
				{
						tdiChainIrp(request);
						tdiRecycleRequest(request);
				}

				//tdiRecycleRequest(request);
				NPROT_ACQUIRE_LOCK( &g_PendingRequestListLock);
				reqOn = (PTDI_CONN_REQUEST)g_PendingRequestList.Flink;
				continue;
			}

			reqOn = (PTDI_CONN_REQUEST)reqOn->m_qLink.Flink;
		}

		NPROT_RELEASE_LOCK( &g_PendingRequestListLock);

	}

	PsTerminateSystemThread(STATUS_SUCCESS);
}

void InjectLoopbackRequest( PFW_REQUEST pFwRequest)
{
	if( ((PTDI_CONN_REQUEST)pFwRequest->m_pFltPacket->m_pBuffer)->m_pIrp ) // Injecting just once
	{
		tdiChainIrp( (PTDI_CONN_REQUEST)pFwRequest->m_pFltPacket->m_pBuffer );
		((PTDI_CONN_REQUEST)pFwRequest->m_pFltPacket->m_pBuffer)->m_pIrp = NULL;
	}
}

void FreeLoopbackRequest( PFW_REQUEST pCloneRequest )
{
	if( ((PTDI_CONN_REQUEST)pCloneRequest->m_pFltPacket->m_pBuffer)->m_pIrp ) // Failing just once
	{
		tdiFailIrp((PTDI_CONN_REQUEST)pCloneRequest->m_pFltPacket->m_pBuffer);
		((PTDI_CONN_REQUEST)pCloneRequest->m_pFltPacket->m_pBuffer)->m_pIrp = NULL;
	}

	tdiRecycleRequest((PTDI_CONN_REQUEST)pCloneRequest->m_pFltPacket->m_pBuffer);

	ExFreeToNPagedLookasideList( &g_FwRequestsLookasideList, pCloneRequest );
}