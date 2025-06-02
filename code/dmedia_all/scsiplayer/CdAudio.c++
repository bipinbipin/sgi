///////////////////////////////////////////////////////////////////////////////
//
// CdAudio.h
//
// 	Functions for playing audio CD's on a CD ROM drive
//
//
// Copyright 1996, Silicon Graphics, Inc.
// ALL RIGHTS RESERVED
//
// UNPUBLISHED -- Rights reserved under the copyright laws of the United
// States.   Use of a copyright notice is precautionary only and does not
// imply publication or disclosure.
//
// U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
// Use, duplication or disclosure by the Government is subject to restrictions
// as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
// in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
// in similar or successor clauses in the FAR, or the DOD or NASA FAR
// Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
// 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
//
// THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
// INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
// DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
// PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
// GRAPHICS, INC.
///////////////////////////////////////////////////////////////////////////////


#include <stdlib.h>
#include <stdio.h>
#include <bstring.h>
#include <mntent.h>
#include <mediad.h>
#include <dslib.h>
#include <fcntl.h>
#include <invent.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/scsi.h>

#include <dmedia/dmedia.h>
#include "CdAudio.h"

// #define DBG(x) {x;}
#define DBG(x) {}  

/*
 * Buffer lengths
 */

#define CD_GRP0_CMDLEN  6
#define CD_GRP6_CMDLEN  10
#define CD_STATUSLEN    11
#define CD_SENSELEN     18
#define MSELPARMLEN 12

#define STA_RETRY       (-1)

/*
 * SCSI commands
 *
 * T_ for Toshiba
 * S_ for Sony
 * S2_ for SCSI II
 * SGI_ for SGI specific firmware stuff
 */

#define T_TRSEARCH          0xc0
#define T_PLAY              0xc1
#define T_STILL             0xc2
#define T_EJECT             0xc4
#define T_SUBCODE           0xc6
#define T_READDISC          0xc7

#define S_SETADDRFMT        0xc0
#define S_TOC               0xc1
#define S_PLAY_MSF          0xc7

#define S2_SUBCODE          0x42
#define S2_TOC              0x43
#define S2_PLAYMSF          0x47
#define S2_PLAYTRACK        0x48
#define S2_PAUSE            0x4b

#define SGI_CDROM           0xc9

#define G0_REZERO           0x01

/*
 * While we've got the device, we'll use CDROM_BLKSIZE.  When
 * we're done, we'll return it to UNIX_BLKSIZE
 */
#define CDROM_BLKSIZE       2048
#define UNIX_BLKSIZE        512

#define CD_PSTATUS_PLAYING  0x00
#define CD_PSTATUS_STILL    0x01
#define CD_PSTATUS_PAUSE    0x02
#define CD_PSTATUS_OTHER    0x03


/*
 * Filling command buffers for vendor-specific commands
 */
#define fillg6cmd fillg1cmd

/*
 * Filling command buffers for SCSI-2 commands
 */
#define fills2cmd fillg1cmd

/*
 * Filling command buffers for SGI specific commands
 */
#define fillsgicmd fillg1cmd

/*
 *  Convert to and from Binary Coded Decimal
 */
#define  TWODIGITBCDTOINT( bcd )    (((bcd) >> 4) * 10 + ((bcd) & 0xf))
#define  INTTOTWODIGITBCD( i )      ((((i) / 10) << 4) | ((i) % 10))

#ifdef DEBUG
#define CDDBG(x) {if (cddbg) {x;}}
#define PRINTSENSE(dsp) { \
	int i;\
	if (STATUS(dsp) == STA_CHECK) {\
		for (i = 0; i < CD_SENSELEN; i++)\
			fprintf( stderr, "%02x ", SENSEBUF(dsp)[i] );\
		fprintf( stderr, "\n" );\
	}}
static int cddbg = 1;
#else
#define CDDBG(x)
#define PRINTSENSE(dsp)
#endif


/*
 * private data structure representing a cd player and its state
 */


/*
 * Convert from the CD six-bit format to ASCII.
 */
static unchar sbtoa[] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9',  0,   0,   0,   0,   0,   0,
    '0', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
    'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z',  0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,
     0,   0,   0,   0,   0,   0,   0,   0,
};

/*
 * private function declarations
 */

static int read_disc( SCSIPLAYER *cdplayer );
static void CDtestready( SCSIPLAYER *cd );
static void act_like_a_cdrom( struct dsreq *dsp );
static int read_toc( SCSIPLAYER *cd );
static int set_blksize( SCSIPLAYER *cd, int blksize );
static void toshiba_modesense( struct dsreq *dsp, char *params, int size );
static void scsi2_modesense( struct dsreq *dsp, char *params, int size );

/*
 * modesense is a little different between Sony and Toshiba
 */
static void (*modesense[])( struct dsreq *, char *, int ) =
	{ NULL, toshiba_modesense, scsi2_modesense, scsi2_modesense };

priv_modeselect15(struct dsreq *dsp, caddr_t data, long datalen, char save, char vu)
{
  fillg0cmd(dsp, (uchar_t*)CMDBUF(dsp), G0_MSEL, save, 0, 0, B1(datalen), B1(vu<<6));
  filldsreq(dsp, (uchar_t*)data, datalen, DSRQ_WRITE|DSRQ_SENSE);
  return(doscsireq(getfd(dsp), dsp));
}

/*
 *  SCSIPLAYER *CDopen( dev, dir )
 *
 *  Description:
 *      Intitializes a data structure for a cdplayer, and returns a pointer to
 *      it.
 *
 *  Parameters:
 *      dev     Device to use for playing CD's; if NULL, attempts to locate
 *              a CD-ROM drive in /dev/scsi
 *
 *	dir	"r", "w" or "rw" indicating what you will do with the drive.
 *		This is for future writable CD devices
 *
 *  Returns:
 *      Pointer to a SCSIPLAYER struct if successful, NULL if error
 */

SCSIPLAYER* CDopen( char const *devscsi, char const *, SCSIPLAYER* player)
{
#define MSEL_SIZE 28
#define INQUIRY_SIZE 98
  struct dsreq* dsp = 0;
  char            devbuf[300];
  inventory_t     *inv;
  int             inquirybuf[(INQUIRY_SIZE + 3)/sizeof(int)];
  unsigned char   *inquiry = (unsigned char *)inquirybuf;
  enum drivetype  type;
  int		mselbuf[(MSEL_SIZE + 3) / sizeof(int)];
  unsigned char	*mselcmd = (unsigned char *)mselbuf;
  // CDTRACKINFO	info;
  int 		excl_id;

  int origTrack = 0;
  int origMin = 0;
  int origSec = 0;
  int origFrame = 0;

  if (devscsi) {
    dsp = dsopen( devscsi, O_RDONLY );
  }
  else {
    setinvent( );
    for (inv = getinvent( ); inv; inv = getinvent( ))
      if (inv->inv_class == INV_SCSI
	  && inv->inv_type == INV_CDROM)
	break;

    if (inv) {
      sprintf(devbuf, "/dev/scsi/sc%dd%dl%d",
	      inv->inv_controller, inv->inv_unit,
	      (inv->inv_state & 0xff00) >> 8 );
      devscsi = devbuf;
      excl_id = 1;		// mediad_get_exclusiveuse(devscsi, "libcdaudio");
      if (excl_id < 0) {
	setoserror(EACCES);
	endinvent();
	return(NULL);
      }
      dsp = dsopen( devscsi, O_RDONLY); 
    } else {
      setoserror(ENODEV);	/* Can't locate a CD-ROM drive */
      endinvent( );
      return (NULL);
    }
    endinvent( );
  }
  
  
  /* if dsopen failed, it should have set errno. */
  if (dsp) {
    
    inquiry12( dsp, (caddr_t)inquiry, INQUIRY_SIZE, 0);

    DBG(fprintf(stderr,"type = %s\n", inquiry+8));
    
    if (strncmp((const char*)inquiry+8, "TOSHIBA CD", 10) == 0)
      type = TOSHIBA_SCSI2;
    else if (strncmp((const char*)inquiry+8, "MATSHITACD", 10) == 0)
      type = MATSHITA;
    else return NULL;

    if (!(SENSEBUF( dsp ) = (char *)malloc( CD_SENSELEN ) )) {
      setoserror(ENOMEM);
      return (NULL);
    }
    SENSELEN( dsp ) = CD_SENSELEN;
    
    // act_like_a_cdrom( dsp );
    if (player == NULL) {
      if (!(player = (SCSIPLAYER *)malloc( sizeof(SCSIPLAYER) ))) {
	free( SENSEBUF( dsp ) );
	setoserror(ENOMEM);
	return (NULL);
      }
      memset( player, 0, sizeof (SCSIPLAYER) );
      player->type = type;
      player->exclId = excl_id;
      player->dev = strdup(devscsi);
      player->lastTrack = 0;	// we use this as a flag to see if we've read the toc.
    }
    else {
      // we have some state from an old player
      origTrack = player->track;
      origMin = player->absMin;
      origSec = player->absSec;
      origFrame = player->absFrame;
    }

    player->scsiRequest = dsp;
    CDtestready( player );
    player->deviceState = (STATUS(player->scsiRequest) == STA_GOOD) ? SCSI_READY :
  (STATUS(player->scsiRequest) == STA_CHECK) ? SCSI_NOMEDIA : SCSI_ERROR;

    CDupdatestatus( (SCSIPLAYER *)player);
    if (player->deviceState == SCSI_READY) CDpreventremoval( player );
    
    /*
     * Switch the drive to reading audio data
     */
    bzero(mselcmd, MSEL_SIZE);
    mselcmd[3] = 8;
    mselcmd[4] = 0x82;
    mselcmd[9] = CDDA_BLOCKSIZE >> 16;
    mselcmd[10] = (CDDA_BLOCKSIZE & 0x0000ff00) >> 8;
    mselcmd[11] = CDDA_BLOCKSIZE & 0x000000ff;
    mselcmd[12] = 2;		/* modeselect page 2 */
    mselcmd[13] = 0xe;	/* length of page 2 */ 
    // mselcmd[14] = 0x0; /* buffer full ratio, don't disconnect */
    mselcmd[14] = 0x6f; /* buffer full ratio - reconnect after
        			   16 blocks of data */

    priv_modeselect15(player->scsiRequest, (caddr_t)mselcmd, MSEL_SIZE, 0x10, 0);
    
    // try to set speed.

    //     mselcmd[12] = 0x20;		/* modeselect page 20h */
    //     mselcmd[13] = 0x1;	/* length of page 20h */ 
    //     mselcmd[14] = 0x1;  // set the double speed bit.
    //     priv_modeselect15(player->scsiRequest, (caddr_t)mselcmd, MSEL_SIZE, 0x10, 0);

    if (STATUS(player->scsiRequest ) == STA_GOOD) {
      player->scsiAudio = DM_TRUE;
    }
    else printf("failed to set audio mode in CDopen...\n"); fflush(stdout);
  }
  if (origTrack !=0) {
    CDseek(player, origMin, origSec, origFrame);
    CDreadda(player, NULL, 0);
    CDupdatestatus(player);
  }
  return (player);
}

/*
 * Seek to a given sub code frame
 */
unsigned long CDseek( SCSIPLAYER *cd, int m, int s, int f)
{
  CDTRACKINFO info;
  unsigned long newblock;
  
  if (!cd->scsiAudio) {
    setoserror(ENXIO);
    return (-1);
  }
  newblock = CDmsftoblock(cd, m, s, f);
  CDgettrackinfo(cd, cd->firstTrack, &info);
  if (CDmsftoblock(cd, info.start_min, info.start_sec, info.start_frame)
      <= newblock) {
    CDgettrackinfo(cd, cd->lastTrack, &info);
    if (CDmsftoblock(cd,
		     info.start_min + info.total_min,
		     info.start_sec + info.total_sec,
		     info.start_frame + info.total_frame)
	>= newblock) {
      cd->curBlock = newblock;
      return newblock;
    }
  }
  setoserror(EINVAL);
  return (-1);
}

/*
 * Seek to a given block
 */
unsigned long CDseekblock( SCSIPLAYER *cd, unsigned long block)
{
  CDTRACKINFO info;
  
  if (!cd->scsiAudio) {
    setoserror(ENXIO);
    return (-1);
  }
  CDgettrackinfo(cd, cd->firstTrack, &info);
  if (CDmsftoblock(cd, info.start_min, info.start_sec, info.start_frame)
      <= block) {
    CDgettrackinfo(cd, cd->lastTrack, &info);
    if (CDmsftoblock(cd,
		     info.start_min + info.total_min,
		     info.start_sec + info.total_sec,
		     info.start_frame + info.total_frame)
	>= block) {
      cd->curBlock = block;
      return block;
    }
  }
  setoserror(EINVAL);
  return (-1);
}

/*
 * Seek to the start of the given program number (track) on the CD.
 */
int CDseektrack(SCSIPLAYER* cd, int t)
{
  CDTRACKINFO info;
  
  if (!cd->scsiAudio) {
    setoserror(ENXIO);
    return (-1);
  }
  if (CDgettrackinfo( cd, t, &info )) {
    CDseek(cd, info.start_min, info.start_sec, info.start_frame);
    CDreadda(cd, NULL, 0);
    return t;
  }
  setoserror(EIO);
  return (-1);
}

/*
 * Read num_frames subcode frames of digital audio data starting at current
 * position. Update position after the read.
 */
int CDreadda(SCSIPLAYER *cd, CDFRAME* buf, int num_frames )
{
  unsigned char  *cmd;  // sense[CD_SENSELEN]
  
  if (!cd->scsiAudio) {
    setoserror(ENXIO);
    return (-1);
  }
  if (!cd->scsiRequest) {
    setoserror(EBADF);
    return (-1);
  }
  
  CDtestready(cd);
  
  if (STATUS(cd->scsiRequest) != STA_GOOD) {
    setoserror(EAGAIN);
    return (-1);
  }
  
  if ((cd->deviceState != SCSI_READY) && (cd->deviceState != SCSI_PAUSED)) {
    setoserror(EIO);
    return (-1);
  }
  
  cmd = (unsigned char*)CMDBUF(cd->scsiRequest);
  CMDLEN(cd->scsiRequest) = 12; /* 12 byte vendor specific command */
  filldsreq(cd->scsiRequest, (uchar_t *)buf, num_frames * CDDA_BLOCKSIZE,
	    DSRQ_READ | DSRQ_SENSE );
  cmd[0] = 0xa8;	    
  cmd[1] = 0;
  cmd[2] = cd->curBlock >> 24;
  cmd[3] = (cd->curBlock & 0x00ff0000) >> 16;
  cmd[4] = (cd->curBlock & 0x0000ff00) >> 8;
  cmd[5] = cd->curBlock & 0xff;
  cmd[6] = num_frames >> 24;
  cmd[7] = (num_frames & 0x00ff0000) >> 16;
  cmd[8] = (num_frames & 0x0000ff00) >> 8;
  cmd[9] = num_frames & 0xff;
  cmd[10] = 0;
  cmd[11] = 0;
  
  doscsireq( getfd( cd->scsiRequest ), cd->scsiRequest );
  if (STATUS(cd->scsiRequest) != STA_GOOD)
    return (0);			/* EOF */
  cd->curBlock += num_frames;
  return (DATASENT(cd->scsiRequest) /  CDDA_BLOCKSIZE);
}

/*
 *  int CDeject( SCSIPLAYER *cd  )
 *
 *  Description:
 *      Ejects the CD caddy from the CD-ROM drive
 *
 *  Parameters:
 *      cd          pointer to a SCSIPLAYER struct
 *
 *  Returns:
 *      non-zero if successful, 0 otherwise
 */

int CDeject( SCSIPLAYER *cdplayer  )
{
  // int             open = 0;
  int             status;
  
  CDtestready( cdplayer );
  
  if (STATUS(cdplayer->scsiRequest) != STA_GOOD) {
    setoserror(EIO);
    return (0);
  }
  
  if (cdplayer->type == TOSHIBA) {
    fillg6cmd(cdplayer->scsiRequest, (uchar_t*)CMDBUF(cdplayer->scsiRequest),
	      T_EJECT, 1, 0, 0, 0, 0, 0, 0, 0, 0);
    filldsreq(cdplayer->scsiRequest, NULL, 0, DSRQ_READ | DSRQ_SENSE);
    doscsireq(getfd( cdplayer->scsiRequest ), cdplayer->scsiRequest);
    status = STATUS(cdplayer->scsiRequest) == STA_GOOD;
  }
  else {
    CDallowremoval( cdplayer );
    fillg0cmd(cdplayer->scsiRequest, (uchar_t*)CMDBUF(cdplayer->scsiRequest),
	      G0_LOAD, 1, 0, 0, 2, 0);
    filldsreq(cdplayer->scsiRequest, NULL, 0, DSRQ_READ | DSRQ_SENSE);
    doscsireq(getfd(cdplayer->scsiRequest), cdplayer->scsiRequest);
    status = STATUS(cdplayer->scsiRequest) == STA_GOOD;
    CDpreventremoval(cdplayer);
  }
  
  cdplayer->paused = 0;
  
  if (!status) {
    setoserror(EIO);
  }
  
  return status;
}

/*
 *  int CDstop( CDPLAYER *cd )
 *
 *  Description:
 *      Stops play of the CD
 *
 *  Parameters:
 *      cd          pointer to a CDPLAYER struct
 *
 *  Returns:
 *      non-zero if successful, 0 otherwise
 */

int CDstop(SCSIPLAYER* cdplayer )
{
  register struct dsreq   *dsp = cdplayer->scsiRequest;
  
  cdplayer->paused = 0;
  if (cdplayer->type == TOSHIBA) {
    fillg0cmd( dsp, (uchar_t*)CMDBUF(dsp), G0_REZERO, 0, 0, 0, 0, 0 );
    filldsreq( dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
    doscsireq( getfd( dsp ), dsp );
  }
  else {
    fillg0cmd( dsp, (uchar_t*)CMDBUF(dsp), G0_STOP, 0, 0, 0, 0, 0 );
    filldsreq( dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
    doscsireq( getfd( dsp ), dsp );
    
    fillg0cmd( dsp, (uchar_t*)CMDBUF(dsp), G0_SEEK, 0, B2( 0 ), 0, 0 );
    filldsreq( dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
    doscsireq( getfd( dsp ), dsp );
  }
  
  if (STATUS(dsp) != STA_GOOD) {
    CDDBG(fprintf(stderr, "%s(%d): CD_ERROR\n", __FILE__, __LINE__ ));
    //cdplayer->state = CD_ERROR;
    return (0);
  }
  
  return (1);
}


/*
 *  int CDclose( SCSIPLAYER *cd  )
 *
 *  Description:
 *      Closes the dsp's file descriptor, and frees previously allocated
 *      memory
 *
 *  Parameters:
 *      cd          Pointer to a SCSIPLAYER struct
 *
 *  Returns:
 *      non-zero if successful, 0 otherwise
 */
int CDclose(SCSIPLAYER *cdplayer)
{
  if (cdplayer->scsiRequest) {
    CDallowremoval( cdplayer );
    if (cdplayer->type == TOSHIBA_SCSI2)
      if (!set_blksize( cdplayer, UNIX_BLKSIZE ))
	fprintf(stderr,"CDclose:  failed to set block size to %d.\n", UNIX_BLKSIZE);
    free( SENSEBUF( cdplayer->scsiRequest ) );
    dsclose( cdplayer->scsiRequest );
  }
  
  free(cdplayer->dev);
  free(cdplayer);
  return (1);
}


/*
 *  static int CDupdatestatus( CDPLAYER *cd )
 *
 *  Description:
 *      Check the status of the CD-ROM drive, updating our data structures
 *
 *  Parameters:
 *      cd          pointer to a CDPLAYER structure
 *
 *  Returns:
 *      non-zero if successful, 0 if error
 */

#define TRACK_INFO_SIZE 4

int CDupdatestatus(SCSIPLAYER* cdplayer)
{
  int             min;
  int             sec;
  int		  track_info_buf[(TRACK_INFO_SIZE + 3)/sizeof(int)];
  unsigned char   *track_info = (unsigned char *)track_info_buf;
  
  CDtestready( cdplayer );
  if (STATUS(cdplayer->scsiRequest) != STA_GOOD) {
    cdplayer->deviceState = SCSI_NOMEDIA;
    return (1);
  }
  
  if (!cdplayer->lastTrack) {
    if (!read_disc( cdplayer )) {
      if (cdplayer->deviceState == SCSI_DATAMEDIA)
	return (1);
      else {
	CDDBG(fprintf(stderr, "%s(%d): CD_ERROR\n", __FILE__, __LINE__ ));
	cdplayer->deviceState = SCSI_ERROR;
	return (0);
      }
    }
  }

  if (cdplayer->type == TOSHIBA) {
    int		    statusbuf[(CD_STATUSLEN + 3)/sizeof(int)];
    unsigned char   *status = (unsigned char *)statusbuf;
    
    fillg6cmd( cdplayer->scsiRequest, (uchar_t*)CMDBUF(cdplayer->scsiRequest),
	       T_SUBCODE, CD_STATUSLEN, 0, 0, 0, 0, 0, 0, 0, 0 );
    filldsreq( cdplayer->scsiRequest, status, CD_STATUSLEN,
	       DSRQ_READ | DSRQ_SENSE );
    doscsireq( getfd( cdplayer->scsiRequest ), cdplayer->scsiRequest );
    
    if (STATUS(cdplayer->scsiRequest) == STA_GOOD) {
      switch (status[0]) {
	
      case CD_PSTATUS_PLAYING:
	cdplayer->deviceState = SCSI_PLAYING;
	break;
	
      case CD_PSTATUS_STILL:
	cdplayer->deviceState = SCSI_STILL;
	break;
	
      case CD_PSTATUS_PAUSE:
	cdplayer->deviceState = SCSI_PAUSED;
	break;
	
      case CD_PSTATUS_OTHER:
	cdplayer->deviceState = SCSI_READY; /* just a guess for now */
	break;
      }
      
      cdplayer->track = TWODIGITBCDTOINT( status[2] );
      cdplayer->progMin = TWODIGITBCDTOINT( status[4] );
      cdplayer->progSec = TWODIGITBCDTOINT( status[5] );
      cdplayer->progFrame = TWODIGITBCDTOINT( status[6] );
      cdplayer->absMin = TWODIGITBCDTOINT( status[7] );
      cdplayer->absSec = TWODIGITBCDTOINT( status[8] );
      cdplayer->absFrame = TWODIGITBCDTOINT( status[9] );
    }
    fillg6cmd( cdplayer->scsiRequest, (uchar_t*)CMDBUF(cdplayer->scsiRequest),
	       T_READDISC, 1, 0, 0, 0, 0, 0, 0, 0, 0 );
    filldsreq( cdplayer->scsiRequest, track_info, TRACK_INFO_SIZE,
	       DSRQ_SENSE | DSRQ_READ );
    doscsireq( getfd( cdplayer->scsiRequest ), cdplayer->scsiRequest );
    
    if (STATUS(cdplayer->scsiRequest) == STA_GOOD) {
      cdplayer->totalMin = TWODIGITBCDTOINT( track_info[0] );
      cdplayer->totalSec = TWODIGITBCDTOINT( track_info[1] );
      cdplayer->totalFrame = TWODIGITBCDTOINT( track_info[2] );
    }
  }
  else {			/* SCSI-2 && SONY */
#define TOC_INFO_SIZE 12
#define SUB_CHANNEL_SIZE 16
    int 	    toc_info_buf[(TOC_INFO_SIZE + 3)/sizeof(int)];
    unsigned char   *toc_info = (unsigned char *)toc_info_buf;
    int 	    sub_channel_buf[(SUB_CHANNEL_SIZE + 3)/sizeof(int)];
    unsigned char   *sub_channel = (unsigned char *)sub_channel_buf;
    int             lead_out;
    
    fills2cmd(cdplayer->scsiRequest, (uchar_t*)CMDBUF(cdplayer->scsiRequest),
	      S2_SUBCODE, 2, 0x40, 1, 0, 0, 0, B2( SUB_CHANNEL_SIZE ), 0 );
    filldsreq( cdplayer->scsiRequest, sub_channel, SUB_CHANNEL_SIZE,
	       DSRQ_SENSE | DSRQ_READ );
    doscsireq( getfd( cdplayer->scsiRequest ), cdplayer->scsiRequest );
    
    if (STATUS( cdplayer->scsiRequest ) != STA_GOOD)
      return (0);
    
    switch (sub_channel[1]) {
      
    case 0x11:
      cdplayer->deviceState = SCSI_PLAYING;
      break;
      
    case 0x12:
      /*
       * The Sony drive likes to always report that it's in
       * pause mode; therefore, it is necessary to make a
       * distinction between "the drive is in paused mode"
       * and "the user pressed the pause button".  The
       * paused field gets turned on in situations where
       * the user pressed the pause button.
       */
      cdplayer->deviceState = cdplayer->paused ? SCSI_PAUSED : SCSI_READY;
      break;
      
    case 0:
    case 0x13:
    case 0x15:
      cdplayer->deviceState = SCSI_READY;
      break;
      
    case 0x14:
    default:
      CDDBG(fprintf(stderr, "%s(%d): CD_ERROR\n", __FILE__, __LINE__ ));
      cdplayer->deviceState = SCSI_ERROR;
      break;
    }
    
    cdplayer->track = sub_channel[6];
    cdplayer->index = sub_channel[7];
    cdplayer->progMin = sub_channel[13];
    cdplayer->progSec = sub_channel[14];
    cdplayer->progFrame = sub_channel[15];
    cdplayer->absMin = sub_channel[9];
    cdplayer->absSec = sub_channel[10];
    cdplayer->absFrame = sub_channel[11];
    
  }
  
  return (1);
}

int CDupdatestatusfromframe(SCSIPLAYER* cdplayer, CDFRAME* suppliedFrame)
{
  int             min;
  int             sec;
  int		  track_info_buf[(TRACK_INFO_SIZE + 3)/sizeof(int)];
  unsigned char   *track_info = (unsigned char *)track_info_buf;
  
  CDtestready( cdplayer );
  if ((cdplayer == NULL)
      || (suppliedFrame == NULL)
      || (STATUS(cdplayer->scsiRequest) != STA_GOOD)) {
    return (0);
  }

  struct subcodeQ* scq = &suppliedFrame->subcode;

  cdplayer->track = scq->data.mode1.track.dhi*10 + scq->data.mode1.track.dlo;
  cdplayer->index = scq->data.mode1.index.dhi*10 + scq->data.mode1.index.dlo;

  cdplayer->progMin = scq->data.mode1.ptime.mhi*10 + scq->data.mode1.ptime.mlo;
  cdplayer->progSec = scq->data.mode1.ptime.shi*10 + scq->data.mode1.ptime.slo;
  cdplayer->progFrame = scq->data.mode1.ptime.fhi*10 + scq->data.mode1.ptime.flo;

  cdplayer->absMin = scq->data.mode1.atime.mhi*10 + scq->data.mode1.atime.mlo;
  cdplayer->absSec = scq->data.mode1.atime.shi*10 + scq->data.mode1.atime.slo;
  cdplayer->absFrame = scq->data.mode1.atime.fhi*10 + scq->data.mode1.atime.flo;

  DBG(fprintf(stderr,"Track = %d; Index = %d; atime = %d:%d:%d; ptime = %d:%d:%d;\n",
	      cdplayer->track, cdplayer->index,
	      cdplayer->absMin, cdplayer->absSec, cdplayer->absFrame,
	      cdplayer->progMin, cdplayer->progSec, cdplayer->progFrame));

  DBG(fprintf(stderr,"curblock = %lu; CDmsftoblock = %lu\n",
	  cdplayer->curBlock,
	  CDmsftoblock(cdplayer, cdplayer->absMin, cdplayer->absSec, cdplayer->absFrame)));

  if (cdplayer->curBlock == CDmsftoblock(cdplayer, cdplayer->absMin,
					 cdplayer->absSec, cdplayer->absFrame))
    return 1;
  else
    return 0;
}


static int read_disc( SCSIPLAYER *cdplayer )
{
#define TRACK_INFO_SIZE 4
#define S2_TRACK_INFO_SIZE 12
  int			  trackInfoBuf[(TRACK_INFO_SIZE + 3)/sizeof(int)];
  unsigned char           *trackInfo = (unsigned char *)trackInfoBuf;
  int			s2trackInfoBuf[(S2_TRACK_INFO_SIZE + 3)/sizeof(int)];
  unsigned char         *s2trackInfo = (unsigned char *)s2trackInfoBuf;
  register struct dsreq   *dsp = cdplayer->scsiRequest;
  
  if (cdplayer->type == TOSHIBA) {
    fillg6cmd( dsp, (uchar_t*)CMDBUF(dsp), T_READDISC, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
    filldsreq( dsp, trackInfo, TRACK_INFO_SIZE,
	      DSRQ_SENSE | DSRQ_READ );
    doscsireq( getfd( dsp ), dsp );
    if (STATUS(dsp) != STA_GOOD)
      return (0);
    if (trackInfo[3] & 0x04) {
      cdplayer->deviceState = SCSI_DATAMEDIA;
      return 0;
    } else {
      cdplayer->firstTrack = TWODIGITBCDTOINT( trackInfo[0] );
      cdplayer->lastTrack = TWODIGITBCDTOINT( trackInfo[1] );
    }
  }
  else {
    fillg6cmd( dsp, (uchar_t*)CMDBUF(dsp), S2_TOC, 0, 0, 0, 0, 0, 1,
	      B2( S2_TRACK_INFO_SIZE ), 0 );
    filldsreq( dsp, s2trackInfo, S2_TRACK_INFO_SIZE,
	      DSRQ_SENSE | DSRQ_READ );
    doscsireq( getfd( dsp ), dsp );
    if (STATUS(dsp) != STA_GOOD)
      return (0);
    if (s2trackInfo[5] & 0x04) {
      cdplayer->deviceState = SCSI_DATAMEDIA;
      return 0;
    } else {
      cdplayer->firstTrack = s2trackInfo[2];
      cdplayer->lastTrack = s2trackInfo[3];
    }
  }
  
  return (1);
}

static void CDtestready( SCSIPLAYER *cd )
{
  int keep_trying;
  //register dsreq_t    *dsp = cd->scsiRequest;
  int changed = 0;
  
  do {
    fillg0cmd( cd->scsiRequest, (uchar_t*)CMDBUF(cd->scsiRequest), G0_TEST, 0, 0, 0, 0, 0 );
    filldsreq( cd->scsiRequest, NULL, 0, DSRQ_READ | DSRQ_SENSE );
    doscsireq( getfd( cd->scsiRequest ), cd->scsiRequest );
    
    if (STATUS( cd->scsiRequest ) == STA_CHECK
	&& (SENSEBUF( cd->scsiRequest )[2] & 0xf) == 6
	&& SENSEBUF( cd->scsiRequest )[12] == 0x29) {
      // act_like_a_cdrom( cd->scsiRequest );
      keep_trying = 1;
      changed = 1;
    }
    else
      keep_trying = 0;
  } while (keep_trying);
  
  if ((changed || STATUS( cd->scsiRequest ) != STA_GOOD) && cd->toc) {
    free (cd->toc);
    cd->toc = 0;
  }
  
}

int CDsbtoa(char *s, const unchar *sb, int count)
{
  int i;
  
  /*
   * XXX Error checking ?
   */
  for (i = 0; i < count; i++)
    *s++ = sbtoa[sb[i]];
  *s = '\0';
  return i;
}

void CDtimetoa(char *s, struct cdtimecode *tp)
{
  s[0] = tp->mhi == 0xA ? '-' : '0' + tp->mhi;
  s[1] = tp->mlo == 0xA ? '-' : '0' + tp->mlo;
  s[3] = tp->shi == 0xA ? '-' : '0' + tp->shi;
  s[4] = tp->slo == 0xA ? '-' : '0' + tp->slo;
  s[6] = tp->fhi == 0xA ? '-' : '0' + tp->fhi;
  s[7] = tp->flo == 0xA ? '-' : '0' + tp->flo;
}

void CDpreventremoval( SCSIPLAYER *cdplayer )
{
  fillg0cmd( cdplayer->scsiRequest, (uchar_t*)CMDBUF(cdplayer->scsiRequest), G0_PREV, 0, 0, 0, 1, 0 );
  filldsreq( cdplayer->scsiRequest, NULL, 0, DSRQ_READ | DSRQ_SENSE );
  doscsireq( getfd( cdplayer->scsiRequest ), cdplayer->scsiRequest );
}

void CDallowremoval( SCSIPLAYER *cdplayer )
{
  fillg0cmd( cdplayer->scsiRequest, (uchar_t*)CMDBUF(cdplayer->scsiRequest), G0_PREV, 0, 0, 0, 0, 0 );
  filldsreq( cdplayer->scsiRequest, NULL, 0, DSRQ_READ | DSRQ_SENSE );
  doscsireq( getfd( cdplayer->scsiRequest ), cdplayer->scsiRequest );
}

/*
 *  int
 *  CDgettrackinfo( SCSIPLAYER *cd, int track, CDTRACKINFO *info )
 *
 *  Description:
 *      Get relevant information about track on cd
 *
 *  Parameters:
 *      cd      The CD to get info about
 *      track   Number of track to get info about
 *      info    Pointer to buffer to receive the information
 *
 *  Returns:
 *      1 if successful, 0 otherwise
 */

int CDgettrackinfo( SCSIPLAYER *cd, int track, CDTRACKINFO *info )
{
  unsigned char           track_info[4];
  register struct dsreq   *dsp = cd->scsiRequest;
  int                     framesTmp, this_track, next_track;
  TRACK                   *trackp;
  
  CDtestready(cd);
  
  if (STATUS(cd->scsiRequest) != STA_GOOD)
    return (0);
  
  if (!cd->toc && !read_toc( cd ))
    return (0);
  
  if (track < cd->firstTrack || track > cd->lastTrack)
    return (0);
  
  trackp = &cd->toc[track - cd->firstTrack];
  info->start_min = trackp->min;
  info->start_sec = trackp->sec;
  info->start_frame = trackp->frame;
  info->start_block = CDmsftoblock(cd, trackp->min, trackp->sec, trackp->frame);
  
  trackp++;			// next start time
  //framesTmp = (((trackp->min * 60) + trackp->sec) * 75 + trackp->frame) - info->start_block;

  framesTmp = ((trackp->min * 60) + trackp->sec) * 75 + trackp->frame;
  framesTmp -= ((info->start_min * 60) + info->start_sec) * 75 + info->start_frame;
  
  info->total_frame = framesTmp % 75;
  framesTmp /= 75;
  info->total_sec = framesTmp % 60;
  info->total_min = framesTmp / 60;
  info->total_blocks = CDmsftoframe(info->total_min, info->total_sec, info->total_frame);
  return (1);
}

/*
 *  static int act_like_a_cdrom( struct dsreq *dsp )
 *
 *  Description:
 *      This command, which must be called after a SCSI reset occurs,
 *      instructs the CD-ROM drive to quit acting like a disk drive and
 *      start acting like a CD-ROM drive.
 *
 *  Parameters:
 *      dsp     devscsi
 */

static void act_like_a_cdrom( struct dsreq *dsp )
{
  fillsgicmd( dsp, (uchar_t*)CMDBUF(dsp), SGI_CDROM, 0, 0, 0, 0, 0, 0, 0, 0, 0 );
  filldsreq( dsp, NULL, 0, DSRQ_READ | DSRQ_SENSE );
  doscsireq( getfd( dsp ), dsp );
}

struct toc_header {
  unsigned short  length;
  unsigned char   first;
  unsigned char   last;
};

struct toc_entry {
  unsigned char   reserved;
  unsigned char   control;
  unsigned char   track;
  unsigned char   reserved2;
  unsigned char   reserved3;
  unsigned char   min;
  unsigned char   sec;
  unsigned char   frame;
};

struct toc {
  struct toc_header   head;
  struct toc_entry    tracks[1];
};

#define MAXTOCLEN   (100 * sizeof (struct toc_entry) \
		     + sizeof (struct toc_header))
     
static int read_toc( SCSIPLAYER *cd )
{
  register dsreq_t        *dsp = cd->scsiRequest;
  int			  trackInfoBuf[(TRACK_INFO_SIZE + 3)/sizeof(int)];
  unsigned char           *track_info = (unsigned char *)trackInfoBuf;
  int			  sonyTrackInfoBuf[(MAXTOCLEN + 3)/sizeof(int)];
  unsigned char           *sony_track_info = (unsigned char *)sonyTrackInfoBuf;
  struct toc              *tocp;
  int                     track, this_track, frames;
  
  cd->toc = (TRACK *)malloc( sizeof (TRACK) * (cd->lastTrack - cd->firstTrack + 2) );
  if (!cd->toc)
    return (0);
  if (cd->type == TOSHIBA) {
    this_track = INTTOTWODIGITBCD( track );
    for (track = cd->firstTrack; track <= cd->lastTrack; track++) {
      this_track = INTTOTWODIGITBCD( track );
      fillg6cmd( dsp, (uchar_t*)CMDBUF(dsp), T_READDISC, 2, this_track,
		0, 0, 0, 0, 0, 0, 0 );
      filldsreq( dsp, track_info, TRACK_INFO_SIZE, DSRQ_SENSE |
		DSRQ_READ );
      doscsireq( getfd( dsp ), dsp );
      if (STATUS(dsp) != STA_GOOD) {
	CDDBG(fprintf(stderr, "%s(%d): SCSI_ERROR\n", __FILE__, __LINE__ ));
	cd->deviceState = SCSI_ERROR;
	free( cd->toc );
	cd->toc = 0;
	return (0);
      }
      if (track_info[3] & 0x04) {
	cd->deviceState = SCSI_DATAMEDIA;
	return 0;
      }
      cd->toc[track - cd->firstTrack].min =
	TWODIGITBCDTOINT( track_info[0] );
      cd->toc[track - cd->firstTrack].sec =
	TWODIGITBCDTOINT( track_info[1] );
      cd->toc[track - cd->firstTrack].frame =
	TWODIGITBCDTOINT( track_info[2] );
    }
    cd->toc[cd->lastTrack + 1 - cd->firstTrack].min = cd->totalMin;
    cd->toc[cd->lastTrack + 1 - cd->firstTrack].sec = cd->totalSec;
    cd->toc[cd->lastTrack + 1 - cd->firstTrack].frame = cd->totalFrame;
  }
  else {			/* SCSI-2 and SONY */
    fills2cmd( dsp, (uchar_t*)CMDBUF(dsp), S2_TOC, 2, 0, 0, 0, 0, 0,
	      B2( MAXTOCLEN ), 0 );
    filldsreq( dsp, sony_track_info, MAXTOCLEN,
	      DSRQ_SENSE | DSRQ_READ );
    doscsireq( getfd( dsp ), dsp );
    if (STATUS(dsp) != STA_GOOD) {
      CDDBG(fprintf(stderr, "%s(%d): SCSI_ERROR\n", __FILE__, __LINE__ ));
      cd->deviceState = SCSI_ERROR;
      free( cd->toc );
      cd->toc = 0;
      return (0);
    }
    if (sony_track_info[5] & 0x04) {
      cd->deviceState = SCSI_DATAMEDIA;
      return 0;
    } else {
      tocp = (struct toc *)sony_track_info;
      for (track = cd->firstTrack; track <= cd->lastTrack + 1; track++) {
	cd->toc[track - cd->firstTrack].min =
	  tocp->tracks[track - cd->firstTrack].min;
	cd->toc[track - cd->firstTrack].sec =
	  tocp->tracks[track - cd->firstTrack].sec;
	cd->toc[track - cd->firstTrack].frame =
	  tocp->tracks[track - cd->firstTrack].frame;
      }
    }
  }
  /*
   * This is because specifying the starting address of the lead-out
   * track as the end-of-play address (for Sony) causes an error; we
   * must back off by one frame.  We do it for Toshiba as well to
   * make the same database work for both.
   */
  frames = cd->toc[cd->lastTrack - cd->firstTrack + 1].frame + 75 *
    (cd->toc[cd->lastTrack - cd->firstTrack + 1].sec + 60 *
     (cd->toc[cd->lastTrack - cd->firstTrack + 1].min));
  frames--;
  cd->toc[cd->lastTrack - cd->firstTrack + 1].frame = frames % 75;
  frames /= 75;
  cd->toc[cd->lastTrack - cd->firstTrack + 1].sec = frames % 60;
  cd->toc[cd->lastTrack - cd->firstTrack + 1].min = frames / 60;
  return (1);
}

static int
set_blksize( SCSIPLAYER *cd, int blksize )
{
#define PARAMS_SIZE 12
  int			  paramsBuf[(PARAMS_SIZE + 3)/sizeof(int)];
  char                    *params = (char *)paramsBuf;
  int                     retries;
  register struct dsreq   *dsp = cd->scsiRequest;
  
  
  retries = 10;
  while (retries--) {
    (*modesense[cd->type])( dsp, params, PARAMS_SIZE );
    
    if (STATUS(dsp) == STA_GOOD) {
      if (*(int *)&params[8] == blksize)
	return (1);
      else
	break;
    }
    else if (STATUS(dsp) == STA_CHECK || STATUS(dsp) == STA_RETRY)
      continue;
    else
      return (0);
  }
  
  retries = 10;
  while (retries--) {
    memset( params, '\0', PARAMS_SIZE );
    params[3] = 0x08;
    *(int *)&params[8] = blksize;
    
    modeselect15( dsp, params, PARAMS_SIZE, 0, 0 );
    
    if (STATUS(dsp) == STA_GOOD)
      return (1);
    else if (STATUS(dsp) == STA_CHECK || STATUS(dsp) == STA_RETRY)
      continue;
    else
      return (0);
  }
  
  return (0);
}

static void
scsi2_modesense( struct dsreq *dsp, char *params, int size )
{
#define SONY_PARAMS_SIZE (MSELPARMLEN + 8)
  int	  sonyParamsBuf[(SONY_PARAMS_SIZE + 3)/sizeof(int)];
  char    *sony_params = (char *)sonyParamsBuf, *databuf;
  int     datalen;
  
  databuf = DATABUF( dsp );
  datalen = DATALEN( dsp );
  /*
   * Amazingly enough, modesense fails if you don't request any
   * page code information
   */
  fillg0cmd( dsp, (uchar_t*)CMDBUF( dsp ), G0_MSEN, 0, 1, 0,
	    SONY_PARAMS_SIZE , 0 );
  filldsreq( dsp, (uchar_t*)sony_params, SONY_PARAMS_SIZE,
	    DSRQ_READ | DSRQ_SENSE );
  doscsireq( getfd( dsp ), dsp );
  
  if (STATUS( dsp ) == STA_GOOD)
    bcopy( sony_params, params, size );
  
  DATABUF( dsp ) = databuf;
  DATALEN( dsp ) = datalen;
}

static void
toshiba_modesense( struct dsreq *dsp, char *params, int size )
{
  modesense1a( dsp, params, size, 0, 0, 0 );
}


// Utility Routines


/*
 * Convert minutes, seconds and frames of to a CD frame number.
 */
unsigned long
CDmsftoframe(int m, int s, int f)
{
  return (m*60*75 + s*75 + f);
}

unsigned long
CDtctoframe(struct cdtimecode* tc)
{
  return CDmsftoframe(tc->mhi * 10 + tc->mlo,
		      tc->shi * 10 + tc->slo,
		      tc->fhi * 10 + tc->flo);
}

/*
 * Convert an ASCII string to a struct {cd,mt}timecode which can then be
 * used as an argument to a search ioctl.
 *
 * This returns 0 if the string does not represent a valid timecode,
 * non-zero otherwise.
 */
int CDatotime(struct cdtimecode *tp, const char *loc)
{
  int m, s, f;
  
  if (!CDatomsf(loc, &m, &s, &f)) {
    return 0;
  }
  
  /*
   * put the fields into the timecode struct
   */
  tp->mhi = m / 10;
  tp->mlo = m % 10;
  tp->shi = s / 10;
  tp->slo = s % 10;
  tp->fhi = f/ 10;
  tp->flo = f % 10;
  return 1;			/* valid timecode */
}

int CDatomsf(const char *loc,  int *m,  int *s, int *f)
{
  char buf[80];
  char *tok;
  int field[3];
  int fno = 0;
  char *str = buf;
  
  /*
   * zero out the fields. Fields which are not specified thus default
   * to zero.
   */
  bzero(field, 3 * sizeof(int));
  /*
   * copy to a temporary buffer; strtok is destructive
   * our token separators are arbitrary here; likely most people will
   * use the ':'
   */
  strncpy(buf, loc, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';
  while ((tok = strtok(str,":-/#|,.;*")) && fno < 3) {
    str = 0;
    field[fno++] = atoi(tok);
  }
  
  /*
   * check fields for validity
   */
  if (field[0] < 0 || field[0] > 119 || /* check minutes */
      field[1] < 0 || field[1] > 59 ||	/* check seconds */
      field[2] < 0 || field[2] > 74) { /* check frames */
    return 0;
  }
  /*
   * put the fields into msf
   */
  *m = field[0];
  *s = field[1];
  *f = field[2];
  return 1;			/* valid timecode */
}



void
CDframetotc(unsigned long fr, struct cdtimecode* tc)
{
  int m,s,f;
  CDframetomsf(fr,&m,&s,&f);
  /*
   * now set the msf fields in the timecode. These are in BCD;
   */
  tc->mhi = m / 10;
  tc->mlo = m % 10;
  tc->shi = s / 10;
  tc->slo = s % 10;
  tc->fhi = f / 10;
  tc->flo = f % 10;
}

void
CDframetomsf(unsigned long fr, int *m, int *s, int *f)
{
  unsigned long mrem;
  *m = fr / (60*75);
  mrem = fr % (60*75);
  *s = mrem/75;
  *f = mrem % 75;
}

/*
 * Convert minutes, seconds and frames of absolute time code to
 * a logical block number on the device.
 */
unsigned long
CDmsftoblock(SCSIPLAYER *cd, int m, int s, int f)
{
  if (cd->type == TOSHIBA_SCSI2 || cd->type == MATSHITA)
    return (m*60*75 + s*75 + f - 150);
  else
    return (m*60*75 + s*75 + f);
}

unsigned long
CDframetoblock(SCSIPLAYER *cd, unsigned long fr)
{
  if (cd->type == TOSHIBA_SCSI2 || cd->type == MATSHITA)
    return (fr - 150);
  else
    return (fr);
}

void
CDblocktomsf(SCSIPLAYER *cd, unsigned long fr, int *m, int *s, int *f)
{
  unsigned long mrem;
  if (cd->type == TOSHIBA_SCSI2 || cd->type == MATSHITA)
    fr += 150;
  *m = fr / (60*75);
  mrem = fr % (60*75);
  *s = mrem/75;
  *f = mrem % 75;
}


