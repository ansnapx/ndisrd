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

BOOL            g_bThread;
PVOID			g_ThreadObj; 
KEVENT		    g_ThreadEvent;

LIST_ENTRY		g_leIntermedQueueHead;  
KSPIN_LOCK		g_slIntermedQueueLock;

BOOL            g_bTerminateThread;
PVOID			g_TtlDecrThreadObj; 
KEVENT		    g_TtlDecrThreadEvent;

LIST_ENTRY		g_leDatagrQueueHead;  
KSPIN_LOCK		g_slDatagrQueueLock;

LIST_ENTRY		g_FreeDatagramWorkBufList;  
KSPIN_LOCK	    g_FreeDatagramWorkBufListLock; 

DWORD gAllocCounter=0;


//
// Prototypes
// 

VOID ruleRecycleDatagramWorkBuffer(PDATA_LIST datagr);
void TtlDecrementThreadFunc(void * context);
void IntermedThreadFunc(void * context);
void Analyze(PFW_REQUEST pRequest);
NTSTATUS Verify(PFW_REQUEST pRequest,DATA_LIST *pDatagrListItem);
USHORT GetChecksum(USHORT* buffer, int size);


//
// Definitions
//

void IntermedThreadFunc(void * context)
{
	LONG				wait = -1; 
	PFW_REQUEST pRequest;
	KIRQL oldIrql;
	PLIST_ENTRY pListEntry;

	while(!g_bThread) 
	{ 


		KeWaitForSingleObject( &g_ThreadEvent, Executive, KernelMode, FALSE, NULL );


		KeAcquireSpinLock( &g_slIntermedQueueLock , &oldIrql );

		dprintf("IntermedThreadFunc\n");

		while( ! IsListEmpty(&g_leIntermedQueueHead) )
		{
			pRequest = CONTAINING_RECORD( RemoveHeadList(&g_leIntermedQueueHead), FW_REQUEST, m_qLink );
			Analyze(pRequest);
			//dprintf("FREE CLONE");
			//FWFreeCloneRequest(pRequest);	

		}

		KeReleaseSpinLock(  &g_slIntermedQueueLock, oldIrql );
	}

	PsTerminateSystemThread(STATUS_SUCCESS);
}




void TtlDecrementThreadFunc(void * context)
{
	DATA_LIST *pFgrament=NULL;          

	LARGE_INTEGER sleep;
	KIRQL oldIrql;
	sleep.QuadPart=-10000*1000;

	dprintf("TTL  thread STARTED");

	while(!g_bTerminateThread)
	{
		KeAcquireSpinLock( &g_slDatagrQueueLock , &oldIrql ); 

		pFgrament = (DATA_LIST*)g_leDatagrQueueHead.Flink;

		while(pFgrament != (DATA_LIST*)&g_leDatagrQueueHead)
		{
			dprintf("TTL in THREAD %d %X %X",pFgrament->ttl,pFgrament,pFgrament->ID);


			if(pFgrament->ttl==0)
			{
				PLIST_ENTRY pListEntry;

				RemoveEntryList(&pFgrament->Link);
				dprintf("RECYCLE FROM THREAD");

				pListEntry = pFgrament->m_FragmentsList.Flink;
				while( pListEntry != &pFgrament->m_FragmentsList )
				{
					PFW_REQUEST pFragment = CONTAINING_RECORD( pListEntry, FW_REQUEST, m_qLink );
					pListEntry = pListEntry->Flink;
					FWFreeCloneRequest( pFragment );
				}	

				ruleRecycleDatagramWorkBuffer(pFgrament);
				pFgrament = (DATA_LIST*)g_leDatagrQueueHead.Flink;
				continue;
			}
			pFgrament->ttl--;

			pFgrament = (DATA_LIST*)pFgrament->Link.Flink;
		}//while


		KeReleaseSpinLock(   &g_slDatagrQueueLock, oldIrql );

		KeDelayExecutionThread(KernelMode,FALSE,&sleep);

	}
	dprintf("TTL thread ENDING");
	PsTerminateSystemThread(STATUS_SUCCESS);
}


PDATA_LIST ruleAllocDatagramWorkBuffer( )
{
	PDATA_LIST pDataBuffer = NULL; 
	KIRQL oldIrql;

	KeAcquireSpinLock( &g_FreeDatagramWorkBufListLock , &oldIrql ); 
	if ( !IsListEmpty ( &g_FreeDatagramWorkBufList ) ) 
	{ 
		pDataBuffer = (PDATA_LIST) RemoveHeadList ( &g_FreeDatagramWorkBufList );

		KeReleaseSpinLock(&g_FreeDatagramWorkBufListLock, oldIrql );
		if(pDataBuffer)
		{
			NdisZeroMemory ( pDataBuffer, sizeof(DATA_LIST) );
			InitializeListHead( &pDataBuffer->m_FragmentsList );
			return pDataBuffer;
		}
	} 
	KeReleaseSpinLock(&g_FreeDatagramWorkBufListLock, oldIrql );

	if( InterlockedCompareExchange(&gAllocCounter, 0, 0)>MAX_WORKBUFFER_COUNT )
	{
		dprintf("Allocation limit is reached.\n");
		return NULL;
	}

	pDataBuffer=ExAllocatePoolWithTag ( NonPagedPool, sizeof(DATA_LIST), 'garF' );  
	if (pDataBuffer)
	{ 
		InterlockedIncrement(&gAllocCounter);
		NdisZeroMemory ( pDataBuffer, sizeof(DATA_LIST) ); 
		InitializeListHead( &pDataBuffer->m_FragmentsList );
		return pDataBuffer; 
	} 

	return NULL;
}

VOID ruleRecycleDatagramWorkBuffer(PDATA_LIST datagr) 
{ 
	KIRQL oldIrql;

	KeAcquireSpinLock( &g_FreeDatagramWorkBufListLock , &oldIrql ); 
	InsertTailList ( &g_FreeDatagramWorkBufList, &datagr->Link ); 
	KeReleaseSpinLock( &g_FreeDatagramWorkBufListLock, oldIrql );
}

void PrepareIpHeader(DATA_LIST*pDatagrListItem)
{	
	iphdr_ptr pIpHdr=NULL;
	pIpHdr = (iphdr_ptr )(pDatagrListItem->finalbuf+MHdrSize);
	pIpHdr->ip_sum=0;
	pIpHdr->ip_len= ntohs(pDatagrListItem->totallen);
	pIpHdr->ip_off=0;
	pIpHdr->ip_sum=GetChecksum((USHORT*)pIpHdr,20);
}


USHORT GetChecksum(USHORT* buffer, int size) 
{
	unsigned long cksum = 0;   
	while (size > 1) {
		cksum += *buffer++;
		size -= sizeof(USHORT);
	}
	if (size) {
		cksum += *(UCHAR*)buffer;
	}
	cksum = (cksum >> 16) + (cksum & 0xffff);
	cksum += (cksum >> 16);   
	return (USHORT)(~cksum);
}



void Analyze(PFW_REQUEST pRequest)
{
	PVOID pBuffer=pRequest->m_pFltPacket->m_pBuffer;
	iphdr_ptr pIpHdr=NULL;
	DWORD i=0;
	BYTE *buf=NULL;
	DWORD source=0;
	DWORD dest=0;
	WORD datagrID=0;
	WORD protocol=0;
	WORD wFlag_Offset=0;
	WORD offset=0;
	WORD datasize=0;
	WORD FullLength=0;	
	DWORD lastbyte=0;	
	PDATA_LIST pDatagrListItem=NULL;
	UCHAR ttl=0;
	WORD ipHeaderSize=0;
	WORD lastbit=0;
	KIRQL oldIrql;


	pIpHdr = (iphdr_ptr)&((BYTE*)pRequest->m_pFltPacket->m_pBuffer)[ETHER_HDR_SIZE];
	buf=(BYTE*)pIpHdr;

	datagrID= ntohs(pIpHdr->ip_id);
	FullLength= ntohs(pIpHdr->ip_len);

	wFlag_Offset= ntohs(pIpHdr->ip_off);
	offset=wFlag_Offset & 0x1FFF;
	ipHeaderSize=pIpHdr->ip_hl*4;
	source= ntohl(pIpHdr->ip_src.S_un.S_addr);
	dest= ntohl(pIpHdr->ip_dst.S_un.S_addr);
	protocol=pIpHdr->ip_p;
	ttl=pIpHdr->ip_ttl;

	dprintf("offset ,LEN and DATAGRID %d %d %X",offset,FullLength,datagrID);	

	if(ipHeaderSize>20)
	{
		return;
	}			

	KeAcquireSpinLock( &g_slDatagrQueueLock , &oldIrql ); 

	pDatagrListItem = (DATA_LIST*)g_leDatagrQueueHead.Flink;

	while(pDatagrListItem != (DATA_LIST*)&g_leDatagrQueueHead)
	{


		if (pDatagrListItem->ID == datagrID && 
			pDatagrListItem->protocol==protocol && 
			pDatagrListItem->destIP==dest && 
			pDatagrListItem->srcIP==source)
		{
			RemoveEntryList( &pRequest->m_qLink );
			InsertHeadList( &pDatagrListItem->m_FragmentsList, &pRequest->m_qLink );


			if (wFlag_Offset&0x2000)
			{

				if (offset==0)//very first packet
				{ 
					pDatagrListItem->m_FwRequest.m_IpProto = pRequest->m_IpProto;
					pDatagrListItem->m_FwRequest.m_SourceAddr = pRequest->m_SourceAddr;
					pDatagrListItem->m_FwRequest.m_DestAddr = pRequest->m_DestAddr;
					pDatagrListItem->m_FwRequest.m_SourcePort = pRequest->m_SourcePort;
					pDatagrListItem->m_FwRequest.m_DestPort = pRequest->m_DestPort;
					pDatagrListItem->m_FwRequest.m_Pid = pRequest->m_Pid;
					pDatagrListItem->m_FwRequest.m_ReqFlags = pRequest->m_ReqFlags;

					RtlCopyMemory(pDatagrListItem->finalbuf,pBuffer,MHdrSize);
					RtlCopyMemory(pDatagrListItem->finalbuf+(offset*8)+MHdrSize,buf,FullLength); 

					lastbit=FullLength/8;

					RtlSetBits(&pDatagrListItem->BitMapHeader,offset,lastbit); 

					if (!pDatagrListItem->bLast)
					{						
						KeReleaseSpinLock(&g_slDatagrQueueLock, oldIrql );
						return;
					}
					Verify(pRequest,pDatagrListItem);


				}////
				else//if not first
				{

					datasize=FullLength-ipHeaderSize;
					if((offset*8)+FullLength > WORKBUFSIZE)
					{
						dprintf("SECOND or OTHER DATAOFFSET  > WORKBUFSIZE %X",(offset*8)+FullLength);
						RemoveEntryList(&pDatagrListItem->Link);
						ruleRecycleDatagramWorkBuffer(pDatagrListItem);
						KeReleaseSpinLock(&g_slDatagrQueueLock, oldIrql );

						return;

					}
					buf=buf+ipHeaderSize;

					RtlCopyMemory(pDatagrListItem->finalbuf,pBuffer,MHdrSize);
					RtlCopyMemory(pDatagrListItem->finalbuf+(offset*8)+ipHeaderSize+MHdrSize,buf,datasize);

					lastbit=(datasize+ipHeaderSize)/8;

					RtlSetBits(&pDatagrListItem->BitMapHeader,offset,lastbit);  

					if (!pDatagrListItem->bLast)
					{
						KeReleaseSpinLock(&g_slDatagrQueueLock, oldIrql );

						return;     
					}

					Verify(pRequest,pDatagrListItem);


				}//else if offset>0 

				KeReleaseSpinLock(&g_slDatagrQueueLock, oldIrql );

				return;               

			}
			else//last fragment //   GET  TOTAL LEN
			{

				datasize=FullLength-ipHeaderSize;
				if((offset*8)+FullLength > WORKBUFSIZE)
				{
					dprintf("LAST DATAOFFSET  > WORKBUFSIZE %X",(offset*8)+FullLength);
					RemoveEntryList(&pDatagrListItem->Link);
					ruleRecycleDatagramWorkBuffer(pDatagrListItem);
					KeReleaseSpinLock(&g_slDatagrQueueLock, oldIrql );

					return;     


				}

				buf=buf+ipHeaderSize;

				pDatagrListItem->bLast=TRUE;

				pDatagrListItem->totallen=(offset*8)+ FullLength;

				RtlCopyMemory(pDatagrListItem->finalbuf,pBuffer,MHdrSize);

				RtlCopyMemory(pDatagrListItem->finalbuf+(offset*8)+ipHeaderSize+MHdrSize,buf,datasize);

				lastbit=(datasize+ipHeaderSize)/8;

				RtlSetBits(&pDatagrListItem->BitMapHeader,offset,lastbit); 								                 

				Verify(pRequest,pDatagrListItem);
				KeReleaseSpinLock(&g_slDatagrQueueLock, oldIrql );

				return;     

			}// 

		}//if(pDatagrListItem->ID == datagrID)
		pDatagrListItem = (DATA_LIST*)pDatagrListItem->Link.Flink;

	}// loop  end 


	do
	{	  
		if((offset*8)+FullLength > WORKBUFSIZE)
		{
			dprintf("DATAOFFSET  > WORKBUFSIZE %X",(offset*8)+FullLength);

			break;

		} 	

		
		pDatagrListItem=ruleAllocDatagramWorkBuffer();



		if (!pDatagrListItem)
		{

			break;

		}
		else
		{	
			RemoveEntryList( &pRequest->m_qLink );
			InsertHeadList( &pDatagrListItem->m_FragmentsList, &pRequest->m_qLink );

			pDatagrListItem->ID=datagrID;
			pDatagrListItem->bLast=FALSE;
			pDatagrListItem->ttl=ttl;
			pDatagrListItem->protocol=protocol;
			pDatagrListItem->destIP=dest;
			pDatagrListItem->srcIP=source;

			//RtlInitializeBitMap(&pDatagrListItem->BitMapHeader,(PULONG)pDatagrListItem->bits,BITSBUFFERLENGHT*8); 
			pDatagrListItem->BitMapHeader.SizeOfBitMap=(ULONG)BITSBUFFERLENGHT*8;
			pDatagrListItem->BitMapHeader.Buffer=(PULONG)pDatagrListItem->bits;

			if (offset==0)
			{
				pDatagrListItem->m_FwRequest.m_IpProto = pRequest->m_IpProto;
				pDatagrListItem->m_FwRequest.m_SourceAddr = pRequest->m_SourceAddr;
				pDatagrListItem->m_FwRequest.m_DestAddr = pRequest->m_DestAddr;
				pDatagrListItem->m_FwRequest.m_SourcePort = pRequest->m_SourcePort;
				pDatagrListItem->m_FwRequest.m_DestPort = pRequest->m_DestPort;
				pDatagrListItem->m_FwRequest.m_Pid = pRequest->m_Pid;
				pDatagrListItem->m_FwRequest.m_ReqFlags = pRequest->m_ReqFlags;

				RtlCopyMemory(pDatagrListItem->finalbuf,pBuffer,MHdrSize);
				RtlCopyMemory(pDatagrListItem->finalbuf+MHdrSize,buf,FullLength);
			}
			else
			{                      

				buf=buf+ipHeaderSize;

				if(wFlag_Offset & 0x2000)
				{                  

					pDatagrListItem->bLast=FALSE;
				}
				else
				{                                     
					dprintf("1 st IS LAST");	 
					pDatagrListItem->totallen=(offset*8)+ FullLength;
					pDatagrListItem->bLast=TRUE;
				}


				datasize=FullLength-ipHeaderSize;

				RtlCopyMemory(pDatagrListItem->finalbuf,pBuffer,MHdrSize);
				RtlCopyMemory(pDatagrListItem->finalbuf+(offset*8)+ipHeaderSize+MHdrSize,buf,datasize);


			}


			lastbit=FullLength/8;

			RtlSetBits(&pDatagrListItem->BitMapHeader,offset,lastbit); 

			InsertTailList(&g_leDatagrQueueHead,&pDatagrListItem->Link);

			break;


		}//if(pDatagrListItem)

	}
	while(TRUE);

	KeReleaseSpinLock(&g_slDatagrQueueLock, oldIrql );


}

NTSTATUS Verify(PFW_REQUEST pRequest,DATA_LIST *pDatagrListItem)
{
	NTSTATUS status = STATUS_SUCCESS;
	WORD lastbyte = pDatagrListItem->totallen/8;

	if(RtlAreBitsSet(&pDatagrListItem->BitMapHeader,0,lastbyte))
	{
		RemoveEntryList(&pDatagrListItem->Link);

		dprintf("Assembled %d",pDatagrListItem->totallen);

		PrepareIpHeader(pDatagrListItem);
		//pDatagrListItem->m_FwRequest.m_IpProto = pRequest->m_IpProto;
		//pDatagrListItem->m_FwRequest.m_SourceAddr = pRequest->m_SourceAddr;
		//pDatagrListItem->m_FwRequest.m_DestAddr = pRequest->m_DestAddr;
		//pDatagrListItem->m_FwRequest.m_SourcePort = pRequest->m_SourcePort;
		//pDatagrListItem->m_FwRequest.m_DestPort = pRequest->m_DestPort;
		//pDatagrListItem->m_FwRequest.m_Pid = pRequest->m_Pid;
		//pDatagrListItem->m_FwRequest.m_ReqFlags = pRequest->m_ReqFlags;

		pDatagrListItem->m_FwRequest.m_ReqFlags |= FL_REQUEST_REASSEMBLED;
		pDatagrListItem->m_FwRequest.m_References = 1;
		pDatagrListItem->m_FwRequest.m_pFltPacket = &pDatagrListItem->m_FltPacket;
		pDatagrListItem->m_FwRequest.m_pFltPacket->m_Direction = pRequest->m_pFltPacket->m_Direction;
		pDatagrListItem->m_FwRequest.m_pFltPacket->m_pBuffer=pDatagrListItem->finalbuf;
		pDatagrListItem->m_FwRequest.m_pFltPacket->m_BufferSize=pDatagrListItem->totallen+ETHER_HDR_SIZE;

		status=FWInjectRequest( NULL, &pDatagrListItem->m_FwRequest );
		FWFreeCloneRequest( &pDatagrListItem->m_FwRequest );
		//RemoveEntryList(&pDatagrListItem->Link);
		//ruleRecycleDatagramWorkBuffer(pDatagrListItem);	
	}

	return status;//SO FAR
}


VOID InitializeReassembly()
{
	HANDLE 	hdl; 

	NTSTATUS Status;

	InitializeListHead ( &g_FreeDatagramWorkBufList );
	InitializeListHead ( &g_leDatagrQueueHead); 
	InitializeListHead ( &g_leIntermedQueueHead);

	KeInitializeSpinLock (&g_slIntermedQueueLock);
	KeInitializeSpinLock (&g_slDatagrQueueLock);
	KeInitializeSpinLock ( &g_FreeDatagramWorkBufListLock);

	g_bThread=FALSE;

	KeInitializeEvent(&g_ThreadEvent,SynchronizationEvent,FALSE);

	Status = PsCreateSystemThread( &hdl, 0, NULL, NULL, NULL, IntermedThreadFunc, NULL);
	if ( NT_SUCCESS(Status) )  
	{ 
		if (ObReferenceObjectByHandle( hdl, THREAD_ALL_ACCESS, NULL, KernelMode, &g_ThreadObj, NULL ) != STATUS_SUCCESS) 
			g_ThreadObj = NULL; 
		else 
			ZwClose( hdl ); 
	} 

	g_bTerminateThread=FALSE;

	Status = PsCreateSystemThread( &hdl, 0, NULL, NULL, NULL,TtlDecrementThreadFunc, NULL);
	if ( NT_SUCCESS(Status) )  
	{ 
		if (ObReferenceObjectByHandle( hdl, THREAD_ALL_ACCESS, NULL, KernelMode, &g_TtlDecrThreadObj, NULL ) != STATUS_SUCCESS) 
			g_TtlDecrThreadObj = NULL; 
		else 
			ZwClose( hdl ); 
	} 
}

VOID UninitializeReassembly( )  
{ 	
	g_bTerminateThread=TRUE;

	if ( g_TtlDecrThreadObj != NULL)  
	{ 		
		KeWaitForSingleObject( g_TtlDecrThreadObj, Executive, KernelMode, FALSE, NULL );
		ObDereferenceObject(  g_TtlDecrThreadObj ); 
		g_TtlDecrThreadObj = NULL; 
	} 

	g_bThread=TRUE;
	KeSetEvent( &g_ThreadEvent, IO_NO_INCREMENT,FALSE); 

	if (  g_ThreadObj != NULL)  
	{ 
		KeWaitForSingleObject( g_ThreadObj, Executive, KernelMode, FALSE, NULL );
		ObDereferenceObject(   g_ThreadObj ); 
		g_ThreadObj = NULL; 
	} 
}

VOID ReassemblePacket( PFW_REQUEST pFwRequest )
{
	KIRQL oldIrql;
	//PFW_REQUEST pClonedRequest;

	//if( FWCloneRequest(pFwRequest,&pClonedRequest)!=STATUS_SUCCESS )
	//	return;

	KeAcquireSpinLock(&g_slIntermedQueueLock, &oldIrql );
	InsertTailList(&g_leIntermedQueueHead, &pFwRequest->m_qLink); 
	KeReleaseSpinLock(&g_slIntermedQueueLock, oldIrql );
	KeSetEvent(&g_ThreadEvent, IO_NO_INCREMENT, FALSE );
}