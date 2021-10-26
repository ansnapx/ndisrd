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
#ifndef __IPHLP_H__
#define __IPHLP_H__

#pragma pack(1) 

///////////////////////////////////////////////////////////////////////////
// packet structures
///////////////////////////////////////////////////////////////////////////
typedef long n_long;
typedef short n_short;
typedef long n_time;
typedef unsigned short u_short;
typedef unsigned long u_long;
typedef unsigned char u_char;

#define ETH_ALEN		6		/* Octets in one ethernet addr	 */
#define ETH_P_ARP		0x0806	/* Address Resolution packet	*/

#define ETH_P_IP		0x0800	/* Internet Protocol packet	*/
#define ETH_P_RARP      0x8035		/* Reverse Addr Res packet	*/
#define ETH_P_ARP		0x0806		/* Address Resolution packet	*/
#define ETH_P_TRAIL		0x1000
#define ETH_P_APOLLO	0x8019


#define NETH_P_IP		0x0008	/* Internet Protocol packet	*/
#define NETH_P_RARP     0x3580		/* Reverse Addr Res packet	*/
#define NETH_P_ARP		0x0608		/* Address Resolution packet	*/
#define NETH_P_TRAIL	0x0010
#define NETH_P_APOLLO	0x1980

#define ARPHRD_ETHER    1               /* Ethernet 10Mbps              */
#define ARPHRD_IEEE802  6               /* IEEE 802.2 Ethernet/TR/TB    */

#define ARPOP_REQUEST   1               /* ARP request                  */
#define ARPOP_REPLY     2               /* ARP reply                    */
#define ARPOP_RREQUEST  3               /* RARP request                 */
#define ARPOP_RREPLY    4               /* RARP reply                   */
#define ARPOP_InREQUEST 8               /* InARP request                */
#define ARPOP_InREPLY   9               /* InARP reply                  */
#define ARPOP_NAK       10              /* (ATM)ARP NAK                 */

/*
 * Protocols
 */
#define IPPROTO_IP              0               /* dummy for IP */
#define IPPROTO_ICMP            1               /* control message protocol */
#define IPPROTO_IGMP            2               /* group management protocol */
#define IPPROTO_GGP             3               /* gateway^2 (deprecated) */
#define IPPROTO_TCP             6               /* tcp */
#define IPPROTO_PUP             12              /* pup */
#define IPPROTO_UDP             17              /* user datagram protocol */
#define IPPROTO_IDP             22              /* xns idp */
#define IPPROTO_ND              77              /* UNOFFICIAL net disk proto */

#define IPPROTO_RAW             255             /* raw IP packet */
#define IPPROTO_MAX             256
#define IPPROTO_ANY             0

#define	IP_OFFMASK 0x3FFF		/* mask for fragmenting bits */

#define AF_INET					2

#define INADDR_ANY              (ULONG)0x00000000
#define INADDR_LOOPBACK         0x7f000001
#define INADDR_BROADCAST        (ULONG)0xffffffff
#define INADDR_NONE             0xffffffff

#ifndef _WS2DEF_
struct   in_addr {
    union   {
         struct{
             unsigned  char   s_b1,
                              s_b2,
                              s_b3,
                              s_b4;
        }  S_un_b;
             struct  {
             unsigned  short  s_w1,
                              s_w2;
              }  S_un_w;
               unsigned long  S_addr;
     } S_un;
};

struct sockaddr {
    unsigned short sa_family;
    char           sa_data[14];
};
#endif // _WS2DEF_

// Ethernet Header
typedef struct ether_header 
{
	unsigned char	h_dest[ETH_ALEN];	/* destination eth addr	*/
	unsigned char	h_source[ETH_ALEN];	/* source ether addr	*/
	unsigned short	h_proto;		/* packet type ID field	*/
} ether_header, *ether_header_ptr;

typedef struct arphdr
{
	unsigned short	ar_hrd;		/* format of hardware address	*/
	unsigned short	ar_pro;		/* format of protocol address	*/
	unsigned char	ar_hln;		/* length of hardware address	*/
	unsigned char	ar_pln;		/* length of protocol address	*/
	unsigned short	ar_op;		/* ARP opcode (command)		*/
} arphdr, *arphdr_ptr;

typedef struct	ether_arp 
{
	struct	arphdr ea_hdr;	/* fixed-size header */
	u_char	arp_sha[ETH_ALEN];	/* sender hardware address */
	u_char	arp_spa[4];	/* sender protocol address */
	u_char	arp_tha[ETH_ALEN];	/* target hardware address */
	u_char	arp_tpa[4];	/* target protocol address */
} ether_arp, *ether_arp_ptr;

#define	arp_hrd	ea_hdr.ar_hrd
#define	arp_pro	ea_hdr.ar_pro
#define	arp_hln	ea_hdr.ar_hln
#define	arp_pln	ea_hdr.ar_pln
#define	arp_op	ea_hdr.ar_op

/* IP Header in Little Endian */
typedef struct iphdr 
{
	u_char	ip_hl:4,		/* header length */
			ip_v:4;			/* version */
	u_char	ip_tos;			/* type of service */
	short	ip_len;			/* total length */
	u_short	ip_id;			/* identification */
	short	ip_off;			/* fragment offset field */
#define	IP_DF 0x4000		/* dont fragment flag */
#define	IP_MF 0x2000		/* more fragments flag */
	u_char	ip_ttl;			/* time to live */
	u_char	ip_p;			/* protocol */
	u_short	ip_sum;			/* checksum */
	struct in_addr ip_src,ip_dst;	/* source and dest address */
} iphdr, *iphdr_ptr;
/////////////////////////////////////////////////////////////////////////
/* UDP header  */
typedef struct	udphdr
{
	u_short	th_sport;		/* source port */
	u_short	th_dport;		/* destination port */
	u_short	length;			/* data length */
	u_short	th_sum;			/* checksum */
} udphdr, *udphdr_ptr;
/////////////////////////////////////////////////////////////////////////
typedef	u_long	tcp_seq;

// TCP header. Per RFC 793, September, 1981. In Little Endian
typedef struct tcphdr {
	u_short	th_sport;		/* source port */
	u_short	th_dport;		/* destination port */
	tcp_seq	th_seq;			/* sequence number */
	tcp_seq	th_ack;			/* acknowledgement number */
	u_char	th_x2:4,		/* (unused) */
		    th_off:4;		/* data offset */
	u_char	th_flags;
#define	TH_FIN	0x01
#define	TH_SYN	0x02
#define	TH_RST	0x04
#define	TH_PUSH	0x08
#define	TH_ACK	0x10
#define	TH_URG	0x20
#define	TH_CWR	0x40
#define	TH_ECE	0x80
	u_short	th_win;			/* window */
	u_short	th_sum;			/* checksum */
	u_short	th_urp;			/* urgent pointer */
} tcphdr, *tcphdr_ptr;

typedef struct pseudo_header
{
  struct in_addr source_address;
  struct in_addr dest_address;
  unsigned char placeholder;
  unsigned char protocol;
  unsigned short tcp_length;

}pseudo_header, *pseudo_header_ptr;

typedef struct _icmphdr  
{
  u_char		icmp_type;
  u_char		icmp_code;
  u_short		icmp_cksum;
  union 
  {
	struct 
	{
		u_short	id;
		u_short	sequence;
	} echo;
	u_long	gateway;
	struct 
	{
		u_short	__unused;
		u_short	mtu;
	} frag;
  } un;
}icmphdr,*icmphdr_ptr;

// Definition of type and code field values.
//
#define ICMP_TYPE_ANY 55
#define ICMP_CODE_ANY 55

#define	ICMP_ECHOREPLY			0		/* echo reply */
#define	ICMP_UNREACH			3		/* dest unreachable, codes: */
#define		ICMP_UNREACH_NET			0		/* bad net */
#define		ICMP_UNREACH_HOST			1		/* bad host */
#define		ICMP_UNREACH_PROTOCOL		2		/* bad protocol */
#define		ICMP_UNREACH_PORT			3		/* bad port */
#define		ICMP_UNREACH_NEEDFRAG		4		/* IP_DF caused drop */
#define		ICMP_UNREACH_SRCFAIL		5		/* src route failed */
#define		ICMP_UNREACH_NET_UNKNOWN	6		/* unknown net */
#define		ICMP_UNREACH_HOST_UNKNOWN	7		/* unknown host */
#define		ICMP_UNREACH_ISOLATED		8		/* src host isolated */
#define		ICMP_UNREACH_NET_PROHIB		9		/* prohibited access */
#define		ICMP_UNREACH_HOST_PROHIB	10		/* ditto */
#define		ICMP_UNREACH_TOSNET			11		/* bad tos for net */
#define		ICMP_UNREACH_TOSHOST		12		/* bad tos for host */
#define	ICMP_SOURCEQUENCH		4		/* packet lost, slow down */
#define	ICMP_REDIRECT			5		/* shorter route, codes: */
#define		ICMP_REDIRECT_NET			0		/* for network */
#define		ICMP_REDIRECT_HOST			1		/* for host */
#define		ICMP_REDIRECT_TOSNET		2		/* for tos and net */
#define		ICMP_REDIRECT_TOSHOST		3		/* for tos and host */
#define	ICMP_ECHO				8		/* echo service */
#define	ICMP_ROUTERADVERT		9		/* router advertisement */
#define	ICMP_ROUTERSOLICIT		10		/* router solicitation */
#define	ICMP_TIMXCEED			11		/* time exceeded, code: */
#define		ICMP_TIMXCEED_INTRANS		0		/* ttl==0 in transit */
#define		ICMP_TIMXCEED_REASS			1		/* ttl==0 in reass */
#define	ICMP_PARAMPROB			12		/* ip header bad */
#define		ICMP_PARAMPROB_OPTABSENT	1		/* req. opt. absent */
#define	ICMP_TSTAMP				13		/* timestamp request */
#define	ICMP_TSTAMPREPLY		14		/* timestamp reply */
#define	ICMP_IREQ				15		/* information request */
#define	ICMP_IREQREPLY			16		/* information reply */
#define	ICMP_MASKREQ			17		/* address mask request */
#define	ICMP_MASKREPLY			18		/* address mask reply */

/* Codes for REDIRECT. */
#define ICMP_REDIR_NET		0	/* Redirect Net			*/
#define ICMP_REDIR_HOST		1	/* Redirect Host		*/
#define ICMP_REDIR_NETTOS	2	/* Redirect Net for TOS		*/
#define ICMP_REDIR_HOSTTOS	3	/* Redirect Host for TOS	*/

/* Codes for TIME_EXCEEDED. */
#define ICMP_EXC_TTL		0	/* TTL count exceeded		*/
#define ICMP_EXC_FRAGTIME	1	/* Fragment Reass time exceeded	*/

#pragma pack()

#endif // __IPHLP_H__