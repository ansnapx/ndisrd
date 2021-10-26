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


//USHORT htons( USHORT hostshort )
//{
//	PUCHAR	pBuffer;
//	USHORT	nResult;
//
//	nResult = 0;
//	pBuffer = (PUCHAR )&hostshort;
//
//	nResult = ( (pBuffer[ 0 ] << 8) & 0xFF00 )
//		| ( pBuffer[ 1 ] & 0x00FF );
//
//	return( nResult );
//}
//
////***********************************************************************************
//// Name: ntohs
////
//// Description: Function converts a USHORT from TCP/IP network byte order to host
//// (which is little-endian). 
////
//// Return value: value in host byte order
////
//// Parameters: 
//// netshort	- 16-bit number in TCP/IP network byte order
////
//// NOTE: None
//// **********************************************************************************
//
//USHORT ntohs( USHORT netshort )
//{
//	PUCHAR	pBuffer;
//	USHORT	nResult;
//
//	nResult = 0;
//	pBuffer = (PUCHAR )&netshort;
//
//	nResult = ( (pBuffer[ 0 ] << 8) & 0xFF00 )
//		| ( pBuffer[ 1 ] & 0x00FF );
//
//	return( nResult );
//}
//
////***********************************************************************************
//// Name: htonl
////
//// Description: Function converts a ULONG from host to TCP/IP network byte order
//// (which is big-endian). 
////
//// Return value: value in TCP/IP network byte order
////
//// Parameters: 
//// hostlong	- 32-bit number in host byte order
////
//// NOTE: None
//// **********************************************************************************
//
//ULONG htonl( ULONG hostlong )
//{
//	ULONG    nResult = hostlong >> 16;
//	USHORT	upper = (USHORT) nResult & 0x0000FFFF;
//	USHORT	lower = (USHORT) hostlong & 0x0000FFFF;
//
//	upper = htons( upper );
//	lower = htons( lower );
//
//    nResult = 0x10000 * lower + upper;
//	return( nResult );
//}
//
////***********************************************************************************
//// Name: ntohl
////
//// Description: Function converts a ULONG from TCP/IP network byte order to host
//// (which is little-endian). 
////
//// Return value: value in host byte order
////
//// Parameters: 
//// netlong	- 32-bit number in TCP/IP network byte order
////
//// NOTE: None
//// **********************************************************************************
//
//ULONG ntohl( ULONG netlong )
//{
//	ULONG    nResult = netlong >> 16;
//	USHORT   upper = (USHORT) nResult & 0x0000FFFF;
//	USHORT	lower = (USHORT) netlong & 0x0000FFFF;
//
//	upper = htons( upper );
//	lower = htons( lower );
//
//    nResult = 0x10000 * lower + upper;
//	return( nResult );
//}

static NTSTATUS
_I_KS_SimpleTdiRequestComplete(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    );

BOOLEAN getLocalInfo( PFILE_OBJECT fileObj, WORD * port, DWORD * addr ) 
{
	PTDI_ADDRESS_INFO	pAddrInfo;
	PTA_IP_ADDRESS		pIpAddr;
	ULONG_PTR 			INFO_SIZE = sizeof(TDI_ADDRESS_INFO) - sizeof (TRANSPORT_ADDRESS) + sizeof(TA_IP_ADDRESS);
	ULONG_PTR			infoLen = INFO_SIZE;
	UCHAR 				infoBuf [sizeof(TDI_ADDRESS_INFO) - sizeof (TRANSPORT_ADDRESS) + sizeof(TA_IP_ADDRESS) + 1];
	NTSTATUS			rcQuery;

	rcQuery = KS_QueryAddressInfo( fileObj, infoBuf, &infoLen );

	if ((! NT_SUCCESS (rcQuery)) || (infoLen < INFO_SIZE)) 
	{
		dprintf("getLocalInfo KS_QueryAddressInfo failed = 0x%x\n", rcQuery);
		return FALSE;
	}

	pAddrInfo = (PTDI_ADDRESS_INFO) infoBuf;

#if (NTDDI_VERSION < NTDDI_LONGHORN)
	if (pAddrInfo->ActivityCount != 1) 
	{
		dprintf("getLocalInfo ActivityCount = %d\n", pAddrInfo->ActivityCount);
		return FALSE;
	}
#endif

	pIpAddr = (PTA_IP_ADDRESS) &(pAddrInfo->Address);

#if (NTDDI_VERSION >= NTDDI_LONGHORN)
	if (pAddrInfo == NULL) 
	{
		dprintf("getLocalInfo pAddrInfo = NULL\n");
		return FALSE;
	}
#endif

	if ( (pIpAddr->TAAddressCount != 1) ||
					(pIpAddr->Address[0].AddressLength != sizeof (TDI_ADDRESS_IP)) ||
					(pIpAddr->Address[0].AddressType != TDI_ADDRESS_TYPE_IP) ) 
	{
		dprintf("getLocalInfo sanitty check failed\n");
		return FALSE;
	}

	*port =  pIpAddr->Address[0].Address[0].sin_port;
	*addr =  pIpAddr->Address[0].Address[0].in_addr;

	dprintf("getLocalInfo IP = %d.%d.%d.%d  Port = %d\n", PRINT_IP_ADDR(*addr), *port);
	return TRUE;
}



/////////////////////////////////////////////////////////////////////////////
//// KS_QueryAddressInfo
//
// Purpose
// KS_QueryAddressInfo makes a TDI_QUERY_INFORMATION request for
// TDI_QUERY_ADDRESS_INFO for the transport address or connection endpoint
// specified by its file object pointer.
//
// Parameters
//   pFileObject
//      Can be either a TDI transport address file object (from KS_ADDRESS)
//      or TDI connection endpoint file object (from KS_ENDPOINT).
//
//    pInfoBuffer
//        Must point to a buffer sufficiently large to hold a TDI_ADDRESS_INFO
//        structure for a TDI_ADDRESS_IP.
//
//    pInfoBufferSize
//       Pointer to a ULONG. On entry, the ULONG pointed to by pInfoBufferSize
//       must be initialized with the size of the buffer at pInfoBuffer. On
//       return, the ULONG pointed to by pInfoBufferSize will be updated to
//       indicate the length of the data written to pInfoBuffer.
//
// Return Value
//
// Remarks
// This function is roughly the TDI equivalent of the Winsock getsockname
// function.
//
// Callers of KS_QueryAddressInfo must be running at IRQL PASSIVE_LEVEL.
//
// The information returned from TDI_QUERY_ADDRESS_INFO depends on the
// type of the file object:
//
//   Transport Address File Object:
//     in_addr: 0.0.0.0; sin_port: 1042 (0x412)
//
//   Connection Endpoint File Object:
//     in_addr: 172.16.1.130; sin_port: 1042 (0x412)
//
// On reflection, this makes sense. An address object has a unique port
// number, but can be used on multiple host IP addresses on a multihomed
// host. Hence, the IP address is zero.
//
// However, once a connection is established the network interface is
// also established. Hence, the IP address on a connected connection
// endpoint is non-zero.
//

NTSTATUS
KS_QueryAddressInfo(
   PFILE_OBJECT      pFileObject,
   PVOID             pInfoBuffer,
   PULONG_PTR        pInfoBufferSize
   )
{
   NTSTATUS          Status = STATUS_UNSUCCESSFUL;
   IO_STATUS_BLOCK   IoStatusBlock;
   PIRP              pIrp = NULL;
   PMDL              pMdl = NULL;
   PDEVICE_OBJECT    pDeviceObject;

   pDeviceObject = IoGetRelatedDeviceObject( pFileObject );

   RtlZeroMemory( pInfoBuffer, *pInfoBufferSize );

   //
   // Allocate IRP For The Query
   //
   pIrp = IoAllocateIrp( pDeviceObject->StackSize, FALSE );

   if( !pIrp )
   {
      return( STATUS_INSUFFICIENT_RESOURCES );
   }

   pMdl = KS_AllocateAndProbeMdl(
            pInfoBuffer,      // Virtual address for MDL construction
            (ULONG)*pInfoBufferSize,  // size of the buffer
            FALSE,
            FALSE,
            NULL
            );

   if( !pMdl )
   {
      IoFreeIrp( pIrp );
      return( STATUS_INSUFFICIENT_RESOURCES );
   }

   TdiBuildQueryInformation(
      pIrp,
      pDeviceObject,
      pFileObject,
      _I_KS_SimpleTdiRequestComplete,  // Completion routine
      NULL,                            // Completion context
      TDI_QUERY_ADDRESS_INFO,
      pMdl
      );

   //
   // Submit The Query To The Transport
   //
   Status = KS_MakeSimpleTdiRequest(
               pDeviceObject,
               pIrp
               );

   //
   // Free Allocated Resources
   //
   KS_UnlockAndFreeMdl(pMdl);

   *pInfoBufferSize = pIrp->IoStatus.Information;

   IoFreeIrp( pIrp );

   return( Status );
}

/////////////////////////////////////////////////////////////////////////////
//// KS_QueryProviderInfo
//
// Purpose
// Fetch the TDI_PROVIDER_INFO for the specified transport driver.
//
// Parameters
//   TransportDeviceNameW
//      Pointer to a zero-terminated wide character string that specifies
//      the transport device. An example would be: L"\\Device\\Tcp"
//
//
// Return Value
//
// Remarks
// Callers of KS_QueryProviderInfo must be running at IRQL PASSIVE_LEVEL.
//

NTSTATUS
KS_QueryProviderInfo(
   PWSTR              TransportDeviceNameW,// Zero-terminated String
   PTDI_PROVIDER_INFO pProviderInfo
   )
{
   NTSTATUS          Status = STATUS_SUCCESS;
   KS_CHANNEL        KS_Channel;
   PDEVICE_OBJECT    pDeviceObject = NULL;
   PIRP              pIrp = NULL;
   PMDL              pMdl;

   //
   // Open A TDI Control Channel To The Specified Transport
   //
   Status = KS_OpenControlChannel(
               TransportDeviceNameW,
               &KS_Channel
               );

   if( !NT_SUCCESS(Status) )
   {
      return( Status );
   }

   //
   // Obtain The Related Device Object
   //
   pDeviceObject = IoGetRelatedDeviceObject( KS_Channel.m_pFileObject );

   //
   // Allocate IRP For The Query
   //
   pIrp = IoAllocateIrp( pDeviceObject->StackSize, FALSE );

   if( !pIrp )
   {
      KS_CloseControlChannel( &KS_Channel );

      return( STATUS_INSUFFICIENT_RESOURCES );
   }

   //
   // Prepare MDL For The Query Buffer
   //
   pMdl = KS_AllocateAndProbeMdl(
            pProviderInfo,           // Virtual address for MDL construction
            sizeof( TDI_PROVIDER_INFO ),  // size of the buffer
            FALSE,
            FALSE,
            NULL
            );

   if( !pMdl )
   {
      IoFreeIrp( pIrp );

      KS_CloseControlChannel( &KS_Channel );

      return( STATUS_INSUFFICIENT_RESOURCES );
   }

   TdiBuildQueryInformation(
      pIrp,
      pDeviceObject,
      KS_Channel.m_pFileObject,
      _I_KS_SimpleTdiRequestComplete,    // Completion routine
      NULL,                            // Completion context
      TDI_QUERY_PROVIDER_INFO,
      pMdl);


   //
   // Submit The Query To The Transport
   //
   Status = KS_MakeSimpleTdiRequest( pDeviceObject, pIrp );

   //
   // Free Allocated Resources
   //
   KS_UnlockAndFreeMdl(pMdl);

   IoFreeIrp( pIrp );

   KS_CloseControlChannel( &KS_Channel );

   return( Status );
}

/////////////////////////////////////////////////////////////////////////////
//// KS_MakeSimpleTdiRequest
//
// Purpose
// This routine submits a request to TDI and waits for it to complete.
//
// Parameters
//   pDeviceObject - Transport device object.
//   pIrp - IRP setup with the TDI request to submit.
//
// Return Value
//
// Remarks
// This is basically a blocking version of IoCallDriver.
//

NTSTATUS
KS_MakeSimpleTdiRequest(
   IN PDEVICE_OBJECT pDeviceObject,
   IN PIRP pIrp
   )
{
   NTSTATUS Status;
   KEVENT Event;

   KeInitializeEvent (&Event, NotificationEvent, FALSE);

   IoSetCompletionRoutine(
      pIrp,                         // The IRP
      _I_KS_SimpleTdiRequestComplete, // The completion routine
      &Event,                       // The completion context
      TRUE,                         // Invoke On Success
      TRUE,                         // Invoke On Error
      TRUE                          // Invoke On Cancel
      );

   //
   // Submit the request
   //
   Status = IoCallDriver( pDeviceObject, pIrp );

   if( !NT_SUCCESS(Status) )
   {
      KdPrint( ("IoCallDriver(pDeviceObject = %p) returned %lx\n",pDeviceObject,Status));
   }

   if ((Status == STATUS_PENDING) || (Status == STATUS_SUCCESS))
   {

      Status = KeWaitForSingleObject(
                  &Event,     // Object to wait on.
                  Executive,  // Reason for waiting
                  KernelMode, // Processor mode
                  FALSE,      // Alertable
                  NULL        // Timeout
                  );

      if (!NT_SUCCESS(Status))
      {
         KdPrint(("KS_MakeSimpleTdiRequest could not wait Wait returned %lx\n",Status));
         return Status;
      }

      Status = pIrp->IoStatus.Status;
   }

   return Status;
}

/////////////////////////////////////////////////////////////////////////////
//// KS_AllocateAndProbeMdl
//
// Purpose
//
// Parameters
//
// Return Value
//
// Remarks
//

PMDL
KS_AllocateAndProbeMdl(
   PVOID VirtualAddress,
   ULONG Length,
   BOOLEAN SecondaryBuffer,
   BOOLEAN ChargeQuota,
   PIRP Irp OPTIONAL
   )
{
   PMDL pMdl = NULL;

   pMdl = IoAllocateMdl(
            VirtualAddress,
            Length,
            SecondaryBuffer,
            ChargeQuota,
            Irp
            );

   if( !pMdl )
   {
      return( (PMDL )NULL );
   }

   try
   {
      MmProbeAndLockPages( pMdl, KernelMode, IoModifyAccess );
   }
   except( EXCEPTION_EXECUTE_HANDLER )
   {
      IoFreeMdl( pMdl );

      pMdl = NULL;

      return( NULL );
   }

   pMdl->Next = NULL;

   return( pMdl );
}


/////////////////////////////////////////////////////////////////////////////
//// KS_UnlockAndFreeMdl
//
// Purpose
//
// Parameters
//
// Return Value
//
// Remarks
//

VOID
KS_UnlockAndFreeMdl(
   PMDL pMdl
   )
{
   MmUnlockPages( pMdl );
   IoFreeMdl( pMdl );
}

/////////////////////////////////////////////////////////////////////////////
//// KS_OpenControlChannel
//
// Purpose
// Open a control channel to the specified transport.
//
// Parameters
//   TransportDeviceNameW
//      Pointer to a zero-terminated wide character string that specifies
//      the transport device. An example would be: L"\\Device\\Tcp"
//
//
// Return Value
//
// Remarks
// Callers of KS_OpenControlChannel must be running at IRQL PASSIVE_LEVEL.
//

NTSTATUS
KS_OpenControlChannel(
   IN PWSTR       TransportDeviceNameW,// Zero-terminated String
   PKS_CHANNEL    pKS_Channel
   )
{
   NTSTATUS          Status = STATUS_SUCCESS;
   UNICODE_STRING    TransportDeviceName;
   HANDLE            hControlChannel;
   PFILE_OBJECT      pControlChannelFileObject = NULL;
   PDEVICE_OBJECT    pDeviceObject = NULL;
   OBJECT_ATTRIBUTES ChannelAttributes;
   IO_STATUS_BLOCK   IoStatusBlock;
   PIRP              pIrp = NULL;
   PMDL              pMdl;

   //
   // Initialize KS_CHANNEL Structure
   //
   pKS_Channel->m_pFileObject = NULL;

   //
   // Setup Transport Device Name
   //
   NdisInitUnicodeString( &TransportDeviceName, TransportDeviceNameW );

   //
   // Open A TDI Control Channel To The Specified Transport
   //

   InitializeObjectAttributes(
      &ChannelAttributes,       // Tdi Control Channel attributes
      &TransportDeviceName,      // Transport Device Name
      OBJ_CASE_INSENSITIVE,     // Attributes
      NULL,                     // RootDirectory
      NULL                      // SecurityDescriptor
      );


   Status = ZwCreateFile(
               &pKS_Channel->m_hControlChannel,// Handle
               GENERIC_READ | GENERIC_WRITE, // Desired Access
               &ChannelAttributes,           // Object Attributes
               &IoStatusBlock,               // Final I/O status block
               NULL,                         // Allocation Size
               FILE_ATTRIBUTE_NORMAL,        // Normal attributes
               FILE_SHARE_READ,              // Sharing attributes
               FILE_OPEN_IF,                 // Create disposition
               0,                            // CreateOptions
               NULL,                         // EA Buffer
               0);                           // EA length

   if( !NT_SUCCESS(Status) )
   {
      return( Status );
   }

   //
   // Obtain A Referenced Pointer To The File Object
   //

   Status = ObReferenceObjectByHandle (
               pKS_Channel->m_hControlChannel,      // Object Handle
               FILE_ANY_ACCESS,                     // Desired Access
               NULL,                                // Object Type
               KernelMode,                          // Processor mode
               (PVOID *)&pKS_Channel->m_pFileObject,// Object pointer
               NULL);                               // Object Handle information

   if( !NT_SUCCESS(Status) )
   {
      ZwClose( pKS_Channel->m_hControlChannel );

      pKS_Channel->m_pFileObject = NULL;
   }

   return( Status );
}


/////////////////////////////////////////////////////////////////////////////
//// KS_CloseControlChannel
//
// Purpose
// Close a control chanel previously opened using KS_OpenControlChannel.
//
// Parameters
//
// Return Value
//
// Remarks
// Callers of KS_CloseControlChannel must be running at IRQL PASSIVE_LEVEL.
//

NTSTATUS
KS_CloseControlChannel(
   PKS_CHANNEL    pKS_Channel
   )
{
   if( pKS_Channel->m_pFileObject )
   {
      ObDereferenceObject( pKS_Channel->m_pFileObject );

      pKS_Channel->m_pFileObject = NULL;

      ZwClose( pKS_Channel->m_hControlChannel );
   }

   return( STATUS_SUCCESS );
}


/////////////////////////////////////////////////////////////////////////////
//// _I_KS_SimpleTdiRequestComplete (INTERNAL/PRIVATE)
//
// Purpose
// Completion callback for driver calls initiated by KS_MakeSimpleTdiRequest.
//
// Parameters
//   pDeviceObject - Transport device object.
//   pIrp - IRP setup with the TDI request to submit.
//
// Return Value
//
// Remarks
//

static NTSTATUS
_I_KS_SimpleTdiRequestComplete(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
   if( Context != NULL )
      KeSetEvent((PKEVENT )Context, 0, FALSE);

   return STATUS_MORE_PROCESSING_REQUIRED;
}

/**
 * This parses an extended attribute list for a given target attribute
 * @param pFirstEA IN The first EA in the list
 * @param *pName IN The name of the EA we are looking for
 * @param nameLen IN The number of characters long pName is
 * @return The ea asked for. NULL if not found
 */
FILE_FULL_EA_INFORMATION UNALIGNED * FindEA( PFILE_FULL_EA_INFORMATION pFirstEA, CHAR *pName, USHORT nameLen ) {

	FILE_FULL_EA_INFORMATION UNALIGNED *pCurrentEA;

	ASSERT( KeGetCurrentIrql() <= PASSIVE_LEVEL );
	PAGED_CODE();

	// if no list - no EA
	if( ! pFirstEA )
		return NULL;

	// walk the list
	do {
		pCurrentEA = pFirstEA;
		pFirstEA += pCurrentEA->NextEntryOffset;

		// lengths must match
		if (pCurrentEA->EaNameLength != nameLen)
			continue;

		// compare the memory
		if (RtlCompareMemory (pCurrentEA->EaName, pName, nameLen) == nameLen)
			return pCurrentEA;
	
	}		
	while (pCurrentEA->NextEntryOffset != 0);

    return(NULL);
}

VOID DumpEA( PFILE_FULL_EA_INFORMATION pFirstEA) 
{
	FILE_FULL_EA_INFORMATION UNALIGNED *pCurrentEA;

	// if no list - no EA
	if( ! pFirstEA )
	{
		dprintf("DumpEA NULL\n");
		return;
	}

	// walk the list
	do {
		pCurrentEA = pFirstEA;
		pFirstEA += pCurrentEA->NextEntryOffset;

		dprintf("DumpEA %s\n", pCurrentEA->EaName);
	
	}		
	while (pCurrentEA->NextEntryOffset != 0);
}

#ifdef DBG
VOID
ndisprotAllocateSpinLock(
    IN  PNPROT_LOCK          pLock,
    IN  ULONG               FileNumber,
    IN  ULONG               LineNumber
)
{
	NdisAllocateSpinLock(&pLock->NdisLock);
	pLock->IsAcquired = 0;
	pLock->TouchedByFileNumber = 0;
	pLock->TouchedInLineNumber = 0;
	DbgPrint("ndisprotAllocateSpinLock( File = %d Line = %d)\n", FileNumber, LineNumber );
}


VOID
ndisprotAcquireSpinLock(
    IN  PNPROT_LOCK          pLock,
    IN  ULONG               FileNumber,
    IN  ULONG               LineNumber
)
{

	NdisAcquireSpinLock(&pLock->NdisLock);

	if((pLock->IsAcquired == 1) && (pLock->OwnerThread == HandleToUlong(PsGetCurrentThreadId())))
	{
		DbgPrint("DEADLOCK : ndisprotAcquireSpinLock SAME THREAD ACQUIRED (File = %d Line = %d)\n", pLock->TouchedByFileNumber, pLock->TouchedInLineNumber );
		KeBugCheckEx(0xC4, LineNumber, pLock->TouchedInLineNumber, pLock->OwnerThread, pLock->IsAcquired);
	}

	pLock->IsAcquired = 1;
	pLock->TouchedByFileNumber = FileNumber;
	pLock->TouchedInLineNumber = LineNumber;
	pLock->OwnerThread = HandleToUlong(PsGetCurrentThreadId());
}


VOID
ndisprotReleaseSpinLock(
    IN  PNPROT_LOCK          pLock,
    IN  ULONG               FileNumber,
    IN  ULONG               LineNumber
)
{
	if(pLock->IsAcquired != 1)
	{
		DbgPrint("WARNING : ndisprotReleaseSpinLock NOT ACQUIRED (File = %d Line = %d)\n", pLock->TouchedByFileNumber, pLock->TouchedInLineNumber );
		KeBugCheckEx(0xC4, pLock->TouchedByFileNumber, pLock->TouchedInLineNumber, pLock->OwnerThread, pLock->IsAcquired);
	}

	pLock->IsAcquired = 0;
	NdisReleaseSpinLock(&pLock->NdisLock);
	
}
#endif