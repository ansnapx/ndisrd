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


#ifndef __TDIFIREWALL_H__
#define __TDIFIREWALL_H__

typedef struct _TDI_CONN_REQUEST {
	LIST_ENTRY m_qLink;
	DWORD	   m_Pid;
	DWORD	   m_SourceAddr;
	DWORD	   m_DestAddr;
	WORD	   m_SourcePort;
	WORD	   m_DestPort;
	WORD	   m_IpProto;
	PIRP       m_pIrp;
	PVOID	   m_pContext;
	PVOID	   m_CompletionFunc;
	NTSTATUS   m_StatusFail;
	UINT	   m_References;
	PTDI_DEV_EXT m_pDevExt;
}TDI_CONN_REQUEST, *PTDI_CONN_REQUEST;

NTSTATUS tdiChainIrp(PTDI_CONN_REQUEST request);
NTSTATUS tdiFailIrp(PTDI_CONN_REQUEST request);
NTSTATUS tdiConnectionOk(PTDI_CONN_REQUEST request);
VOID tdiInitFirewall( );
VOID tdiCloseFirewall( );
void InjectLoopbackRequest( PFW_REQUEST pFwRequest);
void CloneLoopbackRequest( PFW_REQUEST pFwRequest, PFW_REQUEST *ppClonedFwRequest );
void FreeLoopbackRequest( PFW_REQUEST pClonedRequest );

#endif // __FIREWALL_H__

