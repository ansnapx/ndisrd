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
#pragma hdrstop

#pragma NDIS_INIT_FUNCTION(DriverEntry)

NDIS_HANDLE         ProtHandle = NULL;
NDIS_HANDLE         DriverHandle = NULL;
NDIS_MEDIUM         MediumArray[2] =
                    {
                        NdisMedium802_3,    // Ethernet
                        NdisMediumWan       // NDISWAN
                    };

NDIS_SPIN_LOCK     GlobalLock;

PADAPT				pAdaptList = NULL;
LONG				MiniportCount = 0;
//DWORD				g_dwOpenCloseCounter = 0;
DWORD				g_dwMTUDecrement = 0;
DWORD				g_dwStartupMode = 0;
//UINT				DefaultData = 0;

NDIS_HANDLE			NdisWrapperHandle;

//
// To support ioctls from user-mode:
//

//#define LINKNAME_STRING     L"\\DosDevices\\Inspect"
//#define NTDEVICE_STRING     L"\\Device\\Inspect"

NDIS_HANDLE     NdisDeviceHandle = NULL;
PDEVICE_OBJECT  ControlDeviceObject = NULL;

enum _DEVICE_STATE
{
    PS_DEVICE_STATE_READY = 0,    // ready for create/delete
    PS_DEVICE_STATE_CREATING,    // create operation in progress
    PS_DEVICE_STATE_DELETING    // delete operation in progress
} ControlDeviceState = PS_DEVICE_STATE_READY;

//NTSTATUS
//	ndisrdReadMTUDecrement(
//		IN PWSTR ValueName,
//		IN ULONG ValueType,
//		IN PVOID ValueData,
//		IN ULONG ValueLength,
//		IN PVOID Context,
//		IN PVOID EntryContext
//		)
//{
//	if((ValueType == REG_DWORD)&&(ValueData != NULL)) 
//	{
//       	if((*((PULONG)ValueData) != 0))
//		{
//			g_dwMTUDecrement = *((PULONG)ValueData);
//		}
//		else
//		{
//			g_dwMTUDecrement = 0;
//		}
//	}
//	return STATUS_SUCCESS;
//}
//
//NTSTATUS
//	ndisrdReadStartupMode(
//		IN PWSTR ValueName,
//		IN ULONG ValueType,
//		IN PVOID ValueData,
//		IN ULONG ValueLength,
//		IN PVOID Context,
//		IN PVOID EntryContext
//		)
//{
//	if((ValueType == REG_DWORD)&&(ValueData != NULL)) 
//	{
//       	if((*((PULONG)ValueData) != 0))
//		{
//			g_dwStartupMode = *((PULONG)ValueData);
//		}
//		else
//		{
//			g_dwStartupMode = 0;
//		}
//	}
//	return STATUS_SUCCESS;
//}

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT        DriverObject,
    IN PUNICODE_STRING       RegistryPath
    )
/*++

Routine Description:

    First entry point to be called, when this driver is loaded.
    Register with NDIS as an intermediate driver.

Arguments:

    DriverObject - pointer to the system's driver object structure
        for this driver
    
    RegistryPath - system's registry path for this driver
    
Return Value:

    STATUS_SUCCESS if all initialization is successful, STATUS_XXX
    error code if not.

--*/
{
    NDIS_STATUS                        Status;
    NDIS_PROTOCOL_CHARACTERISTICS      PChars;
    NDIS_MINIPORT_CHARACTERISTICS      MChars;
    NDIS_STRING                        Name;
	RTL_QUERY_REGISTRY_TABLE		   QueryTable [4];
	//ULONG osMajorVersion = 0;
	//ULONG osMinorVersion = 0;

    Status = NDIS_STATUS_SUCCESS;
    NdisAllocateSpinLock(&GlobalLock);

    NdisMInitializeWrapper(&NdisWrapperHandle, DriverObject, RegistryPath, NULL);

	//PsGetVersion (&osMajorVersion, &osMinorVersion, NULL, NULL);
	
	//dprintf("Operating system version %d.%d\n", osMajorVersion, osMinorVersion);

	InitializeANSfltr();

	//IB_AllocateIntermediateBufferPool ();

    do
    {
        NdisZeroMemory(&MChars, sizeof(NDIS_MINIPORT_CHARACTERISTICS));

        MChars.MajorNdisVersion = NDISRD_MAJOR_NDIS_VERSION;
        MChars.MinorNdisVersion = NDISRD_MINOR_NDIS_VERSION;

        MChars.InitializeHandler = MPInitialize;
        MChars.QueryInformationHandler = MPQueryInformation;
        MChars.SetInformationHandler = MPSetInformation;
        MChars.ResetHandler = NULL;
        MChars.TransferDataHandler = MPTransferData;
        MChars.HaltHandler = MPHalt;
#ifdef NDIS51_MINIPORT
        MChars.CancelSendPacketsHandler = MPCancelSendPackets;
        MChars.PnPEventNotifyHandler = MPDevicePnPEvent;
        MChars.AdapterShutdownHandler = MPAdapterShutdown;
#endif // NDIS51_MINIPORT

        //
        // We will disable the check for hang timeout so we do not
        // need a check for hang handler!
        //
        MChars.CheckForHangHandler = NULL;
        MChars.ReturnPacketHandler = NULL;

        //
        // Either the Send or the SendPackets handler should be specified.
        // If SendPackets handler is specified, SendHandler is ignored
        //
        MChars.SendHandler = NULL;    // 
        MChars.SendPacketsHandler = MPSendPackets;

        Status = NdisIMRegisterLayeredMiniport(NdisWrapperHandle,
                                                  &MChars,
                                                  sizeof(MChars),
                                                  &DriverHandle);
        if (Status != NDIS_STATUS_SUCCESS)
        {
			dprintf("Failed NdisIMRegisterLayeredMiniport 0x%x\n", Status);
            break;
        }

#ifndef WIN9X
        NdisMRegisterUnloadHandler(NdisWrapperHandle, PtUnload);
#endif

        //
        // Now register the protocol.
        //
        NdisZeroMemory(&PChars, sizeof(NDIS_PROTOCOL_CHARACTERISTICS));
        PChars.MajorNdisVersion = NDISRD_PROT_MAJOR_NDIS_VERSION;
        PChars.MinorNdisVersion = NDISRD_PROT_MINOR_NDIS_VERSION;

        //
        // Make sure the protocol-name matches the service-name
        // (from the INF) under which this protocol is installed.
        // This is needed to ensure that NDIS can correctly determine
        // the binding and call us to bind to miniports below.
        //
        //NdisInitUnicodeString(&Name, L"Inspect");    // Protocol name
		NdisInitUnicodeString(&Name, L"ansfltr");    // Protocol name
        PChars.Name = Name;
        PChars.OpenAdapterCompleteHandler = PtOpenAdapterComplete;
        PChars.CloseAdapterCompleteHandler = PtCloseAdapterComplete;
        PChars.SendCompleteHandler = PtSendComplete;
        PChars.TransferDataCompleteHandler = PtTransferDataComplete;
    
        PChars.ResetCompleteHandler = PtResetComplete;
        PChars.RequestCompleteHandler = PtRequestComplete;
        PChars.ReceiveHandler = PtReceive;
        PChars.ReceiveCompleteHandler = PtReceiveComplete;
        PChars.StatusHandler = PtStatus;
        PChars.StatusCompleteHandler = PtStatusComplete;
        PChars.BindAdapterHandler = PtBindAdapter;
        PChars.UnbindAdapterHandler = PtUnbindAdapter;
        PChars.UnloadHandler = PtUnloadProtocol;

		PChars.ReceivePacketHandler = NULL;//PtReceivePacket;
        PChars.PnPEventHandler= PtPNPHandler;

        NdisRegisterProtocol(&Status,
                             &ProtHandle,
                             &PChars,
                             sizeof(NDIS_PROTOCOL_CHARACTERISTICS));

        if (Status != NDIS_STATUS_SUCCESS)
        {
			dprintf("Failed NdisRegisterProtocol 0x%x\n", Status);
            NdisIMDeregisterLayeredMiniport(DriverHandle);
            break;
        }

        NdisIMAssociateMiniport(DriverHandle, ProtHandle);
    }
    while (FALSE);

	if (Status != NDIS_STATUS_SUCCESS)
    {
		InitializeANSfltr();
        NdisTerminateWrapper(NdisWrapperHandle, NULL);
    }

    return(Status);
}


NDIS_STATUS
PtRegisterDevice(
    VOID
    )
/*++

Routine Description:

    Register an ioctl interface - a device object to be used for this
    purpose is created by NDIS when we call NdisMRegisterDevice.

    This routine is called whenever a new miniport instance is
    initialized. However, we only create one global device object,
    when the first miniport instance is initialized. This routine
    handles potential race conditions with PtDeregisterDevice via
    the ControlDeviceState and MiniportCount variables.

    NOTE: do not call this from DriverEntry; it will prevent the driver
    from being unloaded (e.g. on uninstall).

Arguments:

    None

Return Value:

    NDIS_STATUS_SUCCESS if we successfully register a device object.

--*/
{
    NDIS_STATUS					Status = NDIS_STATUS_SUCCESS;
    UNICODE_STRING				DeviceName;
    UNICODE_STRING				DeviceLinkUnicodeString;
    PDRIVER_DISPATCH			DispatchTable[IRP_MJ_MAXIMUM_FUNCTION+1];
	UINT						Counter = 0;
	
    DBGPRINT(("==>PtRegisterDevice\n"));

    NdisAcquireSpinLock(&GlobalLock);

    ++MiniportCount;
    
    if (1 == MiniportCount)
    {
        ASSERT(ControlDeviceState != PS_DEVICE_STATE_CREATING);

        //
        // Another thread could be running PtDeregisterDevice on
        // behalf of another miniport instance. If so, wait for
        // it to exit.
        //
        while (ControlDeviceState != PS_DEVICE_STATE_READY)
        {
            NdisReleaseSpinLock(&GlobalLock);
            NdisMSleep(1);
            NdisAcquireSpinLock(&GlobalLock);
        }

        ControlDeviceState = PS_DEVICE_STATE_CREATING;

        NdisReleaseSpinLock(&GlobalLock);

		NdisZeroMemory( DispatchTable, sizeof(DispatchTable) );
		DispatchTable[IRP_MJ_CREATE] = FilterDispatch;
		DispatchTable[IRP_MJ_CLEANUP] = FilterDispatch;
		DispatchTable[IRP_MJ_CLOSE] = FilterDispatch;
		DispatchTable[IRP_MJ_INTERNAL_DEVICE_CONTROL] = FilterDeviceIoControl;

        NdisInitUnicodeString(&DeviceName, FLT_DEVICE_NAME);
        NdisInitUnicodeString(&DeviceLinkUnicodeString, FLT_DEVICE_LINK);
        
        Status = NdisMRegisterDevice(
                    NdisWrapperHandle, 
                    &DeviceName,
                    &DeviceLinkUnicodeString,
                    &DispatchTable[0],
                    &ControlDeviceObject,
                    &NdisDeviceHandle
                    );

		if (Status == NDIS_STATUS_SUCCESS)
			ControlDeviceObject->Flags |= DO_BUFFERED_IO;

		//if(Status == STATUS_SUCCESS)
		//	ntSetSecurityObjectForEveryoneByLinkName(&DeviceLinkUnicodeString);

		NdisAcquireSpinLock(&GlobalLock);

        ControlDeviceState = PS_DEVICE_STATE_READY;
    }

    NdisReleaseSpinLock(&GlobalLock);

    DBGPRINT(("<==PtRegisterDevice: %x\n", Status));

    return (Status);
}

NDIS_STATUS
PtDeregisterDevice(
    VOID
    )
/*++

Routine Description:

    Deregister the ioctl interface. This is called whenever a miniport
    instance is halted. When the last miniport instance is halted, we
    request NDIS to delete the device object

Arguments:

    NdisDeviceHandle - Handle returned by NdisMRegisterDevice

Return Value:

    NDIS_STATUS_SUCCESS if everything worked ok

--*/
{
    NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

    DBGPRINT(("==>PassthruDeregisterDevice\n"));

    NdisAcquireSpinLock(&GlobalLock);

    ASSERT(MiniportCount > 0);

    --MiniportCount;
    
    if (0 == MiniportCount)
    {
        //
        // All miniport instances have been halted. Deregister
        // the control device.
        //

        ASSERT(ControlDeviceState == PS_DEVICE_STATE_READY);

        //
        // Block PtRegisterDevice() while we release the control
        // device lock and deregister the device.
        // 
        ControlDeviceState = PS_DEVICE_STATE_DELETING;

        NdisReleaseSpinLock(&GlobalLock);

        if (NdisDeviceHandle != NULL)
        {
            Status = NdisMDeregisterDevice(NdisDeviceHandle);
            NdisDeviceHandle = NULL;
        }

        NdisAcquireSpinLock(&GlobalLock);
        ControlDeviceState = PS_DEVICE_STATE_READY;
    }

    NdisReleaseSpinLock(&GlobalLock);

    DBGPRINT(("<== PassthruDeregisterDevice: %x\n", Status));
    return Status;
    
}

VOID
PtUnload(
    IN PDRIVER_OBJECT        DriverObject
    )
{
	UNREFERENCED_PARAMETER(DriverObject);

    PtUnloadProtocol();
    NdisIMDeregisterLayeredMiniport(DriverHandle);
	UnInitializeANSfltr();
	NdisTerminateWrapper(NdisWrapperHandle, NULL);
}
