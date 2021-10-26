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

DRIVER_INITIALIZE DriverEntry;
DRIVER_DISPATCH TDIH_DefaultDispatch;
DRIVER_DISPATCH TDIH_DispatchInternalDeviceControl;

/////////////////////////////////////////////////////////////////////////////
//// TDIH_DispatchInternalDeviceControl
//
// Purpose
//
// Parameters
//
// Return Value
// 
// Remarks
//

NTSTATUS TDIH_ChainRequest(
			PDEVICE_OBJECT pDeviceObject,
			PIRP           Irp )
{
	if (Irp->CurrentLocation <= 1)
	{
		IoSkipCurrentIrpStackLocation( Irp );
	}
	else
	{
		IoCopyCurrentIrpStackLocationToNext( Irp );
	}


	return IoCallDriver( pDeviceObject, Irp );
}

NTSTATUS
TDIH_DispatchInternalDeviceControl(
   PDEVICE_OBJECT pDeviceObject,
   PIRP           Irp
   )
{
   PIO_STACK_LOCATION      IrpSp = NULL;
   PTDI_DEV_EXT			   pTDIH_DeviceExtension;

   pTDIH_DeviceExtension = (PTDI_DEV_EXT )(pDeviceObject->DeviceExtension);

   IrpSp = IoGetCurrentIrpStackLocation(Irp);

   //if( pTDIH_DeviceExtension->NodeIdentifier.NodeType == TDIH_NODE_TYPE_W32API_DEVICE )
   //{
	  // return W32DevDispatch(pDeviceObject, Irp);
   //}

   if( pTDIH_DeviceExtension->NodeIdentifier.NodeType == TDIH_NODE_TYPE_IP_FILTER_DEVICE )
   {
	   dprintf("TDIH_IpDispatch(Code = 0x%x)\n", IrpSp->Parameters.DeviceIoControl.IoControlCode);

       if(IrpSp->Parameters.DeviceIoControl.IoControlCode == FILTER_ICMP_CODE)
		   return TDIH_IpDispatch( pTDIH_DeviceExtension, Irp, IrpSp);
   }

   if ((PtrToInt(IrpSp->FileObject->FsContext2)) == TDI_CONNECTION_FILE)
   {
      switch(IrpSp->MinorFunction)
      {
         case TDI_ASSOCIATE_ADDRESS:
            return( TDIH_TdiAssociateAddress( pTDIH_DeviceExtension, Irp, IrpSp ) );

         case TDI_DISASSOCIATE_ADDRESS:
            return( TDIH_TdiDisAssociateAddress( pTDIH_DeviceExtension, Irp, IrpSp ) );

         case TDI_CONNECT:
            return( TDIH_TdiConnect( pTDIH_DeviceExtension, Irp, IrpSp ) );

		 case TDI_LISTEN:
            return( TDIH_TdiListen( pTDIH_DeviceExtension, Irp, IrpSp ) );
      }
   }
   else if ((PtrToInt(IrpSp->FileObject->FsContext2)) == TDI_TRANSPORT_ADDRESS_FILE)
   {
      switch(IrpSp->MinorFunction)
      {
	    case TDI_SET_EVENT_HANDLER:
            return( TDIH_TdiSetEvent( pTDIH_DeviceExtension, Irp, IrpSp ) );

        case TDI_CONNECT:
            return( TDIH_TdiConnect( pTDIH_DeviceExtension, Irp, IrpSp ) );

		case TDI_SEND_DATAGRAM:
			return (TDIH_UdpSendDatagram( pTDIH_DeviceExtension, Irp, IrpSp ));

		 case TDI_LISTEN:
            return( TDIH_TdiListen( pTDIH_DeviceExtension, Irp, IrpSp ) );
      }
   }

   return TDIH_ChainRequest(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
}


/////////////////////////////////////////////////////////////////////////////
//// TDIH_Create
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
TDIH_Create(
   PTDI_DEV_EXT   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   )
{
   FILE_FULL_EA_INFORMATION            *ea;
   FILE_FULL_EA_INFORMATION UNALIGNED  *targetEA;

   ea = (PFILE_FULL_EA_INFORMATION) Irp->AssociatedIrp.SystemBuffer;
   //
   // Handle Address Object Open
   //
   targetEA = FindEA(ea, TdiTransportAddress, TDI_TRANSPORT_ADDRESS_LENGTH);

   if (targetEA != NULL)
   {
      return( TDIH_TdiOpenAddress( pTDIH_DeviceExtension, Irp, IrpSp ) );
   }

   //
   // Handle Connection Object Open
   //
   targetEA = FindEA(ea, TdiConnectionContext, TDI_CONNECTION_CONTEXT_LENGTH);

   if (targetEA != NULL)
   {
      return( TDIH_TdiOpenConnection(
               pTDIH_DeviceExtension,
               Irp,
               IrpSp,
               *((CONNECTION_CONTEXT UNALIGNED *)
                  &(targetEA->EaName[targetEA->EaNameLength + 1]))
               )
            );
   }

   return TDIH_ChainRequest(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
}


/////////////////////////////////////////////////////////////////////////////
//// TDIH_Cleanup
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
TDIH_Cleanup(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   )
{
   switch (PtrToInt(IrpSp->FileObject->FsContext2))
   {
   //   case TDI_TRANSPORT_ADDRESS_FILE:
   //      return( TDIH_TdiCloseAddress( pTDIH_DeviceExtension, Irp, IrpSp ) );

   //   case TDI_CONNECTION_FILE:
   //      return( TDIH_TdiCloseConnection( pTDIH_DeviceExtension, Irp, IrpSp ) );
	  //default:
		 // if (pTDIH_DeviceExtension->NodeIdentifier.NodeType == TDIH_NODE_TYPE_IP_FILTER_DEVICE)
			//	return TDIH_TdiCloseAddress( pTDIH_DeviceExtension, Irp, IrpSp );
   }

   return TDIH_ChainRequest(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
}


/////////////////////////////////////////////////////////////////////////////
//// TDIH_Close
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
TDIH_Close(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   )
{
   switch (PtrToInt(IrpSp->FileObject->FsContext2))
   {
      //case TDI_TRANSPORT_ADDRESS_FILE:
      //   dprintf( "TDIH_Close: Address File\n");
      //   break;

      //case TDI_CONNECTION_FILE:
      //   dprintf( "TDIH_Close: Connection File\n");
      //   break;
      case TDI_TRANSPORT_ADDRESS_FILE:
         return( TDIH_TdiCloseAddress( pTDIH_DeviceExtension, Irp, IrpSp ) );

      case TDI_CONNECTION_FILE:
         return( TDIH_TdiCloseConnection( pTDIH_DeviceExtension, Irp, IrpSp ) );
	  default:
		  if (pTDIH_DeviceExtension->NodeIdentifier.NodeType == TDIH_NODE_TYPE_IP_FILTER_DEVICE)
				return TDIH_TdiCloseAddress( pTDIH_DeviceExtension, Irp, IrpSp );
   }

   return TDIH_ChainRequest(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
}

NTSTATUS
TDIH_DefaultDispatch(
   PDEVICE_OBJECT DeviceObject,
   PIRP Irp
   )
{
   PIO_STACK_LOCATION      IrpSp = NULL;
   PTDI_DEV_EXT			   pTDIH_DeviceExtension = (PTDI_DEV_EXT )(DeviceObject->DeviceExtension);

   IrpSp = IoGetCurrentIrpStackLocation(Irp);

   //if( pTDIH_DeviceExtension->NodeIdentifier.NodeType == TDIH_NODE_TYPE_W32API_DEVICE )
   //{
	  // return W32DevDispatch(DeviceObject, Irp);
   //}

   if( pTDIH_DeviceExtension->NodeIdentifier.NodeType == TDIH_NODE_TYPE_IP_FILTER_DEVICE )
   {
      if(IrpSp -> Parameters.DeviceIoControl.IoControlCode == FILTER_ICMP_CODE)
		   return TDIH_IpDispatch( pTDIH_DeviceExtension, Irp, IrpSp);
   }
   
   switch(IrpSp->MajorFunction)
   {
      case IRP_MJ_CREATE:
         return( TDIH_Create(pTDIH_DeviceExtension, Irp, IrpSp) );

      case IRP_MJ_CLEANUP:
         return( TDIH_Cleanup(pTDIH_DeviceExtension, Irp, IrpSp) );

      case IRP_MJ_DEVICE_CONTROL:
         if( TdiMapUserRequest(DeviceObject, Irp, IrpSp) == STATUS_SUCCESS )
         {
            dprintf("TDIH_DefaultDispatch: Mapped DeviceControl To InternalDeviceControl\n" );
            return( TDIH_DispatchInternalDeviceControl(DeviceObject, Irp));
         }

         break;

      case IRP_MJ_CLOSE:
         return( TDIH_Close(pTDIH_DeviceExtension, Irp, IrpSp) );
   }

   return TDIH_ChainRequest(pTDIH_DeviceExtension->LowerDeviceObject, Irp);
}

NTSTATUS FilterAttach( PDRIVER_OBJECT pDriverObject, PWCHAR pDevName, PWCHAR pTargetName, 
						ULONG typ, PDEVICE_OBJECT * ppFilterDeviceObject ) 
{

	NTSTATUS            status;
	UNICODE_STRING      uniNtNameString;
	PTDI_DEV_EXT	 	pDeviceExtension = NULL;
	PFILE_OBJECT        pTargetFileObject = NULL;
	PDEVICE_OBJECT      pTargetDeviceObject = NULL;
	PDEVICE_OBJECT      pLowerDeviceObject = NULL;

	dprintf("FilterAttach: Device %ls, Target %ls\n", pDevName, pTargetName );
	ASSERT( KeGetCurrentIrql() <= PASSIVE_LEVEL );

	// Create counted string version of target TCP device name.
	RtlInitUnicodeString( &uniNtNameString, pTargetName );

	// get the target object
	status = IoGetDeviceObjectPointer( &uniNtNameString, FILE_READ_ATTRIBUTES, 
										&pTargetFileObject, &pTargetDeviceObject );
	if( ! NT_SUCCESS(status) ) 
	{
		dprintf("FilterAttach: Couldn't get the target device object\n");
		return status;
	}

	// Create counted string version of our TCP filter device name.
	RtlInitUnicodeString( &uniNtNameString, pDevName );

	// Create The Filter Device Object
	// Adopt the DeviceType and Characteristics of the target device.
	status = IoCreateDevice( pDriverObject, sizeof( TDI_DEV_EXT ), &uniNtNameString,
						pTargetDeviceObject->DeviceType, pTargetDeviceObject->Characteristics,
						FALSE, ppFilterDeviceObject );

	if( ! NT_SUCCESS(status) ) 
	{
		dprintf( "FilterAttach: Couldn't create the filter device object\n");
		ObDereferenceObject( pTargetFileObject );
		return status;
	}

	// Initialize The Extension For The Filter Device Object
	pDeviceExtension = (PTDI_DEV_EXT) (*ppFilterDeviceObject)->DeviceExtension;
	NdisZeroMemory( pDeviceExtension, sizeof( TDI_DEV_EXT ) );
	pDeviceExtension->NodeIdentifier.NodeType = typ;
	pDeviceExtension->NodeIdentifier.NodeSize = sizeof( TDI_DEV_EXT );
	pDeviceExtension->pFilterDeviceObject = * ppFilterDeviceObject;

	// Attach Our Filter To The target Device Object
	pLowerDeviceObject = IoAttachDeviceToDeviceStack( * ppFilterDeviceObject, pTargetDeviceObject );

	// if we can't attach - undo everything
	if( ! pLowerDeviceObject ) 
	{
		dprintf("FilterAttach: Couldn't attach to target Device Object\n");
		IoDeleteDevice( * ppFilterDeviceObject );
		ObDereferenceObject( pTargetFileObject );
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	// Initialize the TargetDeviceObject field in the extension.
	pDeviceExtension->TargetDeviceObject = pTargetDeviceObject;
	pDeviceExtension->TargetFileObject = pTargetFileObject;
	pDeviceExtension->LowerDeviceObject = pLowerDeviceObject;
	pDeviceExtension->DeviceExtensionFlags |= TDIH_DEV_EXT_ATTACHED;

	// Adopt Target Device I/O Flags
	(*ppFilterDeviceObject)->Flags |= pTargetDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO);

	dprintf("FilterAttach: pDeviceObject = %x\n", *ppFilterDeviceObject );

	return STATUS_SUCCESS;
}

NTSTATUS AttachToStack( PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pRegistryPath) 
{

	NTSTATUS status;
	PDEVICE_OBJECT pTcpDeviceObject, pUdpDeviceObject, pIpDeviceObject, pRawIpDeviceObject, pMcast;
		
	// Attach our filter to the TCP device
	status = FilterAttach( pDriverObject, TDIH_TCP_DEVICE_NAME, DD_TCP_DEVICE_NAME, 
									TDIH_NODE_TYPE_TCP_FILTER_DEVICE, &pTcpDeviceObject );
	// retry if can't get
	if (! NT_SUCCESS (status)) 
	{
		dprintf("AttachToStack: ERROR: could not attach to TCP filter Status = 0x%x\n", status);
		return status;
	}

	// Attach our filter to the UDP device
	status = FilterAttach( pDriverObject, TDIH_UDP_DEVICE_NAME, DD_UDP_DEVICE_NAME, 
									TDIH_NODE_TYPE_UDP_FILTER_DEVICE, &pUdpDeviceObject );
	if (! NT_SUCCESS (status)) 
	{
		dprintf("AttachToStack: ERROR: could not attach to UDP filter Status = 0x%x\n", status);
		return status;
	}
	
	// Attach our filter to the IP device
	status = FilterAttach( pDriverObject, TDIH_IP_DEVICE_NAME, DD_IP_DEVICE_NAME, 
									TDIH_NODE_TYPE_IP_FILTER_DEVICE, &pIpDeviceObject );
	
	if (! NT_SUCCESS (status)) 
	{
		dprintf("AttachToStack: ERROR: could not attach to IP filter Status = 0x%x\n", status);
	}

	status = FilterAttach( pDriverObject, TDIH_RAWIP_DEVICE_NAME, DD_RAWIP_DEVICE_NAME, 
									TDIH_NODE_TYPE_RAWIP_FILTER_DEVICE, &pRawIpDeviceObject );

	if (! NT_SUCCESS (status)) 
	{
		dprintf("AttachToStack: ERROR: could not attach to RAWIP filter Status = 0x%x\n", status);
	}

	/*status = FilterAttach( pDriverObject, TDIH_MCAST_DEVICE_NAME, DD_MCAST_DEVICE_NAME, 
									TDIH_NODE_TYPE_MCAST_FILTER_DEVICE, &pMcast );

	if (! NT_SUCCESS (status)) 
	{
		dprintf("AttachToStack: ERROR: could not attach to Multicast filter Status = 0x%x\n", status);
	}*/

	return STATUS_SUCCESS;
}

NTSTATUS
DriverEntry(
   PDRIVER_OBJECT DriverObject,
   PUNICODE_STRING RegistryPath
   )
{
   NTSTATUS status;
   int ind = 0;
   
   //W32DevInitialize(DriverObject, RegistryPath);

   status = RegisterFilterCallbacks();
   if( !NT_SUCCESS(status) )
   {
	   return status;
   }

   for (ind=0; ind <= IRP_MJ_MAXIMUM_FUNCTION; ind++)
   {
      DriverObject->MajorFunction[ind] = TDIH_DefaultDispatch;
   }
   
   DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] =  TDIH_DispatchInternalDeviceControl;
   DriverObject->DriverUnload = NULL;

   InitializeReassembly();

   initLists( );
   initAddrThread( );
   tdiInitFirewall( );
   
   status = AttachToStack( DriverObject, RegistryPath );
   if( !NT_SUCCESS(status) )
   {
	   DeregisterIpMonitor();
		DeregisterFilterCallbacks();
		return status;
   }

   RegisterIpMonitor( );
   
   return STATUS_SUCCESS;
}

NTSTATUS
  DllInitialize(
    IN PUNICODE_STRING  RegistryPath
    )
{
	dprintf( "===> DllInitialize\n" );
	return STATUS_UNSUCCESSFUL;
}