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

#ifdef NDIS51_MINIPORT
#define NDISRD_MAJOR_NDIS_VERSION            5
#define NDISRD_MINOR_NDIS_VERSION            1
#else
#define NDISRD_MAJOR_NDIS_VERSION            4
#define NDISRD_MINOR_NDIS_VERSION            0
#endif

#ifdef NDIS51
#define NDISRD_PROT_MAJOR_NDIS_VERSION    5
#define NDISRD_PROT_MINOR_NDIS_VERSION    0
#else
#define NDISRD_PROT_MAJOR_NDIS_VERSION    4
#define NDISRD_PROT_MINOR_NDIS_VERSION    0
#endif

#define MAX_BUNDLEID_LENGTH 50
#define ADAPTER_NAME_SIZE	256
#define ADAPTER_LIST_SIZE	32

#define TAG 'ImPa'
#define WAIT_INFINITE 0



//advance declaration
typedef struct _ADAPT ADAPT, *PADAPT;
typedef struct _ADAPT ADAPTER_ENTRY, *PADAPTER_ENTRY;
typedef DWORD _IP;

extern DWORD g_dwMTUDecrement;
extern DWORD g_dwStartupMode;

DRIVER_INITIALIZE DriverEntry;

NDIS_STATUS
PtRegisterDevice(
    VOID
    );

NDIS_STATUS
PtDeregisterDevice(
    VOID
   );

VOID
PtUnloadProtocol(
    VOID
    );

//
// Protocol proto-types
//
extern
VOID
PtOpenAdapterComplete(
    IN NDIS_HANDLE                ProtocolBindingContext,
    IN NDIS_STATUS                Status,
    IN NDIS_STATUS                OpenErrorStatus
    );

extern
VOID
PtCloseAdapterComplete(
    IN NDIS_HANDLE                ProtocolBindingContext,
    IN NDIS_STATUS                Status
    );

extern
VOID
PtResetComplete(
    IN NDIS_HANDLE                ProtocolBindingContext,
    IN NDIS_STATUS                Status
    );

extern
VOID
PtRequestComplete(
    IN NDIS_HANDLE                ProtocolBindingContext,
    IN PNDIS_REQUEST              NdisRequest,
    IN NDIS_STATUS                Status
    );

extern
VOID
PtStatus(
    IN NDIS_HANDLE                ProtocolBindingContext,
    IN NDIS_STATUS                GeneralStatus,
    IN PVOID                      StatusBuffer,
    IN UINT                       StatusBufferSize
    );

extern
VOID
PtStatusComplete(
    IN NDIS_HANDLE                ProtocolBindingContext
    );

extern
VOID
PtSendComplete(
    IN NDIS_HANDLE                ProtocolBindingContext,
    IN PNDIS_PACKET               Packet,
    IN NDIS_STATUS                Status
    );

extern
VOID
PtTransferDataComplete(
    IN NDIS_HANDLE                ProtocolBindingContext,
    IN PNDIS_PACKET               Packet,
    IN NDIS_STATUS                Status,
    IN UINT                       BytesTransferred
    );

extern
NDIS_STATUS
PtReceive(
    IN NDIS_HANDLE                ProtocolBindingContext,
    IN NDIS_HANDLE                MacReceiveContext,
    IN PVOID                      HeaderBuffer,
    IN UINT                       HeaderBufferSize,
    IN PVOID                      LookAheadBuffer,
    IN UINT                       LookaheadBufferSize,
    IN UINT                       PacketSize
    );

extern
VOID
PtReceiveComplete(
    IN NDIS_HANDLE                ProtocolBindingContext
    );

extern
VOID
PtBindAdapter(
    OUT PNDIS_STATUS              Status,
    IN  NDIS_HANDLE               BindContext,
    IN  PNDIS_STRING              DeviceName,
    IN  PVOID                     SystemSpecific1,
    IN  PVOID                     SystemSpecific2
    );

extern
VOID
PtUnbindAdapter(
    OUT PNDIS_STATUS              Status,
    IN  NDIS_HANDLE               ProtocolBindingContext,
    IN  NDIS_HANDLE               UnbindContext
    );
    
DRIVER_UNLOAD PtUnload;

extern 
NDIS_STATUS
PtPNPHandler(
    IN NDIS_HANDLE                ProtocolBindingContext,
    IN PNET_PNP_EVENT             pNetPnPEvent
    );

NDIS_STATUS
PtPnPNetEventReconfigure(
    IN PADAPT            pAdapt,
    IN PNET_PNP_EVENT    pNetPnPEvent
    );    

NDIS_STATUS 
PtPnPNetEventSetPower (
    IN PADAPT                    pAdapt,
    IN PNET_PNP_EVENT            pNetPnPEvent
    );
    

//
// Miniport proto-types
//
NDIS_STATUS
MPInitialize(
    OUT PNDIS_STATUS             OpenErrorStatus,
    OUT PUINT                    SelectedMediumIndex,
    IN PNDIS_MEDIUM              MediumArray,
    IN UINT                      MediumArraySize,
    IN NDIS_HANDLE               MiniportAdapterHandle,
    IN NDIS_HANDLE               WrapperConfigurationContext
    );

VOID
MPSendPackets(
    IN NDIS_HANDLE                MiniportAdapterContext,
    IN PPNDIS_PACKET              PacketArray,
    IN UINT                       NumberOfPackets
    );

NDIS_STATUS
MPQueryInformation(
    IN NDIS_HANDLE                MiniportAdapterContext,
    IN NDIS_OID                   Oid,
    IN PVOID                      InformationBuffer,
    IN ULONG                      InformationBufferLength,
    OUT PULONG                    BytesWritten,
    OUT PULONG                    BytesNeeded
    );

NDIS_STATUS
MPSetInformation(
    IN NDIS_HANDLE                MiniportAdapterContext,
    IN NDIS_OID                   Oid,
    IN PVOID                      InformationBuffer,
    IN ULONG                      InformationBufferLength,
    OUT PULONG                    BytesRead,
    OUT PULONG                    BytesNeeded
    );

VOID
MPHalt(
    IN NDIS_HANDLE                MiniportAdapterContext
    );


VOID
MPQueryPNPCapabilities(  
    OUT PADAPT                    MiniportProtocolContext, 
    OUT PNDIS_STATUS              Status
    );


NDIS_STATUS
MPSetMiniportSecondary ( 
    IN PADAPT                    Secondary, 
    IN PADAPT                    Primary
    );

NDIS_STATUS
MPTransferData(
    OUT PNDIS_PACKET            Packet,
    OUT PUINT                   BytesTransferred,
    IN NDIS_HANDLE              MiniportAdapterContext,
    IN NDIS_HANDLE              MiniportReceiveContext,
    IN UINT                     ByteOffset,
    IN UINT                     BytesToTransfer
    );

#ifdef NDIS51_MINIPORT

VOID
MPCancelSendPackets(
    IN NDIS_HANDLE            MiniportAdapterContext,
    IN PVOID                  CancelId
    );

VOID
MPAdapterShutdown(
    IN NDIS_HANDLE                MiniportAdapterContext
    );

VOID
MPDevicePnPEvent(
    IN NDIS_HANDLE                MiniportAdapterContext,
    IN NDIS_DEVICE_PNP_EVENT      DevicePnPEvent,
    IN PVOID                      InformationBuffer,
    IN ULONG                      InformationBufferLength
    );

#endif // NDIS51_MINIPORT

VOID
MPFreeAllPacketPools(
    IN PADAPT                    pAdapt
    );

NDIS_STATUS 
MPPromoteSecondary ( 
    IN PADAPT                    pAdapt 
    );


NDIS_STATUS 
MPBundleSearchAndSetSecondary (
    IN PADAPT                    pAdapt 
    );

VOID
MPProcessSetPowerOid(
    IN OUT PNDIS_STATUS          pNdisStatus,
    IN PADAPT                    pAdapt,
    IN PVOID                     InformationBuffer,
    IN ULONG                     InformationBufferLength,
    OUT PULONG                   BytesRead,
    OUT PULONG                   BytesNeeded
    );

#define    NUM_PKTS_IN_POOL    256


//
// Protocol reserved part of a sent packet that is allocated by us.
//
//typedef struct _SEND_RSVD
//{
//    PNDIS_PACKET    OriginalPkt;
//} SEND_RSVD, *PSEND_RSVD;

//
// Miniport reserved part of a received packet that is allocated by
// us. Note that this should fit into the MiniportReserved space
// in an NDIS_PACKET.
//
//typedef struct _RECV_RSVD
//{
//    PNDIS_PACKET    OriginalPkt;
//} RECV_RSVD, *PRECV_RSVD;

//C_ASSERT(sizeof(RECV_RSVD) <= sizeof(((PNDIS_PACKET)0)->MiniportReserved));

//
// Event Codes related to the PassthruEvent Structure
//

typedef enum 
{
    Passthru_Invalid,
    Passthru_SetPower,
    Passthru_Unbind

} PASSSTHRU_EVENT_CODE, *PPASTHRU_EVENT_CODE; 

//
// Passthru Event with  a code to state why they have been state
//

typedef struct _PASSTHRU_EVENT
{
    NDIS_EVENT Event;
    PASSSTHRU_EVENT_CODE Code;

} PASSTHRU_EVENT, *PPASSTHRU_EVENT;

#define NUMBER_OF_PACKET_DESCRIPTORS 100
#define NUMBER_OF_BUFFER_DESCRIPTORS 100
//
// Structure used by both the miniport as well as the protocol part of the intermediate driver
// to represent an adapter and its corres. lower bindings
//
typedef struct _ADAPT
{
    struct _ADAPT *                Next;

	//************************************************
	NDIS_HANDLE							m_hPacketPool; // Universal send/receive packet pool
	NDIS_HANDLE							m_hBufferPool; // Universal buffer pool

	NDIS_STRING							m_AdapterName;
	CHAR								m_szAdapterName[ADAPTER_NAME_SIZE];

	// Adapter associated lists
	LIST_ENTRY							m_PendingTransfertData;
	LIST_ENTRY							m_InternalRequest;

	// Locks associated with lists above
	NDIS_SPIN_LOCK						m_PendingTransfertLock;
	NDIS_SPIN_LOCK						m_InternalRequestLock;

	UCHAR								m_CurrentAddress[ETHER_ADDR_LENGTH];// For Ethernet controllers
	NDIS_MEDIUM							m_Medium;
	USHORT								m_usMTU;

	// Information taken from NDIS_LINE_UP structure
	// Senseless for real Ethernet
	_IP									m_LastIndicatedIP;
	_IP									m_LastIndicatedNetworkMask; // On W2k seems always to be 255.255.255.255 for dial-up
	UCHAR								m_RemoteAddress[ETHER_ADDR_LENGTH];
	UCHAR								m_LocalAddress[ETHER_ADDR_LENGTH];
	ULONG								m_LinkSpeed;
	//************************************************
    
    NDIS_HANDLE                    BindingHandle;    // To the lower miniport
    NDIS_HANDLE                    MiniportHandle;    // NDIS Handle to for miniport up-calls
    NDIS_STATUS                    Status;            // Open Status
    NDIS_EVENT                     Event;            // Used by bind/halt for Open/Close Adapter synch.
    NDIS_REQUEST                   Request;        // This is used to wrap a request coming down
                                                // to us. This exploits the fact that requests
                                                // are serialized down to us.
    PULONG                         BytesNeeded;
    PULONG                         BytesReadOrWritten;
    BOOLEAN                        IndicateRcvComplete;
    
    BOOLEAN                        OutstandingRequests;      // TRUE iff a request is pending
                                                        // at the miniport below
    BOOLEAN                        QueuedRequest;            // TRUE iff a request is queued at
                                                        // this IM miniport

    BOOLEAN                        StandingBy;                // True - When the miniport or protocol is transitioning from a D0 to Standby (>D0) State
    BOOLEAN                        UnbindingInProcess;
    NDIS_SPIN_LOCK                 Lock;
                                                        // False - At all other times, - Flag is cleared after a transition to D0

    NDIS_DEVICE_POWER_STATE        MPDeviceState;            // Miniport's Device State 
    NDIS_DEVICE_POWER_STATE        PTDeviceState;            // Protocol's Device State 
    NDIS_STRING                    DeviceName;                // For initializing the miniport edge
    NDIS_EVENT                     MiniportInitEvent;        // For blocking UnbindAdapter while
                                                        // an IM Init is in progress.
    BOOLEAN                        MiniportInitPending;    // TRUE iff IMInit in progress
    NDIS_STATUS                    LastIndicatedStatus;    // The last indicated media status
    NDIS_STATUS                    LatestUnIndicateStatus; // The latest suppressed media status
    ULONG                          OutstandingSends;

	NDISRD_ADAPTER_EXTENSION			m_NdisrdExtension;

} ADAPT, *PADAPT;

typedef void PROTOCOL_ENTRY, *PPROTOCOL_ENTRY;

extern    NDIS_HANDLE                        ProtHandle, DriverHandle;
extern    NDIS_MEDIUM                        MediumArray[2];
extern    PADAPT                             pAdaptList;
extern    NDIS_SPIN_LOCK                     GlobalLock;


#define ADAPT_MINIPORT_HANDLE(_pAdapt)    ((_pAdapt)->MiniportHandle)
#define ADAPT_DECR_PENDING_SENDS(_pAdapt)     \
    {                                         \
        NdisAcquireSpinLock(&(_pAdapt)->Lock);   \
        (_pAdapt)->OutstandingSends--;           \
        NdisReleaseSpinLock(&(_pAdapt)->Lock);   \
    }

//
// Custom Macros to be used by the passthru driver 
//
/*
BOOLEAN
IsIMDeviceStateOn(
   PADAPT 
   )

*/
#define IsIMDeviceStateOn(_pP)        ((_pP)->MPDeviceState == NdisDeviceStateD0 && (_pP)->PTDeviceState == NdisDeviceStateD0 ) 