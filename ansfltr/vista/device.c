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

//
// Global variables
//

static NDIS_HANDLE g_hDevice;

//
// Functions
//

NDIS_STATUS 
RegisterFilterDevice()
{
    NDIS_STATUS status;
    UNICODE_STRING usDeviceName;
    UNICODE_STRING usDeviceLinkUnicodeString;
	PDRIVER_DISPATCH DispatchTable[IRP_MJ_MAXIMUM_FUNCTION+1] = {0};
	NDIS_DEVICE_OBJECT_ATTRIBUTES DeviceAttribute = {0};
	PDEVICE_OBJECT pDeviceObject;
    
    DispatchTable[IRP_MJ_CREATE] = FilterDispatch;
    DispatchTable[IRP_MJ_CLEANUP] = FilterDispatch;
    DispatchTable[IRP_MJ_CLOSE] = FilterDispatch;
	DispatchTable[IRP_MJ_INTERNAL_DEVICE_CONTROL] = FilterDeviceIoControl;
    
    NdisInitUnicodeString(&usDeviceName, FLT_DEVICE_NAME);
	NdisInitUnicodeString(&usDeviceLinkUnicodeString, FLT_DEVICE_LINK);
    
    //
    // Create a device object and register our dispatch handlers
    //
    DeviceAttribute.Header.Type = NDIS_OBJECT_TYPE_DEVICE_OBJECT_ATTRIBUTES;
    DeviceAttribute.Header.Revision = NDIS_DEVICE_OBJECT_ATTRIBUTES_REVISION_1;
    DeviceAttribute.Header.Size = sizeof(NDIS_DEVICE_OBJECT_ATTRIBUTES);
    
    DeviceAttribute.DeviceName = &usDeviceName;
    DeviceAttribute.SymbolicName = &usDeviceLinkUnicodeString;
    DeviceAttribute.MajorFunctions = &DispatchTable[0];
    
    status = NdisRegisterDeviceEx(
                g_hDriver,
                &DeviceAttribute,
                &pDeviceObject,
                &g_hDevice
                );   
   
    if (status == NDIS_STATUS_SUCCESS)
		pDeviceObject->Flags |= DO_BUFFERED_IO;
 
    dprintf( "<=== RegisterFilterDevice: %x\n", status);
        
    return status;
}

VOID
DeregisterFilterDevice()
{
    NdisDeregisterDeviceEx(g_hDevice);
}