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

#include "InitGuid.h" // For definitions in DEFINE_GUID macros

#include <fwpsk.h>

typedef struct _WFP_INFO
{
	UINT64							m_EndpointHandle;
	FWPS_TRANSPORT_SEND_PARAMS0		m_SendArgs;
    COMPARTMENT_ID					m_CompartmentId;
    PNET_BUFFER_LIST				m_pNetBufferList;	

} WFP_INFO, *PWFP_INFO;


//
// Global variables
//

static PDEVICE_OBJECT g_pDeviceObject;
static HANDLE g_hChanges, g_hInjectionTransportV4;
static UINT32 g_AleFlowEstablishedV4Id;

//
// GUIDs
//
// {60312478-C13D-4a6e-929A-E0E8E78DC47C}
DEFINE_GUID(AS_PROVIDER, 
			0x60312478, 0xc13d, 0x4a6e, 0x92, 0x9a, 0xe0, 0xe8, 0xe7, 0x8d, 0xc4, 0x7c);

// {E306ACD6-75E4-4a32-9C51-21A38EB6136A}
DEFINE_GUID(AS_SUBLAYER, 
			0xe306acd6, 0x75e4, 0x4a32, 0x9c, 0x51, 0x21, 0xa3, 0x8e, 0xb6, 0x13, 0x6a);

// {BA499FCD-3C08-42da-8619-95AF55EB53A6}
DEFINE_GUID(CALLOUT_ALE_FLOW_ESTABLISHED_V4, 
			0xba499fcd, 0x3c08, 0x42da, 0x86, 0x19, 0x95, 0xaf, 0x55, 0xeb, 0x53, 0xa6);

// {294AC2FD-297F-4638-85D8-9E9D5B733577}
DEFINE_GUID(FILTER_ALE_FLOW_ESTABLISHED_V4, 
			0x294ac2fd, 0x297f, 0x4638, 0x85, 0xd8, 0x9e, 0x9d, 0x5b, 0x73, 0x35, 0x77);

// {3777BF45-9E86-420c-88FD-840BDC685C9B}
DEFINE_GUID(CALLOUT_ALE_AUTH_CONNECT_V4, 
			0x3777bf45, 0x9e86, 0x420c, 0x88, 0xfd, 0x84, 0xb, 0xdc, 0x68, 0x5c, 0x9b);

// {8689E097-DE0B-4e03-91DE-6675F4936343}
DEFINE_GUID(FILTER_ALE_AUTH_CONNECT_V4, 
			0x8689e097, 0xde0b, 0x4e03, 0x91, 0xde, 0x66, 0x75, 0xf4, 0x93, 0x63, 0x43);

// {68568FBA-0960-4849-9C3C-6D1B419179E5}
DEFINE_GUID(CALLOUT_ALE_RESOURCE_ASSIGNMENT_V4, 
			0x68568fba, 0x960, 0x4849, 0x9c, 0x3c, 0x6d, 0x1b, 0x41, 0x91, 0x79, 0xe5);

// {C2DB61AB-9D9D-4cdd-B974-154CE3B5E85B}
DEFINE_GUID(FILTER_ALE_RESOURCE_ASSIGNMENT_V4, 
			0xc2db61ab, 0x9d9d, 0x4cdd, 0xb9, 0x74, 0x15, 0x4c, 0xe3, 0xb5, 0xe8, 0x5b);

// {E8067587-673C-4977-888F-70B4E9D8999F}
DEFINE_GUID(CALLOUT_OUTBOUND_TRANSPORT_V4, 
			0xe8067587, 0x673c, 0x4977, 0x88, 0x8f, 0x70, 0xb4, 0xe9, 0xd8, 0x99, 0x9f);

// {2CEDD918-E7F7-4a62-88A1-EC87879FEF84}
DEFINE_GUID(FILTER_OUTBOUND_TRANSPORT_V4, 
			0x2cedd918, 0xe7f7, 0x4a62, 0x88, 0xa1, 0xec, 0x87, 0x87, 0x9f, 0xef, 0x84);


//
// Functions declarations
//
void NTAPI AleAuthConnectV4Classify(
	IN const FWPS_INCOMING_VALUES0* inFixedValues,
	IN const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
	IN OUT void* layerData,
	IN const FWPS_FILTER0* filter,
	IN UINT64 flowContext,
	OUT FWPS_CLASSIFY_OUT0* classifyOut
);

NTSTATUS NTAPI AleAuthConnectV4Notify(
	IN FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	IN const GUID* filterKey,
	IN OUT FWPS_FILTER0* filter
);

void NTAPI AleResourceAssignmentV4Classify(
	IN const FWPS_INCOMING_VALUES0* inFixedValues,
	IN const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
	IN OUT void* layerData,
	IN const FWPS_FILTER0* filter,
	IN UINT64 flowContext,
	OUT FWPS_CLASSIFY_OUT0* classifyOut
);

NTSTATUS NTAPI AleResourceAssignmentV4Notify(
	IN FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	IN const GUID* filterKey,
	IN OUT FWPS_FILTER0* filter
);

void NTAPI AleFlowEstablishedV4Classify(
	IN const FWPS_INCOMING_VALUES0* inFixedValues,
	IN const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
	IN OUT void* layerData,
	IN const FWPS_FILTER0* filter,
	IN UINT64 flowContext,
	OUT FWPS_CLASSIFY_OUT0* classifyOut
);

NTSTATUS NTAPI AleFlowEstablishedV4Notify(
	IN FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	IN const GUID* filterKey,
	IN OUT FWPS_FILTER0* filter
);

VOID NTAPI
	AleFlowEstablishedV4Delete(
	IN UINT16  layerId,
	IN UINT32  calloutId,
	IN UINT64  flowContext
);

void NTAPI OutboundTransportV4Classify(
	IN const FWPS_INCOMING_VALUES0* inFixedValues,
	IN const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
	IN OUT void* layerData,
	IN const FWPS_FILTER0* filter,
	IN UINT64 flowContext,
	OUT FWPS_CLASSIFY_OUT0* classifyOut
);

NTSTATUS NTAPI OutboundTransportV4Notify(
	IN FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	IN const GUID* filterKey,
	IN OUT FWPS_FILTER0* filter
);

//VOID NTAPI
//	OutboundTransportV4Delete(
//	IN UINT16  layerId,
//	IN UINT32  calloutId,
//	IN UINT64  flowContext
//);

static NTSTATUS 
RegisterWfpFilter();

static VOID NTAPI 
BfeSubscribeWithoutDeviceCallback(
	IN OUT void  *context,
	IN FWPM_SERVICE_STATE  newState
);

static NTSTATUS 
RegisterCallouts(void* pDeviceObject);

static NTSTATUS 
RegisterCalloutForLayer(
	IN const GUID* pLayerKey,
	IN const GUID* pCalloutKey,
	IN const GUID* pFilterKey,
	IN const GUID* pSublayerKey,
	IN void* pDeviceObject,
	IN WCHAR* pDescription,
	IN FWPS_CALLOUT_CLASSIFY_FN0 pfClassify,
	IN FWPS_CALLOUT_NOTIFY_FN0 pfNotify,
	IN FWPS_CALLOUT_FLOW_DELETE_NOTIFY_FN0 pfFlowDelete,
	IN HANDLE hEngine,
	IN FWP_ACTION_TYPE actionType,
	OUT PUINT32 pCalloutId
);

static VOID 
UnregisterCallouts();

//
// Functions definitions
//

/**
 *	@brief		InitializeWfpFilter - Prepares environment for filtering in WFP
 *	@return		NTSTATUS
 */
NTSTATUS InitializeWfpFilter( PDRIVER_OBJECT pDriverObject )
{
	NTSTATUS status;

	// Create new nameless device
	status = IoCreateDevice (
		pDriverObject,
		0,
		NULL,
		FILE_DEVICE_NETWORK,
		FILE_DEVICE_SECURE_OPEN,
		FALSE,
		&g_pDeviceObject
		);
	if( ! NT_SUCCESS(status) )
	{
		dprintf( "IoCreateDevice failed.\n" );
		return status;
	}

	if( FwpmBfeStateGet0() == FWPM_SERVICE_RUNNING )
	{
		return RegisterCallouts( g_pDeviceObject );
	}
	else
	{
		return FwpmBfeStateSubscribeChanges0( g_pDeviceObject, BfeSubscribeWithoutDeviceCallback, 0, &g_hChanges );
	}
}


/**
 *	@brief		BfeSubscribeWithoutDeviceCallback - Is called by WFP while state changes
 *	@return		NTSTATUS
 */
VOID NTAPI BfeSubscribeWithoutDeviceCallback(
	IN OUT void  *context,
	IN FWPM_SERVICE_STATE  newState
)
{
	if(newState==FWPM_SERVICE_RUNNING)
	{				
		RegisterCallouts( g_pDeviceObject );
	}
}


/**
 *	@brief		RegisterCallouts - Register callouts and filters
 *	@param		pDeviceObject - Our device object
 *	@return		NTSTATUS
 */
NTSTATUS RegisterCallouts(void* pDeviceObject)
{
	NTSTATUS status;
	HANDLE hEngine;
	FWPM_PROVIDER0 asProvider = {0};
	FWPM_SUBLAYER0 asSubLayer = {0};

	status = FwpsInjectionHandleCreate0( AF_INET, FWPS_INJECTION_TYPE_TRANSPORT, &g_hInjectionTransportV4 );
	if( ! NT_SUCCESS(status) )
	{
		dprintf( "FwpsInjectionHandleCreate0 failed.\n" );
		return status;
	}

	status = FwpmEngineOpen0(
		NULL,
		RPC_C_AUTHN_WINNT,
		NULL,
		NULL,
		&hEngine
		);
	if( ! NT_SUCCESS(status) )
	{
		dprintf( "FwpmEngineOpen0 failed.\n" );
		FwpsInjectionHandleDestroy0( g_hInjectionTransportV4 );
		return status;
	}

	status = FwpmTransactionBegin0( hEngine, 0 );
	if( ! NT_SUCCESS(status) ) 
	{
		dprintf( "FwpmTransactionBegin0 failed.\n" );
		FwpmEngineClose0( hEngine );
		FwpsInjectionHandleDestroy0( g_hInjectionTransportV4 );
		return status;
	}

	asProvider.displayData.name = asProvider.displayData.description = L"AS provider";
	asProvider.providerKey = AS_PROVIDER;
	status = FwpmProviderAdd0(hEngine, &asProvider,	0);
	if( ! NT_SUCCESS(status) && status!=STATUS_FWP_ALREADY_EXISTS )
	{
		dprintf( "FwpmProviderAdd0 failed. status %X\n", status );
		FwpmTransactionAbort0(hEngine);
		FwpmEngineClose0( hEngine );
		FwpsInjectionHandleDestroy0( g_hInjectionTransportV4 );
		return status;
	}

	asSubLayer.displayData.name = asSubLayer.displayData.description = L"AS sublayer";
	asSubLayer.providerKey = &(asProvider.providerKey);
	asSubLayer.weight = 0xFFFF;
	asSubLayer.subLayerKey = AS_SUBLAYER;
	status = FwpmSubLayerAdd0(hEngine, &asSubLayer,	0);
	if( ! NT_SUCCESS(status) && status!=STATUS_FWP_ALREADY_EXISTS )
	{
		dprintf(  "FwpmSubLayerAdd0 failed. status %X\n", status );
		FwpmTransactionAbort0(hEngine);
		FwpmEngineClose0( hEngine );
		FwpsInjectionHandleDestroy0( g_hInjectionTransportV4 );
		return status;
	}	

	status = RegisterCalloutForLayer(
		&FWPM_LAYER_ALE_AUTH_CONNECT_V4,
		&CALLOUT_ALE_AUTH_CONNECT_V4,
		&FILTER_ALE_AUTH_CONNECT_V4,
		&AS_SUBLAYER,
		pDeviceObject,
		L"AleAuthConnectV4 callout",
		AleAuthConnectV4Classify,
		AleAuthConnectV4Notify,
		NULL,
		hEngine,
		FWP_ACTION_CALLOUT_INSPECTION,
		NULL );

	if( ! NT_SUCCESS(status) && status!=STATUS_FWP_ALREADY_EXISTS )
	{
		FwpmTransactionAbort0(hEngine);
		FwpmEngineClose0( hEngine );
		FwpsInjectionHandleDestroy0( g_hInjectionTransportV4 );
		return status;
	}

	status = RegisterCalloutForLayer(		
		&FWPM_LAYER_ALE_RESOURCE_ASSIGNMENT_V4,
		&CALLOUT_ALE_RESOURCE_ASSIGNMENT_V4,
		&FILTER_ALE_RESOURCE_ASSIGNMENT_V4,
		&AS_SUBLAYER,
		pDeviceObject,
		L"AleAuthResourceAssignmentV4 callout",
		AleResourceAssignmentV4Classify,
		AleResourceAssignmentV4Notify,
		NULL,
		hEngine,
		FWP_ACTION_CALLOUT_INSPECTION,
		NULL );

	if( ! NT_SUCCESS(status) && status!=STATUS_FWP_ALREADY_EXISTS )
	{
		FwpmTransactionAbort0(hEngine);
		FwpmEngineClose0( hEngine );
		FwpsInjectionHandleDestroy0( g_hInjectionTransportV4 );
		return status;
	}

	status = RegisterCalloutForLayer(
		&FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4,
		&CALLOUT_ALE_FLOW_ESTABLISHED_V4,
		&FILTER_ALE_FLOW_ESTABLISHED_V4,
		&AS_SUBLAYER,
		pDeviceObject,
		L"AleFlowEstablishedV4 callout",
		AleFlowEstablishedV4Classify,
		AleFlowEstablishedV4Notify,
		AleFlowEstablishedV4Delete,
		hEngine,
		FWP_ACTION_CALLOUT_INSPECTION,
		&g_AleFlowEstablishedV4Id );

	if( ! NT_SUCCESS(status) && status!=STATUS_FWP_ALREADY_EXISTS )
	{
		FwpmTransactionAbort0(hEngine);
		FwpmEngineClose0( hEngine );
		FwpsInjectionHandleDestroy0( g_hInjectionTransportV4 );
		return status;
	}

	status = RegisterCalloutForLayer(
		&FWPM_LAYER_OUTBOUND_TRANSPORT_V4,
		&CALLOUT_OUTBOUND_TRANSPORT_V4,
		&FILTER_OUTBOUND_TRANSPORT_V4,
		&AS_SUBLAYER,
		pDeviceObject,
		L"OutboundTransportV4 callout",
		OutboundTransportV4Classify,
		OutboundTransportV4Notify,
		NULL,
		hEngine,
		FWP_ACTION_CALLOUT_TERMINATING,
		NULL );
	if( ! NT_SUCCESS(status) && status!=STATUS_FWP_ALREADY_EXISTS )
	{
		FwpmTransactionAbort0(hEngine);
		FwpmEngineClose0( hEngine );
		FwpsInjectionHandleDestroy0( g_hInjectionTransportV4 );
		return status;
	}

	status = FwpmTransactionCommit0(hEngine);

	FwpmEngineClose0( hEngine );

	return status;
}


/**
 *	@brief		RegisterCalloutForLayer - registers and adds a single callout and filter to WFP
 *	@param		pLayerKey - GUID of layer
 *	@param		pCalloutKey - GUID of callout
 *	@param		pFilterKey
 *	@param		pDeviceObject - Our device object
 *	@param      pDescription - string description
 *	@param		pfClassify - callback for this callout
 *	@param		pfNotify - receives notifications for this callout 
 *	@param		hEngine - previosly opened callout engine
 *	@param		pCalloutId - ID of callout which is received on return
 *	@return		NTSTATUS. If the routine succeeds - STATUS_SUCCESS. Otherwise - error status values.
 */
NTSTATUS RegisterCalloutForLayer(
	IN const GUID* pLayerKey,
	IN const GUID* pCalloutKey,
	IN const GUID* pFilterKey,
	IN const GUID* pSublayerKey,
	IN void* pDeviceObject,
	IN WCHAR* pDescription,
	IN FWPS_CALLOUT_CLASSIFY_FN0 pfClassify,
	IN FWPS_CALLOUT_NOTIFY_FN0 pfNotify,
	IN FWPS_CALLOUT_FLOW_DELETE_NOTIFY_FN0 pfFlowDelete,
	IN HANDLE hEngine,
	IN FWP_ACTION_TYPE actionType,
	OUT PUINT32 pCalloutId
)
{
	NTSTATUS status;

	FWPS_CALLOUT0 sCallout = {0};
	FWPM_CALLOUT0 mCallout = {0};
	FWPM_FILTER0 filter = {0};

	sCallout.calloutKey = *pCalloutKey;
	sCallout.classifyFn = pfClassify; 
	sCallout.notifyFn = pfNotify;
	sCallout.flowDeleteFn = pfFlowDelete;
	status = FwpsCalloutRegister0(				 
		pDeviceObject,
		&sCallout,
		pCalloutId
		);
	if (status!=STATUS_SUCCESS && status!=STATUS_FWP_ALREADY_EXISTS)
	{
		dprintf( "FwpsCalloutRegister0 failed. status %X\n", status );
		return status; 
	}

	mCallout.calloutKey = *pCalloutKey;
	mCallout.displayData.name = pDescription;
	mCallout.displayData.description = pDescription;
	mCallout.applicableLayer = *pLayerKey;
	//mCallout.flags = FWPM_CALLOUT_FLAG_PERSISTENT;
	status = FwpmCalloutAdd0(
		hEngine,
		&mCallout,
		NULL,
		NULL
		);
	if (status!=STATUS_SUCCESS && status!=STATUS_FWP_ALREADY_EXISTS)
	{
		dprintf( "FwpmCalloutAdd0 failed. status %X\n", status );
		return status;
	}

	filter.filterKey = *pFilterKey;
	filter.layerKey = *pLayerKey;
	filter.displayData.name = pDescription;
	filter.displayData.description = pDescription;
	filter.action.type = actionType;
	filter.action.calloutKey = *pCalloutKey;
	filter.subLayerKey = *pSublayerKey;
	//filter.flags = FWPM_FILTER_FLAG_PERSISTENT;
	status = FwpmFilterAdd0(
		hEngine,
		&filter,
		NULL,
		NULL);
	if (status!=STATUS_SUCCESS && status!=STATUS_FWP_ALREADY_EXISTS)
	{
		dprintf( "FwpmFilterAdd0 failed. status %X\n", status );
	}

	return status;
}


/**
 *	@brief		UninitializeWfpFilter - Remove filters, callouts and destroys injections in WFP
 */
VOID UninitializeWfpFilter( )
{
	UnregisterCallouts();

	if( g_hChanges )
	{	
		FwpmBfeStateUnsubscribeChanges0( g_hChanges ); 
		g_hChanges = NULL;
	}

	IoDeleteDevice( g_pDeviceObject );
}


/**
 *	@brief		UnregisterCallouts - Unregister and remove all callouts from filter engine
 */
VOID UnregisterCallouts()
{
	NTSTATUS status;
	HANDLE   hEngine;

	status = FwpmEngineOpen0( NULL, RPC_C_AUTHN_WINNT, NULL, NULL, &hEngine );
	if( NT_SUCCESS(status) )
	{
		FwpmFilterDeleteByKey0( hEngine, &FILTER_ALE_FLOW_ESTABLISHED_V4 );
		FwpmCalloutDeleteByKey0( hEngine, &CALLOUT_ALE_FLOW_ESTABLISHED_V4 );

		FwpmFilterDeleteByKey0( hEngine, &FILTER_OUTBOUND_TRANSPORT_V4 );
		FwpmCalloutDeleteByKey0( hEngine, &CALLOUT_OUTBOUND_TRANSPORT_V4 );

		FwpmFilterDeleteByKey0( hEngine, &FILTER_ALE_RESOURCE_ASSIGNMENT_V4 );
		FwpmCalloutDeleteByKey0( hEngine, &CALLOUT_ALE_RESOURCE_ASSIGNMENT_V4 );

		FwpmFilterDeleteByKey0( hEngine, &FILTER_ALE_AUTH_CONNECT_V4 );
		FwpmCalloutDeleteByKey0( hEngine, &CALLOUT_ALE_AUTH_CONNECT_V4 );

		FwpmSubLayerDeleteByKey0( hEngine, &AS_SUBLAYER );
		FwpmProviderDeleteByKey0( hEngine, &AS_PROVIDER );

		FwpmEngineClose0( hEngine );
	}

	FwpsCalloutUnregisterByKey0( &CALLOUT_ALE_FLOW_ESTABLISHED_V4 );
	FwpsCalloutUnregisterByKey0( &CALLOUT_OUTBOUND_TRANSPORT_V4 );
	FwpsCalloutUnregisterByKey0( &CALLOUT_ALE_RESOURCE_ASSIGNMENT_V4 );
	FwpsCalloutUnregisterByKey0( &CALLOUT_ALE_AUTH_CONNECT_V4 );

	FwpsInjectionHandleDestroy0( g_hInjectionTransportV4 );
}

void NTAPI AleAuthConnectV4Classify(
	IN const FWPS_INCOMING_VALUES0* inFixedValues,
	IN const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
	IN OUT void* layerData,
	IN const FWPS_FILTER0* filter,
	IN UINT64 flowContext,
	OUT FWPS_CLASSIFY_OUT0* classifyOut
)
{
	UINT64 pid = HandleToUlong(PsGetCurrentProcessId());

	AddConnection( 
		(pid!=0?pid:inMetaValues->processId), 
		inFixedValues->incomingValue[FWPS_FIELD_ALE_AUTH_CONNECT_V4_IP_PROTOCOL].value.uint16, 
		htonl(inFixedValues->incomingValue[FWPS_FIELD_ALE_AUTH_CONNECT_V4_IP_LOCAL_ADDRESS].value.uint32),
		htons(inFixedValues->incomingValue[FWPS_FIELD_ALE_AUTH_CONNECT_V4_IP_LOCAL_PORT].value.uint16),
		htonl(inFixedValues->incomingValue[FWPS_FIELD_ALE_AUTH_CONNECT_V4_IP_REMOTE_ADDRESS].value.uint32),
		htons(inFixedValues->incomingValue[FWPS_FIELD_ALE_AUTH_CONNECT_V4_IP_REMOTE_PORT].value.uint16)
		);
}

NTSTATUS NTAPI AleAuthConnectV4Notify(
	IN FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	IN const GUID* filterKey,
	IN OUT FWPS_FILTER0* filter
)
{
	return STATUS_SUCCESS;
}

void NTAPI AleResourceAssignmentV4Classify(
	IN const FWPS_INCOMING_VALUES0* inFixedValues,
	IN const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
	IN OUT void* layerData,
	IN const FWPS_FILTER0* filter,
	IN UINT64 flowContext,
	OUT FWPS_CLASSIFY_OUT0* classifyOut
)
{
	UINT64 pid = HandleToUlong(PsGetCurrentProcessId());

	AddConnection( 
		(pid!=0?pid:inMetaValues->processId), 
		inFixedValues->incomingValue[FWPS_FIELD_ALE_RESOURCE_ASSIGNMENT_V4_IP_PROTOCOL].value.uint16, 
		htonl(inFixedValues->incomingValue[FWPS_FIELD_ALE_RESOURCE_ASSIGNMENT_V4_IP_LOCAL_ADDRESS].value.uint32),
		htons(inFixedValues->incomingValue[FWPS_FIELD_ALE_RESOURCE_ASSIGNMENT_V4_IP_LOCAL_PORT].value.uint16),
		0,
		0
		);
}

NTSTATUS NTAPI AleResourceAssignmentV4Notify(
	IN FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	IN const GUID* filterKey,
	IN OUT FWPS_FILTER0* filter
)
{
	return STATUS_SUCCESS;
}

void NTAPI AleFlowEstablishedV4Classify(
	IN const FWPS_INCOMING_VALUES0* inFixedValues,
	IN const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
	IN OUT void* layerData,
	IN const FWPS_FILTER0* filter,
	IN UINT64 flowContext,
	OUT FWPS_CLASSIFY_OUT0* classifyOut
	)
{
	PVOID pConnection = GetConnection( 
		inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_PROTOCOL].value.uint16, 
		htonl(inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_LOCAL_ADDRESS].value.uint32),
		htons(inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_LOCAL_PORT].value.uint16),
		htonl(inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_REMOTE_ADDRESS].value.uint32),
		htons(inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_REMOTE_PORT].value.uint16)
		);

	if( pConnection && FWPS_IS_METADATA_FIELD_PRESENT(inMetaValues, FWPS_METADATA_FIELD_FLOW_HANDLE) )
	{
		FwpsFlowAssociateContext0( 
			inMetaValues->flowHandle, 
			FWPS_LAYER_ALE_FLOW_ESTABLISHED_V4,
			g_AleFlowEstablishedV4Id,
			(ULONG_PTR)pConnection );
	}
}

NTSTATUS NTAPI AleFlowEstablishedV4Notify(
	IN FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	IN const GUID* filterKey,
	IN OUT FWPS_FILTER0* filter
	)
{
	return STATUS_SUCCESS;
}

VOID NTAPI
	AleFlowEstablishedV4Delete(
	IN UINT16  layerId,
	IN UINT32  calloutId,
	IN UINT64  flowContext
	)
{
	RemoveConnection( (PVOID)flowContext );
}

void NTAPI OutboundTransportV4Classify(
	IN const FWPS_INCOMING_VALUES0* inFixedValues,
	IN const FWPS_INCOMING_METADATA_VALUES0* inMetaValues,
	IN OUT void* layerData,
	IN const FWPS_FILTER0* filter,
	IN UINT64 flowContext,
	OUT FWPS_CLASSIFY_OUT0* classifyOut
)
{
	NTSTATUS status;
	NET_BUFFER_LIST *pClonedNBL, *pNetBufferList = (NET_BUFFER_LIST*)layerData;	
	FWPS_PACKET_INJECTION_STATE injectionState;
	PFW_REQUEST pFwRequest;
	PWFP_INFO pFwpInfo;
	SIZE_T ulBufferLenght;

	dprintf( "OutboundTransportV4Classify: %X %X ",
		inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_IP_PROTOCOL].value.uint16,
		inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_IP_REMOTE_ADDRESS].value.uint32 );

	if (!(classifyOut->rights & FWPS_RIGHT_ACTION_WRITE) && classifyOut->actionType==FWP_ACTION_BLOCK )
	{
		// We can change nothing in this decision
		// Return without specifying an action
		return;
	}

	if (filter->flags & FWPS_FILTER_FLAG_CLEAR_ACTION_RIGHT)
	{
		classifyOut->rights &= ~FWPS_RIGHT_ACTION_WRITE;
	}
	classifyOut->actionType = FWP_ACTION_PERMIT;

	injectionState = FwpsQueryPacketInjectionState0(g_hInjectionTransportV4,pNetBufferList,NULL);
	if (injectionState == FWPS_PACKET_INJECTED_BY_SELF ||
		injectionState == FWPS_PACKET_PREVIOUSLY_INJECTED_BY_SELF ||
		IsLocalAddress(htonl(inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_IP_REMOTE_ADDRESS].value.uint32))==FALSE
		//!(inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_FLAGS].value.uint32&FWP_CONDITION_FLAG_IS_LOOPBACK) )
		/*isLoopback(htonl(inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_IP_REMOTE_ADDRESS].value.uint32))==FALSE /*||
		IsClientConnected()==FALSE*/ )
	{
		return;
	}

	status = FwpsAllocateCloneNetBufferList0( pNetBufferList, NULL, NULL, 0, &pClonedNBL );
	if( ! NT_SUCCESS(status) )
	{
		dprintf( "FwpsAllocateCloneNetBufferList0 failed.\n" );
		return;
	}

	ulBufferLenght = sizeof(FW_REQUEST)+sizeof(FLT_PACKET)+sizeof(WFP_INFO)+sizeof(UINT);
	if( FWPS_IS_METADATA_FIELD_PRESENT(inMetaValues, FWPS_METADATA_FIELD_TRANSPORT_CONTROL_DATA) )
		ulBufferLenght += inMetaValues->controlDataLength;

	pFwRequest = ExAllocatePoolWithTag( 
		NonPagedPool, 
		ulBufferLenght, 
		DRIVER_TAG );
	if( pFwRequest==NULL )
	{
		dprintf( "ExAllocatePoolWithTag failed.\n" );
		FwpsFreeCloneNetBufferList0( pClonedNBL, 0 );
		return;
	}

	RtlZeroMemory( pFwRequest, ulBufferLenght );

	pFwRequest->m_pFltPacket = (PVOID)((ULONG_PTR)pFwRequest+sizeof(FW_REQUEST));
	pFwpInfo = (PWFP_INFO)((ULONG_PTR)pFwRequest->m_pFltPacket+sizeof(FLT_PACKET));
	pFwpInfo->m_SendArgs.remoteAddress = (UCHAR*)((PBYTE)pFwpInfo+sizeof(WFP_INFO));
	
	pFwpInfo->m_EndpointHandle = inMetaValues->transportEndpointHandle;
	*((PUINT)pFwpInfo->m_SendArgs.remoteAddress) = RtlUlongByteSwap(inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_IP_REMOTE_ADDRESS].value.uint32);
	pFwpInfo->m_SendArgs.remoteScopeId = inMetaValues->remoteScopeId;
	if( FWPS_IS_METADATA_FIELD_PRESENT(inMetaValues, FWPS_METADATA_FIELD_TRANSPORT_CONTROL_DATA) )
	{
		pFwpInfo->m_SendArgs.controlData = (WSACMSGHDR*)((PBYTE)pFwpInfo+sizeof(WFP_INFO)+sizeof(UINT));
		RtlCopyMemory( pFwpInfo->m_SendArgs.controlData, inMetaValues->controlData, inMetaValues->controlDataLength );
		pFwpInfo->m_SendArgs.controlDataLength = inMetaValues->controlDataLength;
	}
	if( FWPS_IS_METADATA_FIELD_PRESENT(inMetaValues, FWPS_METADATA_FIELD_COMPARTMENT_ID) )
		pFwpInfo->m_CompartmentId = inMetaValues->compartmentId;
	pFwpInfo->m_pNetBufferList = pClonedNBL;
	

	pFwRequest->m_ReqFlags = FL_REQUEST_LOOPBACK;
	//pFwRequest->m_Direction = PHYSICAL_DIRECTION_OUT;	
	pFwRequest->m_IpProto = (BYTE)inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_IP_PROTOCOL].value.uint16;
	pFwRequest->m_SourceAddr = RtlUlongByteSwap(inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_IP_LOCAL_ADDRESS].value.uint32);
	pFwRequest->m_DestAddr = RtlUlongByteSwap(inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_IP_REMOTE_ADDRESS].value.uint32);
	pFwRequest->m_SourcePort = RtlUshortByteSwap(inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_IP_LOCAL_PORT].value.uint16);
	pFwRequest->m_DestPort = RtlUshortByteSwap(inFixedValues->incomingValue[FWPS_FIELD_OUTBOUND_TRANSPORT_V4_IP_REMOTE_PORT].value.uint16);
	pFwRequest->m_pFltPacket->m_pBuffer = pFwpInfo;
	pFwRequest->m_pFltPacket->m_Direction = PHYSICAL_DIRECTION_OUT;
	pFwRequest->m_pFltPacket->m_BufferSize = 0;
	pFwRequest->m_References = 1;
	//pFwRequest->m_pBuffer = pFwpInfo;
	//pFwRequest->m_BufferSize = 0;
	//pFwRequest->m_Pid = inMetaValues->processId;
	GetPacketPid( pFwRequest );

	FWInjectRequest( NULL, pFwRequest );

	FWFreeCloneRequest( pFwRequest );

	classifyOut->actionType = FWP_ACTION_BLOCK;
	classifyOut->rights &= ~FWPS_RIGHT_ACTION_WRITE;
	classifyOut->flags |= FWPS_CLASSIFY_OUT_FLAG_ABSORB;	

	//status = ProcessRequest( NULL, pFwRequest );
	//if( status!=STATUS_SUCCESS )
	//{
	//	classifyOut->actionType = FWP_ACTION_BLOCK;
	//	classifyOut->rights &= ~FWPS_RIGHT_ACTION_WRITE;
	//	classifyOut->flags |= FWPS_CLASSIFY_OUT_FLAG_ABSORB;	
	//}

	//if( --((PWFP_INFO)pFwRequest->m_pBuffer)->m_References == 0 )
	//{
	//	FwpsFreeCloneNetBufferList0( ((PWFP_INFO)pFwRequest->m_pBuffer)->m_pNetBufferList, 0 );
	//	ExFreePool(pFwRequest);
	//}
}

NTSTATUS NTAPI OutboundTransportV4Notify(
	IN FWPS_CALLOUT_NOTIFY_TYPE notifyType,
	IN const GUID* filterKey,
	IN OUT FWPS_FILTER0* filter
)
{
	return STATUS_SUCCESS;
}

//VOID NTAPI
//	OutboundTransportV4Delete(
//	IN UINT16  layerId,
//	IN UINT32  calloutId,
//	IN UINT64  flowContext
//);

VOID NTAPI
OutboundTransportV4Completion(
    IN VOID  *context,
    IN OUT NET_BUFFER_LIST  *netBufferList,
    IN BOOLEAN  dispatchLevel
    )
{
	FwpsFreeCloneNetBufferList0( netBufferList, 0 );
}

void InjectLoopbackRequest( PFW_REQUEST pFwRequest)
{
	PWFP_INFO pWfpInfo = (PWFP_INFO)pFwRequest->m_pFltPacket->m_pBuffer;
	if( pWfpInfo->m_pNetBufferList )
	{
		//Inject
		if( FwpsInjectTransportSendAsync0( 
			g_hInjectionTransportV4, 
			NULL, 
			pWfpInfo->m_EndpointHandle,
			0,
			&pWfpInfo->m_SendArgs,
			AF_INET,
			pWfpInfo->m_CompartmentId,
			pWfpInfo->m_pNetBufferList,
			OutboundTransportV4Completion,
			NULL )==STATUS_SUCCESS )
		{
			pWfpInfo->m_pNetBufferList = NULL;
		}
	}
}

void FreeLoopbackRequest( PFW_REQUEST pClonedRequest )
{
	if( ((PWFP_INFO)pClonedRequest->m_pFltPacket->m_pBuffer)->m_pNetBufferList )
	{
		FwpsFreeCloneNetBufferList0( ((PWFP_INFO)pClonedRequest->m_pFltPacket->m_pBuffer)->m_pNetBufferList, 0 );
		((PWFP_INFO)pClonedRequest->m_pFltPacket->m_pBuffer)->m_pNetBufferList = NULL;
	}

	ExFreePool(pClonedRequest);
}