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

#ifndef __PACKET_H__
#define __PACKET_H__

typedef struct _NDISHK_PACKET_RESERVED
{
	LIST_ENTRY			m_qLink;
	NDIS_STATUS			m_nTransferDataStatus;
	UINT				m_nBytesTransferred;
	UCHAR				m_IBuffer[MAX_ETHER_SIZE];

} NDISHK_PACKET_RESERVED, *PNDISHK_PACKET_RESERVED;

typedef struct _NDISHK_PACKET
{
	NDIS_PACKET				m_Packet;
	NDISHK_PACKET_RESERVED	m_Reserved;

} NDISHK_PACKET, *PNDISHK_PACKET;

#endif