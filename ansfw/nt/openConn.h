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
#ifndef __OPENCONN_H__
#define __OPENCONN_H__

#define IPPROTO_TCP  6
#define IPPROTO_UDP  17
#define IPPROTO_ICMP 1
#define IPPROTO_IP   0

enum TDI_FLAGS
{
	FL_PENDING = 0x000001,
	FL_ATTACHED = 0x000002,
	FL_LISTEN = 0x000004
};

typedef struct _TDI_OPEN_CONN 
{
	LIST_ENTRY	  m_qLink;
	DWORD		  m_Flags;
	DWORD		  m_Pid;
	DWORD		  m_LocalAddr;
	WORD		  m_LocalPort;
	DWORD		  m_RemoteAddr;
	WORD		  m_RemotePort;
	BYTE		  m_IpProto;
	PIRP		  m_pIrp;
	PVOID		  m_pTcpObj;

	union
	{
		PFILE_OBJECT  m_pFileObject;

		struct
		{
			PFILE_OBJECT  m_pTcpFileObject;
			PVOID		  m_pAddrObj;
		};
	};

	PVOID		  m_pDeviceExtension;
}TDI_OPEN_CONN, *PTDI_OPEN_CONN;

PTDI_OPEN_CONN tdiAllocOpenConn( );
VOID tdiRecycleOpenConn( PTDI_OPEN_CONN conn);
PTDI_OPEN_CONN tdiGetConnObjectFromFileObject(PFILE_OBJECT fileobject, BOOL doLock);
VOID NTAPI GetPidFromLocalInfo(PFW_PID_REQUEST request);
VOID initLists( );
VOID initAddrThread( );
VOID closeAddrThread( );

NTSTATUS
TDIH_TdiOpenAddress(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   );

NTSTATUS
TDIH_TdiCloseAddress(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   );

NTSTATUS
TDIH_TdiOpenConnection(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp,
   PVOID                   ConnectionContext
   );

NTSTATUS
TDIH_TdiCloseConnection(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   );

NTSTATUS
TDIH_TdiAssociateAddress(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   );

NTSTATUS
TDIH_TdiDisAssociateAddress(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   );

NTSTATUS
TDIH_TdiConnect(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   );

NTSTATUS
TDIH_TdiSetEvent(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   );

NTSTATUS
TDIH_IpDispatch(
    PTDI_DEV_EXT			pTDIH_DeviceExtension,
    PIRP                    Irp,
    PIO_STACK_LOCATION      IrpSp
   );

NTSTATUS
TDIH_UdpSendDatagram(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   );

NTSTATUS
TDIH_TdiListen(
   PTDI_DEV_EXT			   pTDIH_DeviceExtension,
   PIRP                    Irp,
   PIO_STACK_LOCATION      IrpSp
   );
#endif // __OPENCONN_H__

