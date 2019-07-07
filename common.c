/*
 * Written by Oron Peled <oron@actcom.co.il>
 * 
 * Modified by Dominique Domet de Mont, 2007, Dominique.Domet-de-Mont@hp.com
 * Copyright (C) 2007 Hewlett-Packard Development Company, L.P.
 *
 * Some code snippets adapted from spak
 * (http://www.xenos.net/software/spak/)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include "config.h"

#include <netinet/in.h>		/* for htons(3)			*/
#include <linux/if_packet.h>

#if HAVE_ETHER_HOSTTON
#include <netinet/ether.h>	/* for ether_hostton(3)		*/
#endif

#include <net/if.h>		/* for IF_NAMESIZE, IFHWADDRLEN */
#include <sys/ioctl.h>		/* for SIOCGIFHWADDR		*/
#include <stdio.h>
#include <string.h>     	/* for memcmp(3) bzero(3)	*/
#include <stdlib.h>		/* for exit(3)			*/
#include <errno.h>
#include <assert.h>

#include "linkloop.h"

/* These are used to define different data structs... */
#if IFHWADDRLEN != ETH_ALEN
#error "SOMETHING IS VERY FISHY: IFHWADDRLEN != ETH_ALEN"
#endif

int debug_flag = 0;

void dump_packet(struct llc_packet *pack) {
	int i;
	u_int8_t *p = (u_int8_t *)pack;
	size_t len = ntohs(pack->eth_hdr.ether_type);

	printf("PACKET DUMP: data size=%d (0x%x)", len, len);
	for (i = 0; i < len; i++, p++) {
		if ((i % 16) == 0)
			printf("\n%04x\t", i);
		printf("%02x ", (unsigned)*p);
	}
	printf("\nEND PACKET DUMP\n");
}

char *mac2str(u_int8_t *s) {
	static char buf[3*ETH_ALEN];

	sprintf(buf, "0x%02X%02X%02X%02X%02X%02X",
		s[0], s[1], s[2], s[3], s[4], s[5]);
	return buf;
}

int parse_address(u_int8_t mac[], const char *str) {
	unsigned a, b, c, d, e, f;
	struct ether_addr ea;

	if (sscanf(str,"%x:%x:%x:%x:%x:%x", &a, &b, &c, &d, &e, &f) == 6) {
		/* A colon separated notation */
		mac[0] = (unsigned char) a;
		mac[1] = (unsigned char) b;
		mac[2] = (unsigned char) c;
		mac[3] = (unsigned char) d;
		mac[4] = (unsigned char) e;
		mac[5] = (unsigned char) f;
	} else if (sscanf(str, "0x%02x%02x%02x%02x%02x%02x", &a, &b, &c, &d, &e, &f) == 6) {
		/* Hexadecimal notation (like HPUX) */
		mac[0] = (unsigned char) a;
		mac[1] = (unsigned char) b;
		mac[2] = (unsigned char) c;
		mac[3] = (unsigned char) d;
		mac[4] = (unsigned char) e;
		mac[5] = (unsigned char) f;
#if HAVE_ETHER_HOSTTON
	} else if (ether_hostton(str, &ea) == 0) {
		/* A name from /etc/ethers */
		memcpy(mac, ea.ether_addr_octet, 8);
#endif
	} else
		return 0;
	return 1;
}

void get_hwaddr(int sock, const char iface[], int *ifindex, u_int8_t mac[]) {
	struct ifreq ifr;

	strncpy(ifr.ifr_name, iface, IFNAMSIZ);
	if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
		perror("ioctl(SIOCGIFHWADDR)");
		exit(1);
	}
	memcpy(mac, ifr.ifr_hwaddr.sa_data, IFHWADDRLEN);

	if (ioctl(sock, SIOCGIFINDEX, &ifr) < 0) {
		perror("ioctl(SIOCGIFINDEX)");
		exit(1);
	}
	*ifindex = ifr.ifr_ifindex;
}

void mk_test_packet(struct llc_packet *pack, struct sockaddr_ll *sll,
		    const u_int8_t src_mac[], int src_ifindex,
		    const u_int8_t dst_mac[], size_t len, int response) {
	int i;

	bzero(sll, sizeof(*sll));
	sll->sll_family = AF_PACKET;
	sll->sll_protocol = htons(ETH_P_802_2);
	sll->sll_ifindex = src_ifindex;
	sll->sll_halen = ETH_ALEN;
	memcpy(sll->sll_addr, dst_mac, ETH_ALEN);

	assert(len <= ETH_DATA_LEN);			/* 0x05DC == 1500 */
	memcpy(pack->eth_hdr.ether_dhost, dst_mac, IFHWADDRLEN);
	memcpy(pack->eth_hdr.ether_shost, src_mac, IFHWADDRLEN);
	pack->eth_hdr.ether_type = htons(len);
	pack->llc.dsap = (response) ? 0x80 : 0x00;
	pack->llc.ssap = (response) ? 0x01 : 0x80;	/* XNS? */
	pack->llc.ctrl = TEST_CMD;			/* TEST */

	for (i = 0; i < DATA_SIZE(len); i++)
		pack->data[i] = i;
}

void send_packet(int sock, struct llc_packet *pack, struct sockaddr_ll *sll) {
	int ret;

	/* Send the packet */
	ret = sendto(sock, pack, FRAME_SIZE(pack), 0, (struct sockaddr *)sll, sizeof(*sll));
	if (ret == -1) {
		perror("sendto");
		exit(1);
	}
	if (ret != FRAME_SIZE(pack))
		fprintf(stderr, "Warning: Incomplete packet sent\n");

	if (debug_flag) {
		printf("sent TEST packet (%d bytes) to %s\n",
			ntohs(pack->eth_hdr.ether_type), mac2str(pack->eth_hdr.ether_dhost));
	}
}

int recv_packet(int sock, struct llc_packet *pack) {
	struct sockaddr_ll sll = {0};
	socklen_t len;
	int ret;

	len = sizeof(sll);
	ret = recvfrom(sock, pack, sizeof(*pack), 0, (struct sockaddr *)&sll, &len);
	if (ret == -1) {
		if(errno == EINTR)	/* We have a timeout */
			return 0;
		perror("recvfrom");
		exit(1);
	}

	if ((pack->llc.ctrl & TEST_CMD) != TEST_CMD) {
		fprintf(stderr, "got unexpected packet\n");
		dump_packet(pack);
		/* continue anyway ! exit(1);*/
	}
	ret = ntohs(pack->eth_hdr.ether_type);
	if (debug_flag)
		printf("received TEST packet (%d bytes) from %s\n", ret, mac2str(pack->eth_hdr.ether_shost));

	return ret;
}
