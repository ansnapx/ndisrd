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

#include <ndis.h>

#include <windef.h>

#define FLT_DEVICE_LINK             L""
#define FLT_DEVICE_NAME             L"\\Device\\ansfltr"

#define	PHYSICAL_DIRECTION_OUT 0x01	
#define	PHYSICAL_DIRECTION_IN  0x02
#define	PHYSICAL_DIRECTION_ANY 0x03

#define		FILTER_REQUEST_ID          'RTLF'
#define		FILTER_ALLOC_TAG           'tliF'
#define		FILTER_TAG                 'dnTF'

#if (NTDDI_VERSION < NTDDI_LONGHORN)

typedef struct _FLT_COMMON_BLOCK_5 {
	LIST_ENTRY			m_qLink;
	PVOID				m_pAdapter;
	LONG				m_References;
	ULONG				m_Flags;	
}FLT_COMMON_BLOCK_5, *PFLT_COMMON_BLOCK_5;

#define FLT_COMMON_BLOCK FLT_COMMON_BLOCK_5

#else // (NTDDI_VERSION < NTDDI_LONGHORN)

typedef struct _FLT_MODULE_CONTEXT {
	LIST_ENTRY			m_qLink;

	LONG				m_FilterState;
    NDIS_HANDLE         m_FilterHandle;
	DWORD				m_LastIndicatedIP;

    NDIS_STRING         m_FilterModuleName;
    NDIS_STRING         m_MiniportFriendlyName;
    NDIS_STRING         m_MiniportName;

	NDIS_FILTER_ATTACH_PARAMETERS   m_AttachParameters;
	
	NDIS_HANDLE						NetBufferListPool;

}FLT_MODULE_CONTEXT, * PFLT_MODULE_CONTEXT;

typedef struct _FLT_COMMON_BLOCK_6 {
	LIST_ENTRY			m_qLink;
	PFLT_MODULE_CONTEXT	m_pAdapter;
	LONG				m_References;

	PNET_BUFFER_LIST	m_pNBL;	
	ULONG				m_Flags;	
	NDIS_PORT_NUMBER	m_NdisPortNumber;

} FLT_COMMON_BLOCK_6, *PFLT_COMMON_BLOCK_6;

#define FLT_COMMON_BLOCK FLT_COMMON_BLOCK_6

#endif // (NTDDI_VERSION < NTDDI_LONGHORN)

typedef struct _FLT_PACKET {
#ifndef __cplusplus
	FLT_COMMON_BLOCK;
#else
	FLT_COMMON_BLOCK	m_CommonBlock;
#endif
	BYTE				m_Direction;
	DWORD				m_BufferSize;
	PVOID				m_pBuffer;
}FLT_PACKET, *PFLT_PACKET;

typedef 
NTSTATUS (*FLT_REQUEST_CALLBACK)(
	IN PFLT_PACKET Packet);

typedef
VOID (*FLT_OPEN_ADAPTER_CALLBACK)(
	IN PVOID pAdapter,
	IN PNDIS_STRING  BindingName);

typedef
VOID (*FLT_CLOSE_ADAPTER_CALLBACK)(
	IN PVOID pAdapter);

typedef
VOID (*FLT_UNLOAD_CALLBACK)( );

typedef struct _FLT_REGISTRATION_CONTEXT {
	FLT_REQUEST_CALLBACK       m_pRequestCallback;
	FLT_OPEN_ADAPTER_CALLBACK  m_pOpenAdapterCallback;
	FLT_CLOSE_ADAPTER_CALLBACK m_pCloseAdapterCallback;
	FLT_UNLOAD_CALLBACK		   m_pUnloadCallback;
} FLT_REGISTRATION_CONTEXT, *PFLT_REGISTRATION_CONTEXT;

typedef struct _FLT_INSTANCE {
	LIST_ENTRY					m_qLink;
	FLT_REGISTRATION_CONTEXT	m_RegistrationContext;
}FLT_INSTANCE, *PFLT_INSTANCE;

typedef
NTSTATUS 
(*FLT_REGISTER_CALLBACKS)(
	IN OUT PFLT_INSTANCE * Instance, 
	IN PFLT_REGISTRATION_CONTEXT Context
	);

typedef
NTSTATUS 
(*FLT_DEREGISTER_CALLBACKS)(IN PFLT_INSTANCE Instance);

typedef
NTSTATUS 
(*FLT_INJECT_REQUEST) ( 	
	IN PFLT_INSTANCE Instance OPTIONAL,
	IN PFLT_PACKET Packet
	);

typedef
NTSTATUS
(*FLT_CLONE_REQUEST) (
	IN PFLT_PACKET pPacket,
	OUT PFLT_PACKET *ppClonePacket
	);

typedef
VOID
(*FLT_FREE_CLONE_REQUEST) (
	IN PFLT_PACKET pClonePacket
	);

typedef struct _FLT_FUNCTIONS {
	FLT_REGISTER_CALLBACKS		m_pFltRegisterCallbacks;
	FLT_DEREGISTER_CALLBACKS	m_pFltDeregisterCallbacks;
	FLT_INJECT_REQUEST			m_pFltInjectRequest;
	FLT_CLONE_REQUEST			m_pFltCloneRequest;
	FLT_FREE_CLONE_REQUEST		m_pFltFreeCloneRequest;
} FLT_FUNCTIONS, *PFLT_FUNCTIONS;

#define IOCTL_OBTAIN_FILTER_FUNCTIONS \
	CTL_CODE(FILE_DEVICE_PHYSICAL_NETCARD, 1, METHOD_BUFFERED, FILE_ANY_ACCESS)