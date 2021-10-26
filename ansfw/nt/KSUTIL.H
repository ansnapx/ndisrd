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
#ifndef __KSUTIL_H__
#define __KSUTIL_H__

/////////////////////////////////////////////////////////////////////////////
//// DEFINITIONS

// This constant is used for places where NdisAllocateMemory
// needs to be called and the HighestAcceptableAddress does
// not matter.
//
#define PRINT_IP_ADDR(addr) \
	((UCHAR *)&(addr))[0], ((UCHAR *)&(addr))[1], ((UCHAR *)&(addr))[2], ((UCHAR *)&(addr))[3]

typedef
struct _KS_CHANNEL
{
   HANDLE         m_hControlChannel; // Handle to the Control Channel object
   PFILE_OBJECT   m_pFileObject;    // The Control Channel File Object
}
   KS_CHANNEL, *PKS_CHANNEL;

/////////////////////////////////////////////////////////////////////////////
//// FUNCTION PROTOTYPES

#ifdef __cplusplus
extern "C" {
#endif

/////////////////////////////////////////////////////////////////////////////
//                    A D D R E S S  F U N C T I O N S                     //
/////////////////////////////////////////////////////////////////////////////
FILE_FULL_EA_INFORMATION UNALIGNED * FindEA( PFILE_FULL_EA_INFORMATION pFirstEA, CHAR *pName, USHORT nameLen );
VOID DumpEA( PFILE_FULL_EA_INFORMATION pFirstEA);
BOOLEAN getLocalInfo( PFILE_OBJECT fileObj, WORD * port, DWORD * addr );

NTSTATUS
KS_QueryAddressInfo(
   PFILE_OBJECT      pFileObject,
   PVOID             pInfoBuffer,
   PULONG_PTR        pInfoBufferSize
   );

/////////////////////////////////////////////////////////////////////////////
//                     U T I L I T Y  F U N C T I O N S                    //
/////////////////////////////////////////////////////////////////////////////

PMDL
KS_AllocateAndProbeMdl(
   PVOID VirtualAddress,
   ULONG Length,
   BOOLEAN SecondaryBuffer,
   BOOLEAN ChargeQuota,
   PIRP Irp OPTIONAL
   );

VOID
KS_UnlockAndFreeMdl(
   PMDL pMdl
   );

NTSTATUS
KS_MakeSimpleTdiRequest(
   IN PDEVICE_OBJECT pDeviceObject,
   IN PIRP pIrp
   );

/////////////////////////////////////////////////////////////////////////////
//           C O N T R O L  C H A N N E L  F U N C T I O N S               //
/////////////////////////////////////////////////////////////////////////////

NTSTATUS
KS_OpenControlChannel(
   IN PWSTR       TransportDeviceNameW,// Zero-terminated String
   PKS_CHANNEL    pKS_Channel
   );

NTSTATUS
KS_CloseControlChannel(
   PKS_CHANNEL    pKS_Channel
   );

NTSTATUS
KS_QueryProviderInfo(
   PWSTR                TransportDeviceNameW,// Zero-terminated String
   PTDI_PROVIDER_INFO   pProviderInfo
   );

NTSTATUS
KS_QueryProviderStatistics(
   PWSTR                    TransportDeviceNameW,// Zero-terminated String
   PTDI_PROVIDER_STATISTICS pProviderStatistics
   );

USHORT htons( USHORT hostshort );
USHORT ntohs( USHORT netshort );
ULONG htonl( ULONG hostlong );
ULONG ntohl( ULONG netlong );

#ifdef DBG

typedef struct _NPROT_LOCK
{
    ULONG					OwnerThread;
	ULONG                   IsAcquired;
    ULONG                   TouchedByFileNumber;
    ULONG                   TouchedInLineNumber;
    NDIS_SPIN_LOCK          NdisLock;
} NPROT_LOCK, *PNPROT_LOCK;

#define NPROT_INIT_LOCK(_pLock)              \
		   DbgPrint("\ndisprotAllocateSpinLock (%s) \n\n", __FILE__);		\
		   ndisprotAllocateSpinLock(_pLock, 0, __LINE__)

#define NPROT_ACQUIRE_LOCK(_pLock)           \
			/*DbgPrint("\ndisprotAcquireSpinLock (%s) \n\n", __FILE__);*/		\
            ndisprotAcquireSpinLock(_pLock, 0, __LINE__)

#define NPROT_RELEASE_LOCK(_pLock)			 \
			/*DbgPrint("\ndisprotReleaseSpinLock (%s) \n\n", __FILE__);*/		\
            ndisprotReleaseSpinLock(_pLock, 0, __LINE__)

VOID
ndisprotAllocateSpinLock(
    IN  PNPROT_LOCK          pLock,
    IN  ULONG               FileNumber,
    IN  ULONG               LineNumber
);


VOID
ndisprotAcquireSpinLock(
    IN  PNPROT_LOCK          pLock,
    IN  ULONG               FileNumber,
    IN  ULONG               LineNumber
);


VOID
ndisprotReleaseSpinLock(
    IN  PNPROT_LOCK          pLock,
    IN  ULONG               FileNumber,
    IN  ULONG               LineNumber
);
#else

#define NPROT_LOCK NDIS_SPIN_LOCK 

#define NPROT_INIT_LOCK(_pLock)           NdisAllocateSpinLock(_pLock)
#define NPROT_ACQUIRE_LOCK(_pLock)        NdisAcquireSpinLock(_pLock)
#define NPROT_RELEASE_LOCK(_pLock)        NdisReleaseSpinLock(_pLock)

#endif

#ifdef __cplusplus
}
#endif

#endif // __KSUTIL_H__

