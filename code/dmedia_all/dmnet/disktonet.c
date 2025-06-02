/*
 * File:          vidtonet.c 
 *
 * Description:   vidtonet captures a video stream using vl into
 *                dmbuffers, and sends these over dmnet
 *
 *                SGI DMBuffer Library Functions used
 *
 *                dmParamsCreate()
 *                dmParamsSetType()
 *	          dmParamsDestroy()
 *	          dmBufferCreatePool()
 *	          dmBufferSetPoolDefaults()
 *	          dmBufferFree()
 *
 *
 *                SGI dmNet Library Functions used
 * 
 *                dmNetOpen()                
 *                dmNetConnect()
 *                dmNetGetParams()
 *                dmNetRegisterPool()
 *                dmNetSend()
 */

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gl/image.h>
#include <limits.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <dmedia/vl.h>

#include <dmedia/dmedia.h>
#include <dmedia/dm_buffer.h>
#include <dmedia/dm_params.h>
#include <sys/dmcommon.h>
#include <dmedia/dmnet.h>
#include <dmnet/dmnet_striped_hippi.h>

#include <dmedia/moviefile.h>
#include <signal.h>
#include <assert.h>

/* #define SEMAPHORE */

#ifdef SEMAPHORE
#include <ulocks.h>
#endif

#define DISK_BLOCK_SIZE         512
#define MV_IMAGE_TIME_SCALE_PAL 25
#define MV_IMAGE_TIME_SCALE_NTSC 30000


char *_progName;

DMbufferpool pool =NULL;
int frame = 0;
int poolsize=8;
int xsize = 0, ysize =0;
int DEBUG = 0;

DMnetconnection cnp;
static char** HippiDevs=0;
static int NHippiDevs=0;

/* thread synchronization */
int dmnetPid=0;
#ifdef SEMAPHORE
usptr_t* arena=NULL;
usema_t* empty=NULL;
usema_t* full=NULL;
#else
DMparams* plistLocal=NULL;
#endif

/* direct i/o and file access related variables */
int movieLength;
int _quit = 0;
int fileFD = -1;
struct iovec *videoData = NULL;
long *origSizeOfPicture = NULL;
char *fileName = NULL;
DMbuffer* bufArray = NULL;
int ioBlockSize;
int frameSize = 0;
MVid mvImageTrack;
long ioVecCountMax = 4;
int vlCaptureType;
MVid movie;
struct timeval startTime;

void cleanup(int);
void printStats(void);
void usage(void);
int readQTFileFormat( void );
int readQTFrames( int, int );
int readQTFields( int, int );
void sigHnd( int );

#define USAGE \
"%s: [-n <num>] [-b <poolsize>] [-v <vector>]\n\
\t\t[-c <conntype>] [-N <plugin>]\n\
\t\t[-l] [-q <port>] [-H <dev#>] [-f <file name>]\n\
\t\t[-L] [-h] -T <hostname>\n\
\n\
\t-n num          number of frames to read (default 1000)\n\
\t-v vector       i/o vector size (default 4)\n\
\t-b poolsize     poolsize (default 8), should be a multiple of vector size\n\
\t-c conntype     DmNet connection type\n\
\t-N plugin       DmNet plugin tag\n\
\t-l              local video server (uses DMNET_LOCAL)\n\
\t-q port         port number\n\
\t-H dev# list    string of hippi dev #'s\n\
\t\t\t-H 38 stripes across /dev/hippi3 and /dev/hippi8\n\
\t-f file name    video file\n\
\t-L              toggle loop mode\n\
\t-h              this help message\n\
\t-T hostname     video server host\n"

void 
err(char* str) 
{
	fprintf(stderr, "%s: %s\n", _progName, str);
	cleanup(1);
}

void
usage(void)
{
    fprintf(stderr, USAGE, _progName);
}

void
dmNetThread(void* unused)
{
#ifndef SEMAPHORE
	DMnetconnection cnpLocal=NULL;
	register int fdLocal=-1;
	fd_set fdset;
	DMbuffer buf;
#endif

	if (DEBUG) fprintf(stderr, "dmNetThread active\n");
    sigset(SIGHUP, SIG_DFL);    /* arrange to die when parent does */
    prctl(PR_TERMCHILD);

#ifndef SEMAPHORE
	dmNetOpen(&cnpLocal);
	/* use same plistLocal as main() */
	if(dmNetListen(cnpLocal, plistLocal)== DM_FAILURE)
	err("dmNetListen Local failed\n");
	if(dmNetAccept(cnpLocal, plistLocal) == DM_FAILURE)
	err("dmNetAccept Local failed\n");
	if ((fdLocal = dmNetDataFd(cnpLocal)) == -1)
	err("dmNetDataFd Local failed\n");
#endif

    for(frame=0;;frame++) {
#ifdef SEMAPHORE
	uspsema(full);
	if ( dmNetSend( cnp, bufArray[ frame%poolsize ] ) != DM_SUCCESS ) {
#else
	FD_ZERO(&fdset); 
	FD_SET(fdLocal, &fdset);
	if(select(fdLocal+1, &fdset, 0, 0, 0) == -1)
	perror("network select");
	if(dmNetRecv(cnpLocal, &buf) == DM_FAILURE)
	err("dmNetRecv Local failed\n");
	if(dmNetSend(cnp, buf) == DM_FAILURE) {
#endif
	    if ( errno == EBUSY ) {
		fprintf( stderr, "%s: dmNetSend napping on frame %d\n", _progName, frame );
		sginap( 1 );
	    } 
	    else 
		fprintf( stderr, "%s: Could not send frame %d\n", _progName, frame );
	}
	else 
	    if ( DEBUG )
		fprintf( stderr, "sent frame %d\n", frame );
#ifdef SEMAPHORE
	usvsema(empty);
#endif
    }
}
	

void
main(int argc, char **argv)
{
  char *hostname=NULL;
  int nframes = 0;
  DMparams* plist;
  int c, j;
  int port = 5555;
  int conntype=DMNET_TCP;
  int i;
  int frame=0;
  int bLoop = 0;
  int framesToRead; 
  char* pluginName=NULL;
#ifdef SEMAPHORE
  char arenaName[64];
#else
  DMnetconnection cnpLocal;
  int portLocal=5987;
#endif

    _progName = argv[0];

    DEBUG = ( int ) getenv( "DEBUG_NETTOVID" );

    while ((c = getopt(argc, argv, "n:b:v:lT:c:N:q:H:f:hL")) != EOF)
    {
	switch (c)
	{
	case 'f':
	    fileName = strdup( optarg );
	    break;
	    
	case 'n':
	    nframes = atoi(optarg);
	    break;
	    
	case 'c' :
	    conntype = atoi(optarg);
	    break;

	case 'N' :
		pluginName = optarg;
		if (!pluginName) {
			fprintf(stderr, "Must supply plugin tag with -N\n");
			usage(); exit(1);
		}
		break;
	    
	case 'v':
	    ioVecCountMax = atoi(optarg);
	    break;

	case 'b':
	    poolsize = atoi(optarg);
	    break;
	    
	case 'l' :
		hostname = getenv("HOST");
		conntype = DMNET_LOCAL;
	    break;
	    
	case 'T' :
	    hostname = optarg;
	    break;
	    
	case 'q' :
	    port = atoi(optarg);
	    break;
	    
	case 'H' :
	    conntype = DMNET_STRIPED_HIPPI;
	    NHippiDevs = strlen(optarg);
	    HippiDevs = (char**)malloc(NHippiDevs*sizeof(char*));
	    for(j=0; j<NHippiDevs; j++){
		char* devname="/dev/hippi";
		int n;
		n = strlen(devname);
		HippiDevs[j] = (char*)malloc(n+2);
		sprintf(HippiDevs[j], "%s%c", devname, optarg[j]);
		/* fprintf(stderr, "%s, strlen=%d\n", HippiDevs[j], n); */
	    }
	    break;
	    
	case 'h':
	    usage();
	    exit(0);
	case 'L':
	  bLoop = ~bLoop;
	  break;
	    
	default:
	    usage();
	    exit(1);
	    break;
	}
    }
    
    bufArray = ( DMbuffer * ) malloc( poolsize * sizeof( DMbuffer ) );

    /* register signal callback */
    signal( SIGINT, sigHnd );
    
    if ( fileName == NULL ) {
	err( "file name has to be provided" );
    }
    
    if (!hostname)
		err("Must provide -T hostname (or -l for local)\n");
    
    if (dmNetOpen(&cnp) != DM_SUCCESS)
	err("dmNetOpen failed\n");
    
    if (dmParamsCreate(&plist) == DM_FAILURE)
	err("Create params failed\n");

	if (pluginName) {
		char fcName[128];

		conntype = DMNET_PLUGIN;
		if (dmParamsSetString(plist, DMNET_PLUGIN_NAME, pluginName)
				== DM_FAILURE)
			err("Couldn't set DMNET_PLUGIN");

		sprintf(fcName, "fc-%s", hostname);
		if (dmParamsSetString(plist, "DMNET_TRANSPORTER_REMOTE_HOSTNAME",
				fcName) == DM_FAILURE)
			err("Couldn't set DMNET_TRANSPORTER_REMOTE_HOSTNAME");
	}
		
    
    if (dmParamsSetInt(plist, DMNET_CONNECTION_TYPE, conntype)
			== DM_FAILURE)
		err("Couldn't set connection type\n");
    
    if (conntype == DMNET_STRIPED_HIPPI){
	register int i;
	char paramName[128];
	
	if(dmParamsSetInt(plist, DMNET_HIPPI_NDEV, NHippiDevs)==DM_FAILURE)
	    err("Couldn't set DMNET_HIPPI_NDEV\n");
	
	for (i=0; i<NHippiDevs; i++) {
	    sprintf(paramName, DMNET_HIPPI_DEV, i);
	    if(dmParamsSetString(plist, paramName, HippiDevs[i]) == DM_FAILURE)
		err("Couldn't set DMNET_HIPPI_DEV\n");
	}
    }
    
    if (dmParamsSetInt(plist, DMNET_PORT, port) == DM_FAILURE)
	err("Couldn't set port number\n");
    
    if (dmParamsSetString(plist, DMNET_REMOTE_HOSTNAME, hostname)
	== DM_FAILURE)
	err("Couldn't set host name\n");
    
    if (dmNetConnect(cnp, plist) != DM_SUCCESS)
	err("dmNetConnect failed\n");
    fprintf(stderr, "%s: connect\n", _progName);
    
    /* open movie file */
    if ( ( int ) readQTFileFormat( ) != DM_SUCCESS )
	err( "accessing video file has failed" );
    
    if ( nframes == 0 || nframes > movieLength )
	nframes = movieLength;
    
    /* Create and register a buffer of poolsize frames... */
    
    if (dmBufferSetPoolDefaults(plist, poolsize, frameSize,
				DM_FALSE, DM_TRUE ) )
	err("dmBufferSetPoolDefaults\n");
    
    printf( "bytes per transfer: %d, poolsize: %d\n", frameSize, poolsize );
    
    if (dmNetGetParams(cnp, plist) == DM_FAILURE)
	err("dmNetGetParams failed\n");
    
    fprintf(stderr, "%s: Pool blk size (dmNetGetParams->vlDMGetParams) = %d\n", 
	    _progName, dmParamsGetInt(plist, DM_BUFFER_SIZE));
    
    if (dmBufferCreatePool(plist, &pool) != DM_SUCCESS)
	err("Create Pool failed\n");
    
    if ( !pool )
	err("No Pool\n");
    
    if (dmNetRegisterPool(cnp, pool) != DM_SUCCESS)
	err("dmNetRegisterPool failed\n");
    
    /* fprintf(stderr, "%s: Create Pool, Register Pool worked\n", _progName); */
    
    dmParamsDestroy(plist);

    /* thread synchronization */
#ifdef SEMAPHORE
    sprintf(arenaName, "/tmp/disktonet_arena_XXXXXX");
    mktemp(arenaName);
    unlink(arenaName);
    /* if (unlink(arenaName) == -1)
	err("Failed to unlink arena name."); */
    if (usconfig(CONF_INITUSERS, 64) == -1)
	err("Failed to configure users for arena");
    if ((arena = usinit(arenaName)) == NULL)
	err("usinit failed");
    if ((empty = usnewsema(arena, poolsize)) == NULL)
	err("Alloc of empty sema failed");
    if ((full = usnewsema(arena, 0)) == NULL)
	err("Alloc of full sema failed");
    if ((dmnetPid = sproc(dmNetThread, PR_SALL, 0)) == -1)
	err("sproc failed\n");
#else
	if(dmParamsCreate(&plistLocal) == DM_FAILURE)
	err("Create local params failed\n");
	if(dmParamsSetInt(plistLocal, DMNET_CONNECTION_TYPE, DMNET_LOCAL) == DM_FAILURE)
	err("Couldn't set DMNET_LOCAL connection type\n");
	if(dmParamsSetInt(plistLocal, DMNET_PORT, portLocal) == DM_FAILURE)
	err("Couldn't set DMNET_PORT to portLocal\n");
    if ((dmnetPid = sproc(dmNetThread, PR_SALL, 0)) == -1)
	err("sproc failed\n");
	sginap(1); /* give dmNetThread a chance to call dmNetAccept - race possible ! */
	if(dmNetOpen(&cnpLocal) != DM_SUCCESS) 
	err("dmNetOpen Local failed\n");
	if(dmNetConnect(cnpLocal, plistLocal) != DM_SUCCESS)
	err("dmNetConnect Local failed\n");
#endif

    
    /* set scheduling, we want it to be as fast as possible */
    
    /*
     * swap permissions so that we become root for a moment. 
     * setreuid() should let us set real uid to effective uid = root
     * if the program is setuid root 
     */ 
    
    setreuid( geteuid( ), getuid( ) );
    setregid( getegid( ), getgid( ) );
    
    if ( schedctl( NDPRI, 0, NDPHIMIN )< 0 ) {
	fprintf( stderr, "Run disktonet as root if you want to enable real time scheduling." );
    }
    
    /* swap permissions back, now we're just "joe user" */
    setreuid( geteuid( ), getuid( ) );
    setregid( getegid( ), getgid( ) );
    
    if ( vlCaptureType == VL_CAPTURE_NONINTERLEAVED ) {
	nframes *= 2;
	printf( "FIELDS\n" );
    }
    else
	printf( "FRAMES\n" );
    
    /* allocate space in the dmBuffer Pool */
    for ( i = 0; i < poolsize; i++ ) { 
	if ( dmBufferAllocate( pool, &bufArray[ i ] ) != DM_SUCCESS )
	    err( "cannot allocate memory." );
	
	if ( ( videoData[ i ].iov_base = dmBufferMapData( bufArray[ i ] ) ) < 0 )
	    err( "cannot allocate memory." );
	
	videoData[ i ].iov_len = frameSize; /* it's what I expect to get */
    }
    
    /* starting time */
    gettimeofday(&startTime, 0);
    for(frame=0; (!_quit) && (bLoop || frame<nframes);) {

      framesToRead = (ioVecCountMax>nframes-frame) ? nframes-frame:ioVecCountMax;
      
#ifdef SEMAPHORE
      for(i=0; i<framesToRead; i++) uspsema(empty);
#endif
      
      if ( vlCaptureType == VL_CAPTURE_INTERLEAVED ) { /* frames */
	if ( readQTFrames( frame%nframes, framesToRead) == DM_FAILURE)
	  err( " cannot read frame from Quicktime file" );
      }
      else /* fields XXX is this correct ? */
	if ( readQTFields( (frame%nframes) / 2, framesToRead) == DM_FAILURE )
	  err( "cannot read field from Quicktime file." );
      
      if (DEBUG)
	fprintf(stderr, "read frame %d - %d\n", frame, frame+framesToRead-1);
      
      for(i=0; i<framesToRead; i++) 
#ifdef SEMAPHORE
		usvsema(full);
#else
		if (dmNetSend(cnpLocal, bufArray[(frame+i)%poolsize]) == DM_FAILURE)
		err("dmNetSend Local failed\n");
#endif
	
      frame += framesToRead;
    }

    cleanup( 0 );
}

int
readQTFileFormat( void )
{
  /*
   * important things are:
   * movie rate
   * interlacing
   * compression
   * packing
   * colorspace
   */

  int flags, oldFlags;
  long num_pages;
  struct dioattr da;
  DMparams* params;
  DMpacking mvPacking;
  int bytesPerPixel;
  int bytesPerImage;
  int ioAlignment;
  struct stat fileStat;

  int vlColorspace;
  int vlPacking;
  DMinterlacing mvInterlacing;
  int ioBlocksPerImage = 0;
  int ioMaxXferSize = 0;

  size_t _size = 0;

  /* open file for reading with direct i/o */
  
  if ( stat( fileName, &fileStat ) < 0 )
    fileStat.st_mode = S_IFREG;
  
  if ( fileStat.st_mode & S_IFCHR ) {
    /* deal directly with the raw device */
    if ( ( fileFD = open( fileName, O_RDONLY ) ) < 0 ) 
      return DM_FAILURE;
    
    /* use Will's /dev/kmem method */
    ioBlockSize = DISK_BLOCK_SIZE;
  }
  else {
    if ( ( fileFD = open( fileName, O_DIRECT | O_RDONLY, 0644 ) ) < 0 ) {
      fprintf( stderr, "File cannot be open for reading." );
      return DM_FAILURE;
    }
    
    /* query the block size and memory alignment */
    if ( fcntl( fileFD, F_DIOINFO, &da ) < 0 ) {
      return DM_FAILURE;
    }
  }

  /*
   * let's do this regardless on the type of iofile
   * query the block size and memory alignment
   */
  if ( fcntl( fileFD, F_DIOINFO, &da ) < 0 ) {
    return DM_FAILURE;
  }
  
  ioBlockSize = da.d_miniosz;
  ioMaxXferSize = da.d_maxiosz;
  
  /* page alignment is best for any mode */
  ioAlignment = getpagesize( );
  
  /* 
   * Note this is not documented in the man pages of writev or direct I/O!
   * If we are using writev we must align to the pagesize not the file
   * system blocksize which is required for direct I/O.
   * if (blocksize >= pagesize) then we do not have to worry but
   * if (blocksize < pagesize) then we must align the I/O request size
   * to the pagesize not the blocksize.  This will waste space but is
   * necessary to maintain performance.
   */
  if ( ioAlignment > ioBlockSize ) {
    if ( DEBUG )
      fprintf( stderr, "page size (%d) is larger than the file system block size (%d) so the I/O request will be aligned the page size\n", ioAlignment, ioBlockSize);
    ioBlockSize = ioAlignment;
  }
  assert( ioBlockSize != 0 );
  
  /*
   * open movie file before I start to use it
   * I need to disable Direct I/O first
   */
  fsync( fileFD );
  oldFlags = flags = fcntl( fileFD, F_GETFL );
  flags &= ~FDIRECT;
  if ( fcntl( fileFD, F_SETFL, flags ) < 0 ) {
    fprintf( stderr, "Unable to reset direct I/O file status." );
    return DM_FAILURE;
  }
  
  /* open movie file */
  if ( mvOpenFD( fileFD, &movie ) == DM_FAILURE ) {
    close( fileFD );
    fprintf( stderr, "Unable to reset direct I/O file status." );
    return DM_FAILURE;
  }

  /* change a file access mode back to Direct I/O */
  fsync( fileFD );
  if ( fcntl( fileFD, F_SETFL, oldFlags ) < 0 ) {
    fprintf( stderr, "Cannot access movie file." );
    return DM_FAILURE;
  }

  /* get all movie file information */
  if ( mvFindTrackByMedium( movie, DM_IMAGE, &mvImageTrack ) == DM_FAILURE ) {
    close( fileFD );
    fprintf( stderr, "Couldn't find image track\n" );
    return DM_FAILURE;
  }
  
  params = mvGetParams( mvImageTrack );

  /* get packing information */
  mvPacking = ( DMimagepacking ) dmParamsGetInt( params, DM_IMAGE_PACKING );
  
  switch ( mvPacking ) {
  case DM_IMAGE_PACKING_RGB:
    vlPacking = VL_PACKING_R444_8;
    bytesPerPixel = 3;
    vlColorspace = VL_COLORSPACE_RGB;
    break;

  case DM_IMAGE_PACKING_CbYCrY:
    vlPacking = VL_PACKING_R242_8;
    bytesPerPixel = 2;
    vlColorspace = VL_COLORSPACE_CCIR601;
    break;

    /*  case DM_IMAGE_PACKING_: */

  case DM_IMAGE_PACKING_RGBA:
    vlPacking = VL_PACKING_R4444_8;
    bytesPerPixel = 4;
    vlColorspace = VL_COLORSPACE_RGB;
    break;

  default:
    fprintf( stderr, "unknown image packing format\n" );
    usage( );
    exit( 1 );
  }
  
  /* get interlacing information */
  mvInterlacing = ( DMimageinterlacing ) dmParamsGetEnum( params, DM_IMAGE_INTERLACING );

  if ( mvInterlacing == DM_IMAGE_NONINTERLACED ) {
    vlCaptureType = VL_CAPTURE_INTERLEAVED;
  }
  else if ( mvInterlacing == DM_IMAGE_INTERLACED_ODD || mvInterlacing == DM_IMAGE_INTERLACED_EVEN ) {
    vlCaptureType = VL_CAPTURE_NONINTERLEAVED;
  }
  else {
    fprintf( stderr, "unknown capture type" );
    return DM_FAILURE;
  }

  movieLength = mvGetTrackLength( mvImageTrack );

  /* get the image dimensions */
  xsize = mvGetImageWidth( mvImageTrack );
  ysize = mvGetImageHeight( mvImageTrack );

  /* verifying frame size */
  {
    int paramsId;
    MVdatatype dataType;
    MVframe frames;
    mvGetTrackDataInfo( mvImageTrack, 0, &_size, &paramsId, &dataType, &frames );
  }
  printf( "Real size: %ld, real bytes/pixel: %d\n", _size, _size / ( xsize * ysize ) );
  

  if ( vlCaptureType == VL_CAPTURE_NONINTERLEAVED )
    ysize /= 2;

  bytesPerImage = xsize * ysize * bytesPerPixel; 

  ioBlocksPerImage = ( bytesPerImage + ioBlockSize - 1 ) / ioBlockSize;
  frameSize = ioBlocksPerImage * ioBlockSize;
  
  /* we must compare the max DMA xfer size with the user's I/O request */
  if ( frameSize > ioMaxXferSize ) {
    fprintf( stderr, "I/O request size = %d exceeds max dma size = %d\n",
	     frameSize, ioMaxXferSize );
    return DM_FAILURE;
  }

  /* we must also check if the # of I/O vectors exceeds the DMA xfer size */
    fprintf( stderr, "transfer size (%d) * vector count (%d) and max io (%d)\n", 
	     frameSize, ioVecCountMax, ioMaxXferSize );
  if ( frameSize * ioVecCountMax > ioMaxXferSize ) {
    /*
     * could readjust ioVecCount but then we would have to do the same check
     * as is done earlier so we punt and give a recommendation.
     *
     * all values in this equation are multiples of the page size so we
     * do not lose anything in our integer arithmetic
     */
    num_pages = ( frameSize * ioVecCountMax ) % ioMaxXferSize;
    num_pages /= ioAlignment; 
    
    return DM_FAILURE;
  }
  
  videoData = ( struct iovec * ) malloc( poolsize * sizeof( struct iovec ) );
  origSizeOfPicture = ( long * ) malloc( ioVecCountMax * sizeof( long ) );
  
  fprintf( stderr, "VL packing: %d, VL colorspace: %d\n", vlPacking, vlColorspace );
  fprintf( stderr, "Frame dimensions: x: %d, y: %d\n", xsize, ysize );

  return DM_SUCCESS;
}

int 
readQTFrames( int startFrame, int range )
{
  off64_t offset = 0;

  /* find startFrame position in the movie file */
  if ( mvGetTrackDataOffset( mvImageTrack, startFrame, &offset ) == DM_FAILURE ) {
    fprintf( stderr, "Cannot read data offset." );
    return DM_FAILURE;
  }

  /* read fields */
  if ( lseek64( fileFD, offset, SEEK_SET ) != offset ) {
    fprintf( stderr, "lseek64 has failed.\n" );
    return DM_FAILURE;
  }

  if(DEBUG)
	fprintf(stderr, "Reading from %d to %d\n", startFrame%poolsize, range);

  if ( readv( fileFD, &videoData[startFrame%poolsize], range ) < 0 ) { 
	perror("readv");
    return DM_FAILURE;
  }
  
  return DM_SUCCESS;
}

int
readQTFields( int _startField, int _range )
{
  size_t firstField, secondField;
  off64_t offset, _tmpOffset1, _tmpOffset2;
  off64_t gap;
  int i;
  
  /* find startField position in the movie file */
  if ( mvGetTrackDataOffset( mvImageTrack, _startField, &offset ) == DM_FAILURE ) {
    fprintf( stderr, "Couldn't read data offset.\n" );
    return DM_FAILURE;
  }

  for( i = 0; i < _range; ) {
    if ( i < _range - 2 ) {
      if ( mvGetTrackDataOffset( mvImageTrack, _startField + ( i / 2 ), &_tmpOffset1 ) == DM_FAILURE ) {
	fprintf( stderr, "Couldn't read data offset.\n" );
	return DM_FAILURE;
      }
      if ( mvGetTrackDataOffset( mvImageTrack, _startField + ( i / 2 ) + 1, &_tmpOffset2 ) == DM_FAILURE ) {
	fprintf( stderr, "Couldn't read data offset.\n" );
	return DM_FAILURE;
      }
    }

    if ( mvGetTrackDataFieldInfo( mvImageTrack, _startField + ( i / 2 ), 
				 &firstField, &gap, &secondField ) == DM_FAILURE ) {
      return DM_FAILURE;
    }

    videoData[ i ].iov_len = firstField + gap; 
    origSizeOfPicture[ i ] = ( long ) firstField;
    i++;

    if ( i < _range - 1 )
      videoData[ i ].iov_len = ( _tmpOffset2 - _tmpOffset1 ) - firstField - gap;
    else
      videoData[ i ].iov_len = ( ( secondField + ioBlockSize - 1 ) / ioBlockSize ) * ioBlockSize;

    origSizeOfPicture[ i ] = ( long ) secondField;
    i++;
  }

  /* read fields */
  if ( lseek64( fileFD, offset, SEEK_SET ) != offset ) {
    if ( DEBUG )
      fprintf( stderr, "lseek64 has failed.\n" );
    return DM_FAILURE;
  }

  if ( readv( fileFD, &videoData[(_startField*2)%poolsize], _range ) < 0 ) {
    fprintf( stderr, "Cannot read data from file." );
    return DM_FAILURE;
    }
 
  return DM_SUCCESS;
}


/*
 * Do general cleanup before exiting. All of our resources should
 * be freed.
 */
void
cleanup(int ret)
{

  printStats();
  sginap(1); /* Give dmnet a chance to finish transfer */
  dmNetClose(cnp);
  pool && dmBufferDestroyPool(pool);

  if (dmnetPid) kill(dmnetPid, SIGKILL);
#ifdef SEMAPHORE
  if (full) usfreesema(full, arena);
  if (empty) usfreesema(empty, arena);
#endif
  if ( videoData != NULL )
    free( videoData );
  if ( bufArray != NULL )
    free( bufArray );
  if ( origSizeOfPicture != NULL )
    free( origSizeOfPicture );
  exit(ret);
}

void
printStats(void)
{
	struct timeval stopTime;
	long sec, usec;
	double diff;

	gettimeofday(&stopTime, 0);
	sec = stopTime.tv_sec - startTime.tv_sec;
	usec = stopTime.tv_usec - startTime.tv_usec;
	diff = sec + usec/1000000.0;

	printf("\n%d frames sent in %.3f real seconds @ %.3f fps, %.3f KB/sec\n",
		frame, diff, frame/diff, frameSize/(diff*1000)*frame);
}

void
sigHnd( int _sig ) {
  _quit = 1;
}
