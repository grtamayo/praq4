/*
	Filename:     praq4.c
	Description:  PPP/LZP with variable-length encoding of MTF (SR) codes.
	Written by:   Gerald R. Tamayo, (July 2009, 1/12/2010, 4/14/2010, 9/2/2017, 01/15/2022)
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>   /* C99 */
#include <time.h>
#include "gtbitio2.c"
#include "ucodes2.c"
#include "mtf.c"

/* Bitsize of the first N (i.e., 1<<BSIZE) high-ranking 
symbols, output codesize = 1+BSIZE */
#define BSIZE        3

#define EOF_PPP    255
#define EOF_VLC    256

#define WBITS       20
#define WSIZE       (1<<WBITS)
#define WMASK       (WSIZE-1)

enum {
	/* modes */
	COMPRESS,
	DECOMPRESS,
};

enum {
	/* codes */
	mPPP = 1,
	mVLCODE
};

typedef struct {
	char alg[4];
} file_stamp;

unsigned char win_buf[ WSIZE ];   /* the prediction buffer. */
unsigned char pattern[ WSIZE ];   /* the "look-ahead" buffer. */
int mcode = 0;

void copyright( void );
void   compress( unsigned char w[], unsigned char p[] );
void decompress( unsigned char w[] );

void usage( void )
{
	fprintf(stderr, "\n Usage: praq4 c[1|2]|d infile outfile\n"
		"\n Commands:\n  c1 = PPP (raw byte output) \n  c2 = MTF coding\n  d  = decoding.\n"
	);
	copyright();
	exit(0);
}

int main( int argc, char *argv[] )
{
	float ratio = 0.0;
	int mode = -1;
	file_stamp fstamp;
	char *cmd = NULL;
	
	clock_t start_time = clock();
	
	if ( argc != 4 ) usage();
	init_buffer_sizes( (1<<15) );
	
	cmd = argv[1];
	while ( cmd ){
		switch( *cmd ) {
			case 'c': if ( mode == -1 ) mode = COMPRESS; else usage(); cmd++; break;
			case 'd':
				if ( mode == -1 ) mode = DECOMPRESS; else usage();
				if ( *(cmd+1) != 0 ) usage(); cmd++; break;
			case '1':
				if ( mode == -1 || mode == DECOMPRESS || mcode ) usage();
				mcode = mPPP; cmd++; break;
			case '2':
				if ( mode == -1 || mode == DECOMPRESS || mcode ) usage();
				mcode = mVLCODE; cmd++; break;
			case 0: cmd = NULL; if ( mcode == 0 ) mcode = mPPP; break;
			default : usage();
		}
	}
	
	if ( (gIN=fopen( argv[2], "rb" )) == NULL ) {
		fprintf(stderr, "\nError opening input file.");
		return 0;
	}
	if ( (pOUT=fopen( argv[3], "wb" )) == NULL ) {
		fprintf(stderr, "\nError opening output file.");
		return 0;
	}
	init_put_buffer();
	
	/* initialize prediction buffer to all zero (0) values. */
	memset( win_buf, 0, WSIZE );
	alloc_mtf(256);
	
	if ( mode == COMPRESS ){
		/* Write the FILE STAMP. */
		strcpy( fstamp.alg, "LZP" );
		fstamp.alg[3] = (char) mcode;
		fwrite( &fstamp, sizeof(file_stamp), 1, pOUT );
		nbytes_out = sizeof(file_stamp);
		
		fprintf(stderr, "\n Encoding [ %s to %s ] ...", 
			argv[2], argv[3] );
		compress( win_buf, pattern );
	}
	else if ( mode == DECOMPRESS ){
		fread( &fstamp, sizeof(file_stamp), 1, gIN );
		mcode = fstamp.alg[3];
		init_get_buffer();
		nbytes_read = sizeof(file_stamp);
		
		fprintf(stderr, "\n Decoding...");
		decompress( win_buf );
		free_get_buffer();
	}
	flush_put_buffer();
	nbytes_read = get_nbytes_read();

	fprintf(stderr, "done.\n  %s (%lld) -> %s (%lld)", 
		argv[2], nbytes_read, argv[3], nbytes_out);	
	if ( mode == COMPRESS ) {
		ratio = (((float) nbytes_read - (float) nbytes_out) /
			(float) nbytes_read ) * (float) 100;
		fprintf(stderr, "\n Compression ratio: %3.2f %%", ratio );
	}
	fprintf(stderr, " in %3.2f secs.\n", (double) (clock()-start_time) / CLOCKS_PER_SEC );
	
	free_put_buffer();
	free_mtf_table();
	if ( gIN ) fclose( gIN );
	if ( pOUT ) fclose( pOUT );
	
	return 0;
}

void copyright( void )
{
	fprintf(stderr, "\n Written by: Gerald R. Tamayo (c) 2010-2022\n");
}

void compress( unsigned char w[], unsigned char p[] )
{
	register int c, b=0, n, rank=0, nread, prev=0;  /* prev = context */
	
	while ( (nread=fread(p, 1, WSIZE, gIN)) ){
		n = 0;
		while ( n < nread ) {
			if ( w[prev] == (c=p[n&WMASK]) ){
				b++;
				if ( mcode == mVLCODE ) {
					if ( ++table[c].f >= table[rank].f ) {
						rank = c;
					}
					/* *rank* is the highest (i.e., index 0) in the MTF list. */
					if ( head->c != rank ) mtf(rank);
				}
			}
			else {
				if ( b ) {
					put_ONE();
					put_vlcode(--b, 0);
				}
				else put_ZERO();
				b = 0;
				if ( mcode == mPPP ) {
					put_nbits(c, 8);
					if ( c == EOF_PPP ) put_ZERO();
				}
				else {
					put_vlcode( mtf(c), BSIZE );
					/* *rank* jumps from symbol to symbol in the MTF list. */
					if ( !(table[rank].f > ++table[c].f && head->c != c) ) {
						rank = c;
					}
				}
				w[prev] = c;
			}
			n++;
			prev = ((prev<<5)+c) & WMASK;
		}
		nbytes_read += nread;
	}
	/* flag EOF */
	if ( b ) {
		put_ONE();
		put_vlcode(--b, 0);
	}
	else put_ZERO();
	if ( mcode == mPPP ) {
		put_nbits(EOF_PPP, 8);
		put_ONE();
	}
	else put_vlcode(EOF_VLC, BSIZE);
}

void decompress( unsigned char w[] )
{
	register int b, c, rank=0, prev = 0;  /* prev = context */
	
	do {
		if ( get_bit() ){
			b = get_vlcode(0)+1;
			do {
				pfputc( c=w[prev] );
				if ( mcode == mVLCODE ) {
					if ( ++table[c].f >= table[rank].f ) {
						rank = c;
					}
					mtf(rank);
				}
				prev = ((prev<<5)+c) & WMASK;
			} while ( --b );
		}
		if ( mcode == mPPP ) {
			c = get_nbits(8);
			if ( c == EOF_PPP && get_bit() ) return;
		}
		else {
			if ( (c=get_vlcode(BSIZE)) == EOF_VLC ) return;
			c = get_mtf_c(c);
			if ( !(table[rank].f > ++table[c].f && head->c != c) ) {
				rank = c;
			}
		}
		pfputc( c );
		w[prev] = c;
		prev = ((prev<<5)+c) & WMASK;
	} while ( 1 );
}

