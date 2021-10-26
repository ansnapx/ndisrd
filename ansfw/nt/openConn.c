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

NPROT_LOCK g_FreeAddrListLock;
LIST_ENTRY g_FreeAddrList;

NPROT_LOCK g_OpenAddrListLock;
LIST_ENTRY g_OpenAddrList;
LIST_ENTRY g_TcpAddrList;

NPROT_LOCK g_PendingAddrListLock;
LIST_ENTRY g_PendingAddrList;

BOOL	   g_DoAddrThread; 
PVOID	   g_AddrThreadObj; 
KEVENT	   g_AddrThreadEvent; 

VOID (NTAPI * ruleUpdateTDIRequest)(DWORD pid, 
								DWORD localAddr, 
								WORD localPort,
								BYTE protocol,
								BOOL bClose);

VOID addrThread( PVOID data );

VOID sendTdiRequest(DWORD pid, 
					DWORD localAddr, 
					WORD localPort,
					BYTE protocol,
					BOOL bClose)
{

	dprintf("sendTdiRequest Addr = 0x%p, &Addr = 0x%p\n", ruleUpdateTDIRequest, &ruleUpdateTDIRequest);

	__try
	{
		if(InterlockedCompareExchangePointer((PVOID *)&ruleUpdateTDIRequest, (PVOID)ruleUpdateTDIRequest, NULL) == NULL)
		{
			dprintf("sendTdiRequest NDIS is down 0x%x\n", ruleUpdateTDIRequest);
			return;
		}

		ruleUpdateTDIRequest(pid, localAddr, localPort, protocol, bClose);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		dprintf("sendTdiRequest() exception 0x%x\n", GetExceptionCode());
	}

}

VOID initLists( )
{
	NPROT_INIT_LOCK(&g_FreeAddrListLock);
	NPROT_INIT_LOCK(&g_OpenAddrListLock);
	NPROT_INIT_LOCK(&g_PendingAddrListLock);

	InitializeListHead(&g_FreeAddrList);
	InitializeListHead(&g_OpenAddrList);
	InitializeListHead(&g_PendingAddrList);	
	InitializeListHead(&g_TcpAddrList);	
}

VOID initAddrThread( )
{
	HANDLE 	hdl;

	KeInitializeEvent(&g_AddrThreadEvent, SynchronizationEvent, FALSE);

	g_DoAddrThread = TRUE;
	
	if (PsCreateSystemThread( &hdl, 0, NULL, NULL, NULL, addrThread, NULL ) != STATUS_SUCCESS) 
	{
		if (ObReferenceObjectByHandle( hdl, THREAD_ALL_ACCESS, NULL, KernelMode, &g_AddrThreadObj, NULL ) != STATUS_SUCCESS)
			g_AddrThreadObj = NULL;
		else
			ZwClose( hdl );
	}
}

VOID closeAddrThread( )
{
	g_DoAddrThread = FALSE;

	KeSetEvent( &g_AddrThreadEvent, IO_NO_INCREMENT, g_AddrThreadObj != NULL );

	if (g_AddrThreadObj != NULL) 
	{
		KeWaitForSingleObject( g_AddrThreadObj, Executive, KernelMode, FALSE, NULL );
		ObDereferenceObject( g_AddrThreadObj );
		g_AddrThreadObj = NULL;
	}
}

PTDI_OPEN_CONN tdiAllocOpenConn( )
{
	PTDI_OPEN_CONN connOn = NULL; 
 
	NPROT_ACQUIRE_LOCK( &g_FreeAddrListLock);
 
	if ( !IsListEmpty ( &g_FreeAddrList ) ) 
	{ 
		connOn = (PTDI_OPEN_CONN) RemoveHeadList ( &g_FreeAddrList ); 
		NPROT_RELEASE_LOCK( &g_FreeAddrListLock);
		NdisZeroMemory ( connOn, sizeof(TDI_OPEN_CONN) ); 
		return connOn; 
	} 
 
	NPROT_RELEASE_LOCK( &g_FreeAddrListLock);
 
	if(NdisAllocateMemoryWithTag ( 
			&connOn, 
			sizeof(TDI_OPEN_CONN), 
			'rtpR'
			) == NDIS_STATUS_SUCCESS) 
	{ 
		 
		NdisZeroMemory ( connOn, sizeof(TDI_OPEN_CONN) ); 
		return connOn; 
	} 
 
	return NULL;
}

VOID tdiRecycleOpenConn( PTDI_OPEN_CONN conn)
{
	NPROT_ACQUIRE_LOCK( &g_FreeAddrListLock);
	InsertTailList(&g_FreeAddrList, &conn->m_qLink);
	NPROT_RELEASE_LOCK( &g_FreeAddrListLock);
}

PTDI_OPEN_CONN tdiGetTcpObjectFromFileObject(PFILE_OBJECT fileobject, BOOL doLock)
{
	PTDI_OPEN_CONN connOn = NULL; 

	if(doLock)
	{
		NPROT_ACQUIRE_LOCK( &g_OpenAddrListLock);
	}

    connOn = (PTDI_OPEN_CONN)g_TcpAddrList.Flink;

    while( connOn != (PTDI_OPEN_CONN )&g_TcpAddrList )
    {
		if(connOn->m_pTcpFileObject == fileobject)
        {
			if(doLock)
			{
				NPROT_RELEASE_LOCK( &g_OpenAddrListLock);
			}

            return connOn;
        }

        connOn = (PTDI_OPEN_CONN )connOn->m_qLink.Flink;
    }

	if(doLock)
	{
		NPROT_RELEASE_LOCK( &g_OpenAddrListLock);
	}

	return NULL;
}

PTDI_OPEN_CONN tdiGetConnObjectFromFileObject(PFILE_OBJECT fileobject, BOOL doLock)
{
	PTDI_OPEN_CONN connOn = NULL; 

	if(doLock)
	{
		NPROT_ACQUIRE_LOCK( &g_OpenAddrListLock);
	}

    connOn = (PTDI_OPEN_CONN)g_OpenAddrList.Flink;

    while( connOn != (PTDI_OPEN_CONN )&g_OpenAddrList )
    {
        if(connOn->m_pFileObject == fileobject)
        {
			if(doLock)
			{
				NPROT_RELEASE_LOCK( &g_OpenAddrListLock);
			}

            return connOn;
        }

        connOn = (PTDI_OPEN_CONN )connOn->m_qLink.Flink;
    }

	if(doLock)
	{
		NPROT_RELEASE_LOCK( &g_OpenAddrListLock);
	}

	return NULL;
}

PTDI_OPEN_CONN tdiGetPendingConnObjectFromFileObject(PFILE_OBJECT fileobject, BOOL doLock)
{
	PTDI_OPEN_CONN connOn = NULL; 

	if(doLock)
	{
		NPROT_ACQUIRE_LOCK( &g_PendingAddrListLock);
	}

    connOn = (PTDI_OPEN_CONN)g_PendingAddrList.Flink;

    while( connOn != (PTDI_OPEN_CONN )&g_PendingAddrList )
    {
        if(connOn->m_pFileObject == fileobject)
        {
			if(doLock)
			{
				NPROT_RELEASE_LOCK( &g_PendingAddrListLock);
			}

            return connOn;
        }

        connOn = (PTDI_OPEN_CONN )connOn->m_qLink.Flink;
    }

	if(doLock)
	{
		NPROT_RELEASE_LOCK( &g_PendingAddrListLock);
	}

	return NULL;
}

BYTE tdiGetIPProtocol(PFILE_OBJECT fileObject)
{
	LONG rtn = IPPROTO_IP;
	WCHAR filename[260] = L"";
	UNICODE_STRING uNum;

	uNum.Buffer = filename;
	uNum.Length = 0;
	uNum.MaximumLength = sizeof(WCHAR) * 260;

	__try
	{
		RtlCopyUnicodeString(&uNum, &fileObject->FileName);

		if(filename[0] == L'\\')
		{
			uNum.Buffer = &filename[1];
			uNum.Length -= sizeof(WCHAR);
		}

		RtlUnicodeStringToInteger( &uNum, 10, &rtn );

		dprintf("tdiGetIPProtocol(%d)", rtn);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
	}
	
	return (BYTE)rtn;
}

VOID NTAPI GetPidFromLocalInfo(PFW_PID_REQUEST request)
{
	PTDI_OPEN_CONN connOn = NULL;
	DWORD pid = 0;

	dprintf("GetPidFromLocalInfo REQUEST(local = %d.%d.%d.%d:%d remote = %d.%d.%d.%d:%d Protocol = %d)\n", 
				PRINT_IP_ADDR((request->m_LocalAddr)), 
				ntohs((WORD)request->m_LocalPort),
				PRINT_IP_ADDR((request->m_RemoteAddr)), 
				ntohs((WORD)request->m_RemotePort),
				request->m_IpProto);

    NPROT_ACQUIRE_LOCK( &g_OpenAddrListLock);

	if((request->m_IpProto == IPPROTO_TCP) || (request->m_IpProto == IPPROTO_UDP))
	{
		connOn = (PTDI_OPEN_CONN)g_OpenAddrList.Flink;

		while( connOn != (PTDI_OPEN_CONN )&g_OpenAddrList )
		{
			dprintf("GetPidFromLocalInfo FIRST PASS (pid = %d proto = %d local = %d.%d.%d.%d:%d remote = %d.%d.%d.%d:%d )\n", 
						connOn->m_Pid, 
						connOn->m_IpProto,
						PRINT_IP_ADDR((connOn->m_LocalAddr)), 
						ntohs((WORD)connOn->m_LocalPort),
						PRINT_IP_ADDR((connOn->m_RemoteAddr)), 
						ntohs((WORD)connOn->m_RemotePort));

			if((connOn->m_LocalPort == request->m_LocalPort) && 
				(connOn->m_IpProto == request->m_IpProto) &&
				(connOn->m_LocalAddr == request->m_LocalAddr ) &&
				(connOn->m_RemotePort == request->m_RemotePort) &&
				(connOn->m_RemoteAddr == request->m_RemoteAddr ) )
			{
				dprintf("GetPidFromLocalInfo FOUND FIRST PASS(pid = %d proto = %d local = %d.%d.%d.%d:%d remote = %d.%d.%d.%d:%d )\n", 
								connOn->m_Pid, 
								connOn->m_IpProto,
								PRINT_IP_ADDR((connOn->m_LocalAddr)), 
								ntohs((WORD)connOn->m_LocalPort),
								PRINT_IP_ADDR((connOn->m_RemoteAddr)), 
								ntohs((WORD)connOn->m_RemotePort));

				request->m_Pid = connOn->m_Pid;
				request->m_Flags = 0;
				NPROT_RELEASE_LOCK( &g_OpenAddrListLock);
				return;
			}

			connOn = (PTDI_OPEN_CONN )connOn->m_qLink.Flink;
		}

		connOn = (PTDI_OPEN_CONN)g_OpenAddrList.Flink;

		while( connOn != (PTDI_OPEN_CONN )&g_OpenAddrList )
		{
			if((connOn->m_LocalPort == request->m_LocalPort) && 
				(connOn->m_IpProto == request->m_IpProto) &&
				((connOn->m_LocalAddr == 0) || (connOn->m_LocalAddr == request->m_LocalAddr )) &&
				(connOn->m_RemotePort == request->m_RemotePort) &&
				(connOn->m_RemoteAddr == request->m_RemoteAddr ) )
			{

				dprintf("GetPidFromLocalInfo FOUND SECOND PASS (pid = %d proto = %d local = %d.%d.%d.%d:%d remote = %d.%d.%d.%d:%d )\n", 
						connOn->m_Pid, 
						connOn->m_IpProto,
						PRINT_IP_ADDR((connOn->m_LocalAddr)), 
						ntohs((WORD)connOn->m_LocalPort),
						PRINT_IP_ADDR((connOn->m_RemoteAddr)), 
						ntohs((WORD)connOn->m_RemotePort));

				request->m_Pid = connOn->m_Pid;
				request->m_Flags = 0;
				NPROT_RELEASE_LOCK( &g_OpenAddrListLock);
				return;
			}

			connOn = (PTDI_OPEN_CONN )connOn->m_qLink.Flink;
		}

		connOn = (PTDI_OPEN_CONN)g_OpenAddrList.Flink;

		while( connOn != (PTDI_OPEN_CONN )&g_OpenAddrList )
		{
			if((connOn->m_LocalPort == request->m_LocalPort) && 
				(connOn->m_IpProto == request->m_IpProto) &&
				((connOn->m_LocalAddr == 0) || (connOn->m_LocalAddr == request->m_LocalAddr )) )
			{

				dprintf("GetPidFromLocalInfo FOUND THIRD PASS (pid = %d proto = %d local = %d.%d.%d.%d:%d remote = %d.%d.%d.%d:%d )\n", 
						connOn->m_Pid, 
						connOn->m_IpProto,
						PRINT_IP_ADDR((connOn->m_LocalAddr)), 
						ntohs((WORD)connOn->m_LocalPort),
						PRINT_IP_ADDR((connOn->m_RemoteAddr)), 
						ntohs((WORD)connOn->m_RemotePort));

				request->m_Pid = connOn->m_Pid;
				request->m_Flags = 0;
				NPROT_RELEASE_LOCK( &g_OpenAddrListLock);
				return;
			}

			connOn = (PTDI_OPEN_CONN )connOn->m_qLink.Flink;
		}
	}
	else
	{

		connOn = (PTDI_OPEN_CONN)g_OpenAddrList.Flink;

		while( connOn != (PTDI_OPEN_CONN )&g_OpenAddrList )
		{
			if((connOn->m_IpProto == request->m_IpProto) &&
			   (connOn->m_RemoteAddr == request->m_RemoteAddr ))
			{
				dprintf("GetPidFromLocalInfo FOUND RAW PASS (pid = %d proto = %d local = %d.%d.%d.%d remote = %d.%d.%d.%d)\n", 
						connOn->m_Pid, 
						connOn->m_IpProto,
						PRINT_IP_ADDR((connOn->m_LocalAddr)), 
						PRINT_IP_ADDR((connOn->m_RemoteAddr)));

				request->m_Pid = connOn->m_Pid;
				request->m_Flags = 0;
				NPROT_RELEASE_LOCK( &g_OpenAddrListLock);
				return;
			}

			connOn = (PTDI_OPEN_CONN )connOn->m_qLink.Flink;
		}

		connOn = (PTDI_OPEN_CONN)g_OpenAddrList.Flink;

		while( connOn != (PTDI_OPEN_CONN )&g_OpenAddrList )
		{
			if((connOn->m_IpProto == 255) &&
			   (connOn->m_RemoteAddr == request->m_RemoteAddr ))
			{
				dprintf("GetPidFromLocalInfo FOUND RAW SECOND PASS (pid = %d proto = %d local = %d.%d.%d.%d remote = %d.%d.%d.%d)\n", 
						connOn->m_Pid, 
						connOn->m_IpProto,
						PRINT_IP_ADDR((connOn->m_LocalAddr)), 
						PRINT_IP_ADDR((connOn->m_RemoteAddr)));

				request->m_Pid = connOn->m_Pid;
				request->m_Flags = 0;
				NPROT_RELEASE_LOCK( &g_OpenAddrListLock);
				return;
			}

			connOn = (PTDI_OPEN_CONN )connOn->m_qLink.Flink;
		}
	}

    NPROT_RELEASE_LOCK( &g_OpenAddrListLock);
}

USHORT _htons( USHORT hostshort ) {

	PUCHAR pBuffer = (PUCHAR )&hostshort;
	return ( (pBuffer[ 0 ] << 8) & 0xFF00 ) | ( pBuffer[ 1 ] & 0x00FF );
}

VOID tdiList( )
{
	PTDI_OPEN_CONN connOn = NULL;
	
    NPROT_ACQUIRE_LOCK( &g_OpenAddrListLock);

    connOn = (PTDI_OPEN_CONN)g_OpenAddrList.Flink;

    while( connOn != (PTDI_OPEN_CONN )&g_OpenAddrList )
    {
		dprintf("* Flags = 0x%x pid = %d, IpProto = %d localAddr = %d.%d.%d.%d, localPort = %d)\n",
			connOn->m_Flags,
			connOn->m_Pid, 
			connOn->m_IpProto,
			PRINT_IP_ADDR(connOn->m_LocalAddr), 
			_htons(connOn->m_LocalPort));

        connOn = (PTDI_OPEN_CONN )connOn->m_qLink.Flink;
    }

    NPROT_RELEASE_LOCK( &g_OpenAddrListLock);
}

VOID SendNtQueryInformationRequestAddr( );
VOID SendTdiUpdateRequestAddr( );
VOID SendTdiActionOkAddr( );

VOID addrThread( PVOID data )
{
	KeSetPriorityThread(KeGetCurrentThread(), LOW_REALTIME_PRIORITY);

	//SendNtQueryInformationRequestAddr( );
	//SendTdiUpdateRequestAddr( );
	//SendTdiActionOkAddr( );
	while(g_DoAddrThread)
	{
		KeWaitForSingleObject( &g_AddrThreadEvent, Executive, KernelMode, FALSE, NULL );

		if(!g_DoAddrThread)
			break;

		NPROT_ACQUIRE_LOCK( &g_PendingAddrListLock);

		while(! IsListEmpty(&g_PendingAddrList))
		{
			PTDI_OPEN_CONN pTcpObj, pAddrObject;
			PTDI_OPEN_CONN connOn = (PTDI_OPEN_CONN)RemoveHeadList(&g_PendingAddrList);

			dprintf("addrThread( pid = %d)\n", connOn->m_Pid);
			NPROT_RELEASE_LOCK( &g_PendingAddrListLock);

			getLocalInfo(connOn->m_pFileObject, &connOn->m_LocalPort, &connOn->m_LocalAddr);

			IoCompleteRequest( connOn->m_pIrp, IO_NO_INCREMENT );

			connOn->m_pIrp = NULL;
			connOn->m_Flags &= ~FL_PENDING;

			if(connOn->m_Flags & FL_LISTEN)
			{
				sendTdiRequest(connOn->m_Pid,
					  connOn->m_LocalAddr,
					  connOn->m_LocalPort,
					  connOn->m_IpProto,
					  FALSE);

				connOn->m_Flags &= ~FL_LISTEN;
			}

			NPROT_ACQUIRE_LOCK( &g_OpenAddrListLock);

			pTcpObj = tdiGetTcpObjectFromFileObject(connOn->m_pFileObject, FALSE);

			if( pTcpObj && pTcpObj->m_pAddrObj)
				pAddrObject = (PTDI_OPEN_CONN)pTcpObj->m_pAddrObj;
			else
				pAddrObject = tdiGetConnObjectFromFileObject(connOn->m_pFileObject, FALSE);

			if(pAddrObject)
			{
				pAddrObject->m_LocalPort = connOn->m_LocalPort;
				pAddrObject->m_LocalAddr = connOn->m_LocalAddr;
				RemoveEntryList(&connOn->m_qLink);
				tdiRecycleOpenConn(connOn);
			}
			else
			{
				InsertTailList(&g_OpenAddrList, &connOn->m_qLink);
			}

			NPROT_RELEASE_LOCK( &g_OpenAddrListLock);

			NPROT_ACQUIRE_LOCK( &g_PendingAddrListLock);
		}

		NPROT_RELEASE_LOCK( &g_PendingAddrListLock);
	}

	PsTerminateSystemThread(STATUS_SUCCESS);
}

/////////////////////////////////////////////////////////////////////////////
//// TDIH_TdiOpenAddressComplete
//
// Purpose
//
// Parameters
//
// Return Value
// 
// Remarks
//

NTSTATUS
TDIH_TdiOpenAddressComplete(
   PDEVICE_OBJECT    pDeviceObject,
   PIRP              Irp,
   void              *Context
   )
{
   PTDI_OPEN_CONN		   pConnObj = NULL;
   PTDI_DEV_EXT			   pTDIH_DeviceExtension = NULL;


   if (Irp->PendingReturned)
      IoMarkIrpPending(Irp);

   pConnObj = (PTDI_OPEN_CONN )Context;

   if(pConnObj == NULL)
   {
	   dprintf("TDIH_TdiOpenAddressComplete null conn obj passed with the context\n");
	   return STATUS_SUCCESS;
   }
   
   pTDIH_DeviceExtension = pConnObj->m_pDeviceExtension;

   if (pTDIH_DeviceExtension->pFilterDeviceObject != pDeviceObject)
   {
	   dprintf("TDIH_TdiOpenAddressComplete Invalid Device Object Pointer\n");
	   tdiRecycleOpenConn(pConnObj);
       return STATUS_SUCCESS;
   }

   if( NT_SUCCESS( Irp->IoStatus.Status ) )
   {
	   if(KeGetCurrentIrql() == PASSIVE_LEVEL)
	   {
		   dprintf("TDIH_TdiOpenAddressComplete: PASSIVE_LEVEL\n");
		   getLocalInfo(pConnObj->m_pFileObject, &pConnObj->m_LocalPort, &pConnObj->m_LocalAddr);

		   NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);
		   InsertTailList(&g_OpenAddrList, &pConnObj->m_qLink);
		   NPROT_RELEASE_LOCK(&g_OpenAddrListLock);

	   }
	   else
	   {
		  dprintf("TDIH_TdiOpenAddressComplete: DISPATCH_LEVEL\n");

		  pConnObj->m_pIrp = Irp;
		  pConnObj->m_Flags |= FL_PENDING;

		  NPROT_ACQUIRE_LOCK(&g_PendingAddrListLock);
		  InsertTailList(&g_PendingAddrList, &pConnObj->m_qLink);
		  NPROT_RELEASE_LOCK(&g_PendingAddrListLock);
		
		  KeSetEvent( &g_AddrThreadEvent, IO_NO_INCREMENT, FALSE );

		  return STATUS_MORE_PROCESSING_REQUIRED;
	   }
   }
   else
   {
	   tdiRecycleOpenConn(pConnObj);
   }

   return STATUS_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////
//// TDIH_TdiOpenAddress
//
// Purpose
// This is the hook for TdiOpenAddress
//
// Parameters
//
// Return Value
// 
// Remarks
//

NTSTATUS
TDIH_TdiOpenAddress(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   )
{
	PTDI_OPEN_CONN pConnObj = NULL;

	if (Irp->CurrentLocation <= 1)
	{
		dprintf("TDIH_TdiOpenAddress: BOGUS stack\n" );
		IoSkipCurrentIrpStackLocation( Irp );
		return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
	}

	pConnObj = tdiAllocOpenConn( );

	if(pConnObj)
	{
	   if( pTDIH_DeviceExtension->NodeIdentifier.NodeType == TDIH_NODE_TYPE_TCP_FILTER_DEVICE )
	   {
		   pConnObj->m_IpProto = IPPROTO_TCP;
	   }
	   else if( pTDIH_DeviceExtension->NodeIdentifier.NodeType == TDIH_NODE_TYPE_UDP_FILTER_DEVICE )
	   {
		   pConnObj->m_IpProto = IPPROTO_UDP;
	   }
	   else if( pTDIH_DeviceExtension->NodeIdentifier.NodeType == TDIH_NODE_TYPE_RAWIP_FILTER_DEVICE )
	   {
		   pConnObj->m_IpProto = tdiGetIPProtocol(IrpSp->FileObject);
		   dprintf("TDIH_TdiOpenAddress(%d) RAW(%d)\n", 
				HandleToUlong(PsGetCurrentProcessId()), 
				pConnObj->m_IpProto);
	   }
	   else if( pTDIH_DeviceExtension->NodeIdentifier.NodeType == TDIH_NODE_TYPE_MCAST_FILTER_DEVICE )
	   {
		   pConnObj->m_IpProto = 255;
		   dprintf("TDIH_TdiOpenAddress(%d) MCAST(%d)\n", 
				HandleToUlong(PsGetCurrentProcessId()), 
				pConnObj->m_IpProto);
	   }

	   pConnObj->m_pFileObject = IrpSp->FileObject;
	   pConnObj->m_pDeviceExtension = pTDIH_DeviceExtension;
	   pConnObj->m_Pid = HandleToUlong(PsGetCurrentProcessId());

	   dprintf( "TDIH_TdiOpenAddress PidCheck: %d\n", pConnObj->m_Pid );
	}

	IoCopyCurrentIrpStackLocationToNext( Irp );

    IoSetCompletionRoutine(
         Irp,
         TDIH_TdiOpenAddressComplete,
         pConnObj,
         TRUE,
         TRUE,
         TRUE
         );

	return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
}


/////////////////////////////////////////////////////////////////////////////
//// TDIH_TdiCloseAddressComplete
//
// Purpose
//
// Parameters
//
// Return Value
// 
// Remarks
//

NTSTATUS
TDIH_TdiCloseAddressComplete(
   PDEVICE_OBJECT    pDeviceObject,
   PIRP              Irp,
   void              *Context
   )
{
   PTDI_DEV_EXT			   pTDIH_DeviceExtension = NULL;

   dprintf("TDIH_TdiCloseAddressComplete: Status: 0x%x\n", Irp->IoStatus.Status);

   //
   // Propogate The IRP Pending Flag
   //
   if (Irp->PendingReturned)
      IoMarkIrpPending(Irp);

   pTDIH_DeviceExtension = (PTDI_DEV_EXT )Context;

    if(pTDIH_DeviceExtension == NULL)
   {
	   dprintf("TDIH_TdiCloseAddressComplete null conn obj passed with the context\n");
	   return(STATUS_SUCCESS);
   }

   if (pTDIH_DeviceExtension->pFilterDeviceObject != pDeviceObject)
   {
	   dprintf("TDIH_TdiCloseAddressComplete Invalid Device Object Pointer\n");
       return(STATUS_SUCCESS);
   }

   if(NT_SUCCESS(Irp->IoStatus.Status))
   {
	   PIO_STACK_LOCATION      IrpSp = NULL;
	   PTDI_OPEN_CONN		   pConnObj = NULL;

	   IrpSp = IoGetCurrentIrpStackLocation(Irp);
	   
	   NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);

	   pConnObj = tdiGetConnObjectFromFileObject(IrpSp->FileObject, FALSE);

	   if( pConnObj )
	   {
		   dprintf("TDIH_TdiCloseAddressComplete pConnObj found in open list pid = %d\n", pConnObj->m_Pid);
		   RemoveEntryList(&pConnObj->m_qLink);

		   if(pConnObj->m_pTcpObj)
		   {
			   dprintf("WARNING! TDIH_TdiCloseAddressComplete pConnObj has a connection !!!!\n", pConnObj->m_Pid);
			   ((PTDI_OPEN_CONN)(pConnObj->m_pTcpObj))->m_pAddrObj = NULL;
		   }
	   }

	   NPROT_RELEASE_LOCK(&g_OpenAddrListLock);

	   if(pConnObj == NULL)
	   {
		   dprintf("TDIH_TdiCloseAddressComplete checking pending objects\n");
		   
		   NPROT_ACQUIRE_LOCK(&g_PendingAddrListLock);

		   pConnObj = tdiGetPendingConnObjectFromFileObject(IrpSp->FileObject, FALSE);

		   if(pConnObj)
		   {
			   dprintf("TDIH_TdiCloseAddressComplete pConnObj found in pending list pid = %d\n", pConnObj->m_Pid);
			   RemoveEntryList(&pConnObj->m_qLink);

			   if(pConnObj->m_pTcpObj)
			   {
				   dprintf("WARNING! TDIH_TdiCloseAddressComplete pending pConnObj has a connection !!!!\n", pConnObj->m_Pid);
				   ((PTDI_OPEN_CONN)(pConnObj->m_pTcpObj))->m_pAddrObj = NULL;
			   }
		   }

		   NPROT_RELEASE_LOCK(&g_PendingAddrListLock);
	   }

	   if(pConnObj)
	   {
		   sendTdiRequest(pConnObj->m_Pid,
						  pConnObj->m_LocalAddr,
						  pConnObj->m_LocalPort,
						  pConnObj->m_IpProto,
						  TRUE);
		   tdiRecycleOpenConn(pConnObj);
	   }
   }

   return(STATUS_SUCCESS);
}


/////////////////////////////////////////////////////////////////////////////
//// TDIH_TdiCloseAddress
//
// Purpose
// This is the hook for TdiCloseAddress
//
// Parameters
//
// Return Value
// 
// Remarks
//

NTSTATUS
TDIH_TdiCloseAddress(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   )
{
	dprintf("TDIH_TdiCloseAddress: Entry...\n" );

	if (Irp->CurrentLocation <= 1)
	{
		dprintf("TDIH_TdiCloseAddress: BOGUS stack\n" );
		IoSkipCurrentIrpStackLocation( Irp );
		return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
	}

    IoCopyCurrentIrpStackLocationToNext( Irp );

	IoSetCompletionRoutine(
	 Irp,
	 TDIH_TdiCloseAddressComplete,
	 pTDIH_DeviceExtension,
	 TRUE,
	 TRUE,
	 TRUE
	 );

	return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
}

/////////////////////////////////////////////////////////////////////////////
//// TDIH_TdiOpenConnectionComplete
//
// Purpose
//
// Parameters
//
// Return Value
// 
// Remarks
//

NTSTATUS
TDIH_TdiOpenConnectionComplete(
   PDEVICE_OBJECT    pDeviceObject,
   PIRP              Irp,
   void              *Context
   )
{
   PTDI_OPEN_CONN		   pConnObj = NULL;
   PTDI_DEV_EXT			   pTDIH_DeviceExtension = NULL;

   dprintf("TDIH_TdiOpenConnectionComplete: Status: 0x%x\n", Irp->IoStatus.Status);

   if (Irp->PendingReturned)
      IoMarkIrpPending(Irp);

   pConnObj = (PTDI_OPEN_CONN )Context;

   if(pConnObj == NULL)
   {
	   dprintf("TDIH_TdiOpenConnectionComplete null conn obj passed with the context\n");
	   return STATUS_SUCCESS;
   }
   
   pTDIH_DeviceExtension = pConnObj->m_pDeviceExtension;

   if (pTDIH_DeviceExtension->pFilterDeviceObject != pDeviceObject)
   {
	   dprintf("TDIH_TdiOpenConnectionComplete Invalid Device Object Pointer\n");
	   tdiRecycleOpenConn(pConnObj);
       return STATUS_SUCCESS;
   }

   if( NT_SUCCESS( Irp->IoStatus.Status ) )
   {
	   NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);
	   InsertTailList(&g_TcpAddrList, &pConnObj->m_qLink);
	   NPROT_RELEASE_LOCK(&g_OpenAddrListLock);
   }
   else
   {
	   tdiRecycleOpenConn(pConnObj);
   }
	  
   return STATUS_SUCCESS;
}


/////////////////////////////////////////////////////////////////////////////
//// TDIH_TdiOpenConnection
//
// Purpose
// This is the hook for TdiOpenConnection
//
// Parameters
//
// Return Value
// 
// Remarks
//

NTSTATUS
TDIH_TdiOpenConnection(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp,
   PVOID                   ConnectionContext
   )
{
	PTDI_OPEN_CONN pConnObj = NULL;

	dprintf("TDIH_TdiOpenConnection: Entry\n" );

	if (Irp->CurrentLocation <= 1)
	{
		dprintf("TDIH_TdiOpenConnection: BOGUS stack\n" );
		IoSkipCurrentIrpStackLocation( Irp );
		return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
	}

	pConnObj = tdiAllocOpenConn( );

	if(pConnObj)
	{
		pConnObj->m_pTcpFileObject = IrpSp->FileObject;
		pConnObj->m_pDeviceExtension = pTDIH_DeviceExtension;
		pConnObj->m_Pid = HandleToUlong(PsGetCurrentProcessId());
		pConnObj->m_IpProto = IPPROTO_TCP;

		dprintf( "TDIH_TdiOpenConnection PidCheck: %d\n", pConnObj->m_Pid );
	}

	IoCopyCurrentIrpStackLocationToNext( Irp );

    IoSetCompletionRoutine(
         Irp,
         TDIH_TdiOpenConnectionComplete,
         pConnObj,
         TRUE,
         TRUE,
         TRUE
         );

	return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
}


/////////////////////////////////////////////////////////////////////////////
//// TDIH_TdiCloseConnectionComplete
//
// Purpose
//
// Parameters
//
// Return Value
// 
// Remarks
//

NTSTATUS
TDIH_TdiCloseConnectionComplete(
   PDEVICE_OBJECT    pDeviceObject,
   PIRP              Irp,
   void              *Context
   )
{
	PTDI_DEV_EXT			pTDIH_DeviceExtension = NULL;
	NTSTATUS                Status = Irp->IoStatus.Status;

	dprintf("TDIH_TdiCloseConnectionComplete: Status: 0x%x\n", Status);

	if (Irp->PendingReturned)
	  IoMarkIrpPending(Irp);

	pTDIH_DeviceExtension = (PTDI_DEV_EXT )Context;

	if(pTDIH_DeviceExtension == NULL)
	{
	   dprintf("TDIH_TdiCloseConnectionComplete null conn obj passed with the context\n");
	   return(STATUS_SUCCESS);
	}

	if (pTDIH_DeviceExtension->pFilterDeviceObject != pDeviceObject)
	{
	   dprintf("TDIH_TdiCloseConnectionComplete Invalid Device Object Pointer\n");
	   return(STATUS_SUCCESS);
	}

	if(NT_SUCCESS(Status))
	{
		PIO_STACK_LOCATION      IrpSp = NULL;
		PTDI_OPEN_CONN		    pConnObj = NULL;

		IrpSp = IoGetCurrentIrpStackLocation(Irp);

		NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);

		pConnObj = tdiGetTcpObjectFromFileObject(IrpSp->FileObject, FALSE);

		if( pConnObj )
		{
			dprintf("TDIH_TdiCloseConnectionComplete TCP conn found in list pid = %d.\n", pConnObj->m_Pid );

		   if(pConnObj->m_pAddrObj)
		   {
			    dprintf("TDIH_TdiCloseConnectionComplete m_pAddrObj = is not null. This should not happen after Disassociate\n");
				((PTDI_OPEN_CONN)pConnObj->m_pAddrObj)->m_pTcpObj = NULL;
				((PTDI_OPEN_CONN)pConnObj->m_pAddrObj)->m_Flags &= ~FL_ATTACHED;
		   }

		   RemoveEntryList(&pConnObj->m_qLink);
		}

		NPROT_RELEASE_LOCK(&g_OpenAddrListLock);

		if(pConnObj)
		{
			sendTdiRequest(pConnObj->m_Pid,
						  pConnObj->m_LocalAddr,
						  pConnObj->m_LocalPort,
						  pConnObj->m_IpProto,
						  TRUE);
		   tdiRecycleOpenConn(pConnObj);
		}
	}

	return(STATUS_SUCCESS);
}


/////////////////////////////////////////////////////////////////////////////
//// TDIH_TdiCloseConnection
//
// Purpose
// This is the hook for TdiCloseConnection
//
// Parameters
//
// Return Value
// 
// Remarks
//

NTSTATUS
TDIH_TdiCloseConnection(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   )
{

	dprintf("TDIH_TdiCloseConnection: Entry...\n" );

	if (Irp->CurrentLocation <= 1)
	{
		dprintf("TDIH_TdiCloseConnection: BOGUS stack\n" );
		IoSkipCurrentIrpStackLocation( Irp );
		return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
	}

    IoCopyCurrentIrpStackLocationToNext( Irp );

	IoSetCompletionRoutine(
	 Irp,
	 TDIH_TdiCloseConnectionComplete,
	 pTDIH_DeviceExtension,
	 TRUE,
	 TRUE,
	 TRUE
	 );

	return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
}

NTSTATUS
TDIH_TdiAssociateAddressComplete(
   PDEVICE_OBJECT    pDeviceObject,
   PIRP              Irp,
   void              *Context
   )
{
   PTDI_OPEN_CONN	pConnObj = NULL;
   PTDI_OPEN_CONN	pTcpObj = NULL;
   PTDI_DEV_EXT		pTDIH_DeviceExtension = NULL;

   dprintf("TDIH_TdiAssociateAddressComplete: Status: 0x%x\n", Irp->IoStatus.Status);

   if (Irp->PendingReturned)
      IoMarkIrpPending(Irp);

   pConnObj = (PTDI_OPEN_CONN )Context;

   if(pConnObj == NULL)
   {
	   dprintf("TDIH_TdiAssociateAddressComplete null pConnObj passed with the context\n");
	   return(STATUS_SUCCESS);
   }

   pTDIH_DeviceExtension = (PTDI_DEV_EXT)pConnObj->m_pDeviceExtension;

   if (pTDIH_DeviceExtension->pFilterDeviceObject != pDeviceObject)
	{
	   dprintf("TDIH_TdiAssociateAddressComplete Invalid Device Object Pointer\n");

	   NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);
	   InsertTailList(&g_OpenAddrList, &pConnObj->m_qLink);
	   NPROT_RELEASE_LOCK(&g_OpenAddrListLock);

	   return(STATUS_SUCCESS);
	}

   if( NT_SUCCESS( Irp->IoStatus.Status ) )
   {
	   PIO_STACK_LOCATION IrpSp = NULL;

	   IrpSp = IoGetCurrentIrpStackLocation(Irp);

	   NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);

	   pTcpObj = tdiGetTcpObjectFromFileObject(IrpSp->FileObject, FALSE);

	   if( pTcpObj )
	   {
		   dprintf("TDIH_TdiAssociateAddressComplete TCP conn found in list pid = %d\n", pTcpObj->m_Pid);
		   pTcpObj->m_pAddrObj = pConnObj;
		   pConnObj->m_pTcpObj = pTcpObj;
		   pConnObj->m_Flags |= FL_ATTACHED;
	   }

       InsertTailList(&g_OpenAddrList, &pConnObj->m_qLink);

	   NPROT_RELEASE_LOCK(&g_OpenAddrListLock);
   }
   else
   {
	   NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);
	   InsertTailList(&g_OpenAddrList, &pConnObj->m_qLink);
	   NPROT_RELEASE_LOCK(&g_OpenAddrListLock);
	   dprintf("TDIH_TdiAssociateAddressComplete: FAILED Irp\n");
   }

   
   return(STATUS_SUCCESS);
}


/////////////////////////////////////////////////////////////////////////////
//// TDIH_TdiAssociateAddress
//
// Purpose
// This is the hook for TdiAssociateAddress
//
// Parameters
//
// Return Value
// 
// Remarks
//

NTSTATUS
TDIH_TdiAssociateAddress(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   )
{
	PTDI_REQUEST_KERNEL_ASSOCIATE	pRKA = NULL;
	PFILE_OBJECT pFileObject = NULL;
	PTDI_OPEN_CONN pConnObj = NULL;

	if (Irp->CurrentLocation <= 1)
	{
		dprintf("TDIH_TdiAssociateAddress: BOGUS stack\n" );
		IoSkipCurrentIrpStackLocation( Irp );
		return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
	}

	pRKA = (PTDI_REQUEST_KERNEL_ASSOCIATE) &IrpSp->Parameters;

	if( NT_SUCCESS( ObReferenceObjectByHandle( pRKA->AddressHandle, FILE_ANY_ACCESS, NULL,
						KernelMode, (PVOID *) &pFileObject, NULL ) ) ) 
	{
	    NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);

	    pConnObj = tdiGetConnObjectFromFileObject(pFileObject, FALSE);

	    if(pConnObj)
	    {
		   dprintf("TDIH_TdiAssociateAddress object associated: pid = %d, currentpid = %d addr = 0x%x, port = 0x%x,  proto = %d\n",
			   pConnObj->m_Pid,
			   PsGetCurrentProcessId(),
			   pConnObj->m_LocalAddr,
			   pConnObj->m_LocalPort,
			   pConnObj->m_IpProto);

		   RemoveEntryList(&pConnObj->m_qLink);
	    }

	   NPROT_RELEASE_LOCK(&g_OpenAddrListLock);

	   ObDereferenceObject( pFileObject );
	}

	IoCopyCurrentIrpStackLocationToNext( Irp );

	IoSetCompletionRoutine(
		Irp,
		TDIH_TdiAssociateAddressComplete,
		pConnObj,
		TRUE,
		TRUE,
		TRUE
		);

	return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
}

/////////////////////////////////////////////////////////////////////////////
//// TDIH_TdiDisAssociateAddressComplete
//
// Purpose
//
// Parameters
//
// Return Value
// 
// Remarks
//

NTSTATUS
TDIH_TdiDisAssociateAddressComplete(
   PDEVICE_OBJECT    pDeviceObject,
   PIRP              Irp,
   void              *Context
   )
{
   PTDI_DEV_EXT		pTDIH_DeviceExtension = NULL;

   dprintf("TDIH_TdiDisAssociateAddressComplete: Status: 0x%x\n", Irp->IoStatus.Status);

   if (Irp->PendingReturned)
      IoMarkIrpPending(Irp);

   pTDIH_DeviceExtension = (PTDI_DEV_EXT )Context;

   if (pTDIH_DeviceExtension->pFilterDeviceObject != pDeviceObject)
	{
	   dprintf("TDIH_TdiDisAssociateAddressComplete Invalid Device Object Pointer\n");

	   return(STATUS_SUCCESS);
	}

   if( NT_SUCCESS( Irp->IoStatus.Status ) )
   {
	   PTDI_OPEN_CONN pTcpObj = NULL;
	   PIO_STACK_LOCATION IrpSp = NULL;

	   IrpSp = IoGetCurrentIrpStackLocation(Irp);

	   NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);

	   pTcpObj = tdiGetTcpObjectFromFileObject(IrpSp->FileObject, FALSE);

	   if( pTcpObj )
	   {
		   dprintf("TDIH_TdiDisAssociateAddressComplete TCP conn found in list pid = %d\n", pTcpObj->m_Pid);

		   if(pTcpObj->m_pAddrObj)
		   {
			   dprintf("TDIH_TdiDisAssociateAddressComplete unlinking address object\n");
			   ((PTDI_OPEN_CONN)pTcpObj->m_pAddrObj)->m_pTcpObj = NULL;
			   ((PTDI_OPEN_CONN)pTcpObj->m_pAddrObj)->m_Flags &= ~FL_ATTACHED;
		   }
		   else
		   {
			   dprintf("TDIH_TdiDisAssociateAddressComplete NULL address object. Something is wrong. Address object must be closed before.\n");
		   }

		   pTcpObj->m_pAddrObj = NULL;
	   }

	   NPROT_RELEASE_LOCK(&g_OpenAddrListLock);
   }
   
   return(STATUS_SUCCESS);
}


/////////////////////////////////////////////////////////////////////////////
//// TDIH_TdiDisAssociateAddress
//
// Purpose
// This is the hook for TdiDisAssociateAddress
//
// Parameters
//
// Return Value
// 
// Remarks
//

NTSTATUS
TDIH_TdiDisAssociateAddress(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   )
{

	if (Irp->CurrentLocation <= 1)
	{
		dprintf("TDIH_TdiDisAssociateAddress: BOGUS stack\n" );
		IoSkipCurrentIrpStackLocation( Irp );
		return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
	}

	IoCopyCurrentIrpStackLocationToNext( Irp );

	IoSetCompletionRoutine(
		Irp,
		TDIH_TdiDisAssociateAddressComplete,
		pTDIH_DeviceExtension,
		TRUE,
		TRUE,
		TRUE
		);

	return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
}

/////////////////////////////////////////////////////////////////////////////
//// TDIH_TdiConnectComplete
//
// Purpose
//
// Parameters
//
// Return Value
// 
// Remarks
//

NTSTATUS
TDIH_TdiConnectComplete1(
   PDEVICE_OBJECT    pDeviceObject,
   PIRP              Irp,
   void              *Context
   )
{
   PTDI_DEV_EXT		pTDIH_DeviceExtension = NULL;
   PTDI_OPEN_CONN   pAddrObject = NULL;

   dprintf("TDIH_TdiConnectComplete1: Status: 0x%x Irp = 0x%x(%d)\n", Irp->IoStatus.Status, Irp, Irp->PendingReturned);

   if (Irp->PendingReturned)
      IoMarkIrpPending(Irp);

   pTDIH_DeviceExtension = (PTDI_DEV_EXT )Context;

   if (pTDIH_DeviceExtension->pFilterDeviceObject != pDeviceObject)
	{
	   dprintf("TDIH_TdiConnectComplete1 Invalid Device Object Pointer\n");

	   return STATUS_SUCCESS;
	}

   if( NT_SUCCESS( Irp->IoStatus.Status ) )
   {
	   PTDI_OPEN_CONN pTcpObj = NULL;
	   PIO_STACK_LOCATION IrpSp = NULL;

	   IrpSp = IoGetCurrentIrpStackLocation(Irp);

	   //NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);

	   //pTcpObj = tdiGetTcpObjectFromFileObject(IrpSp->FileObject, FALSE);

	   //if( pTcpObj && pTcpObj->m_pAddrObj)
		  // pAddrObject = (PTDI_OPEN_CONN)pTcpObj->m_pAddrObj;
	   //else
		  // pAddrObject = tdiGetConnObjectFromFileObject(IrpSp->FileObject, FALSE);

	   //if(pAddrObject)
		  // RemoveEntryList(&pAddrObject->m_qLink);

	   //NPROT_RELEASE_LOCK(&g_OpenAddrListLock);

	   if(pAddrObject)
	   {
		   if(KeGetCurrentIrql() == PASSIVE_LEVEL)
		   {
			   WORD wLocalPort;
			   DWORD dwLocalAddr;
			   getLocalInfo(IrpSp->FileObject, &wLocalPort, &dwLocalAddr);

			   NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);
			   pTcpObj = tdiGetTcpObjectFromFileObject(IrpSp->FileObject, FALSE);

			   if( pTcpObj && pTcpObj->m_pAddrObj)
				   pAddrObject = (PTDI_OPEN_CONN)pTcpObj->m_pAddrObj;
			   else
				   pAddrObject = tdiGetConnObjectFromFileObject(IrpSp->FileObject, FALSE);

			   if(pAddrObject)
				   InsertTailList(&g_OpenAddrList, &pAddrObject->m_qLink);
			   NPROT_RELEASE_LOCK(&g_OpenAddrListLock);
		   }
		   else
		   {
			   PTDI_OPEN_CONN pNewAddrObject = tdiAllocOpenConn();
			   if(pNewAddrObject)
			   {
				   pNewAddrObject->m_pFileObject = IrpSp->FileObject;
				   pNewAddrObject->m_Flags |= FL_PENDING;
				   pNewAddrObject->m_pIrp = Irp;

				   NPROT_ACQUIRE_LOCK(&g_PendingAddrListLock);
				   InsertTailList(&g_PendingAddrList, &pNewAddrObject->m_qLink);
				   NPROT_RELEASE_LOCK(&g_PendingAddrListLock);

				   KeSetEvent( &g_AddrThreadEvent, IO_NO_INCREMENT, FALSE );

				   return STATUS_MORE_PROCESSING_REQUIRED;
			   }
		   }
	   }
   }
   
   return STATUS_SUCCESS;
}

NTSTATUS
TDIH_TdiConnectComplete2(
   PDEVICE_OBJECT    pDeviceObject,
   PIRP              Irp,
   void              *Context
   )
{
   PTDI_DEV_EXT		pTDIH_DeviceExtension = NULL;
   PTDI_OPEN_CONN   pAddrObject = NULL;

   dprintf("TDIH_TdiConnectComplete2: Status: 0x%x Irp = 0x%x(%d)\n", Irp->IoStatus.Status, Irp, Irp->PendingReturned);

   //if (Irp->PendingReturned)
   //   IoMarkIrpPending(Irp);

   pTDIH_DeviceExtension = (PTDI_DEV_EXT )Context;

   if (pTDIH_DeviceExtension->pFilterDeviceObject != pDeviceObject)
	{
	   dprintf("TDIH_TdiConnectComplete2 Invalid Device Object Pointer\n");

	   return STATUS_SUCCESS;
	}

   if( NT_SUCCESS( Irp->IoStatus.Status ) )
   {
	   PTDI_OPEN_CONN pTcpObj = NULL;
	   PIO_STACK_LOCATION IrpSp = NULL;

	   IrpSp = IoGetCurrentIrpStackLocation(Irp);

	   NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);

	   pTcpObj = tdiGetTcpObjectFromFileObject(IrpSp->FileObject, FALSE);

	   if( pTcpObj && pTcpObj->m_pAddrObj)
		   pAddrObject = (PTDI_OPEN_CONN)pTcpObj->m_pAddrObj;
	   else
		   pAddrObject = tdiGetConnObjectFromFileObject(IrpSp->FileObject, FALSE);

	   if(pAddrObject)
		   RemoveEntryList(&pAddrObject->m_qLink);

	   NPROT_RELEASE_LOCK(&g_OpenAddrListLock);

	   if(pAddrObject)
	   {
		   if(KeGetCurrentIrql() == PASSIVE_LEVEL)
		   {
			   getLocalInfo(pAddrObject->m_pFileObject, &pAddrObject->m_LocalPort, &pAddrObject->m_LocalAddr);

			   NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);
			   InsertTailList(&g_OpenAddrList, &pAddrObject->m_qLink);
			   NPROT_RELEASE_LOCK(&g_OpenAddrListLock);
		   }
		   else
		   {
			  pAddrObject->m_Flags |= FL_PENDING;
			  pAddrObject->m_pIrp = Irp;

			  NPROT_ACQUIRE_LOCK(&g_PendingAddrListLock);
			  InsertTailList(&g_PendingAddrList, &pAddrObject->m_qLink);
			  NPROT_RELEASE_LOCK(&g_PendingAddrListLock);

			  KeSetEvent( &g_AddrThreadEvent, IO_NO_INCREMENT, FALSE );

			  return STATUS_MORE_PROCESSING_REQUIRED;
		   }
	   }
   }
   
   return STATUS_SUCCESS;
}
/////////////////////////////////////////////////////////////////////////////
//// TDIH_TdiConnect
//
// Purpose
// This is the hook for TdiConnect
//
// Parameters
//
// Return Value
// 
// Remarks
//

NTSTATUS
TDIH_TdiConnect(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   )
{
	PTA_IP_ADDRESS		pTia = NULL;
	PTDI_OPEN_CONN	pTcpObj = NULL;
    PTDI_OPEN_CONN	pAddrObject = NULL;

	DWORD SourceAddr = 0;
	DWORD DestAddr = 0;
	WORD SourcePort = 0;
	WORD DestPort = 0;
	DWORD pid = 0;
	BYTE protocol = IPPROTO_TCP;

    if (Irp->CurrentLocation <= 1)
	{
		dprintf("TDIH_TdiConnect: BOGUS stack\n" );
		IoSkipCurrentIrpStackLocation( Irp );
		return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
	}
	
	pTia = (PTA_IP_ADDRESS) ((PTDI_REQUEST_KERNEL) &IrpSp->Parameters)->RequestConnectionInformation->RemoteAddress;

	if (pTia && (pTia->TAAddressCount == 1) && (pTia->Address[0].AddressLength == 14) &&
							(pTia->Address[0].AddressType == TDI_ADDRESS_TYPE_IP) ) 
	{
		DestPort = pTia->Address[0].Address[0].sin_port;
		DestAddr = pTia->Address[0].Address[0].in_addr;
	}

	NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);

	pTcpObj = tdiGetTcpObjectFromFileObject(IrpSp->FileObject, FALSE);

	if( pTcpObj && pTcpObj->m_pAddrObj)
	   pAddrObject = (PTDI_OPEN_CONN)pTcpObj->m_pAddrObj;
	else
	   pAddrObject = tdiGetConnObjectFromFileObject(IrpSp->FileObject, FALSE);

	if(pAddrObject)
	{
		pAddrObject->m_RemotePort = DestPort;
		pAddrObject->m_RemoteAddr = DestAddr;
		SourcePort = pAddrObject->m_LocalPort;
		SourceAddr = pAddrObject->m_LocalAddr;
		protocol = pAddrObject->m_IpProto;
		pid = pAddrObject->m_Pid;
	}

	NPROT_RELEASE_LOCK(&g_OpenAddrListLock);

	if(pid)
	{
		//if(isLoopback(DestAddr) || 
		if(IsLocalAddress(DestAddr) || 
		   ((protocol != IPPROTO_UDP) && (protocol != IPPROTO_TCP)))
		{
			TDI_CONN_REQUEST request;
			RtlZeroMemory(&request, sizeof(TDI_CONN_REQUEST));

			request.m_Pid = pid;
			request.m_SourceAddr = SourceAddr;
			request.m_SourcePort = SourcePort;
			request.m_DestAddr = DestAddr;
			request.m_DestPort = DestPort;
			request.m_pIrp = Irp;
			request.m_IpProto = protocol;
			request.m_pContext = pTDIH_DeviceExtension;
			request.m_CompletionFunc = TDIH_TdiConnectComplete2;
			request.m_StatusFail = STATUS_REMOTE_NOT_LISTENING;
			request.m_pDevExt = pTDIH_DeviceExtension;
			return tdiConnectionOk(&request);
		}
	}

	IoCopyCurrentIrpStackLocationToNext( Irp );

	IoSetCompletionRoutine(
		Irp,
		TDIH_TdiConnectComplete1,
		pTDIH_DeviceExtension,
		TRUE,
		TRUE,
		TRUE
		);

	return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
}

NTSTATUS
TDIH_TdiListenComplete(
   PDEVICE_OBJECT    pDeviceObject,
   PIRP              Irp,
   void              *Context
   )
{
   PTDI_DEV_EXT		pTDIH_DeviceExtension = NULL;
   PTDI_OPEN_CONN   pAddrObject = NULL;

   dprintf("TDIH_TdiListenComplete: Status: 0x%x Irp = 0x%x(%d)\n", Irp->IoStatus.Status, Irp, Irp->PendingReturned);

   if (Irp->PendingReturned)
      IoMarkIrpPending(Irp);

   pTDIH_DeviceExtension = (PTDI_DEV_EXT )Context;

   if (pTDIH_DeviceExtension->pFilterDeviceObject != pDeviceObject)
	{
	   dprintf("TDIH_TdiListenComplete Invalid Device Object Pointer\n");

	   return STATUS_SUCCESS;
	}

   if( NT_SUCCESS( Irp->IoStatus.Status ) )
   {
	   PTDI_OPEN_CONN pTcpObj = NULL;
	   PIO_STACK_LOCATION IrpSp = NULL;

	   IrpSp = IoGetCurrentIrpStackLocation(Irp);

	   NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);

	   pTcpObj = tdiGetTcpObjectFromFileObject(IrpSp->FileObject, FALSE);

	   if( pTcpObj && pTcpObj->m_pAddrObj)
		   pAddrObject = (PTDI_OPEN_CONN)pTcpObj->m_pAddrObj;
	   else
		   pAddrObject = tdiGetConnObjectFromFileObject(IrpSp->FileObject, FALSE);

	   if(pAddrObject)
		   RemoveEntryList(&pAddrObject->m_qLink);

	   NPROT_RELEASE_LOCK(&g_OpenAddrListLock);

	   if(pAddrObject)
	   {
		   pAddrObject->m_IpProto = ((PtrToInt(IrpSp->FileObject->FsContext2)) == TDI_TRANSPORT_ADDRESS_FILE) ? IPPROTO_UDP : IPPROTO_TCP;
		  
		   if(KeGetCurrentIrql() == PASSIVE_LEVEL)
		   {
			   getLocalInfo(pAddrObject->m_pFileObject, &pAddrObject->m_LocalPort, &pAddrObject->m_LocalAddr);

			   sendTdiRequest(pAddrObject->m_Pid,
							  pAddrObject->m_LocalAddr,
							  pAddrObject->m_LocalPort,
							  pAddrObject->m_IpProto,
							  FALSE);

			   NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);
			   InsertTailList(&g_OpenAddrList, &pAddrObject->m_qLink);
			   NPROT_RELEASE_LOCK(&g_OpenAddrListLock);
		   }
		   else
		   {
			  pAddrObject->m_Flags |= FL_PENDING | FL_LISTEN;
			  pAddrObject->m_pIrp = Irp;

			  NPROT_ACQUIRE_LOCK(&g_PendingAddrListLock);
			  InsertTailList(&g_PendingAddrList, &pAddrObject->m_qLink);
			  NPROT_RELEASE_LOCK(&g_PendingAddrListLock);

			  KeSetEvent( &g_AddrThreadEvent, IO_NO_INCREMENT, FALSE );

			  return STATUS_MORE_PROCESSING_REQUIRED;
		   }
	   }
   }
   
   return STATUS_SUCCESS;
}

NTSTATUS
TDIH_TdiListen(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   )
{
	PTA_IP_ADDRESS	pTia = NULL;
	
    if (Irp->CurrentLocation <= 1)
	{
		dprintf("TDIH_TdiListen: BOGUS stack\n" );
		IoSkipCurrentIrpStackLocation( Irp );
		return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
	}
	
	IoCopyCurrentIrpStackLocationToNext( Irp );

	IoSetCompletionRoutine(
		Irp,
		TDIH_TdiListenComplete,
		pTDIH_DeviceExtension,
		TRUE,
		TRUE,
		TRUE
		);

	return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
}

NTSTATUS
TDIH_UdpSendDatagram(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   )
{
	PTDI_REQUEST_KERNEL_SENDDG	pRks = NULL;
	PTA_IP_ADDRESS	pTia = NULL;
	PTDI_OPEN_CONN	pTcpObj = NULL;
	PTDI_OPEN_CONN	pAddrObject = NULL;
	DWORD SourceAddr = 0;
	DWORD DestAddr = 0;
	WORD SourcePort = 0;
	WORD DestPort = 0;
	BYTE protocol = IPPROTO_UDP;
	DWORD pid = 0;

    if (Irp->CurrentLocation <= 1)
	{
		IoSkipCurrentIrpStackLocation( Irp );
		return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
	}
	
	pRks = (PTDI_REQUEST_KERNEL_SENDDG) &IrpSp->Parameters;
	pTia = (PTA_IP_ADDRESS) pRks->SendDatagramInformation->RemoteAddress;

	if (pTia && (pTia->TAAddressCount == 1) && (pTia->Address[0].AddressLength == 14) &&
							(pTia->Address[0].AddressType == TDI_ADDRESS_TYPE_IP) ) 
	{
		DestPort = pTia->Address[0].Address[0].sin_port;
		DestAddr = pTia->Address[0].Address[0].in_addr;
	}

	NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);

	pTcpObj = tdiGetTcpObjectFromFileObject(IrpSp->FileObject, FALSE);

	if( pTcpObj && pTcpObj->m_pAddrObj)
	   pAddrObject = (PTDI_OPEN_CONN)pTcpObj->m_pAddrObj;
	else
	   pAddrObject = tdiGetConnObjectFromFileObject(IrpSp->FileObject, FALSE);

	if(pAddrObject)
	{
		pAddrObject->m_RemotePort = DestPort;
		pAddrObject->m_RemoteAddr = DestAddr;
		SourcePort = pAddrObject->m_LocalPort;
		SourceAddr = pAddrObject->m_LocalAddr;
		protocol = pAddrObject->m_IpProto;
		pid = pAddrObject->m_Pid;
	}

	NPROT_RELEASE_LOCK(&g_OpenAddrListLock);

	if(pid)
	{
		//if(isLoopback(DestAddr) || 
		if(IsLocalAddress(DestAddr) || 
		   ((protocol != IPPROTO_UDP) && (protocol != IPPROTO_TCP)))
		{
			TDI_CONN_REQUEST request;
			RtlZeroMemory(&request, sizeof(TDI_CONN_REQUEST));

			request.m_Pid = pid;
			request.m_SourceAddr = SourceAddr;
			request.m_SourcePort = SourcePort;
			request.m_DestAddr = DestAddr;
			request.m_DestPort = DestPort;
			request.m_pIrp = Irp;
			request.m_IpProto = protocol;
			request.m_StatusFail = STATUS_INVALID_ADDRESS;
			request.m_pDevExt = pTDIH_DeviceExtension;
			return tdiConnectionOk(&request);
		}
	}

	IoSkipCurrentIrpStackLocation( Irp );
	return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
}

NTSTATUS
TDIH_TdiSetEventComplete(
   PDEVICE_OBJECT    pDeviceObject,
   PIRP              Irp,
   void              *Context
   )
{

   PTDI_OPEN_CONN pConnObj = NULL;
   PTDI_DEV_EXT	pTDIH_DeviceExtension = NULL;
 
   dprintf( "TDIH_TdiSetEventComplete: Status: 0x%8.8X\n", Irp->IoStatus.Status);

   if (Irp->PendingReturned)
      IoMarkIrpPending(Irp);

   pConnObj = (PTDI_OPEN_CONN)Context;

   if(pConnObj == NULL)
   {
	   dprintf( "TDIH_TdiSetEventComplete: null address object\n");
	   return STATUS_SUCCESS;
   }

   pTDIH_DeviceExtension = pConnObj->m_pDeviceExtension;

   ASSERT( pTDIH_DeviceExtension );

   if (pTDIH_DeviceExtension->pFilterDeviceObject != pDeviceObject)
   {
      dprintf( "TDIH_TdiSetEventComplete: Invalid Device Object Pointer\n" );

	  NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);
	  InsertTailList(&g_OpenAddrList, &pConnObj->m_qLink);
      NPROT_RELEASE_LOCK(&g_OpenAddrListLock);

      return STATUS_SUCCESS;
   }

   if(!NT_SUCCESS(Irp->IoStatus.Status))
   {
	  NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);
	  InsertTailList(&g_OpenAddrList, &pConnObj->m_qLink);
      NPROT_RELEASE_LOCK(&g_OpenAddrListLock);

      return STATUS_SUCCESS;
   }

   if(KeGetCurrentIrql() == PASSIVE_LEVEL)
   {
	   dprintf("TDIH_TdiSetEventComplete: PASSIVE_LEVEL\n");
	   getLocalInfo(pConnObj->m_pFileObject, &pConnObj->m_LocalPort, &pConnObj->m_LocalAddr);

	   sendTdiRequest(pConnObj->m_Pid,
					  pConnObj->m_LocalAddr,
					  pConnObj->m_LocalPort,
					  pConnObj->m_IpProto,
					  FALSE);

	   NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);
	   InsertTailList(&g_OpenAddrList, &pConnObj->m_qLink);
	   NPROT_RELEASE_LOCK(&g_OpenAddrListLock);
   }
   else
   {
	  dprintf("TDIH_TdiSetEventComplete: DISPATCH_LEVEL\n");

	  pConnObj->m_pIrp = Irp;
	  pConnObj->m_Flags |= FL_PENDING | FL_LISTEN;

	  NPROT_ACQUIRE_LOCK(&g_PendingAddrListLock);
	  InsertTailList(&g_PendingAddrList, &pConnObj->m_qLink);
	  NPROT_RELEASE_LOCK(&g_PendingAddrListLock);
	
	  KeSetEvent( &g_AddrThreadEvent, IO_NO_INCREMENT, FALSE );

	  return STATUS_MORE_PROCESSING_REQUIRED;
   }
   
   return STATUS_SUCCESS;
}

NTSTATUS
TDIH_TdiSetEvent(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   )
{
    if (Irp->CurrentLocation <= 1)
	{
		dprintf("TDIH_TdiSetEvent: BOGUS stack\n" );
		IoSkipCurrentIrpStackLocation( Irp );
		return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
	}
   
    if((((PTDI_REQUEST_KERNEL_SET_EVENT)&IrpSp->Parameters)->EventType == TDI_EVENT_CONNECT) &&
		(((PTDI_REQUEST_KERNEL_SET_EVENT)&IrpSp->Parameters)->EventHandler))
    {
	    PTDI_OPEN_CONN   pConnObj = NULL;

	    NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);

		pConnObj = tdiGetConnObjectFromFileObject(IrpSp->FileObject, FALSE);

		if(pConnObj)
	       RemoveEntryList(&pConnObj->m_qLink);

	    NPROT_RELEASE_LOCK(&g_OpenAddrListLock);

	    if(pConnObj)
	    {
			IoCopyCurrentIrpStackLocationToNext( Irp );

			IoSetCompletionRoutine(
				Irp,
				TDIH_TdiSetEventComplete,
				pConnObj,
				TRUE,
				TRUE,
				TRUE
				);

			return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
		}	
    }

	IoSkipCurrentIrpStackLocation( Irp );
	return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
}

NTSTATUS
TDIH_IpDispatch(
    PTDI_DEV_EXT			pTDIH_DeviceExtension,
    PIRP                    Irp,
    PIO_STACK_LOCATION      IrpSp
   )
{
	PTDI_OPEN_CONN pConnObj = NULL;
	DWORD addr = 0;

	if (Irp->CurrentLocation <= 1)
	{
		dprintf("TDIH_IpDispatch: BOGUS stack\n" );
		IoSkipCurrentIrpStackLocation( Irp );
		return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
	}

	__try
	{
		addr = *(DWORD *)Irp->AssociatedIrp.SystemBuffer;
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		addr = 0;
	}

	NPROT_ACQUIRE_LOCK(&g_OpenAddrListLock);

	pConnObj = tdiGetConnObjectFromFileObject(IrpSp->FileObject, FALSE);

	if(pConnObj == NULL)
	{
		pConnObj = tdiAllocOpenConn( );

		if(pConnObj)
		{
			pConnObj->m_IpProto = IPPROTO_ICMP;
			pConnObj->m_RemoteAddr = addr;
			pConnObj->m_pFileObject = IrpSp->FileObject;
			pConnObj->m_pDeviceExtension = pTDIH_DeviceExtension;
			pConnObj->m_Pid = HandleToUlong(PsGetCurrentProcessId());
			InsertTailList(&g_OpenAddrList, &pConnObj->m_qLink);

			dprintf( "TDIH_IpDispatch PidCheck: %d\n", pConnObj->m_Pid );
		}
	}

	NPROT_RELEASE_LOCK(&g_OpenAddrListLock);

	IoCopyCurrentIrpStackLocationToNext( Irp );

	return IoCallDriver(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
}