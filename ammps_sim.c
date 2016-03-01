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
#include <sys/select.h>

extern char *optarg;
extern int optind, opterr, optopt;

/* CAN socket */
int skt;
/* debugging output */
int outputDebug=0;

/* PGN's we send */
struct can_frame frame_event_update;
struct can_frame frame_operating_hours_fuel_level;
 
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

void update_frame_event_update(char ch) {
	static int current_warning=0;
	static int current_fault=0;
	static int estop=0;
	static int battleshort=0;
	static int deadbus=0;
	static int genset_cb_position=0;

	if ( 'a' == ch ) {
		printw("# current warning was: %d\n",current_warning);
		current_warning++;
		printw("# current warning now: %d\n",current_warning);
	}

	if ( 'b' == ch ) {
		printw("# current fault was: %d\n",current_fault);
		current_fault++;
		printw("# current fault now: %d\n",current_fault);
	}

	if ( 'c' == ch ) {
		printw("# estop was: %d\n",estop);
		estop = ! estop;
		printw("# estop now: %d\n",estop);
	}

	if ( 'd' == ch ) {
		printw("# battleshort was: %d\n",battleshort);
		battleshort = ! battleshort;
		printw("# battleshort now: %d\n",battleshort);
	}

	if ( 'e' == ch ) {
		printw("# deadbus was: %d\n",deadbus);
		deadbus = ! deadbus;
		printw("# deadbus now: %d\n",deadbus);
	}

	if ( 'f' == ch ) {
		printw("# genset_cb_position was: %d\n",genset_cb_position);
		genset_cb_position = ! genset_cb_position;
		printw("# genset_cb_position now: %d\n",genset_cb_position);
	}

	memset(&frame_event_update, 0, sizeof(struct can_frame));

//	frame.can_id = 0x003;
	frame_event_update.can_id = 0x18FF2400 | CAN_EFF_FLAG;
	frame_event_update.can_dlc= 8; /* message length */

	frame_event_update.data[0]=(current_warning&0xff); /* current active warning MSB  */
	frame_event_update.data[1]=(current_warning>>8)&0xff; /* current active warning MSB  */
	frame_event_update.data[2]=(current_fault&0xff); /* current active fault MSB  */
	frame_event_update.data[3]=(current_fault>>8)&0xff; /* current active fault MSB  */
	frame_event_update.data[4]|=(estop<<7);
	frame_event_update.data[4]|=(battleshort<<6);
	frame_event_update.data[4]|=(deadbus<<5);
	frame_event_update.data[4]|=(genset_cb_position<<4);


}

void periodic_100ms(void) {
	static int ticks=0;
	static int seconds=0;

	ticks++;

	if ( 10 == ticks ) {
		ticks=0;
		seconds++;

		/* Send a message to the CAN bus */
		if ( outputDebug) printw("# Sending message to CAN bus ... ");

		int bytes_sent = write( skt, &frame_event_update, sizeof(frame_event_update) );
		if ( bytes_sent <= 0 ) {
			printw("\n# Error writing. Bye.\n");
			return;
		}


		if ( outputDebug) printw(" done (%d bytes sent)\n",bytes_sent);


		/* send packet at one second interval */
	}

}

int main(int argc, char **argv) {
	struct timeval t;
	int bytes_read, bytes_sent;
	int queryRegister;
	int i,n;

	int firstRegister=0;
	int lastRegister=39;


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
	
	/* initialize CAN frames */
	update_frame_event_update('\0');


	/* Create the socket */
	skt = socket( PF_CAN, SOCK_RAW, CAN_RAW );
 
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
		int ch;

		/* 100 millisecond timer */
		t.tv_sec = 0;
		t.tv_usec = 100000; 
		select(0, NULL, NULL, NULL, &t); 
		periodic_100ms();

		ch=getch();

		if ( '~' == ch ) {
			break;
		} else if ( ch >= 'a' && ch <= 'f' ) {
			update_frame_event_update(ch);
		}

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
