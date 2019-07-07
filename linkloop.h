/*
 * Written by Oron Peled <oron@actcom.co.il>
 *
 * Some code snippets adapted from spak
 * (http://www.xenos.net/software/spak/)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#ifndef	LINKLOOP_H
#define	LINKLOOP_H

#include <linux/if_packet.h>
#include <netinet/if_ether.h>

#define DATA_SIZE(_pack_size)	(_pack_size - sizeof(struct llc))
#define	MAX_DATA_SIZE		DATA_SIZE(ETH_DATA_LEN)	/* This is what HPUX sends and responds to */
#define FRAME_SIZE(_pack)	(ntohs((_pack)->eth_hdr.ether_type) + sizeof(struct ether_header))

#define	MAX_IFACES	20

/* Describe an 802.2LLC packet */
struct llc {
	unsigned char dsap;
	unsigned char ssap;
	unsigned char ctrl;
} __attribute__ ((__packed__));

struct llc_packet {
	struct ether_header eth_hdr;
	struct llc llc;
	/* Some test data */
	unsigned char data[MAX_DATA_SIZE];
} __attribute__ ((__packed__));

#define	TEST_CMD	0xE3	/* From 802.2LLC (HPUX sends 0xF3) */

extern int debug_flag;

void dump_packet(struct llc_packet *pack);
char *mac2str(u_int8_t *s);
int parse_address(u_int8_t mac[], const char *str);
void get_hwaddr(int sock, const char name[], int *ifindex, u_int8_t mac[]);
void mk_test_packet(struct llc_packet *pack, struct sockaddr_ll *sll,
		    const u_int8_t src_mac[], int src_ifindex,
		    const u_int8_t dst_mac[], size_t len, int response);
void send_packet(int sock, struct llc_packet *pack, struct sockaddr_ll *sll);
int recv_packet(int sock, struct llc_packet *pack);

#endif	/* LINKLOOP_H */
