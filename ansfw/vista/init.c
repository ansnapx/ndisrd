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

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD DriverUnload;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#endif

NTSTATUS
  DllInitialize(
    IN PUNICODE_STRING  RegistryPath
    )
{
	dprintf( "===> DllInitialize\n" );
	return STATUS_UNSUCCESSFUL;
}

//NTSTATUS
//  DllUnload(
//    )
//{
//	dprintf( "===> DllUnload\n" );
//	return STATUS_UNSUCCESSFUL;
//}

NTSTATUS DriverEntry(
	IN  PDRIVER_OBJECT      pDriverObject,
	IN  PUNICODE_STRING     pusRegistryPath
	)
{
	NTSTATUS status;

	dprintf( "===> DriverEntry\n" );

#if DBG
	pDriverObject->DriverUnload = DriverUnload;	
#endif // DBG

	status = RegisterIpMonitor( );
	if( ! NT_SUCCESS(status) )
	{
		return status;
	}

	InitializeConnections();

	status = InitializeWfpFilter( pDriverObject );
	if( ! NT_SUCCESS(status) )
	{
		DeregisterIpMonitor( );
		UninitializeConnections();		
		return status;
	}

	InitializeReassembly();

	status = RegisterFilterCallbacks();
	if( ! NT_SUCCESS(status) )
	{
		DeregisterIpMonitor( );
		UninitializeReassembly();
		UninitializeWfpFilter();
		UninitializeConnections();
		return status;
	}

	return status;
}

VOID DriverUnload(
				  PDRIVER_OBJECT pDriverObject )
{
	dprintf( "===> DriverUnload\n" );

	DeregisterIpMonitor( );
	DeregisterFilterCallbacks();
	UninitializeReassembly();
	UninitializeWfpFilter();
	UninitializeConnections();
}


