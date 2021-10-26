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

VOID
	EXT_InitializeNdisrdExtension (
		PNDISRD_ADAPTER_EXTENSION pExtension
		)
{
	NdisZeroMemory ( pExtension, sizeof(NDISRD_ADAPTER_EXTENSION) );
	NdisAllocateSpinLock ( &pExtension->m_QueuedPacketsLock );
	InitializeListHead ( &pExtension->m_QueuedPacketsList );

	NdisAcquireSpinLock ( &pExtension->m_QueuedPacketsLock );
#ifdef _WINNT
	pExtension->m_pPacketEvent = NULL;
#else
	pExtension->m_hPacketEvent = NULL;
#endif //_WINNT
	NdisReleaseSpinLock ( &pExtension->m_QueuedPacketsLock );

	// Set default start up filtering mode for the interface
	pExtension->m_AdapterMode = g_dwStartupMode;
}

VOID
	EXT_FreeNdisrdExtension (
		PNDISRD_ADAPTER_EXTENSION pExtension
		)
{
	PINTERMEDIATE_BUFFER pBuffer = NULL;

	NdisAcquireSpinLock ( &pExtension->m_QueuedPacketsLock );

	while ( !IsListEmpty(&pExtension->m_QueuedPacketsList) )
	{
		pBuffer = (PINTERMEDIATE_BUFFER)RemoveHeadList(&pExtension->m_QueuedPacketsList);
//		IB_FreeIntermediateBuffer ( pBuffer );
	}

	pExtension->m_QueueSize = 0;

#ifdef _WINNT
	if(pExtension->m_pPacketEvent)
		ObDereferenceObject (pExtension->m_pPacketEvent);
	
	pExtension->m_pPacketEvent = NULL;
#else
	if(pExtension->m_hPacketEvent)
		VWIN32_CloseVxDHandle (pExtension->m_hPacketEvent);
	
	pExtension->m_hPacketEvent = NULL;
#endif //_WINNT

	NdisReleaseSpinLock ( &pExtension->m_QueuedPacketsLock );
}

VOID
	EXT_ResetNdisrdExtension (
		PNDISRD_ADAPTER_EXTENSION pExtension
		)
{
	PINTERMEDIATE_BUFFER pBuffer = NULL;

	pExtension->m_AdapterMode = 0;

	NdisAcquireSpinLock ( &pExtension->m_QueuedPacketsLock );

	while ( !IsListEmpty(&pExtension->m_QueuedPacketsList) )
	{
		pBuffer = (PINTERMEDIATE_BUFFER)RemoveHeadList(&pExtension->m_QueuedPacketsList);
//		IB_FreeIntermediateBuffer ( pBuffer );
	}

	pExtension->m_QueueSize = 0;
	
#ifdef _WINNT
	if(pExtension->m_pPacketEvent)
		ObDereferenceObject (pExtension->m_pPacketEvent);
	
	pExtension->m_pPacketEvent = NULL;
#else
	if(pExtension->m_hPacketEvent)
		VWIN32_CloseVxDHandle (pExtension->m_hPacketEvent);
	
	pExtension->m_hPacketEvent = NULL;
#endif //_WINNT
	NdisReleaseSpinLock ( &pExtension->m_QueuedPacketsLock );
}


VOID
	EXT_ResetRDForAllAdapters (
	)
{
#ifndef _NDISRD_IM
	PPROTOCOL_ENTRY pProto;
	PADAPTER_ENTRY	pAdapter;

	// Walk the list of hooked protocols
	pProto = ( PPROTOCOL_ENTRY ) g_ProtocolList.Flink;
	while ( pProto != ( PPROTOCOL_ENTRY ) &g_ProtocolList )
	{
		pAdapter = ( PADAPTER_ENTRY ) pProto -> m_AdaptersList.Flink;
		
		// For each protocol walk the list of binded adapters 
		while ( pAdapter != ( PADAPTER_ENTRY ) &pProto -> m_AdaptersList )
		{
			EXT_ResetNdisrdExtension ( &pAdapter->m_NdisrdExtension );

			pAdapter = (PADAPTER_ENTRY) pAdapter->m_qLink.Flink;
		}
		pProto = ( PPROTOCOL_ENTRY ) pProto->m_qLink.Flink;
	}

#else
	PADAPT            *ppCursor;

	NdisAcquireSpinLock(&GlobalLock);

    for (ppCursor = &pAdaptList; *ppCursor != NULL; ppCursor = &(*ppCursor)->Next)
    {
        EXT_ResetNdisrdExtension ( &(*ppCursor)->m_NdisrdExtension );
    }

    NdisReleaseSpinLock(&GlobalLock);

#endif
}