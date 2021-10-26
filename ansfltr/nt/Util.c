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

//***********************************************************************************
// Name: MF_FindAdapterByBindingHandle
//
// Description: Routine finds protocol and adapter entrys by adapter binding handle
//
// Return value: pointer to adapter entry if found, NULL otherwise
//
// Parameters: 
// NdisBindingHandle	- adapter binding handle
// pProtocol - pointer to protocol entry will be returned here if found
//
// NOTE: None
// **********************************************************************************

PADAPTER_ENTRY MF_FindAdapterByBindingHandle (
					NDIS_HANDLE NdisBindingHandle,
					PPROTOCOL_ENTRY *pProtocol 
					)
{
	*pProtocol = NULL;
	return (PADAPTER_ENTRY)NdisBindingHandle;
}

PADAPTER_ENTRY MF_FindAdapterByUserHandle (
					NDIS_HANDLE UserHandle,
					PPROTOCOL_ENTRY *pProtocol 
					)
{
	PADAPT            *ppCursor;

	NdisAcquireSpinLock(&GlobalLock);

    for (ppCursor = &pAdaptList; *ppCursor != NULL; ppCursor = &(*ppCursor)->Next)
    {
        if((NDIS_HANDLE)(*ppCursor) == UserHandle )
		{
			*pProtocol = NULL;
			NdisReleaseSpinLock(&GlobalLock);
			return (PADAPTER_ENTRY)*ppCursor;
		}
    }

    NdisReleaseSpinLock(&GlobalLock);

	return NULL;
}

//VOID MF_SetAdapterIPByTCPDeviceName ( 
//					PUNICODE_STRING TCPDeviceName,
//					DWORD dwAddr
//					)
//{
//	PADAPT            *ppCursor;
//
//	NdisAcquireSpinLock(&GlobalLock);
//
//    for (ppCursor = &pAdaptList; *ppCursor != NULL; ppCursor = &(*ppCursor)->Next)
//    {
//		dprintf("PNP_SetAdapterIPByTCPDeviceName() %d.%d.%d.%d",
//			PRINT_IP_ADDR((*ppCursor)->m_LastIndicatedIP));
//
//		if ( ntCompareDeviceName(TCPDeviceName, (PUNICODE_STRING)&((*ppCursor) ->m_AdapterName) ))
//		{
//			(*ppCursor)->m_LastIndicatedIP = dwAddr;
//			NdisReleaseSpinLock(&GlobalLock);
//			return;
//		}
//    }
//
//	NdisReleaseSpinLock(&GlobalLock);
//}


//***********************************************************************************
// Name: UF_FreePacketAndBuffers
//
// Description: Routine frees packet associated resources
//
// Return value: None
//
// Parameters: 
// pHKPacket	- pointer onto packet
//
// NOTE: None
// **********************************************************************************

VOID UF_FreePacketAndBuffers ( PNDISHK_PACKET pHKPacket )
{
   UINT           nDataSize, nBufferCount;
   PNDIS_BUFFER   pBuffer;

   // Sanity Checks
   if( !pHKPacket )
   {
      return;
   }

   // Query our packet
   NdisQueryPacket(
      (PNDIS_PACKET )pHKPacket,
      (PUINT )NULL,
      (PUINT )&nBufferCount,
      &pBuffer,
      &nDataSize
      );

   // Free all of the packet's buffers
   while( nBufferCount-- > 0L )
   {
      NdisUnchainBufferAtFront ( (PNDIS_PACKET ) pHKPacket, &pBuffer );
      NdisFreeBuffer ( pBuffer );
   }

   // Recycle the packet
   NdisReinitializePacket ( (PNDIS_PACKET ) pHKPacket );

   // And free the packet
   NdisFreePacket ( ( PNDIS_PACKET ) pHKPacket );
}

//***********************************************************************************
// Name: UF_ReadOnPacket
//
// Description: Routine copyes data from packet into caller supplyed buffer
//
// Return value: None
//
// Parameters: 
// Packet	- pointer onto packet
// lpBuffer - buffer there packet data should be stored
// nNumberOfBytesToRead - number of bytes to copy
// nOffset - offset to start copying (Starting With MAC Header)
// lpNumberOfBytesRead - caller supplied variable to store number of read bytes
//
// NOTE: None
// **********************************************************************************

VOID UF_ReadOnPacket(
   PNDIS_PACKET Packet,
   PUCHAR lpBuffer,
   ULONG nNumberOfBytesToRead,
   ULONG nOffset,                
   PULONG lpNumberOfBytesRead
   )
{
   PNDIS_BUFFER    CurrentBuffer;
   UINT            nBufferCount, TotalPacketLength;
   PUCHAR          VirtualAddress;
   UINT            CurrentLength, CurrentOffset;
   UINT            AmountToMove;

   *lpNumberOfBytesRead = 0;
   if (!nNumberOfBytesToRead)
      return;

   // Query packet
   NdisQueryPacket(
      (PNDIS_PACKET )Packet,
      (PUINT )NULL,           // Physical Buffer Count
      (PUINT )&nBufferCount,  // Buffer Count
      &CurrentBuffer,         // First Buffer
      &TotalPacketLength      // TotalPacketLength
      );

   // Query the first buffer
   /*NdisQueryBuffer(
      CurrentBuffer,
      &VirtualAddress,
      &CurrentLength
      );*/

   NdisQueryBufferSafe(
	  CurrentBuffer,
      &VirtualAddress,
      &CurrentLength,
	  NormalPagePriority);

   CurrentOffset = 0;

   while( nOffset || nNumberOfBytesToRead )
   {
      while( !CurrentLength )
      {
         NdisGetNextBuffer(
            CurrentBuffer,
            &CurrentBuffer
            );

         // If we've reached the end of the packet.  We return with what
         // we've done so far (which must be shorter than requested).
         if (!CurrentBuffer)
            return;

        /* NdisQueryBuffer(
            CurrentBuffer,
            &VirtualAddress,
            &CurrentLength
            );*/

		 NdisQueryBufferSafe(
				  CurrentBuffer,
				  &VirtualAddress,
				  &CurrentLength,
				  NormalPagePriority);

         CurrentOffset = 0;
      }

      if( nOffset )
      {
         // Compute how much data to move from this fragment
         if( CurrentLength > nOffset )
            CurrentOffset = nOffset;
         else
            CurrentOffset = CurrentLength;

         nOffset -= CurrentOffset;
         CurrentLength -= CurrentOffset;
      }

      if( nOffset )
      {
         CurrentLength = 0;
         continue;
      }

      if( !CurrentLength )
      {
         continue;
      }

      // Compute how much data to move from this fragment
      if (CurrentLength > nNumberOfBytesToRead)
         AmountToMove = nNumberOfBytesToRead;
      else
         AmountToMove = CurrentLength;

      // Copy the data.
      NdisMoveMemory(
         lpBuffer,
         &VirtualAddress[ CurrentOffset ],
         AmountToMove
         );

      // Update destination pointer
      lpBuffer += AmountToMove;

      // Update counters
      *lpNumberOfBytesRead +=AmountToMove;
      nNumberOfBytesToRead -=AmountToMove;
      CurrentLength = 0;
   }
}