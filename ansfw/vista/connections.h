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

VOID InitializeConnections( );
VOID UninitializeConnections( );
PVOID AddConnection( UINT64 pid, WORD IpProto, DWORD LocalAddr, WORD LocalPort, DWORD RemoteAddr, WORD RemotePort );
PVOID GetConnection( WORD IpProto, DWORD LocalAddr, WORD LocalPort, DWORD RemoteAddr, WORD RemotePort );
VOID RemoveConnection( PVOID pConnection  );
VOID GetPidFromLocalInfo( PFW_PID_REQUEST request );
//UINT64 GetConnectionPid( WORD IpProto, DWORD LocalAddr, WORD LocalPort );
