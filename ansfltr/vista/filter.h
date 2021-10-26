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

//
// Global variables
// 

extern NDIS_HANDLE         g_hDriver;

//
// Prototypes
//

NTSTATUS 
RegisterFilterDriver( 
	PDRIVER_OBJECT pDriverObject 
	);

VOID 
DeregisterFilterDriver();

NTSTATUS 
SendPacket( 	
	IN PFLT_PACKET pPacket
	);

VOID FreeNdisData( PFLT_PACKET pPacket );

VOID MF_SetAdapterIPByTCPDeviceName ( 
					PUNICODE_STRING TCPDeviceName,
					DWORD dwAddr
					);