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
#pragma once

#define WORKBUFSIZE 65536
#define WORKBUF 65550//65536+14 for ethernet
#define BITSBUFFERLENGHT 1024
#define ETHER_HDR_SIZE 14
#define MAX_WORKBUFFER_COUNT 100

typedef struct _DATA_LIST
{
	LIST_ENTRY Link;	
	DWORD destIP;
	DWORD srcIP;
	RTL_BITMAP  BitMapHeader;
	BYTE bits[BITSBUFFERLENGHT];
	BYTE finalbuf[WORKBUF];
	USHORT totallen;	
	USHORT ID;
	USHORT protocol;
	UCHAR ttl; 
	BOOL bLast;

	LIST_ENTRY m_FragmentsList;

	FW_REQUEST m_FwRequest;
	FLT_PACKET m_FltPacket;

}DATA_LIST,*PDATA_LIST;

VOID InitializeReassembly();
VOID UninitializeReassembly();
VOID ReassemblePacket( PFW_REQUEST pFwRequest );
VOID ruleRecycleDatagramWorkBuffer(PDATA_LIST datagr);