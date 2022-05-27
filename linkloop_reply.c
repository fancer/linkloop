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
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <sys/select.h>
#include "config.h"
#include "linkloop.h"

/*
 * This program runs as a deamon, listening at the llc level on network interfaces 
 * passed on the command line, and responding by sending back a test packet.
 */

/* Linkloop reply configuration */
static struct linkloop_reply {
	const char *program;

	int ifindex_listen[MAX_IFACES];
	u_int8_t mac_listen[MAX_IFACES][IFHWADDRLEN];
	u_int8_t src_mac[IFHWADDRLEN];
	u_int8_t dst_mac[IFHWADDRLEN];
} llr;
 
int main(int argc, char *argv[]) {
	struct llc_packet spack;
	struct llc_packet rpack;
	struct sockaddr_ll sll;
	int nif = argc - 1;	/* number of interfaces to listen */
	size_t len;
	int sock;
	int i;
	
	llr.program = argv[0];

	if (nif == 0 || nif > MAX_IFACES) {
		/* one parameter expected at least: the lan interface name to listen */
		fprintf(stderr, "Expecting a list of one to %d interfaces to listen; eg: %s eth0 eth3\n",
			MAX_IFACES, llr.program);
		return 1;
	}

	/* Open the socket */
	if ((sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_802_2))) == -1) {
		perror("socket");
		return 1;
	}
	
	/* Getting mac addresses for all listened interfaces */
	for(i = 0; i < nif; ++i) {
		get_hwaddr(sock, argv[i+1], &llr.ifindex_listen[i], llr.mac_listen[i]);
	}

	/* listen and reply forever */
	do {
		len = recv_packet(sock, &rpack);
		memcpy(llr.src_mac, rpack.eth_hdr.ether_shost, IFHWADDRLEN);
		memcpy(llr.dst_mac, rpack.eth_hdr.ether_dhost, IFHWADDRLEN);

		/* check against listened interfaces */
		for(i = 0; i < nif; ++i)
		{
			/* check if this packet has been received on a listened interface */
			if(memcmp(llr.mac_listen[i], llr.dst_mac, IFHWADDRLEN))
				continue;

		        /* skip replies */
			if(rpack.llc.dsap == 0x80) 
				continue;

			/* return a test packet to the sender */
			printf("Received packet on %s\n", argv[i+1]);
			mk_test_packet(&spack, &sll, llr.dst_mac, llr.ifindex_listen[i], llr.src_mac, len, 1);
			send_packet(sock, &spack, &sll);
		}
	} while(1);

	return 0;
}
