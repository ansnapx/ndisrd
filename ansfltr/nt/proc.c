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

LIST_ENTRY		g_IntermediateBufferPool; // Global intermediate buffer list
NDIS_SPIN_LOCK	g_IntermediateBufferLock;

HANDLE			g_LineupEvent = NULL;
HANDLE			g_AdapterEvent = NULL;

UCHAR			g_BroadcastMAC[ETHER_ADDR_LENGTH] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

//***********************************************************************************
// Name: IB_AllocateIntermediateBufferPool
//
// Description: Allocates intermediate buffer pool (for saving and processing packet
// data)
//
// Return value: status of the operation
//
// Parameters: None
//
// NOTE: None
// **********************************************************************************

//NTSTATUS IB_AllocateIntermediateBufferPool ()
//{
//	NDIS_STATUS				nStatus;
//	PINTERMEDIATE_BUFFER	pBuffer;
//	UINT					counter		= INT_BUF_POOL_SIZE;
//
//	// Initialize buffers list
//	InitializeListHead ( &g_IntermediateBufferPool );
//
//	// Initialize spinlock for list protection (can be accessed from DISPATCH and PASSIVE
//	// IRQLs)
//	NdisAllocateSpinLock ( &g_IntermediateBufferLock );
//
//	// Allocate buffers and insert them into the list
//
//	NdisAcquireSpinLock ( &g_IntermediateBufferLock );
//
//	while ( counter-- )
//	{
//		nStatus = NdisAllocateMemoryWithTag ( 
//			&pBuffer, 
//			sizeof(FLT_PACKET), 
//			'rtpR'
//			);
//
//		if ( nStatus == NDIS_STATUS_SUCCESS )
//		{
//			InsertTailList ( &g_IntermediateBufferPool, &pBuffer->m_qLink );
//		}
//		else
//		{
//			NdisReleaseSpinLock ( &g_IntermediateBufferLock );
//			return nStatus;
//		}
//	}
//
//	NdisReleaseSpinLock ( &g_IntermediateBufferLock );
//
//	return STATUS_SUCCESS;
//}
//
////***********************************************************************************
//// Name: IB_DeallocateIntermediateBufferPool
////
//// Description: Deallocates intermediate buffer pool 
////
//// Return value: NONE
////
//// Parameters: None
////
//// NOTE: None
//// **********************************************************************************
//
//VOID IB_DeallocateIntermediateBufferPool ()
//{
//	PFLT_PACKET	pBuffer;
//
//	NdisAcquireSpinLock ( &g_IntermediateBufferLock );
//	
//	while ( !IsListEmpty(&g_IntermediateBufferPool ))
//	{
//		pBuffer = (PFLT_PACKET)RemoveHeadList(&g_IntermediateBufferPool);
//
//		NdisFreeMemory (
//			pBuffer,
//			sizeof(FLT_PACKET),
//			0
//			);
//	}
//
//	NdisReleaseSpinLock ( &g_IntermediateBufferLock );
//}
//
////***********************************************************************************
//// Name: IB_AllocateIntermediateBuffer
////
//// Description: Allocates intermediate buffer from global buffers list
////
//// Return value: pointer to allocated buffer
////
//// Parameters: None
////
//// NOTE: None
//// **********************************************************************************
//
//PFLT_PACKET IB_AllocateIntermediateBuffer ()
//{
//	PFLT_PACKET pBuffer;
//
//	NdisAcquireSpinLock ( &g_IntermediateBufferLock );
//
//	if ( !IsListEmpty ( &g_IntermediateBufferPool ) )
//	{
//		pBuffer = (PFLT_PACKET) RemoveHeadList ( &g_IntermediateBufferPool );
//		NdisReleaseSpinLock ( &g_IntermediateBufferLock );
//		NdisZeroMemory ( pBuffer, sizeof(FLT_PACKET) );
//		return pBuffer;
//	}
//	
//	if(NdisAllocateMemoryWithTag ( 
//			&pBuffer, 
//			sizeof(PFLT_PACKET), 
//			'rtpR'
//			) == NDIS_STATUS_SUCCESS)
//	{
//		NdisReleaseSpinLock ( &g_IntermediateBufferLock );
//		NdisZeroMemory ( pBuffer, sizeof(FLT_PACKET) );
//		return pBuffer;
//	}
//
//	NdisReleaseSpinLock ( &g_IntermediateBufferLock );
//	return NULL;
//} 
//
////***********************************************************************************
//// Name: IB_FreeIntermediateBuffer
////
//// Description: Returns buffer into the global list
////
//// Return value: None
////
//// Parameters: 
//// pBuffer - pointer to buffer
////
//// NOTE: None
//// **********************************************************************************
//
//VOID IB_FreeIntermediateBuffer ( PINTERMEDIATE_BUFFER pBuffer )
//{
//	// Sanity check
//	if ( pBuffer )
//	{
//		NdisAcquireSpinLock ( &g_IntermediateBufferLock );
//		InsertTailList ( &g_IntermediateBufferPool, &pBuffer->m_qLink );
//		NdisReleaseSpinLock ( &g_IntermediateBufferLock );
//	}
//}

VOID FreeNdisData( PFLT_PACKET pClonePacket )
{
	NdisFreeMemory( pClonePacket, 0, 0 );
}