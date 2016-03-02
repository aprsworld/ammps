#include <stdio.h>
#include <string.h>

int bit_test(int var, int bit) {
	return ((var & (1<<bit)) != 0);
}

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
	printf("fuel level:                0x%02x%02x (%d)\n",
		canBytes[0],
		canBytes[1],
		(canBytes[1]<<8) + canBytes[0]
	);
	printf("MLD maintenance countdown: 0x%02x%02x (%d)\n",
		canBytes[2],
		canBytes[3],
		(canBytes[3]<<8) + canBytes[2]
	);
	printf("MLD genset run time:       0x%02x%02x%02x (%d)\n",
		canBytes[4],
		canBytes[5],
		canBytes[6],
		(canBytes[6]<<16) + (canBytes[5]<<8) + canBytes[4]
	);

	return 0;
}
