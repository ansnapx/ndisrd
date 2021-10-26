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

#ifndef __ETHER_H__
#define __ETHER_H__

#define	ETHER_ADDR_LENGTH		6
#define	ETHER_TYPE_LENGTH		2
#define	MAX_802_3_LENGTH		1500	// Maximum Value For 802.3 Length Field
#define	MAX_ETHER_SIZE			1514	// Maximum Ethernet Packet Length
#define	MIN_ETHER_SIZE	  		60		// Minimum Ethernet Packet Length
#define MAX_TCP_HEADER_SIZE     60
#define MAX_ICMP_PACKET_SIZE    2064
/* Offsets Into Ethernet 802.3 Medium Access Control (MAC) Packet Header
------------------------------------------------------------------------ */
#define	MDstAddr	0								// Offset To Destination Address
#define	MSrcAddr	ETHER_ADDR_LENGTH				// Offset To Source Address
#define	MLength		(MSrcAddr + ETHER_ADDR_LENGTH)	// Of Bytes Following MAC Header
#define	MHdrSize	(MLength + ETHER_TYPE_LENGTH )	// MAC 802.3 Header Size

/* Offsets Into Ethernet 802.2 LLC (Type 1) Packet Header (From MAC Data)
------------------------------------------------------------------------- */
#define	LDSAP			0					// Destination Service Access Point
#define	LSSAP			(LDSAP + 1)			// Source Service Access Point
#define	LCntrl			(LSSAP + 1)			// LLC Control Field
#define	LHdrSize		(LCntrl + 1)		// LLC Header Size


/* Offsets Into Sub-Network Access Protocol (SNAP) Header (From MAC Data)
------------------------------------------------------------------------- */
#define	SType				LHdrSize		// SNAP Type
#define	SHdrSize			(SType + 5)		// SNAP Type Size

#ifndef ETHERTYPE_PUP
#define	ETHERTYPE_PUP		0x0200	/* PUP protocol */
#endif
#ifndef ETHERTYPE_IP
#define	ETHERTYPE_IP		0x0800	/* IP protocol */
#endif
#ifndef ETHERTYPE_ARP
#define ETHERTYPE_ARP		0x0806	/* Addr. resolution protocol */
#endif
#ifndef ETHERTYPE_NS
#define ETHERTYPE_NS		0x0600
#endif
#ifndef	ETHERTYPE_SPRITE
#define	ETHERTYPE_SPRITE	0x0500
#endif
#ifndef ETHERTYPE_TRAIL
#define ETHERTYPE_TRAIL		0x1000
#endif
#ifndef	ETHERTYPE_DECEXP
#define	ETHERTYPE_DECEXP	0x6000
#endif
#ifndef	ETHERTYPE_MOPDL
#define	ETHERTYPE_MOPDL		0x6001
#endif
#ifndef	ETHERTYPE_MOPRC
#define	ETHERTYPE_MOPRC		0x6002
#endif
#ifndef	ETHERTYPE_DN
#define	ETHERTYPE_DN		0x6003
#endif
#ifndef	ETHERTYPE_LAT
#define	ETHERTYPE_LAT		0x6004
#endif
#ifndef ETHERTYPE_SCA
#define ETHERTYPE_SCA		0x6007
#endif
#ifndef ETHERTYPE_REVARP
#define ETHERTYPE_REVARP	0x8035
#endif
#ifndef	ETHERTYPE_LANBRIDGE
#define	ETHERTYPE_LANBRIDGE	0x8038
#endif
#ifndef	ETHERTYPE_DECDNS
#define	ETHERTYPE_DECDNS	0x803c
#endif
#ifndef	ETHERTYPE_DECDTS
#define	ETHERTYPE_DECDTS	0x803e
#endif
#ifndef	ETHERTYPE_VEXP
#define	ETHERTYPE_VEXP		0x805b
#endif
#ifndef	ETHERTYPE_VPROD
#define	ETHERTYPE_VPROD		0x805c
#endif
#ifndef ETHERTYPE_ATALK
#define ETHERTYPE_ATALK		0x809b
#endif
#ifndef ETHERTYPE_AARP
#define ETHERTYPE_AARP		0x80f3
#endif
#ifndef	ETHERTYPE_LOOPBACK
#define	ETHERTYPE_LOOPBACK	0x9000
#endif

#endif // __ETHER_H__