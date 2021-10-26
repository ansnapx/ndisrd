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

NTSTATUS 
SendPacket( 	
	IN PFLT_PACKET pPacket
	)
{
	if( pPacket->m_pAdapter==NULL )
		return STATUS_UNSUCCESSFUL;

	if( pPacket->m_Direction == PHYSICAL_DIRECTION_IN )
	{
		return UF_SendPacketToProtocol(pPacket);
	}	
	else
	{
		return UF_SendPacketToAdapter(pPacket);
	}
}


//***********************************************************************************
// Name: UF_SendPacketToProtocol
//
// Description: Routine indicates packet to prtocol
//
// Return value: status of the operation
//
// Parameters: 
// NdisBindingHandle	- adapter binding handle
// Buffer				- pointer onto packet data
// BufferSize			- sizeof data
//
// NOTE: None
// **********************************************************************************

NTSTATUS UF_SendPacketToProtocol (
			PFLT_PACKET pPacket
			)
{
	NDIS_STATUS		nStatus = STATUS_SUCCESS;
	PUCHAR			pHeader = (PUCHAR) pPacket->m_pBuffer;
	UINT			iHeaderSize	= MHdrSize; 
	PUCHAR			pLookAheadBuffer = pHeader + MHdrSize;
	UINT			iLookAheadBufferSize = pPacket->m_BufferSize - MHdrSize;
	PNDISHK_PACKET	pHKPacket;
	PNDIS_BUFFER	pHKBuffer;
	PNDIS_PACKET_OOB_DATA pOOBData = NULL;
	ether_header_ptr	pEth = (ether_header_ptr)pPacket->m_pBuffer;
	PADAPTER_ENTRY pAdapter = pPacket->m_pAdapter;

	NdisAcquireSpinLock(&pAdapter->Lock);

	if ( pAdapter && (pAdapter->MPDeviceState == NdisDeviceStateD0)&& (pAdapter->UnbindingInProcess == FALSE))
	{
		NdisReleaseSpinLock(&pAdapter->Lock);

		//if (!((pPacket->m_BufferSize <= MAX_ETHER_SIZE))) //Sanity check
		//	return STATUS_BUFFER_OVERFLOW;

		// Allocate packet from adapter associated packet pool

		NdisAllocatePacket ( 
			&nStatus,
			(PNDIS_PACKET*)(&pHKPacket),
			pAdapter -> m_hPacketPool
			);

		if( nStatus != NDIS_STATUS_SUCCESS) return STATUS_INSUFFICIENT_RESOURCES;

		NdisAllocateBuffer ( 
			&nStatus, 
			&pHKBuffer, 
			pAdapter->m_hBufferPool, 
			(PVOID) pPacket->m_pBuffer, 
			pPacket->m_BufferSize
			);

		//if(pPacket->m_BufferSize<=MAX_ETHER_SIZE)
		//{
		//	// Allocate buffer from adapter associated buffer pool
		//	NdisAllocateBuffer ( 
		//		&nStatus, 
		//		&pHKBuffer, 
		//		pAdapter->m_hBufferPool, 
		//		(PVOID) pHKPacket->m_Reserved.m_IBuffer, 
		//		pPacket->m_BufferSize
		//		);
		//}
		//else
		//{
		//	// Use existing buffer
		//	NdisAllocateBuffer ( 
		//		&nStatus, 
		//		&pHKBuffer, 
		//		pAdapter->m_hBufferPool, 
		//		pPacket->m_pBuffer,
		//		pPacket->m_BufferSize
		//		);
		//}

		if (nStatus != NDIS_STATUS_SUCCESS)
		{
			// Release resources in case of unsuccess
			UF_FreePacketAndBuffers ( pHKPacket );
			return STATUS_INSUFFICIENT_RESOURCES;
		}
			
		// Chain our buffer to our packet
		NdisChainBufferAtFront (
			(PNDIS_PACKET )pHKPacket, 
			pHKBuffer 
			);

		//if(pPacket->m_BufferSize <= MAX_ETHER_SIZE)
		//{
		//	//Copy our data to buffer
		//	NdisMoveMemory ( 
		//		pHKBuffer -> MappedSystemVa,
		//		pPacket->m_pBuffer,
		//		pPacket->m_BufferSize
		//		);
		//}

		pHKPacket->m_Packet.Private.Flags = pPacket->m_Flags;

		pOOBData = NDIS_OOB_DATA_FROM_PACKET ( (PNDIS_PACKET)pHKPacket );
		pOOBData->HeaderSize = MHdrSize;
		pOOBData->Status = NDIS_STATUS_RESOURCES;

		// Indicate packet to protocol
		NdisMIndicateReceivePacket(pAdapter->MiniportHandle, &(PNDIS_PACKET)pHKPacket, 1);

		UF_FreePacketAndBuffers ( pHKPacket );
		
		return nStatus;
	}
	else
		NdisReleaseSpinLock(&pAdapter->Lock);

	return  STATUS_NOT_FOUND;
}

//***********************************************************************************
// Name: UF_SendPacketToAdapter
//
// Description: Routine puts packet on the network
//
// Return value: status of the operation
//
// Parameters: 
// NdisBindingHandle	- adapter binding handle
// Buffer				- pointer onto packet data
// BufferSize			- sizeof data
//
// NOTE: None
// **********************************************************************************

NTSTATUS UF_SendPacketToAdapter (
			PFLT_PACKET pPacket
			)
{
	NDIS_STATUS			nStatus;
	PNDISHK_PACKET		pHKPacket;
	PNDIS_BUFFER		pHKBuffer;
	SEND_HANDLER		SendHandler;
	PADAPTER_ENTRY pAdapter = pPacket->m_pAdapter;

	NdisAcquireSpinLock(&pAdapter->Lock);

	if ( pAdapter && (pAdapter->MPDeviceState == NdisDeviceStateD0)&& (pAdapter->UnbindingInProcess == FALSE))
	{
		NdisReleaseSpinLock(&pAdapter->Lock);

		if (!((pPacket->m_BufferSize <= MAX_ETHER_SIZE) && (pPacket->m_BufferSize > 0))) //Sanity check
			return STATUS_BUFFER_OVERFLOW;

		// Allocate packet from adapter associated packet pool
		NdisAllocatePacket ( 
			&nStatus,
			(PNDIS_PACKET*)(&pHKPacket),
			pAdapter -> m_hPacketPool
			);

		if( nStatus != NDIS_STATUS_SUCCESS) return STATUS_INSUFFICIENT_RESOURCES;

		// Allocate buffer from adapter associated buffer pool
		NdisAllocateBuffer ( 
			&nStatus, 
			&pHKBuffer, 
			pAdapter->m_hBufferPool, 
			(PVOID) pHKPacket->m_Reserved.m_IBuffer, 
			pPacket->m_BufferSize
			);

		if (nStatus != NDIS_STATUS_SUCCESS)
		{
			// Release resources in case of unsuccess
			UF_FreePacketAndBuffers ( pHKPacket );
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		
		// Chain our buffer to our packet
		NdisChainBufferAtFront (
			(PNDIS_PACKET )pHKPacket, 
			pHKBuffer 
			);

		//Copy our data to buffer
		NdisMoveMemory ( 
			pHKBuffer -> MappedSystemVa,
			pPacket->m_pBuffer,
			pPacket->m_BufferSize
			);

		pHKPacket->m_Packet.Private.Flags = pPacket->m_Flags;

		NdisSend(&nStatus,
                 pAdapter->BindingHandle,
                 (PNDIS_PACKET) pHKPacket);


        if (nStatus != NDIS_STATUS_PENDING)
        {
            UF_FreePacketAndBuffers(pHKPacket);
			return nStatus;
        }

		// return SUCCESS even if PENDING was returned
		return STATUS_SUCCESS;
	}
	else
		NdisReleaseSpinLock(&pAdapter->Lock);

	return STATUS_NOT_FOUND;
}

////***********************************************************************************
//// Name: UF_SendPacketToProtocol
////
//// Description: Routine indicates packet to prtocol
////
//// Return value: status of the operation
////
//// Parameters: 
//// NdisBindingHandle	- adapter binding handle
//// Buffer				- pointer onto packet data
//// BufferSize			- sizeof data
////
//// NOTE: None
//// **********************************************************************************
//
//NTSTATUS UF_SendPacketToProtocol (
//			PFLT_PACKET pPacket
//			)
//{
//	NDIS_STATUS		nStatus = STATUS_SUCCESS;
//	PUCHAR			pHeader = (PUCHAR) pPacket->m_pBuffer;
//	UINT			iHeaderSize	= MHdrSize; 
//	PUCHAR			pLookAheadBuffer = pHeader + MHdrSize;
//	UINT			iLookAheadBufferSize = pPacket->m_BufferSize - MHdrSize;
//	PNDISHK_PACKET	pHKPacket;
//	PNDIS_BUFFER	pHKBuffer;
//	PNDIS_PACKET_OOB_DATA pOOBData = NULL;
//	ether_header_ptr	pEth = (ether_header_ptr)pPacket->m_pBuffer;
//	PADAPTER_ENTRY pAdapter = pPacket->m_pAdapter;
//
//	NdisAcquireSpinLock(&pAdapter->Lock);
//
//	if ( pAdapter && (pAdapter->MPDeviceState == NdisDeviceStateD0)&& (pAdapter->UnbindingInProcess == FALSE))
//	{
//		NdisReleaseSpinLock(&pAdapter->Lock);
//
//		//if (!((pPacket->m_BufferSize <= MAX_ETHER_SIZE))) //Sanity check
//		//	return STATUS_BUFFER_OVERFLOW;
//
//		// Allocate packet from adapter associated packet pool
//
//		NdisAllocatePacket ( 
//			&nStatus,
//			(PNDIS_PACKET*)(&pHKPacket),
//			pAdapter -> m_hPacketPool
//			);
//
//		if( nStatus != NDIS_STATUS_SUCCESS) return STATUS_INSUFFICIENT_RESOURCES;
//
//		if(pPacket->m_BufferSize<=MAX_ETHER_SIZE)
//		{
//			// Allocate buffer from adapter associated buffer pool
//			NdisAllocateBuffer ( 
//				&nStatus, 
//				&pHKBuffer, 
//				pAdapter->m_hBufferPool, 
//				(PVOID) pHKPacket->m_Reserved.m_IBuffer, 
//				pPacket->m_BufferSize
//				);
//		}
//		else
//		{
//			// Use existing buffer
//			NdisAllocateBuffer ( 
//				&nStatus, 
//				&pHKBuffer, 
//				pAdapter->m_hBufferPool, 
//				pPacket->m_pBuffer,
//				pPacket->m_BufferSize
//				);
//		}
//
//		if (nStatus != NDIS_STATUS_SUCCESS)
//		{
//			// Release resources in case of unsuccess
//			UF_FreePacketAndBuffers ( pHKPacket );
//			return STATUS_INSUFFICIENT_RESOURCES;
//		}
//			
//		// Chain our buffer to our packet
//		NdisChainBufferAtFront (
//			(PNDIS_PACKET )pHKPacket, 
//			pHKBuffer 
//			);
//
//		if(pPacket->m_BufferSize <= MAX_ETHER_SIZE)
//		{
//			//Copy our data to buffer
//			NdisMoveMemory ( 
//				pHKBuffer -> MappedSystemVa,
//				pPacket->m_pBuffer,
//				pPacket->m_BufferSize
//				);
//		}
//
//		pHKPacket->m_Packet.Private.Flags = pPacket->m_Flags;
//
//		pOOBData = NDIS_OOB_DATA_FROM_PACKET ( (PNDIS_PACKET)pHKPacket );
//		pOOBData->HeaderSize = MHdrSize;
//		pOOBData->Status = NDIS_STATUS_RESOURCES;
//
//		// Indicate packet to protocol
//		NdisMIndicateReceivePacket(pAdapter->MiniportHandle, &(PNDIS_PACKET)pHKPacket, 1);
//
//		UF_FreePacketAndBuffers ( pHKPacket );
//		
//		return nStatus;
//	}
//	else
//		NdisReleaseSpinLock(&pAdapter->Lock);
//
//	return  STATUS_NOT_FOUND;
//}
//
////***********************************************************************************
//// Name: UF_SendPacketToAdapter
////
//// Description: Routine puts packet on the network
////
//// Return value: status of the operation
////
//// Parameters: 
//// NdisBindingHandle	- adapter binding handle
//// Buffer				- pointer onto packet data
//// BufferSize			- sizeof data
////
//// NOTE: None
//// **********************************************************************************
//
//NTSTATUS UF_SendPacketToAdapter (
//			PFLT_PACKET pPacket
//			)
//{
//	NDIS_STATUS			nStatus;
//	PNDISHK_PACKET		pHKPacket;
//	PNDIS_BUFFER		pHKBuffer;
//	SEND_HANDLER		SendHandler;
//	PADAPTER_ENTRY pAdapter = pPacket->m_pAdapter;
//
//	NdisAcquireSpinLock(&pAdapter->Lock);
//
//	if ( pAdapter && (pAdapter->MPDeviceState == NdisDeviceStateD0)&& (pAdapter->UnbindingInProcess == FALSE))
//	{
//		NdisReleaseSpinLock(&pAdapter->Lock);
//
//		if (!((pPacket->m_BufferSize <= MAX_ETHER_SIZE) && (pPacket->m_BufferSize > 0))) //Sanity check
//			return STATUS_BUFFER_OVERFLOW;
//
//		// Allocate packet from adapter associated packet pool
//		NdisAllocatePacket ( 
//			&nStatus,
//			(PNDIS_PACKET*)(&pHKPacket),
//			pAdapter -> m_hPacketPool
//			);
//
//		if( nStatus != NDIS_STATUS_SUCCESS) return STATUS_INSUFFICIENT_RESOURCES;
//
//		// Allocate buffer from adapter associated buffer pool
//		NdisAllocateBuffer ( 
//			&nStatus, 
//			&pHKBuffer, 
//			pAdapter->m_hBufferPool, 
//			(PVOID) pHKPacket->m_Reserved.m_IBuffer, 
//			pPacket->m_BufferSize
//			);
//
//		if (nStatus != NDIS_STATUS_SUCCESS)
//		{
//			// Release resources in case of unsuccess
//			UF_FreePacketAndBuffers ( pHKPacket );
//			return STATUS_INSUFFICIENT_RESOURCES;
//		}
//		
//		// Chain our buffer to our packet
//		NdisChainBufferAtFront (
//			(PNDIS_PACKET )pHKPacket, 
//			pHKBuffer 
//			);
//
//		//Copy our data to buffer
//		NdisMoveMemory ( 
//			pHKBuffer -> MappedSystemVa,
//			pPacket->m_pBuffer,
//			pPacket->m_BufferSize
//			);
//
//		pHKPacket->m_Packet.Private.Flags = pPacket->m_Flags;
//
//		NdisSend(&nStatus,
//                 pAdapter->BindingHandle,
//                 (PNDIS_PACKET) pHKPacket);
//
//
//        if (nStatus != NDIS_STATUS_PENDING)
//        {
//            UF_FreePacketAndBuffers(pHKPacket);
//			return nStatus;
//        }
//
//		// return SUCCESS even if PENDING was returned
//		return STATUS_SUCCESS;
//	}
//	else
//		NdisReleaseSpinLock(&pAdapter->Lock);
//
//	return STATUS_NOT_FOUND;
//}