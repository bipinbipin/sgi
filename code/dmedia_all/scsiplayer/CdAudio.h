#ifndef __CDAUDIO_H_
#define __CDAUDIO_H_

#include "PlayerState.h"


/*
 * cdaudio.h
 *
 * Functions for playing audio CD's on a CD ROM drive
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

/*
 * Block size information for digital audio data
 */
#define CDDA_DATASIZE		2352
#define CDDA_SUBCODESIZE	(sizeof(struct subcodeQ))
#define CDDA_BLOCKSIZE		(sizeof(struct cdframe))
#define CDDA_NUMSAMPLES		(CDDA_DATASIZE/2)
/*
 * The ideal number of frames to read to get continuous
 * streaming data.  This is related to the buffer full ratio
 * used in CDopen.
 */
#define CDDA_NUMFRAMES 12
#define CDDA_FRAMES_PER_SECOND 75

enum drivetype { UNKNOWN, TOSHIBA, SONY, TOSHIBA_SCSI2, MATSHITA};

/*
 * Buffer to receive information about a particular track
 */

// typedef struct {
// 	int     start_min;
// 	int     start_sec;
// 	int     start_frame;
// 	int     total_min;
// 	int     total_sec;
// 	int     total_frame;
// } CDTRACKINFO;

typedef TRACKINFO CDTRACKINFO;
/*************************************************************************
 *
 * Defines and structures for CD digital audio data
 *
 *************************************************************************/

/*
 * Defines for control field
 */
#define CDQ_PREEMP_MASK		0xd
#define CDQ_COPY_MASK		0xb
#define	CDQ_DDATA_MASK		0xd
#define CDQ_BROADCAST_MASK	0x8
#define CDQ_PREEMPHASIS		0x1
#define CDQ_COPY_PERMITTED	0x2		
#define CDQ_DIGITAL_DATA	0x4
#define CDQ_BROADCAST_USE	0x8

/*
 * Defines for the type field
 */
#define CDQ_MODE1		0x1
#define CDQ_MODE2		0x2
#define CDQ_MODE3		0x3

/*
 * Useful sub-structures
 */
struct cdpackedbcd { unchar dhi:4, dlo:4; };

struct cdtimecode {
    unchar mhi:4, mlo:4;
    unchar shi:4, slo:4;
    unchar fhi:4, flo:4;
};

struct cdident {
    unchar country[2];
    unchar owner[3];
    unchar year[2];
    unchar serial[5];
};

/*
 * Structure of CD subcode Q
 */
typedef struct subcodeQ {
    unchar control;
    unchar type;
    union {
        struct {
	    struct cdpackedbcd track;
	    struct cdpackedbcd index;	/* aka point during track 0 */
	    struct cdtimecode ptime;
            struct cdtimecode atime;
            unchar fill[6];
        } mode1;
        struct {
            unchar catalognumber[13];
            struct cdpackedbcd aframe;
        } mode2;
        struct {
	    struct cdident ident;
            struct cdpackedbcd aframe;
            unchar fill;
        } mode3;
    } data;
} CDSUBCODEQ;

/*
 * Structure of the digital audio (DA) data as delivered by the drive
 * In CD parlance this is a one subcode frame
 */
typedef struct cdframe {
    char audio[CDDA_DATASIZE];
    struct subcodeQ subcode;
} CDFRAME;

/********************************************************************/


/*
 * Open a devscsi device corresponding to a CD-ROM drive for audio.
 * Specifying NULL for devscsi causes the hardware inventory
 * to be consulted for a CD-ROM drive.
 *
 * direction specifies way the device is to be used "r", "w", or "rw".
 * It is for future support of writable CD's and is currently ignored.
 */
SCSIPLAYER * CDopen( char const *devscsi, char const *direction, SCSIPLAYER* player);

int CDupdatestatus( SCSIPLAYER *cdplayer);

int CDupdatestatusfromframe( SCSIPLAYER *cdplayer, CDFRAME* suppliedFrame);

/*
 * Read digital audio data
 */
int CDreadda( SCSIPLAYER *cd, CDFRAME *buf, int num_frames );


/*
 * Position drive at minute, second, frame for reading digital audio
 */
unsigned long CDseek( SCSIPLAYER *cd, int min, int sec, int frame );

/*
 * Position drive at given logical block number.
 */
unsigned long CDseekblock( SCSIPLAYER *cd, unsigned long block);

/*
 * Position drive at start of track t.
 */
int CDseektrack( SCSIPLAYER *cd, int t );

/*
 * Stop play (or pause)
 */
int CDstop( SCSIPLAYER *cd );

/*
 * Eject the caddy
 */
int CDeject( SCSIPLAYER *cd );

/*
 * Enable the eject button
 */
void CDallowremoval( SCSIPLAYER *cd );

/*
 * Disable the eject button
 */
void CDpreventremoval( SCSIPLAYER *cd );

/*
 * close the devscsi device
 */
int CDclose( SCSIPLAYER *cd );

/*
 * Get information about a track
 */
int CDgettrackinfo( SCSIPLAYER *cd, int track, CDTRACKINFO *info );

/*
 * Convert an ascii string to timecode
 * return 0 if timecode invalid
 */
int CDatotime(struct cdtimecode *tc, const char *loc);

/*
 * Convert an ascii string to msf
 * return 0 if timecode invalid
 */
int CDatomsf(const char *loc, int *m, int *s, int *f);

/*
 * Convert a frame number to struct timecode
 */

void CDframetotc(unsigned long frame, struct cdtimecode *tc);

/*
 * Convert a frame number to mins/secs/frames
 */

void CDframetomsf(unsigned long frame, int *m, int *s, int *f);

/*
 * Convert a struct time code to a displayable ASCII string
 */
void CDtimetoa( char *s, struct cdtimecode *tp );

/*
 * Convert an array of 6-bit values to an ASCII string
 * This function returns the number of characters converted
 */
int CDsbtoa( char *s, const unchar *sixbit, int count );

/*
 * Convert (minutes, seconds, frame) value to logical block number
 * on given device.  This function is device specific.  You should
 * use CDmsftoframe instead.
 */
unsigned long CDmsftoblock( SCSIPLAYER *cd, int m, int s, int f );

/*
 * Convert a logical block number to a (minutes, seconds, frame) value.
 */
void CDblocktomsf( SCSIPLAYER *cd, unsigned long frame, int *m, int *s,  int*f );

/*
 * Convert (minutes, seconds, frame) value to CD frame number.
 * This is useful when you need to compare two values.
 */
unsigned long CDmsftoframe( int m, int s, int f );

/*
 * Convert struct timecode value to CD frame number.
 * This is useful when you need to compare two timecode values.
 */
unsigned long CDtctoframe( struct cdtimecode* tc );

unsigned long
CDframetoblock(SCSIPLAYER *cd, unsigned long fr);


#endif /* !__CDAUDIO_H_ */
