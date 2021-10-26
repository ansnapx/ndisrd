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

#ifndef __REQUEST_H__
#define __REQUEST_H__

//
// Wrapper around NDIS_REQUEST for queueing 
//
typedef struct _EXT_NDIS_REQUEST
{
	LIST_ENTRY		m_qLink;
	NDIS_REQUEST	m_NdisRequest;

	BOOL			m_bIsUserRequest;
	PIRP			m_Irp;

} EXT_NDIS_REQUEST, *PEXT_NDIS_REQUEST;

PEXT_NDIS_REQUEST	RF_AllocateRequest ();

VOID				RF_FreeRequest ( 
						PEXT_NDIS_REQUEST pRequest
						);

VOID				RF_FilterRequestCompleteHandler (
						PADAPTER_ENTRY pAdapter,
						PNDIS_REQUEST NdisRequest
						);

#endif