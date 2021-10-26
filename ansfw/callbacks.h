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

typedef struct _FW_PID_REQUEST
{
	DWORD	m_Flags;
	UINT64	m_Pid;
	WORD	m_IpProto;
	DWORD   m_LocalAddr;
	DWORD   m_LocalPort;
	DWORD   m_RemoteAddr;
	DWORD   m_RemotePort;
}FW_PID_REQUEST, *PFW_PID_REQUEST;

NTSTATUS RegisterFilterCallbacks();
NTSTATUS DeregisterFilterCallbacks();

BOOLEAN IsClientConnected();
VOID GetPacketPid( PFW_REQUEST pFwRequest );

NTSTATUS ProcessRequest(
	PFW_INSTANCE pFwInstance,
	PFW_REQUEST pFwRequest 
	);

extern NPAGED_LOOKASIDE_LIST g_FwRequestsLookasideList;