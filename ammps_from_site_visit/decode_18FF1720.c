#include <stdio.h>
#include <string.h>

int bit_test(int var, int bit) {
	return ((var & (1<<bit)) != 0);
}

const char *gensetStatusString[] = {
"NOT READY TO CRANK",
"READY TO CRANK",
"DELAY TO CRANK DUE TO WINTER KIT",
"DELAY TO CRANK DUE TO GLOW PLUG",
"CRANK",
"RUNNING",
"EMERGENCY MODE",
"IDLE MODE",
"POWERING DOWN",
"FACTORY TEST",
"DELAY TO CRANK DUE TO WINTER KIT TEST",
"DELAY TO CRANK DUE TO WINTER KIT DETECT",
"DELAY TO CRAK DUE TO ECM DATA SAVE",
"RUNNING - SYNCHRONIZING",
"RUNNING - SYNCHRONIZED",
"INVALID GENSET STATUS"
};

int main(int argc, char **argv) {
	int canBytes[8];
	int canBits[8*8];
	int i,j;

	if ( 9 != argc ) {
		printf("argc: %d\n",argc);
		printf("usage: %s hex hex hex hex hex hex hex hex\n",argv[0]);
		return 1;
	}

	for ( i=0 ; i<argc ; i++ ) {
		printf("%s ",argv[i]);
	}
	printf("\n");

	/* read hex bytes and put in canBytes */
	for ( i=0 ; i<8 ; i++ ) {
		sscanf(argv[i+1],"%X",&canBytes[i]);
	}



	printf("# bit string:\n");
	for ( i=0 ; i<8*8 ; i++ ) {
//		printf("(t%d)",i/8);
		if ( 0==i%8 ) {
			printf("canBytes[%d]=0x%02x: ",i/8,canBytes[i/8]);
		}
		printf("[%02d] %d ",i,bit_test(canBytes[i/8],i%8));
		canBits[i]=bit_test(canBytes[i/8],i%8);
		if ( 0 != i && 0==(i+1)%8 ) {
			printf("\n");
		}
	}
	printf("\n");



//	for ( i=0 ; i<8*8 ; i++ ) {
//		printf(">%02d< %d\n",i,canBits[i]);
//	}

	printf("Decoded:\n");
	int status=canBytes[0]&0x0f;
	printf("status:               0x%1x (%d %d %d %d) >%s<\n",
		status,
		canBits[3],
		canBits[2],
		canBits[1],
		canBits[0],
		gensetStatusString[status]
	);

	int loadSwitchPosition=(canBytes[0]>>4)&0b11;
	printf("load switch position: 0x%1x (%d %d) ",
		loadSwitchPosition,
		canBits[5],
		canBits[4]
	);

	switch ( loadSwitchPosition ) {
		case 0b00: printf(">OFF<\n"); break;
		case 0b01: printf(">PRIME AND RUN AUX FUEL<\n"); break;
		case 0b10: printf(">PRIME AND RUN<\n"); break;
		case 0b11: printf(">START<\n"); break;
	}


	return 0;
}
