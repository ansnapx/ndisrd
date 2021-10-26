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
#ifndef __EXTENS_H__
#define __EXTENS_H__

typedef
struct _NDISRD_ADAPTER_EXTENSION
{
	NDIS_SPIN_LOCK	m_QueuedPacketsLock;
	LIST_ENTRY		m_QueuedPacketsList;
	DWORD			m_AdapterMode;
	DWORD			m_QueueSize;
#ifdef _WINNT
	PKEVENT			m_pPacketEvent;
#else
	HANDLE			m_hPacketEvent;
#endif //_WINNT
} 
NDISRD_ADAPTER_EXTENSION, *PNDISRD_ADAPTER_EXTENSION;

VOID
	EXT_InitializeNdisrdExtension (
		PNDISRD_ADAPTER_EXTENSION pExtension
		);

VOID
	EXT_FreeNdisrdExtension (
		PNDISRD_ADAPTER_EXTENSION pExtension
		);

VOID
	EXT_ResetNdisrdExtension (
		PNDISRD_ADAPTER_EXTENSION pExtension
		);

VOID
	EXT_ResetRDForAllAdapters (
	);

#endif //__EXTENS_H__