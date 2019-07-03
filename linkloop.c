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
#include <net/if.h>	/* for IF_NAMESIZE, IFHWADDRLEN */
#include <arpa/inet.h>
#include "config.h"
#include "linkloop.h"

/* Statistic counters */
static unsigned total_sent = 0;
static unsigned total_good = 0;
static unsigned total_timeout = 0;
static unsigned total_bad = 0;

/* Configurable options */
size_t	pack_size = ETH_DATA_LEN;	/* 0x05DC == 1500 */
int	timeout = 2;
int	retries = 1;
char	*iface = "eth0";

extern int	debug_flag;

#define	OPTSTRING	"di:t:n:s:"
const char *program = NULL;
const char *arg_addr;

void usage() {
	fprintf(stderr, "Usage: %s [option...] mac_addr\n"
		"\t-d		Debug\n"
		"\t-i<iface>	Network interface\n"
		"\t-t<timeout>	Timeout between packets\n"
		"\t-n<retries>	Number of retries\n"
		"\t-s<size>	Packet size in bytes\n"
		, program
	);
	exit(1);
}

void handle_options(int argc, char * const argv[]) {
	int c;

	program = argv[0];
	while((c = getopt(argc, argv, OPTSTRING)) != -1)
		switch(c) {
		case 'd':
			debug_flag = 1;
			break;
		case 'i':
			iface = optarg;
			break;
		case 't':
			timeout = strtol(optarg, NULL, 0);
			break;
		case 'n':
			retries = strtol(optarg, NULL, 0);
			break;
		case 's':
			pack_size = strtol(optarg, NULL, 0);
			if(pack_size > ETH_DATA_LEN) {
				fprintf(stderr, "%s: Illegal size specified (%d) > ETH_DATA_LEN (%d)\n",
					program, pack_size, ETH_DATA_LEN);
				exit(1);
			}
			break;
		default:
			fprintf(stderr, "%s: unknown option code 0x%x\n", program, c);
		case '?':
			usage();
		}
	if(debug_flag)
		fprintf(stderr, "interface=%s timeout=%d num=%d size=%d\n",
			iface, timeout, retries, pack_size);
	if(optind != argc - 1) {
		fprintf(stderr, "%s: missing target address\n");
		usage();
	}
	arg_addr = argv[optind];
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

static int linkloop(int sock, const u_int8_t mac_src[], const u_int8_t mac_dst[]) {
	int ret;
	struct llc_packet spack;
	struct llc_packet rpack;
	int datasize = pack_size - sizeof(struct llc) - sizeof(struct ether_header);

	mk_test_packet(&spack, mac_src, mac_dst, pack_size, 0);
	if(alarm(timeout) < 0) {
		perror("alarm");
		exit(1);
	}
	send_packet(sock, iface, &spack);
	total_sent++;
	ret = recv_packet(sock, &rpack);
	if(ret == 0) {		/* timeout */
		fprintf(stderr, "  ** TIMEOUT (%d seconds)\n", timeout);
		total_timeout++;
		return 0;
	}
	if(spack.eth_hdr.ether_type != rpack.eth_hdr.ether_type) {
		total_bad++;
		printf("  ** BAD RECEIVED LENTH = %d\n", rpack.eth_hdr.ether_type);
		return 0;
	}
	if(memcmp(spack.eth_hdr.ether_dhost, rpack.eth_hdr.ether_shost, IFHWADDRLEN) != 0) {
		total_bad++;
		printf("  ** ROGUE RESPONDER: received from %s\n",
			mac2str(rpack.eth_hdr.ether_shost));
		return 0;
	}
	if(memcmp(spack.data, rpack.data, datasize) != 0) {
		total_bad++;
		printf("  ** BAD RESPONSE\n");
		dump_packet(&rpack);
		return 0;
	}
	total_good++;
	return 1;
}

int main(int argc, char * const argv[]) {
	int sock;
	int i;

	u_int8_t mac_src[IFHWADDRLEN];
	u_int8_t mac_dst[IFHWADDRLEN];

	handle_options(argc, argv);

	if(!parse_address(mac_dst, arg_addr)) {
		fprintf(stderr, "%s: bad DST address - %s\n", program, arg_addr);
		return 1;
	}

	printf("Link connectivity to LAN station: %s (HW addr %s)\n", arg_addr, mac2str(mac_dst));

	/* Open a socket */
	if((sock = socket(AF_INET, SOCK_PACKET, htons(ETH_P_802_2))) == -1) {
		perror("socket");
		return 1;
	}
	if(debug_flag)
		fprintf(stderr, "Getting MAC address of interface '%s'\n", iface);
	get_hwaddr(sock, iface, mac_src);
	if(debug_flag)
		fprintf(stderr, "Testing via %s (HW addr %s)\n", iface, mac2str(mac_src));

	set_sighandlers();
	for(i = 0; i < retries; i++) {
		if(linkloop(sock, mac_src, mac_dst))
			;
		if(debug_flag)
			printf("Retry %d...\n", i);
	}

	if(total_sent == total_good)
		printf("  -- OK -- %d packets\n", total_sent);
	if(total_good + total_bad == 0)
		printf("  -- NO RESPONSE --\n");
	return 0;
}
