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

NTSTATUS
FilterDispatch(
    IN PDEVICE_OBJECT       pDeviceObject,
    IN PIRP                 Irp
    )
{
	NTSTATUS status;	

	if( Irp->RequestorMode == KernelMode )
		status = STATUS_SUCCESS;
	else
		status = STATUS_UNSUCCESSFUL;

    Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

	dprintf( "===> FilterDispatch. status = %X\n", status );

    return status;
}
                
NTSTATUS
FilterDeviceIoControl(
    IN PDEVICE_OBJECT        pDeviceObject,
    IN PIRP                  Irp
    )
{
    NTSTATUS status;
	ULONG uInfo;
    PIO_STACK_LOCATION pIrpSp = IoGetCurrentIrpStackLocation(Irp);

    switch (pIrpSp->Parameters.DeviceIoControl.IoControlCode)
    {
	case IOCTL_OBTAIN_FILTER_FUNCTIONS:

		dprintf( "IoControlCode is IOCTL_OBTAIN_FILTER_FUNCTIONS\n");

		uInfo = sizeof(FLT_FUNCTIONS);

		if( pIrpSp->Parameters.DeviceIoControl.OutputBufferLength != sizeof(FLT_FUNCTIONS) ||
			Irp->AssociatedIrp.SystemBuffer == NULL )
		{
			status = STATUS_INVALID_PARAMETER;
		}
		else
		{
			((PFLT_FUNCTIONS)Irp->AssociatedIrp.SystemBuffer)->m_pFltRegisterCallbacks = FLTRegisterCallbacks;
			((PFLT_FUNCTIONS)Irp->AssociatedIrp.SystemBuffer)->m_pFltDeregisterCallbacks = FLTDeregisterCallbacks;
			((PFLT_FUNCTIONS)Irp->AssociatedIrp.SystemBuffer)->m_pFltInjectRequest = FLTInjectRequest;
			((PFLT_FUNCTIONS)Irp->AssociatedIrp.SystemBuffer)->m_pFltCloneRequest = FLTCloneRequest;
			((PFLT_FUNCTIONS)Irp->AssociatedIrp.SystemBuffer)->m_pFltFreeCloneRequest = FLTFreeCloneRequest;
			status = STATUS_SUCCESS;
		}		
		break;
             
    default:
		uInfo = 0;
		status = STATUS_NOT_IMPLEMENTED;
		break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = uInfo;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

	dprintf( "===> FilterDeviceIoControl. status = %X\n", status);

    return status;
}