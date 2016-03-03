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

#define OVERRIDE_MODE_OFF     0
#define OVERRIDE_MODE_ON      1
#define OVERRIDE_MODE_AUTO    2
#define OVERRIDE_MODE_UNKNOWN 65535

int override_switch(char *swAfilename, char *swBfilename) {
	char a, b;
	FILE *fr;

	if ( NULL==swAfilename || NULL==swBfilename ) {
		return OVERRIDE_MODE_UNKNOWN;
	}

	/*
	A B VALUE
	0 0 INVALID / NOT POSSIBLE
	0 1 OFF  OVERRIDE
	1 0 ON   OVERRIDE
	1 1 AUTO MODE
	*/

	fr=fopen(swAfilename,"r");
	a=fgetc(fr);
	fclose(fr);

	fr=fopen(swBfilename,"r");
	b=fgetc(fr);
	fclose(fr);

	if ( '0' == a && '1' == b ) {
		return OVERRIDE_MODE_OFF;
	} else if ( '1' == a && '0' == b ) {
		return OVERRIDE_MODE_ON;
	} else if ( '1' == a && '1' == b ) {
		return OVERRIDE_MODE_AUTO;
	} else {
		/* shouldn't get here. Switch doesn't allow both contacts to be closed */
		return OVERRIDE_MODE_UNKNOWN;
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

	char *swAfilename;
	char *swBfilename;

	/* set initial values */
	strcpy(canInterface,"can0");
	generatorRequestedState=SIGNAL_GENERATOR_STOP;
	delayMilliseconds=500;
	swAfilename=NULL;
	swBfilename=NULL;


	/* write gives a SIGPIPE */
	signal(SIGPIPE, sighandler);
	/* signals we are expecting from external source that will cause us to
	 * send appropriate CAN messages to generator 
	 */
	signal(SIGNAL_GENERATOR_RUN_OPEN_CONTACTOR,   sighandler);
	signal(SIGNAL_GENERATOR_RUN_CLOSED_CONTACTOR, sighandler);
	signal(SIGNAL_GENERATOR_STOP,                 sighandler);

	while ((n = getopt (argc, argv, "a:b:d:hi:s:v")) != -1) {
		switch (n) {
			case 'a':
				swAfilename=optarg;
				fprintf(stdout,"# Switch A filename: %s\n",swAfilename);
				break;
			case 'b':
				swBfilename=optarg;
				fprintf(stdout,"# Switch B filename: %s\n",swBfilename);
				break;
			case 'd':
				delayMilliseconds=atoi(optarg);
				fprintf(stdout,"# %d millisecond delay between sending CAN commands\n",delayMilliseconds);
				break;
			case 'h':
				fprintf(stdout,"# -a switch A state file\t0 or 1\n");
				fprintf(stdout,"# -b switch B state file\t0 or 1\n");
				fprintf(stdout,"# -d milliseconds\tdelay between CAN commands\n");
				fprintf(stdout,"# -h\tThis help message then exit\n");
				fprintf(stdout,"# -i\tCAN interface to use (eg can0, can1, etc)\n");
				fprintf(stdout,"# -s seconds\tstartup delay\n");
				fprintf(stdout,"# -v\tOutput verbose / debugging to stderr\n");
				return 0;
			case 'i':
				strncpy(canInterface,optarg,sizeof(canInterface));
				canInterface[sizeof(canInterface)-1]='\0';
				fprintf(stderr,"# CAN interface = %s\n",canInterface);
				break;
			case 's':
				n=atoi(optarg);
				fprintf(stdout,"# Delaying startup for %d seconds ",n);
				fflush(stdout);
				for ( i=0 ; i<n ; i++ ) {
					sleep(1);
					fputc('.',stdout);
					fflush(stdout);
				}
				fprintf(stdout," done\n");
				fflush(stdout);
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

	if ( NULL==swAfilename || NULL==swBfilename ) {
		fprintf(stderr,"# swA and/or swB file not specified, override switch disabled\n");
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

		/* override switch status */
		int override = override_switch(swAfilename,swBfilename);
 
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

		if ( OVERRIDE_MODE_OFF == override ) {
			printf("OVERRIDE OFF BY SWITCH\n");
			frame.data[0]=0x55;
		} else if ( OVERRIDE_MODE_ON == override ) {
			printf("OVERRIDE ON (CLOSED CONTACTOR) BY SWITCH\n");
			frame.data[0]=0xA5;
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
