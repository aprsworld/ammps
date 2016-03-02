#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>

#include <sys/select.h>
#include <errno.h>


extern char *optarg;
extern int optind, opterr, optopt;


/* At time of writing, these constants are not defined in the headers */
#ifndef PF_CAN
#define PF_CAN 29
#endif

#ifndef AF_CAN
#define AF_CAN PF_CAN
#endif

#define SIGNAL_GENERATOR_RUN_OPEN_CONTACTOR   SIGUSR1 /* send 0x65 FF FF FF FF FF FF FF to 0x18FF1610 */
#define SIGNAL_GENERATOR_RUN_CLOSED_CONTACTOR SIGUSR2 /* send 0xA5 FF FF FF FF FF FF FF to 0x18FF1610 */
#define SIGNAL_GENERATOR_STOP                 SIGURG  /* send 0x55 FF FF FF FF FF FF FF to 0x18FF1610 */
int generatorRequestedState;


/* signal handler installed in main */
void sighandler(int signum) {
	if ( SIGPIPE == signum ) {
		fprintf(stderr,"\n# Caught SIGPIPE. Broken pipe for sending CAN data.\n");
		fprintf(stderr,"# Terminating.\n");
		exit(2);
	} else if ( SIGNAL_GENERATOR_RUN_OPEN_CONTACTOR == signum ) {
		fprintf(stderr,"\n# Caught SIGNAL_GENERATOR_RUN_OPEN_CONTACTOR signal.\n");
		generatorRequestedState=SIGNAL_GENERATOR_RUN_OPEN_CONTACTOR;
	} else if ( SIGNAL_GENERATOR_RUN_CLOSED_CONTACTOR == signum ) {
		fprintf(stderr,"\n# Caught SIGNAL_GENERATOR_RUN_CLOSED_CONTACTOR signal.\n");
		generatorRequestedState=SIGNAL_GENERATOR_RUN_CLOSED_CONTACTOR;
	} else if ( SIGNAL_GENERATOR_STOP == signum ) {
		fprintf(stderr,"\n# Caught SIGNAL_GENERATOR_STOP signal.\n");
		generatorRequestedState=SIGNAL_GENERATOR_STOP;
	} else {
		fprintf(stderr,"\n# Caught unexpected signal %d.\n",signum);
		fprintf(stderr,"# Terminating.\n");
		exit(3);
	}

}

int main(int argc, char **argv) {
	struct timeval t;
	int bytes_read, bytes_sent;
	int delayMilliseconds;
	int i,n;
	int resetTimeout=1;

	int outputDebug=0;

	char canInterface[128];

	struct can_frame frame;

	/* set initial values */
	strcpy(canInterface,"can0");
	generatorRequestedState=SIGNAL_GENERATOR_STOP;
	delayMilliseconds=500;


	/* write gives a SIGPIPE */
	signal(SIGPIPE, sighandler);
	/* signals we are expecting from external source that will cause us to
	 * send appropriate CAN messages to generator 
	 */
	signal(SIGNAL_GENERATOR_RUN_OPEN_CONTACTOR,   sighandler);
	signal(SIGNAL_GENERATOR_RUN_CLOSED_CONTACTOR, sighandler);
	signal(SIGNAL_GENERATOR_STOP,                 sighandler);

	while ((n = getopt (argc, argv, "d:hi:v")) != -1) {
		switch (n) {
			case 'd':
				delayMilliseconds=atoi(optarg);
				fprintf(stdout,"# %d millisecond delay between sending CAN commands\n",delayMilliseconds);
				break;
			case 'h':
				fprintf(stdout,"# -d milliseconds\tdelay between CAN commands\n");
				fprintf(stdout,"# -v\tOutput verbose / debugging to stderr\n");
				fprintf(stdout,"#\n");
				fprintf(stdout,"# -h\tThis help message then exit\n");
				fprintf(stdout,"#\n");
				fprintf(stdout,"# -i\tCAN interface to use (eg can0, can1, etc)\n");
				return 0;
			case 'i':
				strncpy(canInterface,optarg,sizeof(canInterface));
				canInterface[sizeof(canInterface)-1]='\0';
				fprintf(stderr,"# CAN interface = %s\n",canInterface);
				break;
			case 'v':
				outputDebug=1;
				fprintf(stderr,"# verbose (debugging) output to stderr enabled\n");
				break;
		}
	}

	if ( !outputDebug  ) {
		fprintf(stderr,"# warning: no output format is selected. Nothing will show.\n");
	}


	/* Create the CAN socket */
	int skt = socket( PF_CAN, SOCK_RAW, CAN_RAW );

	/* Locate the interface you wish to use */
	if ( outputDebug) fprintf(stderr,"# Locating interface %s ... ",canInterface);
	struct ifreq ifr;
	strcpy(ifr.ifr_name, canInterface);
	ioctl(skt, SIOCGIFINDEX, &ifr); /* ifr.ifr_ifindex gets filled  with that device's index */
	if ( outputDebug) fprintf(stderr,"done\n");

	/* Select that CAN interface, and bind the socket to it. */
	if ( outputDebug) fprintf(stderr,"# Binding to interface ... ");
	struct sockaddr_can addr;
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if ( 0 != bind( skt, (struct sockaddr*)&addr, sizeof(addr) ) ) {
		fprintf(stderr,"\n# Error binding to interface. Bye.\n");
		return 1;
	}
	if ( outputDebug) fprintf(stderr,"done\n");

	/* build CAN frame to send */
	memset(&frame, 0, sizeof(struct can_frame));
	frame.can_id = 0x18FF1610 | CAN_EFF_FLAG;
	frame.can_dlc= 8; /* message length */
	/* initialize data bytes to 0xff */
	memset(&frame.data, 0xff, 8);

	resetTimeout=1;
	for ( n=0 ; ; n++ ) {
		if ( resetTimeout ) {
				/* delayMilliseconds * 100 millisecond delay */
				t.tv_sec = 0;
				t.tv_usec = delayMilliseconds * 1000;
		}

		
		/* delay. If interrupted, then delay in next loop */
		if ( -1 == select(0, NULL, NULL, NULL, &t) ) {
			if ( EINTR == errno ) {
				/* interrupted, so sleep not complete */
				printf("select sleep interferred with...\n");
				/* t structure now has time remaining, leave it and get back into select */
				resetTimeout=0;
				continue;
			} else {
				/* some other error. Reset */
				resetTimeout=1;
			}
		} else {
			/* reset timeout just to be sure */
			resetTimeout=1;
		}


 
		/* set the first byte of the launch control message to our desired generator state */
 		printf("# Sending (n=%d) ",n);

		if (  SIGNAL_GENERATOR_RUN_OPEN_CONTACTOR == generatorRequestedState ) {
			printf("GENERATOR_RUN_OPEN_CONTACTOR CAN message\n");
			frame.data[0]=0x65;
		} else if (  SIGNAL_GENERATOR_RUN_CLOSED_CONTACTOR == generatorRequestedState ) {
			printf("GENERATOR_RUN_CLOSED_CONTACTOR CAN message\n");
			frame.data[0]=0xA5;
		} else {
			printf("GENERATOR_STOP CAN message\n");
			frame.data[0]=0x55;
		}

		/* send launch control message */
		int bytes_sent = write( skt, &frame, sizeof(frame) );
		if ( bytes_sent <= 0 ) {
			printf("# error writing to CAN socket. Aborting.\n");
			exit(4);
		}

	}

	return 0;
}
