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

NDIS_SPIN_LOCK g_FltInstancesLock;
LIST_ENTRY g_FltInstancesList;
NDIS_SPIN_LOCK g_PacketLock;
LIST_ENTRY g_PacketsList;

NTSTATUS
FilterAllocateCallbacks(
	IN OUT PFLT_INSTANCE *ppFilter, 
	IN PFLT_REGISTRATION_CONTEXT pContext
	)
{
	dprintf( "===> FilterAllocateCallbacks.\n" );

#if (NTDDI_VERSION < NTDDI_LONGHORN)
	if( NdisAllocateMemoryWithTag(ppFilter, sizeof(FLT_INSTANCE), DRIVER_TAG)==NDIS_STATUS_FAILURE )
		return STATUS_INSUFFICIENT_RESOURCES;
#else
	*ppFilter = NdisAllocateMemoryWithTagPriority( 
		NULL, 
		sizeof(FLT_INSTANCE), 
		DRIVER_TAG, 
		NormalPoolPriority );

	if( *ppFilter == NULL )
		return STATUS_INSUFFICIENT_RESOURCES;
#endif

	(*ppFilter)->m_RegistrationContext.m_pRequestCallback = pContext->m_pRequestCallback;
	(*ppFilter)->m_RegistrationContext.m_pOpenAdapterCallback = pContext->m_pOpenAdapterCallback;
	(*ppFilter)->m_RegistrationContext.m_pCloseAdapterCallback = pContext->m_pCloseAdapterCallback;
	(*ppFilter)->m_RegistrationContext.m_pUnloadCallback = pContext->m_pUnloadCallback;

	return STATUS_SUCCESS;
}

NTSTATUS 
FLTRegisterCallbacks(
	IN OUT PFLT_INSTANCE * ppInstance, 
	IN PFLT_REGISTRATION_CONTEXT pContext
)
{
	NTSTATUS status;
	PFLT_INSTANCE pFltInstance;

	dprintf( "===> RegisterFilterCallbacks. ppInstance = %p, pContext = %p.\n", ppInstance, pContext );

	if(ppInstance==NULL || pContext==NULL || pContext->m_pRequestCallback==NULL )
		return STATUS_INVALID_PARAMETER;

	status = FilterAllocateCallbacks(&pFltInstance,	pContext);

	if(status == STATUS_SUCCESS)
	{
		*ppInstance = pFltInstance;

		NdisAcquireSpinLock( &g_FltInstancesLock );
		InsertHeadList( &g_FltInstancesList, &pFltInstance->m_qLink );
		NdisReleaseSpinLock( &g_FltInstancesLock );
	}	
	
	return status;			
}

NTSTATUS
FLTDeregisterCallbacks(
	IN PFLT_INSTANCE pInstance
	)
{
	dprintf( "===> DeregisterFilterCallbacks. pInstance = %p.\n", pInstance );

	NdisAcquireSpinLock( &g_FltInstancesLock );
	RemoveEntryList( &pInstance->m_qLink );
	NdisReleaseSpinLock( &g_FltInstancesLock );

	NdisFreeMemory( pInstance, 0, 0 );

	return STATUS_SUCCESS;
}

NTSTATUS 
FLTInjectRequest ( 	
	IN PFLT_INSTANCE pInstance OPTIONAL,
	IN PFLT_PACKET pPacket
)
{
	NTSTATUS Status = STATUS_SUCCESS;
	PLIST_ENTRY pListEntry;

	NdisAcquireSpinLock( &g_FltInstancesLock );

	if( pInstance )
		pListEntry = pInstance->m_qLink.Flink;
	else
		pListEntry = g_FltInstancesList.Flink;

	while( pListEntry != &g_FltInstancesList )
	{
		PFLT_INSTANCE pFltInstance = CONTAINING_RECORD( pListEntry, FLT_INSTANCE, m_qLink );
		pListEntry = pListEntry->Flink;

		if( pFltInstance->m_RegistrationContext.m_pRequestCallback )
			Status = pFltInstance->m_RegistrationContext.m_pRequestCallback( pPacket );

		if( Status != STATUS_SUCCESS )
		{
			break;
		}
	}

	NdisReleaseSpinLock( &g_FltInstancesLock );

	if( Status == STATUS_SUCCESS )
	{
		SendPacket( pPacket );
	}

	return Status;
}

NTSTATUS
FLTCloneRequest (
	IN PFLT_PACKET pPacket,
	OUT PFLT_PACKET *ppClonePacket
	)
{
	ASSERT(pPacket->m_References>0);
	InterlockedIncrement(&pPacket->m_References);
	*ppClonePacket = pPacket;
	return STATUS_SUCCESS;
}

VOID
FLTFreeCloneRequest (
	IN PFLT_PACKET pClonePacket
	)
{
	if( InterlockedDecrement(&pClonePacket->m_References)==0 )
	{		
		NdisAcquireSpinLock( &g_PacketLock );
		RemoveEntryList( &pClonePacket->m_qLink );
		NdisReleaseSpinLock( &g_PacketLock );

		FreeNdisData( pClonePacket );
	}
}

VOID OpenAdapter( PVOID pAdapter, PNDIS_STRING pBindingName )
{
	PLIST_ENTRY pListEntry;

	NdisAcquireSpinLock( &g_FltInstancesLock );

	pListEntry = g_FltInstancesList.Flink;
	while( pListEntry != &g_FltInstancesList )
	{
		PFLT_INSTANCE pFltInstance = CONTAINING_RECORD( pListEntry, FLT_INSTANCE, m_qLink );

		if( pFltInstance->m_RegistrationContext.m_pOpenAdapterCallback )
			pFltInstance->m_RegistrationContext.m_pOpenAdapterCallback( pAdapter, pBindingName );

		pListEntry = pListEntry->Flink;
	}

	NdisReleaseSpinLock( &g_FltInstancesLock );
}

VOID CloseAdapter( PVOID pAdapter )
{
	PLIST_ENTRY pListEntry;

	// Mark closing adapter related packets as irrelevant

	NdisAcquireSpinLock( &g_PacketLock );
	pListEntry = g_PacketsList.Flink;
	while( pListEntry != &g_PacketsList )
	{
		PFLT_PACKET pPacket = CONTAINING_RECORD( pListEntry, FLT_PACKET, m_qLink );
		pListEntry = pListEntry->Flink;

		if( pPacket->m_pAdapter == pAdapter )
			pPacket->m_pAdapter = NULL;
	}
	NdisReleaseSpinLock( &g_PacketLock );


	NdisAcquireSpinLock( &g_FltInstancesLock );
	pListEntry = g_FltInstancesList.Flink;
	while( pListEntry != &g_FltInstancesList )
	{
		PFLT_INSTANCE pFltInstance = CONTAINING_RECORD( pListEntry, FLT_INSTANCE, m_qLink );
		pListEntry = pListEntry->Flink;

		if( pFltInstance->m_RegistrationContext.m_pCloseAdapterCallback )
			pFltInstance->m_RegistrationContext.m_pCloseAdapterCallback( pAdapter );
	}
	NdisReleaseSpinLock( &g_FltInstancesLock );
}

VOID InitializeANSfltr()
{
	InitializeListHead( &g_FltInstancesList );
	NdisAllocateSpinLock( &g_FltInstancesLock );

	InitializeListHead( &g_PacketsList );
	NdisAllocateSpinLock( &g_PacketLock );
}

VOID UnInitializeANSfltr()
{
	PLIST_ENTRY pListEntry;

	NdisAcquireSpinLock( &g_FltInstancesLock );
	while( (pListEntry=RemoveHeadList(&g_FltInstancesList))!=&g_FltInstancesList )
	{
		PFLT_INSTANCE pFltInstance = CONTAINING_RECORD( pListEntry, FLT_INSTANCE, m_qLink );

		if( pFltInstance->m_RegistrationContext.m_pUnloadCallback )
			pFltInstance->m_RegistrationContext.m_pUnloadCallback();

		NdisFreeMemory( pFltInstance, 0, 0 );
	}
	NdisReleaseSpinLock( &g_FltInstancesLock );

	NdisFreeSpinLock( &g_FltInstancesLock );

	NdisAcquireSpinLock( &g_PacketLock );
	while( (pListEntry=RemoveHeadList(&g_PacketsList))!=&g_PacketsList )
	{
		NdisReleaseSpinLock( &g_PacketLock );
		FreeNdisData( CONTAINING_RECORD( pListEntry, FLT_PACKET, m_qLink ) );
		NdisAcquireSpinLock( &g_PacketLock );
	}
	NdisReleaseSpinLock( &g_PacketLock );

	NdisFreeSpinLock( &g_PacketLock );
}

BOOLEAN IsInstancesListEmpty()
{
	BOOLEAN result;

	NdisAcquireSpinLock( &g_FltInstancesLock );
	result = IsListEmpty( &g_FltInstancesList );
	NdisReleaseSpinLock( &g_FltInstancesLock );

	return result;
}