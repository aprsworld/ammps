#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
 
#include <linux/can.h>
#include <linux/can/raw.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <ncurses.h>

extern char *optarg;
extern int optind, opterr, optopt;

 
/* At time of writing, these constants are not defined in the headers */
#ifndef PF_CAN
#define PF_CAN 29
#endif
 
#ifndef AF_CAN
#define AF_CAN PF_CAN
#endif

typedef union u_float {
	float f;
	uint8_t b[sizeof(float)];
} u_float;

typedef union u_uint32 {
	uint32_t i;
	uint8_t b[sizeof(uint32_t)];
} u_uint32;

float vcsBytesToFloat(uint8_t *b) {
	u_float u;
	
	/* little endian ~9.55 */
	//u.b[0]=0x30;
	//u.b[1]=0xd0;
	//u.b[2]=0x18;
	//u.b[3]=0x41;

	u.b[0]=b[0];
	u.b[1]=b[1];
	u.b[2]=b[2];
	u.b[3]=b[3];

	return u.f;
}

int main(int argc, char **argv) {
	int bytes_read, bytes_sent;
	int queryRegister;
	int i,n;

	int firstRegister=0;
	int lastRegister=39;

	int outputDebug=0;

	char canInterface[128];
	strcpy(canInterface,"can0");

	while ((n = getopt (argc, argv, "hvi:")) != -1) {
		switch (n) {
			case 'h':
				fprintf(stdout,"# -p\tOutput a pretty human readable display to stderr\n");
				fprintf(stdout,"#\n");
				fprintf(stdout,"# -h\tThis help message then exit\n");
				fprintf(stdout,"#\n");
				fprintf(stdout,"# -i\tCAN interface to use (eg can0, can1, etc)\n");
				return 0;
			case 'i':
				strncpy(canInterface,optarg,sizeof(canInterface));
				canInterface[sizeof(canInterface)-1]='\0';
				fprintf(stdout,"# CAN interface = %s\n",canInterface);
				break;
			case 'v':
				outputDebug=1;
				fprintf(stdout,"# verbose (debugging) output to stderr enabled\n");
				break;
		}
	}

	if ( !outputDebug ) {
		fprintf(stdout,"# warning: no output format is selected. Nothing will show.\n");
	}

	/* setup ncurses screen */
	initscr();
	cbreak();
	noecho();
	/* non-blocking non-buffered character reads */
	nodelay(stdscr, TRUE);
	scrollok(stdscr, TRUE);


	/* Create the socket */
	int skt = socket( PF_CAN, SOCK_RAW, CAN_RAW );
 
	/* Locate the interface you wish to use */
	if ( outputDebug) printw("# Locating interface %s ... ",canInterface);
	struct ifreq ifr;
	strcpy(ifr.ifr_name, canInterface);
	ioctl(skt, SIOCGIFINDEX, &ifr); /* ifr.ifr_ifindex gets filled  with that device's index */
	if ( outputDebug) printw("done\n");
 
	/* Select that CAN interface, and bind the socket to it. */
	if ( outputDebug) printw("# Binding to interface ... ");
	struct sockaddr_can addr;
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if ( 0 != bind( skt, (struct sockaddr*)&addr, sizeof(addr) ) ) {
		printw("\n# Error binding to interface. Bye.\n");
		return 1;
	}
	if ( outputDebug) printw("done\n");
 
 	for ( ; ; ) {
		int ch=getch();

		if ( ERR == ch ) {
			usleep(1000);
			continue;
		} else if ( 'q' == ch ) {
			break;
		}


		/* Send a message to the CAN bus */
		if ( outputDebug) printw("# Sending message to CAN bus ... ");
		struct can_frame frame;
		memset(&frame, 0, sizeof(struct can_frame));
		/* mailbox to put request register in */
		frame.can_id = 0x003;

		u_uint32 u_qr;
		u_qr.i=queryRegister;
		frame.data[0]=u_qr.b[0];
		frame.data[1]=u_qr.b[1];
		frame.data[2]=u_qr.b[2];
		frame.data[3]=u_qr.b[3];

		/* four byte message */
		frame.can_dlc=4;

		bytes_sent = write( skt, &frame, sizeof(frame) );
		if ( bytes_sent <= 0 ) {
			printw("\n# Error writing. Bye.\n");
			return 1;
		}


		if ( outputDebug) printw(" done (%d bytes sent)\n",bytes_sent);
	}
 
 #if 0
		/* Read a message back from the CAN bus */
		if ( outputDebug) fprintf("# Reading response from CAN bus ... ");
		bytes_read = read( skt, &frame, sizeof(frame) );
		if ( outputDebug) fprintf(" done (%d bytes read)\n",bytes_read);

		if ( outputDebug ) {
			fprintf(stdout,"# frame.can_id = 0x%03x\n",frame.can_id);
			fprintf(stdout,"# frame.can_dlc=%d\n",frame.can_dlc);

			for ( i=0 ; i<bytes_read ; i++ ) {
				fprintf(stdout,"# frame.data[%d]=0x%02x\n",i,frame.data[i]);
			}
		}
#endif

	
	/* tear down ncurses window */
	endwin();

	return 0;
}
