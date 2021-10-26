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

#define FILTER_MAJOR_NDIS_VERSION   6
#define FILTER_MINOR_NDIS_VERSION   0

#define FILTER_FRIENDLY_NAME        L"AS NDIS Filter Driver"
#define FILTER_UNIQUE_NAME          L"{B6C12C2C-A121-4b19-9736-22283D4917B8}"
#define FILTER_SERVICE_NAME         L"ansfltr"

typedef struct _NDIS_OID_REQUEST *FILTER_REQUEST_CONTEXT,**PFILTER_REQUEST_CONTEXT;

#define FILTER_STATE_PAUSED 0
#define FILTER_STATE_PAUSING 1
#define FILTER_STATE_RUNNING 2

//
// Prototypes
//

NDIS_STATUS
FilterAttach(
	IN  NDIS_HANDLE NdisFilterHandle,
	IN  NDIS_HANDLE FilterDriverContext,
	IN  PNDIS_FILTER_ATTACH_PARAMETERS AttachParameters
	);

VOID
FilterDetach(
	IN  NDIS_HANDLE FilterInstaceContext
	);

NDIS_STATUS
FilterRestart(
	IN  NDIS_HANDLE FilterModuleContext,
	IN  PNDIS_FILTER_RESTART_PARAMETERS RestartParameters
	);

NDIS_STATUS
FilterPause(
	IN  NDIS_HANDLE FilterModuleContext,
	IN  PNDIS_FILTER_PAUSE_PARAMETERS PauseParameters
	);

VOID
FilterStatus(
	IN  NDIS_HANDLE FilterModuleContext,
	IN  PNDIS_STATUS_INDICATION StatusIndication
	);

VOID
FilterSendNetBufferLists(
	IN  NDIS_HANDLE FilterModuleContext,
	IN  PNET_BUFFER_LIST NetBufferLists,
	IN  NDIS_PORT_NUMBER PortNumber,
	IN  ULONG SendFlags
	);

VOID
FilterReturnNetBufferLists(
	IN  NDIS_HANDLE FilterModuleContext,
	IN  PNET_BUFFER_LIST NetBufferLists,
	IN  ULONG ReturnFlags
	);

VOID
FilterSendNetBufferListsComplete(
	IN  NDIS_HANDLE FilterModuleContext,
	IN  PNET_BUFFER_LIST NetBufferLists,
	IN  ULONG SendCompleteFlags
	);

VOID
FilterReceiveNetBufferLists(
	IN  NDIS_HANDLE FilterModuleContext,
	IN  PNET_BUFFER_LIST NetBufferLists,
	IN  NDIS_PORT_NUMBER PortNumber,
	IN  ULONG NumberOfNetBufferLists,
	IN  ULONG ReceiveFlags
	);

VOID
FilterCancelSendNetBufferLists(
	IN  NDIS_HANDLE FilterModuleContext,
	IN  PVOID CancelId
	);

VOID
FilterDevicePnPEventNotify(
	IN  NDIS_HANDLE             FilterModuleContext,
	IN  PNET_DEVICE_PNP_EVENT   NetDevicePnPEvent
	);

NDIS_STATUS
FilterNetPnPEvent(
	IN  NDIS_HANDLE             FilterModuleContext,
	IN  PNET_PNP_EVENT_NOTIFICATION NetPnPEventNotification
	);

NTSTATUS 
SendPacket( 	
	IN PFLT_PACKET pPacket
	);

PNET_BUFFER_LIST CreateNetBufferList( 
	PFLT_PACKET pPacket,
	PNET_BUFFER_LIST pParentNBL
	);

//
// Global variables
// 

NDIS_SPIN_LOCK g_AdapterLock;
LIST_ENTRY g_AdapterList;
NDIS_HANDLE g_hDriver; // NDIS handle for filter driver


//
// Functions
//

NTSTATUS
RegisterFilterDriver( PDRIVER_OBJECT pDriverObject )
{
	NDIS_STATUS status;	
	NDIS_FILTER_DRIVER_CHARACTERISTICS FChars = {0};

	dprintf( "===> RegisterFilterDriver. pDriverObject = %p.\n", pDriverObject );

	InitializeListHead( &g_AdapterList );
	NdisAllocateSpinLock( &g_AdapterLock );

	FChars.Header.Type = NDIS_OBJECT_TYPE_FILTER_DRIVER_CHARACTERISTICS;
	FChars.Header.Size = sizeof(NDIS_FILTER_DRIVER_CHARACTERISTICS);
	FChars.Header.Revision = NDIS_FILTER_CHARACTERISTICS_REVISION_1;
	FChars.MajorNdisVersion = FILTER_MAJOR_NDIS_VERSION;
	FChars.MinorNdisVersion = FILTER_MINOR_NDIS_VERSION;
	FChars.MajorDriverVersion = 1;
	FChars.MinorDriverVersion = 0;
	FChars.Flags = 0;

	RtlInitUnicodeString(&FChars.ServiceName, FILTER_SERVICE_NAME);
	RtlInitUnicodeString(&FChars.FriendlyName, FILTER_FRIENDLY_NAME);
	RtlInitUnicodeString(&FChars.UniqueName, FILTER_UNIQUE_NAME);

	//
	// for the time being, there is no additional options to register
	// but let's have this handler anyway
	//
	//FChars.SetOptionsHandler = FilterRegisterOptions;        
	FChars.AttachHandler = FilterAttach;
	FChars.DetachHandler = FilterDetach;
	FChars.RestartHandler = FilterRestart;
	FChars.PauseHandler = FilterPause;
	//FChars.SetFilterModuleOptionsHandler = FilterSetModuleOptions;           
	//FChars.OidRequestHandler = FilterOidRequest;
	//FChars.OidRequestCompleteHandler = FilterOidRequestComplete;
	//FChars.CancelOidRequestHandler = FilterCancelOidRequest;

	FChars.SendNetBufferListsHandler = FilterSendNetBufferLists;  
	FChars.SendNetBufferListsCompleteHandler = FilterSendNetBufferListsComplete;
	FChars.CancelSendNetBufferListsHandler = FilterCancelSendNetBufferLists;
	FChars.ReturnNetBufferListsHandler = FilterReturnNetBufferLists;	
	FChars.ReceiveNetBufferListsHandler = FilterReceiveNetBufferLists;
	FChars.DevicePnPEventNotifyHandler = FilterDevicePnPEventNotify;
	FChars.NetPnPEventHandler = FilterNetPnPEvent;
	FChars.StatusHandler = FilterStatus;

	g_hDriver = NULL;

	status = NdisFRegisterFilterDriver(
		pDriverObject,
		(NDIS_HANDLE)pDriverObject,
		&FChars, 
		&g_hDriver);

	if( status != NDIS_STATUS_SUCCESS )
	{
		dprintf( "NdisFRegisterFilterDriver failed.\n");
	}

	return status;
}

VOID 
DeregisterFilterDriver()
{
	dprintf( "===> DeregisterFilterDriver.\n");

	NdisFDeregisterFilterDriver( g_hDriver );

	NdisFreeSpinLock( &g_AdapterLock );
}

NDIS_STATUS
FilterRegisterOptions(
        IN NDIS_HANDLE  NdisFilterDriverHandle,
        IN NDIS_HANDLE  FilterDriverContext
        )
{
	dprintf( "===> FilterRegisterOptions. NdisFilterDriverHandle = %p. FilterDriverContext = %p\n", NdisFilterDriverHandle, FilterDriverContext);
	return NDIS_STATUS_SUCCESS;
}
 
/**
*	@brief Create filter's context, and allocate NetBufferLists and NetBuffers pools and any
*	       other resources, read configuration if needed.
*	@param NdisFilterHandle - Specify a handle identifying this instance of the filter. 
*	@param FilterDriverContext - Filter driver context passed to NdisFRegisterFilterDriver.
*	@param AttachParameters - attach parameters
*	@return NDIS_STATUS
*/
NDIS_STATUS
FilterAttach(
    IN  NDIS_HANDLE                     NdisFilterHandle,
    IN  NDIS_HANDLE                     FilterDriverContext,
    IN  PNDIS_FILTER_ATTACH_PARAMETERS  AttachParameters
    )
{	
	PFLT_MODULE_CONTEXT pFilter;
	NDIS_STATUS status;
	NDIS_FILTER_ATTRIBUTES FilterAttributes;
	ULONG Size;
	PLIST_ENTRY pListEntry;
	NET_BUFFER_LIST_POOL_PARAMETERS PoolParameters;

	dprintf( "===> FilterAttach. NdisFilterHandle = %p\n", NdisFilterHandle);

	if ( AttachParameters->MiniportMediaType != NdisMedium802_3 && 
		AttachParameters->MiniportMediaType != NdisMediumWan )
	{
		return NDIS_STATUS_INVALID_PARAMETER;
	}

	Size = sizeof(FLT_MODULE_CONTEXT) + 
		AttachParameters->FilterModuleGuidName->Length + 
		AttachParameters->BaseMiniportInstanceName->Length + 
		AttachParameters->BaseMiniportName->Length;

	pFilter = (PFLT_MODULE_CONTEXT)NdisAllocateMemoryWithTagPriority(NdisFilterHandle, Size, '2lfc', NormalPoolPriority);
	if (pFilter == NULL)
	{
		dprintf( "Failed to allocate context structure.\n");
		return NDIS_STATUS_RESOURCES;
	}

	NdisZeroMemory(pFilter, sizeof(FLT_MODULE_CONTEXT));

	pFilter->m_FilterModuleName.Length = pFilter->m_FilterModuleName.MaximumLength = AttachParameters->FilterModuleGuidName->Length;
	pFilter->m_FilterModuleName.Buffer = (PWSTR)((PUCHAR)pFilter + sizeof(FLT_MODULE_CONTEXT));
	NdisMoveMemory(pFilter->m_FilterModuleName.Buffer, 
		AttachParameters->FilterModuleGuidName->Buffer,
		pFilter->m_FilterModuleName.Length);

	pFilter->m_MiniportFriendlyName.Length = pFilter->m_MiniportFriendlyName.MaximumLength = AttachParameters->BaseMiniportInstanceName->Length;
	pFilter->m_MiniportFriendlyName.Buffer = (PWSTR)((PUCHAR)pFilter->m_FilterModuleName.Buffer + pFilter->m_FilterModuleName.Length);
	NdisMoveMemory(pFilter->m_MiniportFriendlyName.Buffer, 
		AttachParameters->BaseMiniportInstanceName->Buffer,
		pFilter->m_MiniportFriendlyName.Length);

	pFilter->m_MiniportName.Length = pFilter->m_MiniportName.MaximumLength = AttachParameters->BaseMiniportName->Length;
	pFilter->m_MiniportName.Buffer = (PWSTR)((PUCHAR)pFilter->m_MiniportFriendlyName.Buffer + pFilter->m_MiniportFriendlyName.Length);
	NdisMoveMemory(pFilter->m_MiniportName.Buffer, 
		AttachParameters->BaseMiniportName->Buffer,
		pFilter->m_MiniportName.Length);

	pFilter->m_FilterHandle = NdisFilterHandle;
	pFilter->m_AttachParameters = *AttachParameters;

	pFilter->m_FilterState = FILTER_STATE_PAUSED;

	NdisZeroMemory(&FilterAttributes, sizeof(NDIS_FILTER_ATTRIBUTES));
	FilterAttributes.Header.Revision = NDIS_FILTER_ATTRIBUTES_REVISION_1;
	FilterAttributes.Header.Size = sizeof(NDIS_FILTER_ATTRIBUTES);
	FilterAttributes.Header.Type = NDIS_OBJECT_TYPE_FILTER_ATTRIBUTES;
	FilterAttributes.Flags = 0;

	status = NdisFSetAttributes(
		NdisFilterHandle, 
		pFilter,
		&FilterAttributes);
	if (status != NDIS_STATUS_SUCCESS)
	{
		dprintf( "Failed to set attributes.\n");
		NdisFreeMemory(pFilter, 0, 0);
		return status;
	}

	NdisZeroMemory(&PoolParameters, sizeof(NET_BUFFER_LIST_POOL_PARAMETERS));

	PoolParameters.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
	PoolParameters.Header.Revision = NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
	PoolParameters.Header.Size = sizeof(PoolParameters);
	PoolParameters.ProtocolId = 0;
	PoolParameters.ContextSize = 0;
	PoolParameters.fAllocateNetBuffer = TRUE;
	PoolParameters.PoolTag = '3lft';

	pFilter->NetBufferListPool = NdisAllocateNetBufferListPool(
		NdisFilterHandle,
		&PoolParameters);

	if (pFilter->NetBufferListPool == NULL)
	{
		NdisFreeMemory(pFilter, 0, 0);
		return NDIS_STATUS_RESOURCES;
	}

	NdisAcquireSpinLock( &g_AdapterLock );
	InsertHeadList( &g_AdapterList, &pFilter->m_qLink );
	NdisReleaseSpinLock( &g_AdapterLock );

	OpenAdapter( pFilter, &pFilter->m_MiniportName );

	dprintf( "<=== FilterAttach. status %x\n", status);
	return status;
}

/**
*	@brief Filter pause routine.
*	       Complete all the outstanding sends and queued sends,
*	       Waiting for all the outstanding recvs to be returned 
*	       and return all the queued receives
*	@param FilterModuleContext - pointer to the filter context stucture. 
*	@return STATUS_SUCCESS if filter pauses successful, STATUS_PENDING if not.
*/
NDIS_STATUS
FilterPause(
        IN  NDIS_HANDLE                     FilterModuleContext,
        IN  PNDIS_FILTER_PAUSE_PARAMETERS   PauseParameters
        )
{
	PFLT_MODULE_CONTEXT pFilter = (PFLT_MODULE_CONTEXT)FilterModuleContext;

	dprintf( "===> FilterPause. FilterModuleContext = %p\n", FilterModuleContext);

	InterlockedExchange( &pFilter->m_FilterState, FILTER_STATE_PAUSED );

	return NDIS_STATUS_SUCCESS;
}

/**
*	@brief Restart the filter
*	@param FilterModuleContext - pointer to the filter context stucture. 
*	@return NDIS_STATUS_SUCCESS: if filter restarts successfully
*/
NDIS_STATUS
FilterRestart(
    IN  NDIS_HANDLE                     FilterModuleContext,
    IN  PNDIS_FILTER_RESTART_PARAMETERS RestartParameters
    )
{
	PFLT_MODULE_CONTEXT pFilter = (PFLT_MODULE_CONTEXT)FilterModuleContext;

	dprintf( "===> FilterRestart. FilterModuleContext = %p\n", FilterModuleContext);

	InterlockedExchange( &pFilter->m_FilterState, FILTER_STATE_RUNNING );

	return NDIS_STATUS_SUCCESS;
}

/**
*	@brief This is a required function that will deallocate all the resources allocated during
*	       FitlerAttach. NDIS calls FilterAttach to remove a filter instance from a fitler stack.
*	@param FilterModuleContext - pointer to the filter context stucture. 
*/
VOID
FilterDetach(
        IN  NDIS_HANDLE     FilterModuleContext
        )
{
	PLIST_ENTRY pListEntry;
	PFLT_MODULE_CONTEXT pFilter = (PFLT_MODULE_CONTEXT)FilterModuleContext;

	dprintf( "===> FilterDetach. FilterModuleContext = %p\n", FilterModuleContext);	

	CloseAdapter( pFilter );

	if (pFilter->NetBufferListPool != NULL)
	{
		NdisFreeNetBufferListPool(pFilter->NetBufferListPool);
		pFilter->NetBufferListPool = NULL;
	}

	NdisAcquireSpinLock( &g_AdapterLock );
	RemoveEntryList( &pFilter->m_qLink );
	NdisReleaseSpinLock( &g_AdapterLock );

	NdisFreeMemory( pFilter, 0, 0 );
}

VOID
FilterSendNetBufferLists(
						 IN  NDIS_HANDLE         FilterModuleContext,
						 IN  PNET_BUFFER_LIST    NetBufferLists,
						 IN  NDIS_PORT_NUMBER    PortNumber,
						 IN  ULONG               SendFlags
						 )
{
	PFLT_MODULE_CONTEXT pFilter = (PFLT_MODULE_CONTEXT)FilterModuleContext;
	PNET_BUFFER_LIST pNetBufferList;

	dprintf( "===> SendNetBufferList. NetBufferLists = %p, PortNumber = %X, SendFlags = %X.\n", NetBufferLists, PortNumber, SendFlags);

	if( InterlockedExchange(&pFilter->m_FilterState, pFilter->m_FilterState) == FILTER_STATE_RUNNING )
	{

		if( IsInstancesListEmpty() )
		{
			NdisFSendNetBufferLists(pFilter->m_FilterHandle, NetBufferLists, PortNumber, SendFlags);
			return;
		}

		pNetBufferList = NetBufferLists;
		while (pNetBufferList)
		{
			PNET_BUFFER pNetBuffer = NET_BUFFER_LIST_FIRST_NB(pNetBufferList);

			while( pNetBuffer )
			{						
				PFLT_PACKET pPacket = NdisAllocateMemoryWithTagPriority( pFilter->m_FilterHandle, sizeof(FLT_PACKET)+NET_BUFFER_DATA_LENGTH(pNetBuffer), '3lfc', NormalPoolPriority );

				if( pPacket )
				{
					PVOID pBuffer;					
					pPacket->m_pBuffer = (PVOID)((ULONG_PTR)pPacket+sizeof(FLT_PACKET));
					pPacket->m_BufferSize = NET_BUFFER_DATA_LENGTH(pNetBuffer);		

					pPacket->m_Direction = PHYSICAL_DIRECTION_OUT;
					pPacket->m_pAdapter = pFilter;
					pPacket->m_NdisPortNumber = PortNumber;
					pPacket->m_Flags = (SendFlags & NDIS_SEND_FLAGS_CHECK_FOR_LOOPBACK);	
					pBuffer = NdisGetDataBuffer( pNetBuffer, pPacket->m_BufferSize, pPacket->m_pBuffer, 1, 0 );
					if( pBuffer != pPacket->m_pBuffer )
						RtlCopyMemory( pPacket->m_pBuffer, pBuffer, pPacket->m_BufferSize );
					pPacket->m_pNBL = CreateNetBufferList( pPacket, pNetBufferList );
					if( pPacket->m_pNBL )
					{
						pPacket->m_References = 1;

						NdisAcquireSpinLock( &g_PacketLock );
						InsertTailList( &g_PacketsList, &pPacket->m_qLink );
						NdisReleaseSpinLock( &g_PacketLock );

						FLTInjectRequest( NULL, pPacket );

						KdPrint(("FilterSendNetBufferList %d %p (FilterReceiveNetBufferLists)", pPacket->m_References, pPacket ));
						FLTFreeCloneRequest( pPacket );
					}
					else
					{
						NdisFreeMemory( pPacket, 0, 0 );
					}
				}

				pNetBuffer = NET_BUFFER_NEXT_NB( pNetBuffer );
			}

			pNetBufferList = NET_BUFFER_LIST_NEXT_NBL(pNetBufferList);
		}
	}

	NdisFSendNetBufferListsComplete( pFilter->m_FilterHandle, NetBufferLists,  
		NDIS_TEST_SEND_AT_DISPATCH_LEVEL(SendFlags)?NDIS_SEND_COMPLETE_FLAGS_DISPATCH_LEVEL : 0);
}

/**
*	@brief FilterCancelSendNetBufferLists cancels any NET_BUFFER_LISTs pended in the filter
*	@param FilterModuleContext - pointer to the filter context stucture. 
*	@param CancelId
*/
VOID
FilterCancelSendNetBufferLists(
    IN  NDIS_HANDLE             FilterModuleContext,
    IN  PVOID                   CancelId
    )
{
	PFLT_MODULE_CONTEXT  pFilter = (PFLT_MODULE_CONTEXT)FilterModuleContext;

	dprintf( "===> FilterCancelSendNetBufferLists. CancelId = %p.\n", CancelId );

	NdisFCancelSendNetBufferLists(pFilter->m_FilterHandle, CancelId);
}


VOID
FilterSendNetBufferListsComplete(
        IN  NDIS_HANDLE         FilterModuleContext,
        IN  PNET_BUFFER_LIST    NetBufferLists,
        IN  ULONG               SendCompleteFlags
		)
{
	PFLT_MODULE_CONTEXT pFilter = (PFLT_MODULE_CONTEXT)FilterModuleContext;

	dprintf( "===> FilterSendNetBufferListsComplete. NetBufferList = %p, SendCompleteFlags = %X.\n", NetBufferLists, SendCompleteFlags);

	while( NetBufferLists )
	{
		PNET_BUFFER_LIST CurrNbl = NetBufferLists;
		NetBufferLists = NET_BUFFER_LIST_NEXT_NBL(NetBufferLists);

		if( CurrNbl->SourceHandle == pFilter->m_FilterHandle ) // Our NBL
		{		
			if ( NdisGetPoolFromNetBufferList(CurrNbl) == pFilter->NetBufferListPool )
			{
				FLTFreeCloneRequest( (PFLT_PACKET)(CurrNbl->ProtocolReserved[3]) );
			}
		}
		else
		{
			NET_BUFFER_LIST_NEXT_NBL(CurrNbl) = NULL;
			NdisFSendNetBufferListsComplete( pFilter->m_FilterHandle, CurrNbl, SendCompleteFlags );
		}
	}
}

/**
*	@brief FilterReceiveNetBufferLists
*	@param FilterModuleContext - pointer to the filter context stucture. 
*	@param NetBufferLists - Pointer to a List of NetBufferLists.
*	@param PortNumber - Port on which the Receive is indicated
*	@param NumberOfNetBufferLists
*	@param ReceiveFlags - Flags associated with the Receive such as whether the filter
           can pend the receive
*/
VOID
FilterReceiveNetBufferLists(
	IN  NDIS_HANDLE         FilterModuleContext,
	IN  PNET_BUFFER_LIST    NetBufferLists,
	IN  NDIS_PORT_NUMBER    PortNumber,
	IN  ULONG               NumberOfNetBufferLists,
	IN  ULONG               ReceiveFlags
	 )
{
	PFLT_MODULE_CONTEXT pFilter = (PFLT_MODULE_CONTEXT)FilterModuleContext;
	PNET_BUFFER_LIST pNetBufferList;

	dprintf( "===> SendNetBufferList. NetBufferLists = %p, PortNumber = %X, ReceiveFlags = %X.\n", NetBufferLists, PortNumber, ReceiveFlags);

	if( InterlockedExchange(&pFilter->m_FilterState, pFilter->m_FilterState) == FILTER_STATE_RUNNING )
	{

		if( IsInstancesListEmpty() )
		{
			NdisFIndicateReceiveNetBufferLists(
				pFilter->m_FilterHandle,
				NetBufferLists,
				PortNumber, 
				NumberOfNetBufferLists,
				ReceiveFlags);
			return;
		}

		pNetBufferList = NetBufferLists;
		while (pNetBufferList)
		{
			PNET_BUFFER pNetBuffer = NET_BUFFER_LIST_FIRST_NB(pNetBufferList);

			while( pNetBuffer )
			{
				PFLT_PACKET pPacket = NdisAllocateMemoryWithTagPriority( pFilter->m_FilterHandle, sizeof(FLT_PACKET)+NET_BUFFER_DATA_LENGTH(pNetBuffer), '4lfc', NormalPoolPriority );

				if( pPacket )
				{
					PVOID pBuffer;
					pPacket->m_pBuffer = (PVOID)((ULONG_PTR)pPacket+sizeof(FLT_PACKET));
					pPacket->m_BufferSize = NET_BUFFER_DATA_LENGTH(pNetBuffer);		

					pPacket->m_Direction = PHYSICAL_DIRECTION_IN;
					pPacket->m_pAdapter = pFilter;
					pPacket->m_NdisPortNumber = PortNumber;
					pPacket->m_Flags = (ReceiveFlags & (NDIS_RECEIVE_FLAGS_SINGLE_ETHER_TYPE|NDIS_RECEIVE_FLAGS_SINGLE_VLAN|NDIS_RECEIVE_FLAGS_PERFECT_FILTERED));	
					pBuffer = NdisGetDataBuffer( pNetBuffer, pPacket->m_BufferSize, pPacket->m_pBuffer, 1, 0 );
					if( pBuffer != pPacket->m_pBuffer )
						RtlCopyMemory( pPacket->m_pBuffer, pBuffer, pPacket->m_BufferSize );
					pPacket->m_pNBL = CreateNetBufferList( pPacket, pNetBufferList );
					if( pPacket->m_pNBL )
					{
						pPacket->m_References = 1;

						NdisAcquireSpinLock( &g_PacketLock );
						InsertTailList( &g_PacketsList, &pPacket->m_qLink );
						NdisReleaseSpinLock( &g_PacketLock );

						FLTInjectRequest( NULL, pPacket );

						KdPrint(("FLTFreeCloneRequest %d %p (FilterReceiveNetBufferLists)", pPacket->m_References, pPacket ));
						FLTFreeCloneRequest( pPacket );
					}
					else
					{
						NdisFreeMemory( pPacket, 0, 0 );
					}
				}

				pNetBuffer = NET_BUFFER_NEXT_NB( pNetBuffer );
			}

			pNetBufferList = NET_BUFFER_LIST_NEXT_NBL(pNetBufferList);
		}

	}

	if( ! NDIS_TEST_RECEIVE_FLAG( ReceiveFlags, NDIS_RECEIVE_FLAGS_RESOURCES ) )
		NdisFReturnNetBufferLists( pFilter->m_FilterHandle, NetBufferLists, 
		NDIS_TEST_RECEIVE_AT_DISPATCH_LEVEL(ReceiveFlags) ? NDIS_RETURN_FLAGS_DISPATCH_LEVEL : 0); 
}

/**
*	@brief FilerReturnNetBufferLists handler
*	@param FilterModuleContext - pointer to the filter context stucture. 
*	@param NetBufferLists - Pointer to a List of NetBufferLists.
*   @param ReturnFlags: Flags specifying if the caller is at DISPATCH_LEVEL
*/
VOID
FilterReturnNetBufferLists(
        IN  NDIS_HANDLE         FilterModuleContext,
        IN  PNET_BUFFER_LIST    NetBufferLists,
        IN  ULONG               ReturnFlags
        )
{
	PFLT_MODULE_CONTEXT pFilter = (PFLT_MODULE_CONTEXT)FilterModuleContext;

	dprintf( "===> FilterReturnNetBufferLists. NetBufferList = %p, ReturnFlags = %X.\n", NetBufferLists, ReturnFlags);

	while( NetBufferLists )
	{
		PNET_BUFFER_LIST CurrNbl = NetBufferLists;
		NetBufferLists = NET_BUFFER_LIST_NEXT_NBL(NetBufferLists);

		if( CurrNbl->SourceHandle == pFilter->m_FilterHandle ) // Our NBL
		{		
			if ( NdisGetPoolFromNetBufferList(CurrNbl) == pFilter->NetBufferListPool )
			{
				FLTFreeCloneRequest( (PFLT_PACKET)(CurrNbl->ProtocolReserved[3]) );
			}
		}
		else
		{
			NET_BUFFER_LIST_NEXT_NBL(CurrNbl) = NULL;
			NdisFReturnNetBufferLists( pFilter->m_FilterHandle, CurrNbl, ReturnFlags ); 
		}
	}
}

/**
*	@brief Indicate Status Handle
*	@param FilterModuleContext - pointer to the filter context stucture. 
*	@param StatusIndication
*/
VOID
FilterStatus(
        IN  NDIS_HANDLE             FilterModuleContext,
        IN  PNDIS_STATUS_INDICATION StatusIndication
        )
{
	PFLT_MODULE_CONTEXT pFilter = (PFLT_MODULE_CONTEXT)FilterModuleContext;

	dprintf( "===> FilterStatus. StatusCode = %X.\n", StatusIndication->StatusCode);

	NdisFIndicateStatus(pFilter->m_FilterHandle, StatusIndication);
}

VOID
FilterDevicePnPEventNotify(
        IN  NDIS_HANDLE             FilterModuleContext,
        IN  PNET_DEVICE_PNP_EVENT   NetDevicePnPEvent
        )
{
    PFLT_MODULE_CONTEXT pFilter = (PFLT_MODULE_CONTEXT)FilterModuleContext;

    NdisFDevicePnPEventNotify(pFilter->m_FilterHandle, NetDevicePnPEvent);
}


NDIS_STATUS
FilterNetPnPEvent(
        IN  NDIS_HANDLE             FilterModuleContext,
        IN  PNET_PNP_EVENT_NOTIFICATION NetPnPEventNotification
        )
{
    PFLT_MODULE_CONTEXT pFilter = (PFLT_MODULE_CONTEXT)FilterModuleContext;
    
    return NdisFNetPnPEvent(pFilter->m_FilterHandle, NetPnPEventNotification);
}

PNET_BUFFER_LIST CreateNetBufferList( 
	PFLT_PACKET pPacket,
	PNET_BUFFER_LIST pParentNBL
	)
{
	PNET_BUFFER_LIST pNetBufferList;
	PMDL pMdl;

	pMdl = NdisAllocateMdl( pPacket->m_pAdapter->m_FilterHandle, pPacket->m_pBuffer, pPacket->m_BufferSize );
	if( pMdl == NULL )
	{
		return NULL;
	}

	pNetBufferList = NdisAllocateNetBufferAndNetBufferList( pPacket->m_pAdapter->NetBufferListPool, 0, 0, pMdl, 0, pPacket->m_BufferSize );
	if( pNetBufferList == NULL )
	{
		NdisFreeMdl( pMdl );
		return NULL;
	}

	pNetBufferList->ProtocolReserved[3] = (PNET_BUFFER_LIST_CONTEXT)pPacket;
	pNetBufferList->SourceHandle = pPacket->m_pAdapter->m_FilterHandle;

	if( pPacket->m_Direction == PHYSICAL_DIRECTION_IN )
	{
		NdisCopyReceiveNetBufferListInfo( pNetBufferList, pParentNBL );
	}
	else
	{
		NdisCopySendNetBufferListInfo( pNetBufferList, pParentNBL );
	}

	return pNetBufferList;
}

NTSTATUS 
SendPacket( 	
	IN PFLT_PACKET pPacket
	)
{
	NDIS_HANDLE FilterHandle;

	NdisAcquireSpinLock( &g_PacketLock );
	FilterHandle = pPacket->m_pAdapter ? pPacket->m_pAdapter->m_FilterHandle : NULL;
	NdisReleaseSpinLock( &g_PacketLock );

	if( FilterHandle )
	{
		KIRQL Irql;

		FLTCloneRequest( pPacket, &pPacket );

		KeRaiseIrql(DISPATCH_LEVEL, &Irql);

		if( pPacket->m_Direction == PHYSICAL_DIRECTION_IN )
		{
			NdisFIndicateReceiveNetBufferLists(
				FilterHandle,
				pPacket->m_pNBL,
				pPacket->m_NdisPortNumber, 
				1,
				pPacket->m_Flags );
		}
		else
		{
			NdisFSendNetBufferLists( 
				FilterHandle,
				pPacket->m_pNBL,
				pPacket->m_NdisPortNumber,
				pPacket->m_Flags | NDIS_SEND_FLAGS_DISPATCH_LEVEL );
		}

		KeLowerIrql(Irql);

		return STATUS_SUCCESS;
	}
	else
	{
		return STATUS_INVALID_PARAMETER;
	}
}

VOID FreeNdisData( PFLT_PACKET pPacket )
{
	NdisFreeMdl(NET_BUFFER_CURRENT_MDL(NET_BUFFER_LIST_FIRST_NB(pPacket->m_pNBL))); 
	NdisFreeNetBufferList( pPacket->m_pNBL ); 
	NdisFreeMemory( pPacket, 0, 0 );
}

//VOID MF_SetAdapterIPByTCPDeviceName ( 
//	PUNICODE_STRING TCPDeviceName,
//	DWORD dwAddr
//	)
//{
//	PLIST_ENTRY pListEntry;
//
//	NdisAcquireSpinLock( &g_AdapterLock );
//
//	pListEntry = g_AdapterList.Flink;
//	while( pListEntry != &g_AdapterList )
//	{
//		PFLT_MODULE_CONTEXT pFltModuleContext = CONTAINING_RECORD( pListEntry, FLT_MODULE_CONTEXT, m_qLink );
//		pListEntry = pListEntry->Flink;
//
//		dprintf("PNP_SetAdapterIPByTCPDeviceName %d.%d.%d.%d",
//			PRINT_IP_ADDR(pFltModuleContext->m_LastIndicatedIP) );
//
//		if( ntCompareDeviceName(TCPDeviceName, &pFltModuleContext->m_MiniportName) )
//		{
//			pFltModuleContext->m_LastIndicatedIP = dwAddr;
//			NdisReleaseSpinLock( &g_AdapterLock );
//			return;
//		}
//	}
//
//	NdisReleaseSpinLock( &g_AdapterLock );
//}