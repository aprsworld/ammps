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

#include <string.h>
#include <netinet/in.h>
#include <netdb.h> 

extern char *optarg;
extern int optind, opterr, optopt;

 
/* At time of writing, these constants are not defined in the headers */
#ifndef PF_CAN
#define PF_CAN 29
#endif
 
#ifndef AF_CAN
#define AF_CAN PF_CAN
#endif

#define SIGNAL_GENERATOR_RUN_OPEN_CONTACTOR   SIGUSR1
#define SIGNAL_GENERATOR_RUN_CLOSED_CONTACTOR SIGUSR2
#define SIGNAL_GENERATOR_STOP                 SIGURG


/* signal handler installed in main */
void sighandler(int signum) {
	if (  SIGNAL_GENERATOR_RUN_OPEN_CONTACTOR == signum ) {
		fprintf(stderr,"\n# Caught SIGNAL_GENERATOR_RUN_OPEN_CONTACTOR.\n");
	} else if (  SIGNAL_GENERATOR_RUN_CLOSED_CONTACTOR == signum ) {
		fprintf(stderr,"\n# Caught SIGNAL_GENERATOR_RUN_CLOSED_CONTACTOR.\n");
	} else if (  SIGNAL_GENERATOR_STOP == signum ) {
		fprintf(stderr,"\n# Caught SIGNAL_GENERATOR_STOP.\n");
	} else {
		fprintf(stderr,"\n# Caught unexpected signal %d.\n",signum);
		fprintf(stderr,"# Terminating.\n");
		exit(2);
	}

}

int main(int argc, char **argv) {
	int bytes_read, bytes_sent;
	int delaySeconds=0;
	int i,n;

	int outputDebug=0;

	char canInterface[128];

	/* set initial values */
	strcpy(canInterface,"can0");


	signal(SIGALRM, sighandler);
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
				delaySeconds=atoi(optarg);
				fprintf(stdout,"# %d second delay between sending CAN commands\n",alarmSeconds);
				break;
			case 'h':
				fprintf(stdout,"# -d seconds\tdelay between CAN commands\n");
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

 	for ( n=0 ; ; n++ ) {
		struct can_frame frame;

		printf("## sleeping 10 seconds\n");
		sleep(10);


#if 0
		/* Read a message back from the CAN bus */
		memset(&frame, 0, sizeof(struct can_frame));
		if ( outputDebug) fprintf(stderr,"# Reading (n=%d) from CAN bus ... ",n);
		bytes_read = read( skt, &frame, sizeof(frame) );
		if ( outputDebug) fprintf(stderr," done (%d bytes read)\n",bytes_read);

		if ( outputDebug ) {
			/* strip extended off */
			fprintf(stdout,"# frame.can_id =0x%08x\n",frame.can_id ^ CAN_EFF_FLAG);
			fprintf(stdout,"# frame.can_dlc=%d\n",frame.can_dlc);

			for ( i=0 ; i<frame.can_dlc ; i++ ) {
				fprintf(stdout,"# frame.data[%d]=0x%02x\n",i,frame.data[i]);
			}

			fprintf(stdout,"\n\n");
			fflush(stdout);
		}
#endif
	}

	return 0;
}
