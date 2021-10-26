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
// Name: RF_AllocateRequest
//
// Description: Routine allocates new EXT_NDIS_REQUEST structure 
//
// Return value: pointer to allocated structure
//
// Parameters: None
//
// NOTE: None
// **********************************************************************************

PEXT_NDIS_REQUEST RF_AllocateRequest ()
{
	PEXT_NDIS_REQUEST		pRequest;
	NDIS_STATUS				status;

	// Allocate structure from nonpaged memory
	status = NdisAllocateMemoryWithTag ( 
						&pRequest, 
						sizeof (EXT_NDIS_REQUEST), 
						'rtpR'
						);
	
	// If succesfully allocated return pointer to it
	if ( status == NDIS_STATUS_SUCCESS )
	{
		NdisZeroMemory ( pRequest, sizeof (EXT_NDIS_REQUEST));
		return pRequest;
	}
	else
	{
		return NULL;
	}
}

//***********************************************************************************
// Name: RF_FreeRequest
//
// Description: Routine deallocates EXT_NDIS_REQUEST structure 
//
// Return value: None
//
// Parameters: 
// pRequest - ponter to request structure to free
//
// NOTE: None
// **********************************************************************************

VOID RF_FreeRequest ( PEXT_NDIS_REQUEST pRequest )
{
	// Sanity check
	if ( pRequest )
	{
		NdisFreeMemory ( pRequest, sizeof (EXT_NDIS_REQUEST), 0);
	}
}

//***********************************************************************************
// Name: RF_FilterRequestCompleteHandler
//
// Description: Routine for request postprocessing, can be used for request results
// modification 
//
// Return value: None
//
// Parameters: 
// pAdapter		- ponter adapter entry request associted with
// NdisRequest	- request pointer
//
// NOTE: None
// **********************************************************************************

VOID RF_FilterRequestCompleteHandler ( 
		PADAPTER_ENTRY pAdapter,
		PNDIS_REQUEST NdisRequest
		)
{
	PUCHAR	InformationBuffer;
	PUSHORT pMTU;

//	DbgPrint ( "RF_FilterRequestCompleteHandler\n" );

	// We manually set maximum MTU as MAX_802_3_LENGTH bytes to avoid situation with 
	// 9000 byte packets like on some Gigabit Ethernet adapters possible.
	// An example such protocols, like IPSEC require adding information into packet
	// so lower MTU if you will need this
	if ( NdisRequest -> DATA.QUERY_INFORMATION.Oid == OID_GEN_MAXIMUM_FRAME_SIZE )
	{
		pMTU = (PUSHORT) NdisRequest -> DATA.QUERY_INFORMATION.InformationBuffer;
		if ( *pMTU > MAX_802_3_LENGTH )
			*pMTU = MAX_802_3_LENGTH;

		pAdapter -> m_usMTU = *pMTU; // Save MTU info in adapter relative storage
		*pMTU = *pMTU - (USHORT)g_dwMTUDecrement;
	}

	// Process OID_GEN_MAXIMUM_TOTAL_SIZE also, this is the same value aas returned 
	// by OID_GEN_MAXIMUM_FRAME_SIZE plus packet header length
	if ( NdisRequest -> DATA.QUERY_INFORMATION.Oid == OID_GEN_MAXIMUM_TOTAL_SIZE )
	{
		pMTU = (PUSHORT) NdisRequest -> DATA.QUERY_INFORMATION.InformationBuffer;
		if ( *pMTU > MAX_ETHER_SIZE )
			*pMTU = MAX_ETHER_SIZE;

		*pMTU = *pMTU - (USHORT)g_dwMTUDecrement;
	}

	// Save request result for OID_802_3_CURRENT_ADDRESS
	if ( NdisRequest -> DATA.QUERY_INFORMATION.Oid == OID_802_3_CURRENT_ADDRESS)
	{
		InformationBuffer = (PUCHAR) NdisRequest -> DATA.QUERY_INFORMATION.InformationBuffer;
		NdisMoveMemory ( 
			pAdapter ->m_CurrentAddress,
			InformationBuffer,
			ETHER_ADDR_LENGTH
			);
	}

}