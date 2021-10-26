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
#ifndef __ANSHLP_H__
#define __ANSHLP_H__

//
// "Borrow" Windows 2000 DDK IoCopyCurrentIrpStackLocationToNext MACRO
//
#ifndef IoCopyCurrentIrpStackLocationToNext
#define IoCopyCurrentIrpStackLocationToNext( Irp ) { \
    PIO_STACK_LOCATION irpSp; \
    PIO_STACK_LOCATION nextIrpSp; \
    irpSp = IoGetCurrentIrpStackLocation( (Irp) ); \
    nextIrpSp = IoGetNextIrpStackLocation( (Irp) ); \
    RtlCopyMemory( nextIrpSp, irpSp, FIELD_OFFSET(IO_STACK_LOCATION, CompletionRoutine)); \
    nextIrpSp->Control = 0; }
#endif // IoCopyCurrentIrpStackLocationToNext

//
// "Borrow" Windows 2000 DDK IoSkipCurrentIrpStackLocation MACRO
//
#ifndef IoSkipCurrentIrpStackLocation
#define IoSkipCurrentIrpStackLocation( Irp ) { \
    (Irp)->CurrentLocation++; \
    (Irp)->Tail.Overlay.CurrentStackLocation++; }
#endif // IoSkipCurrentIrpStackLocation



// TCP/UDP/ Device Names
#define DD_TCP_DEVICE_NAME      L"\\Device\\Tcp"
#define DD_UDP_DEVICE_NAME      L"\\Device\\Udp"
#define DD_IP_DEVICE_NAME       L"\\Device\\Ip"
#define DD_RAWIP_DEVICE_NAME    L"\\Device\\RawIp"
#define DD_MCAST_DEVICE_NAME    L"\\Device\\MULTICAST"

// TCP/UDP/ Filter Device Names
#define TDIH_TCP_DEVICE_NAME        L"\\Device\\ANSTcpFlt"
#define TDIH_UDP_DEVICE_NAME        L"\\Device\\ANSUdpFlt"
#define TDIH_IP_DEVICE_NAME         L"\\Device\\ANSIpFlt"
#define TDIH_RAWIP_DEVICE_NAME      L"\\Device\\ANSRawFlt"
#define TDIH_MCAST_DEVICE_NAME      L"\\Device\\ANSMCastFlt"

#define TDIH_W32API_DEVICE_NAME     L"\\Device\\anshlp"
#define TDIH_W32API_DOS_DEVICE_NAME L"\\DosDevices\\anshlp"
/*
 * Each structure has a unique "node type" or signature associated with it
 */
#define   TDIH_NODE_TYPE_GLOBAL_DATA            (0xF8267A83)
#define   TDIH_NODE_TYPE_TCP_FILTER_DEVICE      (0xF8267A6F)
#define   TDIH_NODE_TYPE_UDP_FILTER_DEVICE      (0xF8267AF0)
#define   TDIH_NODE_TYPE_IP_FILTER_DEVICE       (0xF8267A16)
#define   TDIH_NODE_TYPE_RAWIP_FILTER_DEVICE    (0xF8267A7E)
#define   TDIH_NODE_TYPE_MCAST_FILTER_DEVICE    (0xF8267AFF)
#define   TDIH_NODE_TYPE_W32API_DEVICE          (0xF8267A9B)

#define FILTER_ICMP_CODE 0x00120000
/**
 * Every structure has a node type, and a node size associated with it.
 * The node type serves as a signature field. The size is used for
 * consistency checking ...
 */
typedef struct _TDI_NODE_ID {
   ULONG    NodeType;         // a 32 bit identifier for the structure
   ULONG    NodeSize;         // computed as sizeof(structure)
} TDI_NODE_ID, * PTDI_NODE_ID;

typedef struct _TDI_DEV_EXT
{
   // A signature (including device extension size).
   TDI_NODE_ID			   NodeIdentifier;

   // For convenience, a back ptr to the device object that contains this
   // extension.
   PDEVICE_OBJECT          pFilterDeviceObject; // Our Device Object

   LIST_ENTRY              NextDeviceObject;

   // See Flag definitions below.
   ULONG                   DeviceExtensionFlags;

   // The target (lowest level) device object we are attached to.
   PDEVICE_OBJECT          TargetDeviceObject;

   // The file object of the device we are attached to.
   PFILE_OBJECT            TargetFileObject;

   // The device object immediately below us.
   PDEVICE_OBJECT          LowerDeviceObject;

   ULONG                   PackingInsurance; // Do Not Touch!!!
}TDI_DEV_EXT, *PTDI_DEV_EXT;

#define   TDIH_DEV_EXT_ATTACHED   (0x00000001)


/////////////////////////////////////////////////////////////////////////////
//                      W I N 3 2  A P I  D E V I C E                      //
/////////////////////////////////////////////////////////////////////////////

NTSTATUS TDIH_ChainRequest(
			PDEVICE_OBJECT pDeviceObject,
			PIRP           Irp );

NTSTATUS W32DevInitialize ( 
			IN PDRIVER_OBJECT DriverObject,
			IN PUNICODE_STRING RegistryPath
			);

NTSTATUS W32DevDispatch( 
			IN PDEVICE_OBJECT DeviceObject, 
			IN PIRP Irp );
#endif //__ANSHLP_H__

