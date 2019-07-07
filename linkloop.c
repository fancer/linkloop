/*
 * linkloop: Loopback testing for layer-2 connectivity
 *           via 802.2LLC TEST message.
 *
 * Tested against HPUX-11.0
 *
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

static char rcsid[] = "$Id: linkloop.c,v 0.4 2005/04/18 08:10:09 oron Exp $";

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/if.h>	/* for IF_NAMESIZE, IFHWADDRLEN */
#include <arpa/inet.h>
#include "config.h"
#include "linkloop.h"

/* Linkloop configuration */
static struct linkloop {
	const char *program;

	size_t pack_size;
	int timeout;
	int retries;

	const char *src_iface;
	int src_ifindex;
	u_int8_t src_mac[IFHWADDRLEN];
	const char *dst_mac_str;
	u_int8_t dst_mac[IFHWADDRLEN];

	unsigned int total_sent;
	unsigned int total_good;
	unsigned int total_timeout;
	unsigned int total_bad;
} ll = {
	.pack_size = ETH_DATA_LEN,	/* 0x05DC == 1500 */
	.timeout = 2,
	.retries = 1,
	.src_iface = "eth0"
};

void usage() {
	fprintf(stderr, "Usage: %s [option...] mac_addr\n"
		"\t-d		Debug\n"
		"\t-i<iface>	Network interface\n"
		"\t-t<timeout>	Timeout between packets\n"
		"\t-n<retries>	Number of retries\n"
		"\t-s<size>	Packet size in bytes\n"
		, ll.program
	);
	exit(1);
}

void handle_options(int argc, char * const argv[]) {
	int c;

	ll.program = argv[0];
	while((c = getopt(argc, argv, "di:t:n:s:")) != -1)
		switch(c) {
		case 'd':
			debug_flag = 1;
			break;
		case 'i':
			ll.src_iface = optarg;
			break;
		case 't':
			ll.timeout = strtol(optarg, NULL, 0);
			break;
		case 'n':
			ll.retries = strtol(optarg, NULL, 0);
			break;
		case 's':
			ll.pack_size = strtol(optarg, NULL, 0);
			if(ll.pack_size > ETH_DATA_LEN) {
				fprintf(stderr, "%s: Illegal size specified %d > %d (ETH_DATA_LEN)\n",
					ll.program, ll.pack_size, ETH_DATA_LEN);
				exit(1);
			}
			break;
		default:
			fprintf(stderr, "%s: unknown option code 0x%x\n", ll.program, c);
		case '?':
			usage();
		}
	if(debug_flag)
		fprintf(stderr, "interface=%s timeout=%d num=%d size=%d\n",
			ll.src_iface, ll.timeout, ll.retries, ll.pack_size);
	if(optind != argc - 1) {
		fprintf(stderr, "%s: missing target address\n", ll.program);
		usage();
	}
	ll.dst_mac_str = argv[optind];
}

void sig_alarm(int signo) {
}

static void set_sighandlers() {
	struct sigaction sa;

	sa.sa_handler = sig_alarm;
	sa.sa_flags = 0;
	sigfillset(&sa.sa_mask);	/* Block spurious signals during handler */
	sigdelset(&sa.sa_mask, SIGINT);	/* But normal signals should kill us */
	sigdelset(&sa.sa_mask, SIGQUIT);
	sigdelset(&sa.sa_mask, SIGTERM);
	if(sigaction(SIGALRM, &sa, NULL) < 0) {
		perror("sigaction");
		exit(1);
	}
}

static int linkloop(int sock) {
	struct llc_packet spack;
	struct llc_packet rpack;
	struct sockaddr_ll sll;
	int ret;

	mk_test_packet(&spack, &sll, ll.src_mac, ll.src_ifindex, ll.dst_mac, ll.pack_size, 0);
	if(alarm(ll.timeout) < 0) {
		perror("alarm");
		exit(1);
	}
	send_packet(sock, &spack, &sll);
	ll.total_sent++;
	ret = recv_packet(sock, &rpack);
	if(ret == 0) {		/* timeout */
		fprintf(stderr, "  ** TIMEOUT (%d seconds)\n", ll.timeout);
		ll.total_timeout++;
		return 0;
	}
	if(spack.eth_hdr.ether_type != rpack.eth_hdr.ether_type) {
		ll.total_bad++;
		printf("  ** BAD RECEIVED LENGTH = %d\n", rpack.eth_hdr.ether_type);
		return 0;
	}
	if(memcmp(spack.eth_hdr.ether_dhost, rpack.eth_hdr.ether_shost, IFHWADDRLEN) != 0 &&
	   memcmp(spack.eth_hdr.ether_shost, rpack.eth_hdr.ether_shost, IFHWADDRLEN) != 0) {
		ll.total_bad++;
		printf("  ** ROGUE RESPONDER: received from %s\n",
			mac2str(rpack.eth_hdr.ether_shost));
		return 0;
	}
	if(memcmp(spack.data, rpack.data, DATA_SIZE(ll.pack_size)) != 0) {
		ll.total_bad++;
		printf("  ** BAD RESPONSE\n");
		dump_packet(&rpack);
		return 0;
	}
	ll.total_good++;
	return 1;
}

int main(int argc, char * const argv[]) {
	int sock, i;

	handle_options(argc, argv);

	if(!parse_address(ll.dst_mac, ll.dst_mac_str)) {
		fprintf(stderr, "%s: bad DST address - %s\n", ll.program, ll.dst_mac_str);
		return 1;
	}

	printf("Link connectivity to LAN station: %s (HW addr %s)\n", ll.dst_mac_str, mac2str(ll.dst_mac));

	/* Open a socket */
	if((sock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_802_2))) == -1) {
		perror("socket");
		return 1;
	}
	if(debug_flag)
		fprintf(stderr, "Getting MAC address of interface '%s'\n", ll.src_iface);
	get_hwaddr(sock, ll.src_iface, &ll.src_ifindex, ll.src_mac);
	if(debug_flag)
		fprintf(stderr, "Testing via %s (HW addr %s)\n", ll.src_iface, mac2str(ll.src_mac));

	set_sighandlers();
	for(i = 0; i < ll.retries; i++) {
		if(linkloop(sock))
			;
		if(debug_flag)
			printf("Retry %d...\n", i);
	}

	if(ll.total_sent == ll.total_good) {
		printf("  -- OK -- %d packets\n", ll.total_sent);
		return 0;
	} else if (ll.total_sent == ll.total_timeout) {
		printf("  -- NO RESPONSE --\n");
	} else {
		printf("  -- %u packets transmitted, %u received, %u timed out, %u bad --\n",
			ll.total_sent, ll.total_good, ll.total_timeout, ll.total_bad);
	}

	return 1;
}
