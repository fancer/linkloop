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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include "config.h"
#include "linkloop.h"

/* Linkloop configuration */
static struct lanscan {
	char displayHeader; 	/* display header before listing interfaces */
	char displayMac; 	/* display the MAC address */
	char displayNames;	/* display interface names */
	char displayState;	/* display interface states */
	char displayOneline;	/* display one line summary */

} ls = {
	.displayHeader = 1,
	.displayMac = 1,
	.displayNames = 1,
	.displayState = 1,
	.displayOneline = 0
};

static struct if_flag {
	short int flag;
	char *flag_name;
} if_flags[] = {
	{ IFF_UP, "UP" },
	{ IFF_BROADCAST, "BROADCAST" },
	{ IFF_DEBUG, "DEBUG" },
	{ IFF_LOOPBACK, "LOOPBACK" },
	{ IFF_POINTOPOINT, "POINTOPOINT" },
	{ IFF_NOTRAILERS, "NOTRAILERS" },
	{ IFF_RUNNING, "RUNNING" },
	{ IFF_NOARP, "NOARP" },
	{ IFF_PROMISC, "PROMISC" },
	{ IFF_ALLMULTI, "ALLMULTI" },
	{ IFF_MASTER, "MASTER" },
	{ IFF_SLAVE, "SLAVE" },
	{ IFF_MULTICAST, "MULTICAST" },
	{ IFF_PORTSEL, "PORTSEL" },
	{ IFF_AUTOMEDIA, "AUTOMEDIA" },
};

static short int get_ifflags(int sock, const char name[]) {
	struct ifreq ifr;

	strncpy(ifr.ifr_name, name, IFNAMSIZ);
	if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0) {
		perror("ioctl(SIOCGIFFLAGS)");
		exit(1);
	}
	return ifr.ifr_flags;
}

static void usage(const char *program) {
	fprintf(stderr, "Usage: %s -[option...]\n"
		"\t-h		Print this help text\n"
		"\t-?		Print this help text\n"
		"\t-i<face>	Print network interface names\n"
		"\t-a<mAc>	Print network interface MAC address\n"
		"\t-s<tate>	Print network interface state\n"
		"\t-o<ne>	Print one line summary, interface names only\n"
		, program
	);
	exit(1);
}

int main(int argc, char *argv[]) {
	struct ifconf ifc;
	struct ifreq ifr_x[MAX_IFACES];
	int sock, err, ifindex, i;
	u_int8_t myMAC[IFHWADDRLEN];
	short int flags;

	/* check options if any */
	if(argc > 1) {
		/* unsupported options or help */
		if(strpbrk(argv[1], "aiso") == NULL) {
			usage(argv[0]);
		}
		
		/* supported options provided: turn the header display off */
		ls.displayHeader = 0;
		if (strchr(argv[1], 'i') == NULL)
			ls.displayNames = 0;
		if (strchr(argv[1], 'a') == NULL)
			ls.displayMac = 0;
		if (strchr(argv[1], 's') == NULL)
			ls.displayState = 0;
		if (strchr(argv[1], 'o')) {
			ls.displayOneline = 1;
			ls.displayState = ls.displayMac = ls.displayNames = 0;
		}
	}
	
	if ((sock = socket(PF_PACKET, SOCK_PACKET, 0)) < 0) {
		perror("socket");
		exit(1);
	}

	ifc.ifc_len = MAX_IFACES * sizeof(struct ifreq);
	ifc.ifc_req = ifr_x;
	if ((err = ioctl(sock, SIOCGIFCONF, &ifc)) < 0) {
		perror("ioctl");
		exit(1);
	}
	if (ls.displayHeader)
		printf("retrieved info for %i interface(s)\n", ifc.ifc_len / sizeof(struct ifreq));

	for (err = 0; err < ifc.ifc_len / sizeof(struct ifreq); err++) {
		flags = get_ifflags(sock, ifr_x[err].ifr_name);
		get_hwaddr(sock, ifr_x[err].ifr_name, &ifindex, myMAC);
		
		/* Discard local loopback port */
		if (strcmp(ifr_x[err].ifr_name, "lo") == 0)
			continue;
			
		/* Discard alias ports, ie name including a column ':' */
		if (strchr(ifr_x[err].ifr_name, ':'))
			continue;
			
		if (ls.displayOneline) {
			printf("%s\t", ifr_x[err].ifr_name);
			continue;
		}
		
		if (ls.displayMac)
			printf("%s\t", mac2str(myMAC));

		/* here, in order to mimic the HP UX output, we print the interface name,
		 * interface physical point of attachement, physical point ID
		 * as the same "interface name"
		 */
		if (ls.displayNames) {
			printf("%s %s %s\t", ifr_x[err].ifr_name, ifr_x[err].ifr_name,
					     ifr_x[err].ifr_name);
		}

		if (ls.displayState) {
			putchar('<');
			for (i = 0; i < sizeof(if_flags)/sizeof(if_flags[0]); i++) {
				if (flags & if_flags[i].flag)
					printf("%s ", if_flags[i].flag_name);
			}
			putchar('>');
		}
		putchar('\n');
	}

	return EXIT_SUCCESS;
}
