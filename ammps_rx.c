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

/* signal handler installed in main */
void sighandler(int signum) {
	if ( SIGALRM == signum ) {
		fprintf(stderr,"\n# Timeout while waiting for CAN data.\n");
	} else if ( SIGPIPE == signum ) {
		fprintf(stderr,"\n# Broken pipe to TCP server.\n");
	} else {
		fprintf(stderr,"\n# Caught unexpected signal %d.\n",signum);
	}

	fprintf(stderr,"# Terminating.\n");
	exit(1);
}

/* CRC calculator for WorldData packet */
unsigned int crc_chk(unsigned char* data, unsigned char length) {
	int j;
	unsigned int reg_crc=0xFFFF;

	while ( length-- ) {
		reg_crc ^= *data++;
		for ( j=0 ; j<8 ; j++ ) {
			if( reg_crc & 0x01 ) { /* LSB(b0)=1 */
				reg_crc=(reg_crc>>1) ^ 0xA001;
			} else {
				reg_crc=reg_crc >>1;
			}
		}
	}
	return reg_crc;
}


int main(int argc, char **argv) {
	int bytes_read, bytes_sent;
	int alarmSeconds=5;
	int i,n;

	int outputDebug=0;

	char canInterface[128];

	int sockfd;
	char tcpHost[256];
	int tcpPort;

	struct sockaddr_in serveraddr;
	struct hostent *server;
	char world[20];

	/* set initail values */
	strcpy(canInterface,"can0");
	strcpy(tcpHost,"localhost");
	tcpPort=4010;


	signal(SIGALRM, sighandler);
	signal(SIGPIPE, sighandler);

	while ((n = getopt (argc, argv, "a:hi:p:s:t:v")) != -1) {
		switch (n) {
			case 'a':
				alarmSeconds=atoi(optarg);
				fprintf(stdout,"# terminate program after %d seconds without receiving data\n",alarmSeconds);
				break;
			case 'p':
				tcpPort=atoi(optarg);
				fprintf(stdout,"# TCP server port = %d\n",tcpPort);
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

			case 't':
				strncpy(tcpHost,optarg,sizeof(tcpHost));
				tcpHost[sizeof(tcpHost)-1]='\0';
				fprintf(stderr,"# TCP server hostname = %s\n",tcpHost);
				break;
			case 'h':
				fprintf(stdout,"# -a seconds\tTerminate after seconds without data\n");
				fprintf(stdout,"# -s seconds\tstartup delay\n");
				fprintf(stdout,"# -t tcpHost\tTCP server hostname\n");
				fprintf(stdout,"# -p tcpPort\tTCP server port number\n");
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
	if ( outputDebug) fprintf(stderr,"# Locating CAN interface %s ... ",canInterface);
	struct ifreq ifr;
	strcpy(ifr.ifr_name, canInterface);
	ioctl(skt, SIOCGIFINDEX, &ifr); /* ifr.ifr_ifindex gets filled  with that device's index */
	if ( outputDebug) fprintf(stderr,"done\n");

	/* Select that CAN interface, and bind the socket to it. */
	if ( outputDebug) fprintf(stderr,"# Binding to CAN interface ... ");
	struct sockaddr_can addr;
	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;
	if ( 0 != bind( skt, (struct sockaddr*)&addr, sizeof(addr) ) ) {
		fprintf(stderr,"\n# Error binding to interface. Bye.\n");
		return 1;
	}
	if ( outputDebug) fprintf(stderr,"done\n");


	/* setup TCP connection */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr,"\n# Error opening TCP socket. Bye.\n");
		return 1;
	}

	/* gethostbyname: get the server's DNS entry */
	server = gethostbyname(tcpHost);
	if ( NULL == server ) {
		fprintf(stderr,"\n# No such hostname %s.\n",tcpHost);
		return 2;
	}

	/* Construct the server sockaddr_in structure */
	memset(&serveraddr, 0, sizeof(serveraddr));			/* Clear struct */
	serveraddr.sin_family = AF_INET;							/* Internet/IP */
	serveraddr.sin_addr = *(struct in_addr *) server->h_addr;
	serveraddr.sin_port = htons(tcpPort);					/* server port */

	/* Establish connection */
	if ( outputDebug) fprintf(stderr,"# Connecting to %s:%d ... ",tcpHost,tcpPort);
	if ( connect(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
		fprintf(stderr,"\n# Error connecting to TCP server %s:%d. Bye.\n",tcpHost,tcpPort);
		return 3;
	}
	if ( outputDebug) fprintf(stderr,"done\n");


	for ( n=0 ; ; n++ ) {
		struct can_frame frame;

		/* cancel pending alarm */
		alarm(0);
		/* set an alarm to send a SIGALARM if data not received within alarmSeconds */
		alarm(alarmSeconds);


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

			sprintf(world,"# n=%d\n",n);

			/* build world data packet */
			world[0]='#'; /* STX */
			world[1]='A'; /* SERIAL PREFIX */
			world[2]='M'; /* SERIAL NUMBER MSB */
			world[3]='M'; /* SERIAL NUMBER LSB */
			world[4]=20;  /* PACKET LENGTH, COMPLETE */
			world[5]=34;  /* PACKET TYPE */
			/* CAN ID (4 bytes) */
			world[6]=((frame.can_id ^ CAN_EFF_FLAG)>>24) & 0xff;
			world[7]=((frame.can_id ^ CAN_EFF_FLAG)>>16) & 0xff;
			world[8]=((frame.can_id ^ CAN_EFF_FLAG)>>8)  & 0xff;
			world[9]= (frame.can_id ^ CAN_EFF_FLAG)      & 0xff;
			/* CAN DATA (8 bytes) */
			for ( i=0 ; i<8 ; i++ ) {
				world[10+i]=frame.data[i] & 0xff;
			}
			/* calculate CRC */
			short lCRC=crc_chk(world+1,17);
			world[18]=(lCRC>>8) & 0xff;
			world[19]= lCRC     & 0xff;

			/* send the message line to the server */
			int nb = write(sockfd, world, sizeof(world));
			if ( nb < 0 ) {
				fprintf(stderr,"\n# Write returned %d.\n",nb);
				return 4;
			}
		}
	}

	return 0;
}
