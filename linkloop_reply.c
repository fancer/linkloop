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
#include <net/if.h>
#include <sys/select.h>
#include "config.h"
#include "linkloop.h"

/*
 * This program runs as a deamon, listening at the llc level on network interfaces 
 * passed on the command line, and responding by sending back a test packet.
 */
 
static char	*program = NULL;

u_int8_t	mac_listened[MAX_IFACES][IFHWADDRLEN];
u_int8_t	mac_dst[IFHWADDRLEN];
u_int8_t	mac_src[IFHWADDRLEN];

int main(int argc, char *argv[]) {
	int sock;
	size_t len;
	struct llc_packet spack;
	struct llc_packet rpack;
    int nif = argc -1; 	/* number of interfaces to listen */
    int i = nif;	/* loop iterator */
	
	program = argv[0];

	if(nif == 0 || nif > MAX_IFACES)
	{
		/* one parameter expected at least: the lan interface name to listen */
		printf("Expecting a list of one to %d interfaces to listen; eg: %s eth0 eth3\n", MAX_IFACES, program);
		return 1;
	}

	/* Open the socket */
	printf ("Opening a socket\n");
	if ((sock = socket(AF_INET, SOCK_PACKET, htons(ETH_P_802_2))) == -1) {
		perror("socket");
		return 1;
	}
	
	/* Getting mac addresses for all listened interfaces */
	for(; i--;)
	{
		get_hwaddr(sock, argv[i+1], mac_listened[i]);
	}

	/* listen and reply forever */
	do {
		len = recv_packet(sock, &rpack);
		memcpy(mac_src, rpack.eth_hdr.ether_shost, IFHWADDRLEN);
		memcpy(mac_dst, rpack.eth_hdr.ether_dhost, IFHWADDRLEN);
		
		/* check against listened interfaces */
		for(i = nif; i--;)
		{
			/* check if this packet has been received on a listened interface */
			if(memcmp(mac_listened[i], mac_dst, IFHWADDRLEN))
				continue;
				
			/* return a test packet to the sender */
			printf("Received packet on %s\n", argv[i+1]);
			mk_test_packet(&spack, mac_dst, mac_src, len, 1);
			send_packet(sock, argv[i+1], &spack);
		}
	} while(1);
	return 0;
}
