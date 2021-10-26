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

#define MAX_PACKET_POOL_SIZE 0x0000FFFF
#define MIN_PACKET_POOL_SIZE 0x000000FF

VOID
PtBindAdapter(
    OUT PNDIS_STATUS            Status,
    IN  NDIS_HANDLE             BindContext,
    IN  PNDIS_STRING            DeviceName,
    IN  PVOID                   SystemSpecific1,
    IN  PVOID                   SystemSpecific2
    )
/*++

Routine Description:

    Called by NDIS to bind to a miniport below.

Arguments:

    Status            - Return status of bind here.
    BindContext        - Can be passed to NdisCompleteBindAdapter if this call is pended.
    DeviceName         - Device name to bind to. This is passed to NdisOpenAdapter.
    SystemSpecific1    - Can be passed to NdisOpenProtocolConfiguration to read per-binding information
    SystemSpecific2    - Unused

Return Value:

    NDIS_STATUS_PENDING    if this call is pended. In this case call NdisCompleteBindAdapter
    to complete.
    Anything else          Completes this call synchronously

--*/
{
    NDIS_HANDLE                     ConfigHandle = NULL;
    PNDIS_CONFIGURATION_PARAMETER   Param;
    NDIS_STRING                     DeviceStr = NDIS_STRING_CONST("UpperBindings");
    PADAPT                          pAdapt = NULL;
    NDIS_STATUS                     Sts, nStatus;
    UINT                            MediumIndex;
    ULONG                           TotalSize;
	unsigned char					szAdapterNameBuffer [ADAPTER_NAME_SIZE];
	ANSI_STRING						szAdapterName = {0, ADAPTER_NAME_SIZE, szAdapterNameBuffer};


    UNREFERENCED_PARAMETER(BindContext);
    UNREFERENCED_PARAMETER(SystemSpecific2);
    
    DBGPRINT(("==> Protocol BindAdapter\n"));

    do
    {
        //
        // Access the configuration section for our binding-specific
        // parameters.
        //
        NdisOpenProtocolConfiguration(Status,
                                       &ConfigHandle,
                                       SystemSpecific1);

        if (*Status != NDIS_STATUS_SUCCESS)
        {
            break;
        }

        //
        // Read the "UpperBindings" reserved key that contains a list
        // of device names representing our miniport instances corresponding
        // to this lower binding. Since this is a 1:1 IM driver, this key
        // contains exactly one name.
        //
        // If we want to implement a N:1 mux driver (N adapter instances
        // over a single lower binding), then UpperBindings will be a
        // MULTI_SZ containing a list of device names - we would loop through
        // this list, calling NdisIMInitializeDeviceInstanceEx once for
        // each name in it.
        //
        NdisReadConfiguration(Status,
                              &Param,
                              ConfigHandle,
                              &DeviceStr,
                              NdisParameterString);
        if (*Status != NDIS_STATUS_SUCCESS)
        {
            break;
        }

        //
        // Allocate memory for the Adapter structure. This represents both the
        // protocol context as well as the adapter structure when the miniport
        // is initialized.
        //
        // In addition to the base structure, allocate space for the device
        // instance string.
        //
        TotalSize = sizeof(ADAPT) + Param->ParameterData.StringData.MaximumLength;
        NdisAllocateMemoryWithTag(&pAdapt, TotalSize, TAG);

        if (pAdapt == NULL)
        {
            *Status = NDIS_STATUS_RESOURCES;
            break;
        }

        //
        // Initialize the adapter structure. We copy in the IM device
        // name as well, because we may need to use it in a call to
        // NdisIMCancelInitializeDeviceInstance. The string returned
        // by NdisReadConfiguration is active (i.e. available) only
        // for the duration of this call to our BindAdapter handler.
        //
        NdisZeroMemory(pAdapt, TotalSize);
        pAdapt->DeviceName.MaximumLength = Param->ParameterData.StringData.MaximumLength;
        pAdapt->DeviceName.Length = Param->ParameterData.StringData.Length;
        pAdapt->DeviceName.Buffer = (PWCHAR)((ULONG_PTR)pAdapt + sizeof(ADAPT));
        NdisMoveMemory(pAdapt->DeviceName.Buffer,
                       Param->ParameterData.StringData.Buffer,
                       Param->ParameterData.StringData.MaximumLength);

        NdisInitializeEvent(&pAdapt->Event);
        NdisAllocateSpinLock(&pAdapt->Lock);
        
		//******************************************************
		// Allocate universal packet pool
		NdisAllocatePacketPool( 
			Status,
			&pAdapt->m_hPacketPool, 
			NUMBER_OF_PACKET_DESCRIPTORS, 
			sizeof(NDISHK_PACKET_RESERVED)
			);
		
		if( *Status != NDIS_STATUS_SUCCESS )
		{
			break;
		}

		NdisAllocateBufferPool ( 
			Status, 
			&pAdapt->m_hBufferPool, 
			NUMBER_OF_BUFFER_DESCRIPTORS
			);

		if( *Status != NDIS_STATUS_SUCCESS )
		{
			break;
		}

		// Copy the adapter name below us
		nStatus  = NdisAllocateMemoryWithTag ( 
						&pAdapt->m_AdapterName.Buffer, 
						DeviceName->MaximumLength, 
						'rtpR'
						);

		if (nStatus == NDIS_STATUS_SUCCESS)
		{
			NdisMoveMemory ( 
				pAdapt->m_AdapterName.Buffer,
				DeviceName->Buffer,
				DeviceName->MaximumLength
				);

			pAdapt->m_AdapterName.Length = DeviceName->Length;
			pAdapt->m_AdapterName.MaximumLength = DeviceName->MaximumLength;

			NdisUnicodeStringToAnsiString (
				&szAdapterName,
				&pAdapt->m_AdapterName
				);

			NdisMoveMemory (
				&pAdapt->m_szAdapterName,
				szAdapterName.Buffer,
				ADAPTER_NAME_SIZE
				);
		}
		else
		{
			pAdapt->m_AdapterName.Buffer = NULL;
			pAdapt->m_AdapterName.Length = 0;
			pAdapt->m_AdapterName.MaximumLength = 0;
		}

		// Transfer data internal structures
		InitializeListHead(&pAdapt->m_PendingTransfertData);
		NdisAllocateSpinLock(&pAdapt->m_PendingTransfertLock);

		// NDIS request functionality internal structures
		InitializeListHead(&pAdapt->m_InternalRequest);
		NdisAllocateSpinLock(&pAdapt->m_InternalRequestLock);

		// Initialize extension
		EXT_InitializeNdisrdExtension(&pAdapt->m_NdisrdExtension);

		//*******************************************************
        //
        // Now open the adapter below and complete the initialization
        //
        NdisOpenAdapter(Status,
                          &Sts,
                          &pAdapt->BindingHandle,
                          &MediumIndex,
                          MediumArray,
                          sizeof(MediumArray)/sizeof(NDIS_MEDIUM),
                          ProtHandle,
                          pAdapt,
                          DeviceName,
                          0,
                          NULL);

        if (*Status == NDIS_STATUS_PENDING)
        {
            NdisWaitEvent(&pAdapt->Event, 0);
            *Status = pAdapt->Status;
        }

        if (*Status != NDIS_STATUS_SUCCESS)
        {
            break;
        }

		// Signal adapter list change event if g_AdapterEvent initialized
		if ( g_AdapterEvent )
			KeSetEvent (g_AdapterEvent, 1, FALSE);

        pAdapt->m_Medium = MediumArray[MediumIndex];

        //
        // Now ask NDIS to initialize our miniport (upper) edge.
        // Set the flag below to synchronize with a possible call
        // to our protocol Unbind handler that may come in before
        // our miniport initialization happens.
        //
        pAdapt->MiniportInitPending = TRUE;
        NdisInitializeEvent(&pAdapt->MiniportInitEvent);

        *Status = NdisIMInitializeDeviceInstanceEx(DriverHandle,
                                           &pAdapt->DeviceName,
                                           pAdapt);

        if (*Status != NDIS_STATUS_SUCCESS)
        {
            DBGPRINT(("BindAdapter: Adapt %p, IMInitializeDeviceInstance error %x\n",
                pAdapt, *Status));
            break;
        }

    } while(FALSE);

    //
    // Close the configuration handle now - see comments above with
    // the call to NdisIMInitializeDeviceInstanceEx.
    //
    if (ConfigHandle != NULL)
    {
        NdisCloseConfiguration(ConfigHandle);
    }

    if (*Status != NDIS_STATUS_SUCCESS)
    {
        if (pAdapt != NULL)
        {
            if (pAdapt->BindingHandle != NULL)
            {
                NDIS_STATUS    LocalStatus;

                //
                // Close the binding we opened above.
                //

                NdisResetEvent(&pAdapt->Event);
                
                NdisCloseAdapter(&LocalStatus, pAdapt->BindingHandle);
                pAdapt->BindingHandle = NULL;

                if (LocalStatus == NDIS_STATUS_PENDING)
                {
                     NdisWaitEvent(&pAdapt->Event, 0);
                     LocalStatus = pAdapt->Status;
                }
            }

			if (pAdapt->m_hPacketPool != NULL)
            {
                 NdisFreePacketPool(pAdapt->m_hPacketPool);
            }

			if (pAdapt->m_hBufferPool != NULL)
            {
                 NdisFreeBufferPool(pAdapt->m_hBufferPool);
            }

			if (pAdapt->m_AdapterName.Buffer)
			{
				NdisFreeMemory(pAdapt->m_AdapterName.Buffer, pAdapt->m_AdapterName.MaximumLength, 0);
			}

			NdisFreeMemory(pAdapt, 0, 0);
            pAdapt = NULL;
        }
    }


    DBGPRINT(("<== Protocol BindAdapter: pAdapt %p, Status %x\n", pAdapt, *Status));
}


VOID
PtOpenAdapterComplete(
    IN  NDIS_HANDLE             ProtocolBindingContext,
    IN  NDIS_STATUS             Status,
    IN  NDIS_STATUS             OpenErrorStatus
    )
/*++

Routine Description:

    Completion routine for NdisOpenAdapter issued from within the PtBindAdapter. Simply
    unblock the caller.

Arguments:

    ProtocolBindingContext    Pointer to the adapter
    Status                    Status of the NdisOpenAdapter call
    OpenErrorStatus            Secondary status(ignored by us).

Return Value:

    None

--*/
{
    PADAPT      pAdapt =(PADAPT)ProtocolBindingContext;

	DBGPRINT(("==> PtOpenAdapterComplete: Adapt %p\n", pAdapt));
    
    UNREFERENCED_PARAMETER(OpenErrorStatus);
    
    DBGPRINT(("==> PtOpenAdapterComplete: Adapt %p, Status %x\n", pAdapt, Status));
    pAdapt->Status = Status;
    NdisSetEvent(&pAdapt->Event);

	DBGPRINT(("<== PtOpenAdapterComplete: Adapt %p\n", pAdapt));
}


VOID
PtUnbindAdapter(
    OUT PNDIS_STATUS        Status,
    IN  NDIS_HANDLE            ProtocolBindingContext,
    IN  NDIS_HANDLE            UnbindContext
    )
/*++

Routine Description:

    Called by NDIS when we are required to unbind to the adapter below.
    This functions shares functionality with the miniport's HaltHandler.
    The code should ensure that NdisCloseAdapter and NdisFreeMemory is called
    only once between the two functions

Arguments:

    Status                    Placeholder for return status
    ProtocolBindingContext    Pointer to the adapter structure
    UnbindContext            Context for NdisUnbindComplete() if this pends

Return Value:

    Status for NdisIMDeinitializeDeviceContext

--*/
{
    PADAPT         pAdapt =(PADAPT)ProtocolBindingContext;
    NDIS_STATUS    LocalStatus;

    UNREFERENCED_PARAMETER(UnbindContext);

	DBGPRINT(("==> PtUnbindAdapter: Adapt %p\n", pAdapt));

	//
    // Set the flag that the miniport below is unbinding, so the request handlers will
    // fail any request comming later
    // 
    NdisAcquireSpinLock(&pAdapt->Lock);
    pAdapt->UnbindingInProcess = TRUE;
    if (pAdapt->QueuedRequest == TRUE)
    {
        pAdapt->QueuedRequest = FALSE;
        NdisReleaseSpinLock(&pAdapt->Lock);

        PtRequestComplete(pAdapt,
                         &pAdapt->Request,
                         NDIS_STATUS_FAILURE );

    }
    else
    {
        NdisReleaseSpinLock(&pAdapt->Lock);
    }
#ifndef WIN9X
    //
    // Check if we had called NdisIMInitializeDeviceInstanceEx and
    // we are awaiting a call to MiniportInitialize.
    //
    if (pAdapt->MiniportInitPending == TRUE)
    {
        //
        // Try to cancel the pending IMInit process.
        //
        LocalStatus = NdisIMCancelInitializeDeviceInstance(
                        DriverHandle,
                        &pAdapt->DeviceName);

        if (LocalStatus == NDIS_STATUS_SUCCESS)
        {
            //
            // Successfully cancelled IM Initialization; our
            // Miniport Initialize routine will not be called
            // for this device.
            //
            pAdapt->MiniportInitPending = FALSE;
            ASSERT(pAdapt->MiniportHandle == NULL);
        }
        else
        {
            //
            // Our Miniport Initialize routine will be called
            // (may be running on another thread at this time).
            // Wait for it to finish.
            //
            NdisWaitEvent(&pAdapt->MiniportInitEvent, 0);
            ASSERT(pAdapt->MiniportInitPending == FALSE);
        }

    }
#endif // !WIN9X

    //
    // Call NDIS to remove our device-instance. We do most of the work
    // inside the HaltHandler.
    //
    // The Handle will be NULL if our miniport Halt Handler has been called or
    // if the IM device was never initialized
    //
    
    if (pAdapt->MiniportHandle != NULL)
    {
        *Status = NdisIMDeInitializeDeviceInstance(pAdapt->MiniportHandle);

        if (*Status != NDIS_STATUS_SUCCESS)
        {
            *Status = NDIS_STATUS_FAILURE;
        }
    }
    else
    {
        //
        // We need to do some work here. 
        // Close the binding below us 
        // and release the memory allocated.
        //
        if(pAdapt->BindingHandle != NULL)
        {
            NdisResetEvent(&pAdapt->Event);

            NdisCloseAdapter(Status, pAdapt->BindingHandle);

            //
            // Wait for it to complete
            //
            if(*Status == NDIS_STATUS_PENDING)
            {
                 NdisWaitEvent(&pAdapt->Event, 0);
                 *Status = pAdapt->Status;
            }
            pAdapt->BindingHandle = NULL;
        }
        else
        {
            //
            // Both Our MiniportHandle and Binding Handle  should not be NULL.
            //
            *Status = NDIS_STATUS_FAILURE;
            ASSERT(0);
        }

        //
        //    Free the memory here, if was not released earlier(by calling the HaltHandler)
        //
		if (pAdapt->m_AdapterName.Buffer)
		{
			NdisFreeMemory(pAdapt->m_AdapterName.Buffer, pAdapt->m_AdapterName.MaximumLength, 0);
		}

        NdisFreeMemory(pAdapt, 0, 0);
    }

    DBGPRINT(("<== PtUnbindAdapter: Adapt %p\n", pAdapt));
}

VOID
PtUnloadProtocol(
    VOID
)
{
    NDIS_STATUS Status;

    if (ProtHandle != NULL)
    {
        NdisDeregisterProtocol(&Status, ProtHandle);
        ProtHandle = NULL;
    }

    DBGPRINT(("PtUnloadProtocol: done!\n"));
}



VOID
PtCloseAdapterComplete(
    IN    NDIS_HANDLE            ProtocolBindingContext,
    IN    NDIS_STATUS            Status
    )
/*++

Routine Description:

    Completion for the CloseAdapter call.

Arguments:

    ProtocolBindingContext    Pointer to the adapter structure
    Status                    Completion status

Return Value:

    None.

--*/
{
    PADAPT      pAdapt =(PADAPT)ProtocolBindingContext;

    DBGPRINT(("CloseAdapterComplete: Adapt %p, Status %x\n", pAdapt, Status));
    pAdapt->Status = Status;
    NdisSetEvent(&pAdapt->Event);
//	stateOnCloseAdapterWithLock(pAdapt);
//	ruleCloseAdapter(pAdapt);
}


VOID
PtResetComplete(
    IN  NDIS_HANDLE            ProtocolBindingContext,
    IN  NDIS_STATUS            Status
    )
/*++

Routine Description:

    Completion for the reset.

Arguments:

    ProtocolBindingContext    Pointer to the adapter structure
    Status                    Completion status

Return Value:

    None.

--*/
{

    UNREFERENCED_PARAMETER(ProtocolBindingContext);
    UNREFERENCED_PARAMETER(Status);
    //
    // We never issue a reset, so we should not be here.
    //
    ASSERT(0);
}


VOID
PtRequestComplete(
    IN  NDIS_HANDLE            ProtocolBindingContext,
    IN  PNDIS_REQUEST          NdisRequest,
    IN  NDIS_STATUS            Status
    )
/*++

Routine Description:

    Completion handler for the previously posted request. All OIDS
    are completed by and sent to the same miniport that they were requested for.
    If Oid == OID_PNP_QUERY_POWER then the data structure needs to returned with all entries =
    NdisDeviceStateUnspecified

Arguments:

    ProtocolBindingContext    Pointer to the adapter structure
    NdisRequest                The posted request
    Status                    Completion status

Return Value:

    None

--*/
{
    PADAPT        pAdapt = (PADAPT)ProtocolBindingContext;
    NDIS_OID      Oid = pAdapt->Request.DATA.SET_INFORMATION.Oid ;

	PEXT_NDIS_REQUEST	pRequest;
	PIRP                Irp;
	PIO_STACK_LOCATION  IrpSp;
	UINT                ControlCode;
	PPACKET_OID_DATA    OidData;
	PADAPT				pAdapter = pAdapt;

	DBGPRINT(("==> PtRequestComplete: Adapt %p\n", pAdapt));

	if( !pAdapt )
		return;

	// Check if it was internal request and process it
	NdisAcquireSpinLock ( &pAdapter->m_InternalRequestLock );

	if ( !IsListEmpty ( &pAdapter -> m_InternalRequest ) )
	{
		pRequest = ( PEXT_NDIS_REQUEST ) pAdapter -> m_InternalRequest.Flink;
		while ( pRequest != ( PEXT_NDIS_REQUEST ) &pAdapter -> m_InternalRequest )
		{
			if ( & pRequest -> m_NdisRequest == NdisRequest )
			{
				// It was internal request
				/*if (pRequest->m_bIsUserRequest)
				{
				// It was user-mode request
				Irp = pRequest->m_Irp;
				IrpSp = IoGetCurrentIrpStackLocation(Irp);

				ControlCode=IrpSp->Parameters.DeviceIoControl.IoControlCode;

				OidData=Irp->AssociatedIrp.SystemBuffer;

				if (ControlCode == IOCTL_NDISRD_NDIS_SET_REQUEST) 
				{
				if (Status == NDIS_STATUS_SUCCESS)
				OidData->Length = pRequest->m_NdisRequest.DATA.SET_INFORMATION.BytesRead;
				else
				OidData->Length = Status;
				}
				else
				{
				if (ControlCode == IOCTL_NDISRD_NDIS_GET_REQUEST) 
				{
				if (Status == NDIS_STATUS_SUCCESS)
				OidData->Length = pRequest->m_NdisRequest.DATA.QUERY_INFORMATION.BytesWritten;
				else
				OidData->Length = Status;
				}

				}
				Irp->IoStatus.Status = Status;
				Irp->IoStatus.Information = IrpSp->Parameters.DeviceIoControl.InputBufferLength;

				IoCompleteRequest(Irp, IO_NO_INCREMENT);
				}*/

				RemoveEntryList ( &pRequest->m_qLink );
				NdisReleaseSpinLock ( &pAdapter->m_InternalRequestLock );
				RF_FreeRequest ( pRequest );
				DBGPRINT(("<== PtRequestComplete: Adapt %p\n", pAdapt));
				return;
			}
			pRequest = ( PEXT_NDIS_REQUEST ) pRequest -> m_qLink.Flink;
		}

		NdisReleaseSpinLock ( &pAdapter->m_InternalRequestLock );

		if ( Status == NDIS_STATUS_SUCCESS )
		{
			// Filter request
			RF_FilterRequestCompleteHandler ( pAdapter, NdisRequest );
		}
	}
	else
	{
		NdisReleaseSpinLock ( &pAdapter->m_InternalRequestLock );
	}

    //
    // Since our request is not outstanding anymore
    //
    ASSERT(pAdapt->OutstandingRequests == TRUE);

    pAdapt->OutstandingRequests = FALSE;

    //
    // Complete the Set or Query, and fill in the buffer for OID_PNP_CAPABILITIES, if need be.
    //
    switch (NdisRequest->RequestType)
    {
      case NdisRequestQueryInformation:

        //
        // We never pass OID_PNP_QUERY_POWER down.
        //
        ASSERT(Oid != OID_PNP_QUERY_POWER);

        if ((Oid == OID_PNP_CAPABILITIES) && (Status == NDIS_STATUS_SUCCESS))
        {
            MPQueryPNPCapabilities(pAdapt, &Status);
        }
        *pAdapt->BytesReadOrWritten = NdisRequest->DATA.QUERY_INFORMATION.BytesWritten;
        *pAdapt->BytesNeeded = NdisRequest->DATA.QUERY_INFORMATION.BytesNeeded;

        if ((Oid == OID_GEN_MAC_OPTIONS) && (Status == NDIS_STATUS_SUCCESS))
        {
            //
            // Remove the no-loopback bit from mac-options. In essence we are
            // telling NDIS that we can handle loopback. We don't, but the
            // interface below us does. If we do not do this, then loopback
            // processing happens both below us and above us. This is wasteful
            // at best and if Netmon is running, it will see multiple copies
            // of loopback packets when sniffing above us.
            //
            // Only the lowest miniport is a stack of layered miniports should
            // ever report this bit set to NDIS.
            //
            *(PULONG)NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer &= ~NDIS_MAC_OPTION_NO_LOOPBACK;
        }

        NdisMQueryInformationComplete(pAdapt->MiniportHandle,
                                      Status);
        break;

      case NdisRequestSetInformation:

        ASSERT( Oid != OID_PNP_SET_POWER);

        *pAdapt->BytesReadOrWritten = NdisRequest->DATA.SET_INFORMATION.BytesRead;
        *pAdapt->BytesNeeded = NdisRequest->DATA.SET_INFORMATION.BytesNeeded;
        NdisMSetInformationComplete(pAdapt->MiniportHandle,
                                    Status);
        break;

      default:
        ASSERT(0);
        break;
    }

	DBGPRINT(("<== PtRequestComplete: Adapt %p\n", pAdapt));    
}


VOID
PtStatus(
    IN  NDIS_HANDLE         ProtocolBindingContext,
    IN  NDIS_STATUS         GeneralStatus,
    IN  PVOID               StatusBuffer,
    IN  UINT                StatusBufferSize
    )
/*++

Routine Description:

    Status handler for the lower-edge(protocol).

Arguments:

    ProtocolBindingContext    Pointer to the adapter structure
    GeneralStatus             Status code
    StatusBuffer              Status buffer
    StatusBufferSize          Size of the status buffer

Return Value:

    None

--*/
{
    PADAPT				pAdapter = (PADAPT)ProtocolBindingContext;
	PNDIS_WAN_LINE_UP	pLineUp;

	DBGPRINT(("==> PtStatus: Adapt %p\n", pAdapter));

	// In case of NDIS_STATUS_WAN_LINE_UP we will initialize some adapter entry
	// fields (speed of connection, MACs of NDISWANIP and remote peer, indicated IP
	// address and network mask if they are dynamic)
	if ( GeneralStatus == NDIS_STATUS_WAN_LINE_UP )
	{
		pLineUp = (PNDIS_WAN_LINE_UP) StatusBuffer;

		NdisMoveMemory (
			pAdapter -> m_RemoteAddress,
			pLineUp -> RemoteAddress,
			ETHER_ADDR_LENGTH
			);
		pAdapter -> m_LinkSpeed = pLineUp -> LinkSpeed;
			
		pAdapter -> m_LastIndicatedNetworkMask = *(((PULONG)(pLineUp -> ProtocolBuffer)) + 1);
		pAdapter -> m_LastIndicatedIP = *(((PULONG)(pLineUp -> ProtocolBuffer)) + 2);
	
		// Save and decrement MTU if necessary
		pAdapter->m_usMTU = (USHORT)pLineUp->MaximumTotalSize;
		pLineUp->MaximumTotalSize = pLineUp->MaximumTotalSize - g_dwMTUDecrement;

	}
	else if (NDIS_STATUS_WAN_LINE_DOWN == GeneralStatus)
	{
		// Reset some fields in case of connection termination
		NdisZeroMemory (
			pAdapter -> m_RemoteAddress,
			ETHER_ADDR_LENGTH
			);

		NdisZeroMemory (
			pAdapter -> m_LocalAddress,
			ETHER_ADDR_LENGTH
			);

		pAdapter->m_LastIndicatedIP = pAdapter->m_LastIndicatedNetworkMask = 0;
		pAdapter->m_LastIndicatedNetworkMask = 0;
//		stateOnCloseAdapterWithLock(pAdapter);
//		ruleCloseAdapter(pAdapter);
			
	}

	if ( (GeneralStatus == NDIS_STATUS_WAN_LINE_UP)
		  ||(GeneralStatus == NDIS_STATUS_WAN_LINE_DOWN ) 
		  )
	{
		if (g_LineupEvent)
			KeSetEvent (g_LineupEvent, 1, FALSE);
	}

    //
    // Pass up this indication only if the upper edge miniport is initialized
    // and powered on. Also ignore indications that might be sent by the lower
    // miniport when it isn't at D0.
    //
    if ((pAdapter->MiniportHandle != NULL)  &&
        (pAdapter->MPDeviceState == NdisDeviceStateD0) &&
        (pAdapter->PTDeviceState == NdisDeviceStateD0))    
    {
        if ((GeneralStatus == NDIS_STATUS_MEDIA_CONNECT) || 
            (GeneralStatus == NDIS_STATUS_MEDIA_DISCONNECT))
        {
            
            pAdapter->LastIndicatedStatus = GeneralStatus;
        }
        NdisMIndicateStatus(pAdapter->MiniportHandle,
                            GeneralStatus,
                            StatusBuffer,
                            StatusBufferSize);

		if ( GeneralStatus == NDIS_STATUS_WAN_LINE_UP )
		{
			// Copy local address to adapter associated storage

			NdisMoveMemory (
				pAdapter -> m_LocalAddress,
				pLineUp -> LocalAddress,
				ETHER_ADDR_LENGTH
				);
		}
    }
    //
    // Save the last indicated media status 
    //
    else
    {
        if ((pAdapter->MiniportHandle != NULL) && 
        ((GeneralStatus == NDIS_STATUS_MEDIA_CONNECT) || 
            (GeneralStatus == NDIS_STATUS_MEDIA_DISCONNECT)))
        {
            pAdapter->LatestUnIndicateStatus = GeneralStatus;
        }
    }

	if(GeneralStatus == NDIS_STATUS_MEDIA_DISCONNECT)
	{
//		stateOnCloseAdapterWithLock(pAdapter);
//		ruleCloseAdapter(pAdapter);
	}

    DBGPRINT(("<== PtStatus: Adapt %p\n", pAdapter));
}


VOID
PtStatusComplete(
    IN NDIS_HANDLE            ProtocolBindingContext
    )
/*++

Routine Description:


Arguments:


Return Value:


--*/
{
    PADAPT      pAdapt = (PADAPT)ProtocolBindingContext;

	DBGPRINT(("==> PtStatusComplete: Adapt %p\n", pAdapt));
    //
    // Pass up this indication only if the upper edge miniport is initialized
    // and powered on. Also ignore indications that might be sent by the lower
    // miniport when it isn't at D0.
    //
    if ((pAdapt->MiniportHandle != NULL)  &&
        (pAdapt->MPDeviceState == NdisDeviceStateD0) &&
        (pAdapt->PTDeviceState == NdisDeviceStateD0))    
    {
        NdisMIndicateStatusComplete(pAdapt->MiniportHandle);
    }

	DBGPRINT(("<== PtStatusComplete: Adapt %p\n", pAdapt));
}


VOID
PtSendComplete(
    IN  NDIS_HANDLE            ProtocolBindingContext,
    IN  PNDIS_PACKET           Packet,
    IN  NDIS_STATUS            Status
    )
/*++

Routine Description:

    Called by NDIS when the miniport below had completed a send. We should
    complete the corresponding upper-edge send this represents.

Arguments:

    ProtocolBindingContext - Points to ADAPT structure
    Packet - Low level packet being completed
    Status - status of send

Return Value:

    None

--*/
{
    UF_FreePacketAndBuffers ((PNDISHK_PACKET)Packet);
}       


VOID
PtTransferDataComplete(
    IN  NDIS_HANDLE         ProtocolBindingContext,
    IN  PNDIS_PACKET        Packet,
    IN  NDIS_STATUS         Status,
    IN  UINT                BytesTransferred
    )
/*++

Routine Description:

    Entry point called by NDIS to indicate completion of a call by us
    to NdisTransferData.

    See notes under SendComplete.

Arguments:

Return Value:

--*/
{
    PADAPT      pAdapter =(PADAPT)ProtocolBindingContext;
	PNDISHK_PACKET			pHKPacket;
	PFLT_PACKET	pBuffer;
	PLIST_ENTRY				pLink;
	PNDIS_BUFFER			pResidualBuffer;
	UINT uPacketLength;

	DBGPRINT(("==> PtTransferDataComplete: Adapt %p\n", pAdapter));

    if(pAdapter->MiniportHandle)
    {
		NdisAcquireSpinLock ( &pAdapter->m_PendingTransfertLock );
		
		if ( !IsListEmpty ( &pAdapter -> m_PendingTransfertData ) )
		{
			// We did calls to TransferDataHandler or directly to this handler
			// Find assiciated pending tranfert data call
			pLink = pAdapter -> m_PendingTransfertData.Flink;

			while ( pLink != &pAdapter -> m_PendingTransfertData )
			{
				pHKPacket = CONTAINING_RECORD ( 
									pLink,
									NDISHK_PACKET,
									m_Reserved.m_qLink
									);
				
				if ( (PNDIS_PACKET) pHKPacket == Packet)
				{
					// It is our packet and we should process it
					
					if (pHKPacket -> m_Reserved.m_nTransferDataStatus != NDIS_STATUS_SUCCESS)
					{
						// If an error occured during transfer free associated resources
						RemoveEntryList ( &pHKPacket->m_Reserved.m_qLink );
						NdisReleaseSpinLock ( &pAdapter->m_PendingTransfertLock );
						UF_FreePacketAndBuffers ( pHKPacket );
						DBGPRINT(("<== PtTransferDataComplete: Adapt %p\n", pAdapter));
						return;
					}

					//
					// Packet processing
					//

					// Allocate intermediate buffer to save packet
					//pBuffer = IB_AllocateIntermediateBuffer ();
					NdisQueryPacketLength( Packet, &uPacketLength );
					// If buffer succesfully allocated process packet
					if (NT_SUCCESS(NdisAllocateMemoryWithTag(&pBuffer, sizeof(FLT_PACKET)+uPacketLength, DRIVER_TAG)))
					{
						pBuffer->m_pBuffer = (PVOID)((ULONG_PTR)pBuffer+sizeof(FLT_PACKET));
						// Reorder buffers in packet (call to transfer data
						// copys remain of data to the first buffer, but the beggining
						// of the packet is in the second)
						NdisUnchainBufferAtFront(Packet, &pResidualBuffer);
						NdisChainBufferAtBack (Packet, pResidualBuffer);
						// Copy packet content into intermediate buffer
						UF_ReadOnPacket (
							Packet,
							(PUCHAR)pBuffer->m_pBuffer,
							uPacketLength,
							0,
							&pBuffer->m_BufferSize
							);
						//FLT_FilterReceivedPacket ( pAdapter, pBuffer );

						// Free resources associated with packet
						RemoveEntryList ( &pHKPacket->m_Reserved.m_qLink );
						NdisReleaseSpinLock ( &pAdapter->m_PendingTransfertLock );

						//pBuffer -> m_Flags = NdisGetPacketFlags ( pPacketPtr[counter] );
						pBuffer -> m_pAdapter = pAdapter;
						pBuffer -> m_Direction = PHYSICAL_DIRECTION_IN;
						pBuffer -> m_References = 1;

						NdisAcquireSpinLock( &g_PacketLock );
						InsertTailList( &g_PacketsList, &pBuffer->m_qLink );
						NdisReleaseSpinLock( &g_PacketLock );

						FLTInjectRequest( NULL, pBuffer ); 

						FLTFreeCloneRequest( pBuffer );
					}
					else
					{
						// Free resources associated with packet
						RemoveEntryList ( &pHKPacket->m_Reserved.m_qLink );
						NdisReleaseSpinLock ( &pAdapter->m_PendingTransfertLock );
					}

					UF_FreePacketAndBuffers ( pHKPacket );
					
					DBGPRINT(("<== PtTransferDataComplete: Adapt %p\n", pAdapter));
					return;
				}
				pLink = pLink -> Flink;
			}
			
			NdisReleaseSpinLock ( &pAdapter->m_PendingTransfertLock );
			DBGPRINT(("<== PtTransferDataComplete: Adapt %p\n", pAdapter));
			return;
		}

		NdisReleaseSpinLock ( &pAdapter->m_PendingTransfertLock );

        NdisMTransferDataComplete(pAdapter->MiniportHandle,
                                  Packet,
                                  Status,
                                  BytesTransferred);
    }
	DBGPRINT(("<== PtTransferDataComplete: Adapt %p\n", pAdapter));
}

NDIS_STATUS
PtReceive(
    IN  NDIS_HANDLE         ProtocolBindingContext,
    IN  NDIS_HANDLE         MacReceiveContext,
    IN  PVOID               HeaderBuffer,
    IN  UINT                HeaderBufferSize,
    IN  PVOID               LookAheadBuffer,
    IN  UINT                LookAheadBufferSize,
    IN  UINT                PacketSize
    )
/*++

Routine Description:

    Handle receive data indicated up by the miniport below. We pass
    it along to the protocol above us.

    If the miniport below indicates packets, NDIS would more
    likely call us at our ReceivePacket handler. However we
    might be called here in certain situations even though
    the miniport below has indicated a receive packet, e.g.
    if the miniport had set packet status to NDIS_STATUS_RESOURCES.
        
Arguments:

    <see DDK ref page for ProtocolReceive>

Return Value:

    NDIS_STATUS_SUCCESS if we processed the receive successfully,
    NDIS_STATUS_XXX error code if we discarded it.

--*/
{
    PADAPT          pAdapter = (PADAPT)ProtocolBindingContext;
    UINT			nResult;
	UINT			nDataSize;
	UINT			nResidualSize;
	PNDISHK_PACKET	pHKPacket;
	PNDIS_BUFFER	pHKBuffer, ResidualBuffer;
	NDIS_STATUS		nStatus;
	DWORD			dwAdapterMode;

	DBGPRINT(("==> PtReceive: Adapt %p\n", pAdapter));

    if ((!pAdapter->MiniportHandle) || (pAdapter->MPDeviceState > NdisDeviceStateD0))
    {
        nStatus = NDIS_STATUS_FAILURE;
    }
    else do
    {
		// Check if it was loopback packet
		if ( pAdapter -> m_Medium == NdisMediumWan)
		{
		  ETH_COMPARE_NETWORK_ADDRESSES(
			&((PCHAR )HeaderBuffer)[ETH_LENGTH_OF_ADDRESS],
			pAdapter -> m_LocalAddress,
			&nResult
			);
		}
		else
		{
			  // 802.3
		  ETH_COMPARE_NETWORK_ADDRESSES(
			&((PCHAR )HeaderBuffer)[ETH_LENGTH_OF_ADDRESS],
			pAdapter -> m_CurrentAddress,
			&nResult
			);
		}
		
		dwAdapterMode = pAdapter->m_NdisrdExtension.m_AdapterMode;

		if( !nResult )
		{
			// Process loopback packet in depending of loopback flags

			/*if ((dwAdapterMode & MSTCP_FLAG_LOOPBACK_BLOCK) &&
				((!(*((PUCHAR)HeaderBuffer) & 0x01))
					||(pAdapter -> m_Medium == NdisMediumWan)) // Check if destination multicast/broadcast (don't check for WAN)
				)
			{
				DBGPRINT(("<== PtReceive: Adapt %p\n", pAdapter));
				return NDIS_STATUS_NOT_ACCEPTED;
			}

			if (!(dwAdapterMode & MSTCP_FLAG_LOOPBACK_FILTER))
			{
				NdisMEthIndicateReceive(
								pAdapter->MiniportHandle,
                                MacReceiveContext,
                                HeaderBuffer,
                                HeaderBufferSize,
                                LookAheadBuffer,
                                LookAheadBufferSize,
                                PacketSize
								);

				DBGPRINT(("<== PtReceive: Adapt %p\n", pAdapter));

				return NDIS_STATUS_SUCCESS;
			}*/

			NdisMEthIndicateReceive(
								pAdapter->MiniportHandle,
                                MacReceiveContext,
                                HeaderBuffer,
                                HeaderBufferSize,
                                LookAheadBuffer,
                                LookAheadBufferSize,
                                PacketSize
								);

			DBGPRINT(("<== PtReceive: Adapt %p\n", pAdapter));

			return NDIS_STATUS_SUCCESS;
		}

		nDataSize = HeaderBufferSize + PacketSize;
		nResidualSize = 0;
						
		// Sanity check
		if ( nDataSize > MAX_ETHER_SIZE )
		{
			// Block is more then we can handle, this can be on some Gigabit 
			// Ethernet adapters (MTU up to 9000 bytes ), but we will lower MTU in 
			// RequestHandler
			// Here simply pass to TCPIP, this code should never work
			NdisMEthIndicateReceive(
								pAdapter->MiniportHandle,
                                MacReceiveContext,
                                HeaderBuffer,
                                HeaderBufferSize,
                                LookAheadBuffer,
                                LookAheadBufferSize,
                                PacketSize
								);

			DBGPRINT(("<== PtReceive: Adapt %p\n", pAdapter));

			return NDIS_STATUS_SUCCESS;
		}

		if (LookAheadBufferSize < PacketSize)
		{
			// Some old adapters can't indicate the whole packet
			// so we need caling TransferDataHandler for them
			nResidualSize = PacketSize - LookAheadBufferSize;
		}

		// Allocate new packet from adapter entry associated pool
		NdisAllocatePacket ( 
			&nStatus, 
			(PNDIS_PACKET*)(&pHKPacket),
			pAdapter -> m_hPacketPool
			);

		if( nStatus != NDIS_STATUS_SUCCESS) 
		{
			NdisMEthIndicateReceive(
								pAdapter->MiniportHandle,
                                MacReceiveContext,
                                HeaderBuffer,
                                HeaderBufferSize,
                                LookAheadBuffer,
                                LookAheadBufferSize,
                                PacketSize
								);

			DBGPRINT(("<== PtReceive: Adapt %p\n", pAdapter));

			return NDIS_STATUS_SUCCESS;
		}

		// Allocate new buffer from adapter entry associated pool
		NdisAllocateBuffer ( 
			&nStatus,
			&pHKBuffer,
			pAdapter->m_hBufferPool,
			(PVOID) pHKPacket->m_Reserved.m_IBuffer,
			nDataSize - nResidualSize
			);

		if (nStatus != NDIS_STATUS_SUCCESS)
		{
			UF_FreePacketAndBuffers ( pHKPacket );
		
			NdisMEthIndicateReceive(
								pAdapter->MiniportHandle,
                                MacReceiveContext,
                                HeaderBuffer,
                                HeaderBufferSize,
                                LookAheadBuffer,
                                LookAheadBufferSize,
                                PacketSize
								);

			DBGPRINT(("<== PtReceive: Adapt %p\n", pAdapter));

			return NDIS_STATUS_SUCCESS;
		}

		// Chain Our Buffer To Our Packet
		NdisChainBufferAtFront((PNDIS_PACKET )pHKPacket, pHKBuffer );

		// Header buffer to our packet data
		NdisMoveMemory (
			pHKPacket->m_Reserved.m_IBuffer,
			HeaderBuffer,
			HeaderBufferSize
			);

		//Lookahead buffer to our packet data
		NdisMoveMemory (
			&pHKPacket->m_Reserved.m_IBuffer[ HeaderBufferSize],
			LookAheadBuffer,
			LookAheadBufferSize
			);

		pHKPacket->m_Reserved.m_nBytesTransferred = 0;
		pHKPacket -> m_Reserved.m_nTransferDataStatus = NDIS_STATUS_SUCCESS;
		
		// Insert into adapter associated list
		NdisAcquireSpinLock ( &pAdapter -> m_PendingTransfertLock );
		InsertHeadList (
			&pAdapter -> m_PendingTransfertData,
			&pHKPacket->m_Reserved.m_qLink
			);
		NdisReleaseSpinLock ( &pAdapter -> m_PendingTransfertLock );

		if ( nResidualSize )
		{
			// Not whole packet was indicated here
			// Allocate and chain temporary buffer to our packet
			NdisAllocateBuffer (
				&nStatus,
				&ResidualBuffer,
				pAdapter->m_hBufferPool,
				(PVOID) &pHKPacket->m_Reserved.m_IBuffer[HeaderBufferSize+LookAheadBufferSize],
				nResidualSize
				);

			if (nStatus == NDIS_STATUS_SUCCESS)
			{
				NdisChainBufferAtFront((PNDIS_PACKET)pHKPacket, ResidualBuffer);

				// Call NDIS to transfer data remain
				NdisTransferData (
					&pHKPacket -> m_Reserved.m_nTransferDataStatus,
					pAdapter ->BindingHandle,
					MacReceiveContext,
					LookAheadBufferSize,
					nResidualSize,
					(PNDIS_PACKET)pHKPacket,
					&pHKPacket->m_Reserved.m_nBytesTransferred
					);
			}
			else
			{
				// Free packet associated resources and call old ReceiveHandler
				NdisAcquireSpinLock ( &pAdapter->m_PendingTransfertLock );
				RemoveEntryList ( &pHKPacket->m_Reserved.m_qLink );
				NdisReleaseSpinLock ( &pAdapter->m_PendingTransfertLock );
				
				UF_FreePacketAndBuffers ( pHKPacket );
				
				NdisMEthIndicateReceive(
								pAdapter->MiniportHandle,
                                MacReceiveContext,
                                HeaderBuffer,
                                HeaderBufferSize,
                                LookAheadBuffer,
                                LookAheadBufferSize,
                                PacketSize
								);

				DBGPRINT(("<== PtReceive: Adapt %p\n", pAdapter));

				return NDIS_STATUS_SUCCESS;
			}
		}
		// Call PtTransferDataComplete
		if (pHKPacket -> m_Reserved.m_nTransferDataStatus != NDIS_STATUS_PENDING)
			PtTransferDataComplete(
				pAdapter,
				(PNDIS_PACKET) pHKPacket,
				pHKPacket -> m_Reserved.m_nTransferDataStatus,
				pHKPacket -> m_Reserved.m_nBytesTransferred
				);

		DBGPRINT(("<== PtReceive: Adapt %p\n", pAdapter));

		return NDIS_STATUS_SUCCESS;
 
    } while(FALSE);

	DBGPRINT(("<== PtReceive: Adapt %p\n", pAdapter));

    return nStatus;
}


VOID
PtReceiveComplete(
    IN NDIS_HANDLE        ProtocolBindingContext
    )
/*++

Routine Description:

    Called by the adapter below us when it is done indicating a batch of
    received packets.

Arguments:

    ProtocolBindingContext    Pointer to our adapter structure.

Return Value:

    None

--*/
{
    PADAPT        pAdapt =(PADAPT)ProtocolBindingContext;

	DBGPRINT(("==> PtReceiveComplete: Adapt %p\n", pAdapt));

    if (((pAdapt->MiniportHandle != NULL)
                && (pAdapt->MPDeviceState > NdisDeviceStateD0))
                && (pAdapt->IndicateRcvComplete))
    {
        switch (pAdapt->m_Medium)
        {
            case NdisMedium802_3:
            case NdisMediumWan:
                NdisMEthIndicateReceiveComplete(pAdapt->MiniportHandle);
                break;

            default:
                ASSERT(FALSE);
                break;
        }
    }

    pAdapt->IndicateRcvComplete = FALSE;

	DBGPRINT(("<== PtReceiveComplete: Adapt %p\n", pAdapt));
}

NDIS_STATUS
PtPNPHandler(
    IN NDIS_HANDLE        ProtocolBindingContext,
    IN PNET_PNP_EVENT     pNetPnPEvent
    )

/*++
Routine Description:

    This is called by NDIS to notify us of a PNP event related to a lower
    binding. Based on the event, this dispatches to other helper routines.

    NDIS 5.1: forward this event to the upper protocol(s) by calling
    NdisIMNotifyPnPEvent.

Arguments:

    ProtocolBindingContext - Pointer to our adapter structure. Can be NULL
                for "global" notifications

    pNetPnPEvent - Pointer to the PNP event to be processed.

Return Value:

    NDIS_STATUS code indicating status of event processing.

--*/
{
    PADAPT            pAdapt  =(PADAPT)ProtocolBindingContext;
    NDIS_STATUS       Status  = NDIS_STATUS_SUCCESS;

    DBGPRINT(("PtPnPHandler: Adapt %p, Event %d\n", pAdapt, pNetPnPEvent->NetEvent));

    switch (pNetPnPEvent->NetEvent)
    {
        case NetEventSetPower:
            Status = PtPnPNetEventSetPower(pAdapt, pNetPnPEvent);
            break;

         case NetEventReconfigure:
            Status = PtPnPNetEventReconfigure(pAdapt, pNetPnPEvent);
            break;

         default:
#ifdef NDIS51
            //
            // Pass on this notification to protocol(s) above, before
            // doing anything else with it.
            //
            if (pAdapt && pAdapt->MiniportHandle)
            {
                Status = NdisIMNotifyPnPEvent(pAdapt->MiniportHandle, pNetPnPEvent);
            }
#else
            Status = NDIS_STATUS_SUCCESS;

#endif // NDIS51

            break;
    }

    return Status;
}


NDIS_STATUS
PtPnPNetEventReconfigure(
    IN PADAPT            pAdapt,
    IN PNET_PNP_EVENT    pNetPnPEvent
    )
/*++
Routine Description:

    This routine is called from NDIS to notify our protocol edge of a
    reconfiguration of parameters for either a specific binding (pAdapt
    is not NULL), or global parameters if any (pAdapt is NULL).

Arguments:

    pAdapt - Pointer to our adapter structure.
    pNetPnPEvent - the reconfigure event

Return Value:

    NDIS_STATUS_SUCCESS

--*/
{
    NDIS_STATUS    ReconfigStatus = NDIS_STATUS_SUCCESS;
    NDIS_STATUS    ReturnStatus = NDIS_STATUS_SUCCESS;

    do
    {
        //
        // Is this is a global reconfiguration notification ?
        //
        if (pAdapt == NULL)
        {
            //
            // An important event that causes this notification to us is if
            // one of our upper-edge miniport instances was enabled after being
            // disabled earlier, e.g. from Device Manager in Win2000. Note that
            // NDIS calls this because we had set up an association between our
            // miniport and protocol entities by calling NdisIMAssociateMiniport.
            //
            // Since we would have torn down the lower binding for that miniport,
            // we need NDIS' assistance to re-bind to the lower miniport. The
            // call to NdisReEnumerateProtocolBindings does exactly that.
            //
            NdisReEnumerateProtocolBindings (ProtHandle);        
            break;
        }

#ifdef NDIS51
        //
        // Pass on this notification to protocol(s) above before doing anything
        // with it.
        //
        if (pAdapt->MiniportHandle)
        {
            ReturnStatus = NdisIMNotifyPnPEvent(pAdapt->MiniportHandle, pNetPnPEvent);
        }
#endif // NDIS51

        ReconfigStatus = NDIS_STATUS_SUCCESS;

    } while(FALSE);

    DBGPRINT(("<==PtPNPNetEventReconfigure: pAdapt %p\n", pAdapt));

#ifdef NDIS51
    //
    // Overwrite status with what upper-layer protocol(s) returned.
    //
    ReconfigStatus = ReturnStatus;
#endif

    return ReconfigStatus;
}


NDIS_STATUS
PtPnPNetEventSetPower(
    IN PADAPT            pAdapt,
    IN PNET_PNP_EVENT    pNetPnPEvent
    )
/*++
Routine Description:

    This is a notification to our protocol edge of the power state
    of the lower miniport. If it is going to a low-power state, we must
    wait here for all outstanding sends and requests to complete.

    NDIS 5.1:  Since we use packet stacking, it is not sufficient to
    check usage of our local send packet pool to detect whether or not
    all outstanding sends have completed. For this, use the new API
    NdisQueryPendingIOCount.

    NDIS 5.1: Use the 5.1 API NdisIMNotifyPnPEvent to pass on PnP
    notifications to upper protocol(s).

Arguments:

    pAdapt            -    Pointer to the adpater structure
    pNetPnPEvent    -    The Net Pnp Event. this contains the new device state

Return Value:

    NDIS_STATUS_SUCCESS or the status returned by upper-layer protocols.

--*/
{
    PNDIS_DEVICE_POWER_STATE       pDeviceState  =(PNDIS_DEVICE_POWER_STATE)(pNetPnPEvent->Buffer);
    NDIS_DEVICE_POWER_STATE        PrevDeviceState = pAdapt->PTDeviceState;  
    NDIS_STATUS                    Status;
    NDIS_STATUS                    ReturnStatus;
#ifdef NDIS51
    ULONG                          PendingIoCount = 0;
#endif // NDIS51

    ReturnStatus = NDIS_STATUS_SUCCESS;

	DBGPRINT(("==> PtPnPNetEventSetPower: Adapt %p\n", pAdapt));

    //
    // Set the Internal Device State, this blocks all new sends or receives
    //
    NdisAcquireSpinLock(&pAdapt->Lock);
    pAdapt->PTDeviceState = *pDeviceState;

    //
    // Check if the miniport below is going to a low power state.
    //
    if (pAdapt->PTDeviceState > NdisDeviceStateD0)
    {
        //
        // If the miniport below is going to standby, fail all incoming requests
        //
        if (PrevDeviceState == NdisDeviceStateD0)
        {
            pAdapt->StandingBy = TRUE;
        }

        NdisReleaseSpinLock(&pAdapt->Lock);

#ifdef NDIS51
        //
        // Notify upper layer protocol(s) first.
        //
        if (pAdapt->MiniportHandle != NULL)
        {
            ReturnStatus = NdisIMNotifyPnPEvent(pAdapt->MiniportHandle, pNetPnPEvent);
        }
#endif // NDIS51

        //
        // Wait for outstanding sends and requests to complete.
        //
        while (pAdapt->OutstandingSends != 0)
        {
            NdisMSleep(2);
        }

        while (pAdapt->OutstandingRequests == TRUE)
        {
            //
            // sleep till outstanding requests complete
            //
            NdisMSleep(2);
        }

        //
        // If the below miniport is going to low power state, complete the queued request
        //
        NdisAcquireSpinLock(&pAdapt->Lock);
        if (pAdapt->QueuedRequest)
        {
            pAdapt->QueuedRequest = FALSE;
            NdisReleaseSpinLock(&pAdapt->Lock);
            PtRequestComplete(pAdapt, &pAdapt->Request, NDIS_STATUS_FAILURE);
        }
        else
        {
            NdisReleaseSpinLock(&pAdapt->Lock);
        }
            
        ASSERT(pAdapt->OutstandingRequests == FALSE);
    }
    else
    {
        //
        // If the physical miniport is powering up (from Low power state to D0), 
        // clear the flag
        //
        if (PrevDeviceState > NdisDeviceStateD0)
        {
            pAdapt->StandingBy = FALSE;
        }
        //
        // The device below is being turned on. If we had a request
        // pending, send it down now.
        //
        if (pAdapt->QueuedRequest == TRUE)
        {
            pAdapt->QueuedRequest = FALSE;
        
            pAdapt->OutstandingRequests = TRUE;
            NdisReleaseSpinLock(&pAdapt->Lock);

            NdisRequest(&Status,
                        pAdapt->BindingHandle,
                        &pAdapt->Request);

            if (Status != NDIS_STATUS_PENDING)
            {
                PtRequestComplete(pAdapt,
                                  &pAdapt->Request,
                                  Status);
                
            }
        }
        else
        {
            NdisReleaseSpinLock(&pAdapt->Lock);
        }


#ifdef NDIS51
        //
        // Pass on this notification to protocol(s) above
        //
        if (pAdapt->MiniportHandle)
        {
            ReturnStatus = NdisIMNotifyPnPEvent(pAdapt->MiniportHandle, pNetPnPEvent);
        }
#endif // NDIS51

    }

	DBGPRINT(("<== PtPnPNetEventSetPower: Adapt %p\n", pAdapt));

    return ReturnStatus;
}


