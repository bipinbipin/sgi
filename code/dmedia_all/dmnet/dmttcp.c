/*
 * Copyright 1997, Silicon Graphics, Inc.
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
 *
 * $Revision: 1.1 $
 */

/*
 * dmttcp.c - test dmnet tcp connections.
 * 	Based on original ttcp.
 *
 * Test TCP connection.  Makes a connection on port 5555
 * and transfers fabricated buffers or data copied from stdin.
 */

#define BSD43
/* #define BSD42 */
/* #define BSD41a */
/* #define SYSV */	/* required on SGI IRIX releases before 3.3 */

#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>		/* struct timeval */

#include <sys/dms.h>
#include <dmedia/dmedia.h>
#include <dmedia/dm_buffer.h>
#include <dmedia/dm_params.h>
#include <dmedia/dm_stream.h>
#include <dmedia/dm_buffer_dms_private.h>

#include <sys/dmcommon.h>
#include <dmedia/dmnet.h>
#include <dmnet/dmnet_striped_hippi.h>
#include <dmnet/dmnet_transporter.h>
#include <dmnet/dmnet_hippi-fp.h>

#if defined(SYSV)
#include <sys/times.h>
#include <sys/param.h>
struct rusage {
    struct timeval ru_utime, ru_stime;
};
#define RUSAGE_SELF 0
#else
#include <sys/resource.h>
#endif

int fd;				/* fd of network socket */

int buflen = 262144; /*  720 * 486 * 4 */
int altbuflen = 1470 * 4;       /* default length of buffers */
int ratioAltBuf = 0;            /* num altbuf packets sent for every buf packet */
int ratioBuf = 1;               /* = ratioBuf/ratioAltBuf */
int biggerbuflen = 0;           /* The bigger of the two buffers */
int freeflag=0;                 /* To free dumbufs after use or not */
DMbuffer buf;			/* ptr to dynamic buffer */
int poolsize = 5;		/* number of buffers in pool */
int nbuf = 1024;	      	/* number of buffers to send */

int bufalign = 16*1024;		/* modulo this */

int udp = 0;			/* 0 = tcp, !0 = udp */
int debug = 0;			/* various debug options */
int ustflag = 0;		/* set UST value on send */
int options = 0;		/* socket options */
int port = 5555;		/* dmNet TCP port number */
char *host = NULL;		/* ptr to name of host */
int trans;			/* 0=receive, !0=transmit mode */
int verbose = 0;		/* 0=print basic info, 1=print cpu rate, proc
				 * resource usage. */
int nodelay = 0;		/* set TCP_NODELAY socket option */
char fmt = 'K';			/* output format: k = kilobits, K = kilobytes,
				 *  m = megabits, M = megabytes, 
				 *  g = gigabits, G = gigabytes */
int checkpat=0;                 /* flag 1=check for data corruption */
int varpool = 0;		/* set for variable sized dmbuffer allocation */
int bPreAllocBuf=0;
char* prognm;

static char** HippiDevs=0;
static int NHippiDevs=0;
/*
static char *HippiDevs[] = {
    "/dev/hippi1",
    "/dev/hippi3",
    "/dev/hippi2",
};
static int NHippiDevs = sizeof(HippiDevs)/sizeof(char *);
*/


struct hostent *addr;
extern int errno;
extern int optind;
extern char *optarg;

char Usage[] = "\
Usage: dmttcp -t [-options] host [ < in ]\n\
       dmttcp -r [-options > out]\n\
Common options:\n\
	-t	write DMbuffers to network\n\
	-r	read DMbuffers from network\n\
	-c	dmNet connection type [TCP by default]\n\
	-N	name of plugin to use with -c 99\n\
	-H ###	list of Hippi character driver numbers \n\
				(-H 38 stripes across /dev/hippi3 and /dev/hippi8)\n\
	-p##	port number to send to or listen at (default 5555)\n\
	-b##	number of buffers to create in pool (default 5)\n\
	-l##	read/write network buffer1 length(default 1399680)\n\
	-L##    read/write network buffer2 length(default 5880)\n\
	-P#:#   #buffer2 pkts to #buffer1 pkts send ratio (default 0:1)\n\
	-n##	number of source bufs written to network (default 1024)\n\
	-a      specify local address\n\
	-u	use UDP instead of TCP\n\
	-U	set (& show with -d) UST on send, dump UST on receive\n\
	-A	align the start of buffers to this modulus (default 16384)\n\
	-v	verbose: print more statistics\n\
	-f X	format for rate: k,K = kilo{bit,byte}; m,M = mega; g,G = giga\n\
	-C      check for data corruption\n\
	-F      free dumbufs after send\n\
";	

char stats[128];
double nbytes = 0;		/* bytes on net */
int nframes = 0;                /* frames on net */
unsigned long numCalls;		/* # of I/O system calls */
double cput, realt;		/* user, real time (seconds) */

int connectiontype = DMNET_TCP;
char *pluginName = NULL;

void err(char*);
void mes(char*);
void pattern(int* cp, int pat, int size);
void checkPattern(int* cp, int pat, int size);
void prep_timer();
double read_timer(char*, int);
void delay();
char* outfmt(double);

static struct	timeval time0;	/* Time at which timing started */
static struct	rusage ru0;	/* Resource utilization at the start */

static void prusage(struct rusage*, struct rusage*,
	struct timeval*, struct timeval*, char*);
static void tvadd(struct timeval*, struct timeval*, struct timeval*);
static void tvsub(struct timeval*, struct timeval*, struct timeval*);
static void psecs(long, char*);

int
main(int argc, char** argv)
{
    int c, j;
    DMparams* plist;
    DMnetconnection cnp;
    DMbufferpool pool;
    fd_set fdset;
    int maxfd, nfd;
    char* addr;
    u_char userData[DM_BUFFER_MAX_AUX_DATA_SIZE];
    u_long local_sockaddr = 0;
	char connectionString[128];


    if (argc < 2) goto usage;

    prognm = argv[0];

    while ((c = getopt(argc, argv, "drtuvCDFUVa:b:c:N:f:n:p:A:l:L:P:H:h")) != -1) {
	switch (c) {
	    char* ratioptr;
	    struct hostent* hp;

	case 't':
	    trans = 1;
	    break;
	case 'r':
	    trans = 0;
	    break;
	/*
	 * We should have a way of setting particular socket options.
	 * e.g. SO_DEBUG or !TCP_NODELAY (which is on by default).
	 */
	case 'd':
	    options |= SO_DEBUG;
	    debug++;
	    break;
	case 'D':
#ifdef TCP_NODELAY
	    nodelay = 1;
#else
	    fprintf(stderr, 
"ttcp: -D option ignored: TCP_NODELAY socket option not supported\n");
#endif
	    break;
	case 'C' :
	    checkpat=1;
	    break;
	case 'F' :
	    freeflag=~freeflag;
		bPreAllocBuf=~bPreAllocBuf;
	    break;
	case 'c':
	    connectiontype = atoi(optarg);
	    break;
	case 'N':
	    pluginName = strdup(optarg);
		if(!pluginName) {
			fprintf(stderr, "Must supply plugin name with -N\n");
			goto usage;
		}
	    break;
	case 'b':
	    poolsize = atoi(optarg);
	    break;

	case 'n':
	    nbuf = atoi(optarg);
	    break;

	case 'l':
	    buflen = atoi(optarg);
	    break;
	case 'L':
	    altbuflen = atoi(optarg);
	    break;
	case 'P' :
	    ratioptr = optarg;
	    ratioAltBuf = atoi(ratioptr);
	    while(*ratioptr++ != ':');
	    ratioBuf = atoi(ratioptr);
	    if (!(ratioBuf || ratioAltBuf)) {
	      mes("ratio 0:0 not allowed\n");
	      goto usage;
	    }
	    break;
	case 'a':
	    ratioptr = optarg;
	    if ((hp = gethostbyname(ratioptr)) == (struct hostent *)NULL) {
		mes("bad local address");
		goto usage;
	    }
	    bcopy(hp->h_addr, &local_sockaddr, hp->h_length);
	    local_sockaddr = ntohl(local_sockaddr);
	    break;
	case 'p':
	    port = atoi(optarg);
	    break;
	case 'u':
	    udp = 1;
	    connectiontype = 2; /* DMNET_UDP in dmnet_private.h */
	    break;
	case 'U':
	    ustflag = 1;
	    break;
	case 'V':
	    varpool++;
	    break;
	case 'v':
	    verbose = 1;
	    break;
	case 'A':
	    bufalign = atoi(optarg);
	    break;
	case 'f':
	    fmt = *optarg;
	    break;
	case 'H':
		connectiontype = DMNET_STRIPED_HIPPI;
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

	case 'h' :
	default:
	    goto usage;
	}
    }
    
    biggerbuflen = (ratioAltBuf&&ratioBuf)?((buflen>altbuflen)?buflen:altbuflen)
	:(ratioBuf?buflen:altbuflen);

    if (trans) {
	/* xmitr */
	if (optind == argc)
		goto usage;
	host = argv[optind];
    }

    if (udp && buflen < 5) {
	buflen = 5;		/* send more than the sentinel size */
    }

    if (dmNetOpen(&cnp) != DM_SUCCESS)
		err("dmNetOpen failed");

    if (dmParamsCreate(&plist) == DM_FAILURE )
		err("dmParamsCreate failed");

	if (pluginName) {
		connectiontype = DMNET_PLUGIN;
		if (dmParamsSetString(plist, DMNET_PLUGIN_NAME, 
				pluginName) == DM_FAILURE) {
			fprintf(stderr, "Couldn't set DMNET_PLUGIN_NAME to %s\n",
				pluginName);
			return 1;
	    }
		if (HippiDevs && dmParamsSetString(plist, DMNET_HIPPI_DEVICE,
				HippiDevs[0]) == DM_FAILURE) {
			fprintf(stderr, "Couldn't set %s to %s\n",
				DMNET_HIPPI_DEVICE, HippiDevs[0]);
			return 1;
		}
	
		if (trans) {
			char fcName[128];
			sprintf(fcName, "fc-%s", host);
			/* fprintf(stderr, "fcName = %s\n", fcName); */
			if (dmParamsSetString(plist, DMNET_TRANSPORTER_REMOTE_HOSTNAME,
					fcName) == DM_FAILURE)
				err("DMNET_TRANSPORTER_REMOTE_HOSTNAME failed\n");
		}
	}

    if (dmParamsSetInt(plist, DMNET_CONNECTION_TYPE,
	    	connectiontype) == DM_FAILURE)
		err("Couldn't set DMNET_CONNECTION_TYPE");

    if (connectiontype == DMNET_STRIPED_HIPPI) {
		register int i;
		char paramName[128];
		
		if (dmParamsSetInt(plist, DMNET_HIPPI_NDEV,
		   		NHippiDevs) == DM_FAILURE) {
    		fprintf(stderr, "Couldn't set DMNET_HIPPI_NDEV to %d\n",
	    		NHippiDevs);
    		return 1;
		}
		for (i = 0; i < NHippiDevs; i++) {
    		sprintf(paramName, DMNET_HIPPI_DEV, i);
    		if (dmParamsSetString(plist, paramName,
		  			HippiDevs[i]) == DM_FAILURE) {
				fprintf(stderr, "Couldn't set %s to %s\n", paramName,
					HippiDevs[i]);
				return 1;
	    	}
		}
		if (dmParamsSetInt(plist, DMNET_HIPPI_STRIPE,
			   	biggerbuflen/NHippiDevs) == DM_FAILURE) {
	    	fprintf(stderr, "Couldn't set DMNET_HIPPI_STRIPE to %d\n",
		    	biggerbuflen/NHippiDevs);
	    	return 1;
		} 
	} 
#if 0
	else if (connectiontype == DMNET_TRANSPORTER) {
		char fcName[128];
	
		if(trans) {
			sprintf(fcName, "fc-%s", host);
			if (dmParamsSetString(plist, DMNET_TRANSPORTER_REMOTE_HOST, fcName)
					== DM_FAILURE) {
				fprintf(stderr, "Couldn't set transporter remote hostname %s\n", fcName);
				return 1;
			}
		}
	}
#endif

    if (dmParamsSetInt(plist, DMNET_PORT, port) == DM_FAILURE) 
		err("Couldn't set port number");
    
    if (local_sockaddr && (dmParamsSetInt(plist, DMNET_LOCAL_SOCKADDR, 
			(int)local_sockaddr) == DM_FAILURE))
		err("Couldn't set DMNET_LOCAL_SOCKADDR");

    if (trans) {
		if (dmParamsSetString(plist, DMNET_REMOTE_HOSTNAME,
				host) == DM_FAILURE)
			err("Couldn't set DMNET_REMOTE_HOSTNAME");
    }

	switch (connectiontype) {
	case DMNET_TCP :
		sprintf(connectionString, "tcp");
		break;
	case 2 :
		sprintf(connectionString, "udp");
		break;
	case DMNET_PLUGIN :
		sprintf(connectionString, pluginName);
		break;
	case DMNET_STRIPED_HIPPI :
		sprintf(connectionString, "hippi");
		break;
#if 0
	case DMNET_TRANSPORTER : /* Needs to be replaced */
		sprintf(connectionString, "transporter");
		break;
#endif
	default :
		sprintf(connectionString, "Unknown connection");
	}
    
    fprintf(stdout, "dmttcp-%c: buflens=", trans?'t':'r');
    if (!(ratioAltBuf && ratioBuf)) /* both are not zero */
		fprintf(stdout, "%d, ", biggerbuflen);
    else
		fprintf(stdout, "%d, %d (%d:%d); ", 
			altbuflen, buflen, ratioAltBuf, ratioBuf);
    fprintf(stdout, "nbuf=%d, poolsize=%d, align=%d, port=%d",
	    nbuf, poolsize, bufalign, port);
	fprintf(stdout, " %s", connectionString);
    if (trans) fprintf(stdout, " -> %s", host);
	fprintf(stdout, "\n");

	if (trans) {
		if (dmNetConnect(cnp, plist) != DM_SUCCESS)
			err("dmNetConnect");

		mes("connect");
    } else {
		/* Test code that increments port numbers when dmNetListen fails
		c = 0;
		while(dmNetListen(cnp, plist) != DM_SUCCESS) {
	    	c++;
	    	fprintf(stderr,
		    	"dmttcp-r: Can't bind to port %d :", port);
	    	perror("");
	    	if (c >=5) break;
	    	fprintf(stderr,
		    	"dmttcp-r: Trying port %d\n", ++port);
	    	if (dmParamsSetInt(plist, DMNET_PORT, port) == DM_FAILURE) {
			fprintf(stderr, 
				"dmttcp-r: Couldn't set port number %d\n", port);
			return 1;
	    	}
		}
		*/
		if (dmNetListen(cnp, plist) != DM_SUCCESS) {
	    	fprintf(stderr,
		    	"dmttcp-r: Can't bind to port %d :", port);
	    	perror("");
	    	return 1;
		}
		if (dmNetAccept(cnp, plist) != DM_SUCCESS)
			err("dmNetAccept");
		mes("accept");
    }
    
    if (varpool) {
	/*
	 * If allocating a pool allowing variable sized buffers allocations,
	 * double the number of buffers and set the pool size explicitly.
	 * The default buffer allocation size is biggerbuflen.
	 */
		if (dmBufferSetPoolDefaults(plist, poolsize * 2, biggerbuflen, DM_TRUE,
			DM_TRUE) == DM_FAILURE ) {
	    	fprintf(stderr, "Couldn't set pool defaults\n");
	    	return 1;
	}
	if (dmParamsSetEnum(plist, DM_POOL_VARIABLE, DM_TRUE) ||
		dmParamsSetLongLong(plist, DM_POOL_SIZE,
			(poolsize * biggerbuflen))) {
	    fprintf(stderr, "Couldn't set variable pool defaults\n");
	    return 1;
	}
    } else if (dmBufferSetPoolDefaults(plist, poolsize, biggerbuflen, 
		(trans) ? DM_TRUE:DM_FALSE, DM_TRUE) == DM_FAILURE ) { 
	fprintf(stderr, "Couldn't set pool defaults\n");
	return 1;
    }

    if (dmNetGetParams(cnp, plist) == DM_FAILURE) {
	fprintf(stderr, "%s: dmNetGetParams failure\n", prognm);
	return 1;
    }

    /*
     * error checking for alignment?
     */
#if 0
    if (dmParamsSetInt(plist, DM_BUFFER_ALIGNMENT, bufalign) == DM_FAILURE) {
	fprintf(stderr, "Couldn't set port number\n");
	return 1;
    }
#endif
    
    /* create the buffer pool */
    if (dmBufferCreatePool(plist, &pool) == DM_FAILURE )
		err("create pool failed");

	if (bPreAllocBuf) {
		if (dmNetRegisterBuffer(cnp) != DM_SUCCESS) 
			err("dmNetRegisterBuffer failed");
	} else {
    	if (dmNetRegisterPool(cnp, pool) != DM_SUCCESS) 
			err("dmNetRegisterPool failed");
	}

    /* Sock Buf Size
    if (!trans){
      int s, sockbufsize = 61320/2, len = sizeof(int);
      
      s = dmNetDataFd(cnp);
      if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &sockbufsize,
		     len) < 0)
	fprintf(stderr, "getsockopt failed on SO_RCVBUF");
      fprintf(stderr, "Sockbuf size = %d\n", sockbufsize);
    }
    */

    gettimeofday(&time0, (struct timezone *)0);
    getrusage(RUSAGE_SELF, &ru0);
    errno = 0;
    if (trans)  {
	int i;
	DMbuffer *bufs;

	/*
	 * pattern the DMbuffers first - else we spend all our time
	 * touching the bytes, not shipping data
	 */

	if ((bufs = (DMbuffer *)calloc(poolsize, sizeof(DMbuffer *)))
	    == NULL) {
	    fprintf(stderr, "no memory for DMbuffers\n");
	    exit(1);
	}
	
	for (i = 0; i < poolsize; i++) {
#if 0
	    if ((bufs[i] = calloc(1, sizeof(DMbuffer))) == NULL) {
		fprintf(stderr, "no memory for DMbuffer\n");
		exit(1);
	    }
#endif
	    if (!freeflag) {
		while (dmBufferAllocate(pool, &bufs[i]) != DM_SUCCESS) {
		    /* should do a select here */
		    fprintf(stderr, "Could not allocate buffer temporarily\n");
		    sginap(1);
		}
		addr = dmBufferMapData(bufs[i]);
		pattern ((int*)addr, i, biggerbuflen);
		pattern ((int*)userData, i+1, DM_BUFFER_MAX_AUX_DATA_SIZE);
		dmBufferSetUserData(bufs[i], userData,DM_BUFFER_MAX_AUX_DATA_SIZE);
	    }
	}

	for(nframes=0; nframes<nbuf; nframes++) {
	    int		bl = (nframes%(ratioBuf+ratioAltBuf)<ratioAltBuf) ?
				altbuflen:buflen;
	    
	    buf = bufs[i=nframes%poolsize];
	    if (freeflag) {
		while (dmBufferAllocate(pool, &buf) != DM_SUCCESS) {
		    fprintf(stderr, "Could not allocate buffer temporarily\n");
		    sginap(1);
		}
		addr = dmBufferMapData(buf);
		pattern ((int*)addr, i, bl);
		pattern ((int*)userData, i+1, DM_BUFFER_MAX_AUX_DATA_SIZE);
		dmBufferSetUserData(buf, userData, DM_BUFFER_MAX_AUX_DATA_SIZE);
	    }
	    if (dmBufferSetSize(buf, bl) != DM_SUCCESS)
		fprintf(stderr, "dmBufferSetSize failed\n");
	    nbytes += bl;

#ifndef	DMEDIA_FICUS
	    if (ustflag) {
		USTMSCpair	ustmsc;
		stamp_t		ust;
		struct timeval	tv;
		
		dmGetUSTCurrentTimePair((unsigned long long *)&ust, &tv);
		ustmsc.ust = ust;
		/* ustmsc.ust = 0xABCDEF0123456789; */
		ustmsc.msc = 0;	/* let dmNetSend fill it in */
		/* if (debug) */ {
		    char	*timestr = ctime(&tv.tv_sec);

		    timestr[24] = NULL;	/* remove newline */
		    fprintf(stderr, "UST %016llx MSC %016llx (%s + %d usec)\n",
			    ustmsc.ust, ustmsc.msc, timestr, tv.tv_usec);
			}
		if (dmBufferSetUSTMSCpair(buf, ustmsc) == DM_FAILURE)
			fprintf(stderr, "%s-t: Could not set ust msc for buffer %d\n", 
				prognm, nframes);
		}
#endif
	    
	    if (dmNetSend(cnp, buf) != DM_SUCCESS) {
		if (errno == EBUSY) {
		    sginap(5);
		} else {
		    /* fprintf(stderr, "%s: dmNetSend: ", prognm); perror(""); */
			int err;
			char errStr[DM_MAX_ERROR_DETAIL];
			const char* ret = dmGetError(&err, errStr);
			fprintf(stderr, "%s dmNetSend: %s errno=%d, %s\n", 
				prognm, ret, err, errStr);
			break;
		}
	    }
	    if (freeflag) {
		(void)dmBufferFree(buf);
	    }
	}
    } else { /* receiver */
	int bl;
	if (bPreAllocBuf && (dmBufferAllocate(pool, &buf) != DM_SUCCESS))
		err("dmBufferAllocatePool failed");
	for(nframes=0; nframes<nbuf; nframes++) {
	    while (dmNetRecv(cnp, &buf) != DM_SUCCESS) {
			if (errno != EBUSY) goto out;
			FD_ZERO(&fdset);
			FD_SET(nfd, &fdset);
			if (select(maxfd, &fdset, 0, 0, 0) == -1) 
				err("select after dmNetRecv failed");
	    }
#ifndef	DMEDIA_FICUS
	    if (ustflag) {
		USTMSCpair	ustmsc;
		stamp_t		ust, delta_ust;
		struct timeval	tv_ust, tv;
		time_t		secs;
		long		usecs;
		int		nanos;
		char		*timestr;
		
		ustmsc = dmBufferGetUSTMSCpair(buf);
		dmGetUSTCurrentTimePair((unsigned long long *)&ust, &tv);
		
		/* now need to compute equivalent time for received UST */
		
		delta_ust = ustmsc.ust - ust;
		nanos = delta_ust % 1000;
		delta_ust /= 1000;		/* now usecs */
		secs = (long) delta_ust / 1000000;
		usecs = (long) delta_ust % 1000000;

		tv_ust.tv_sec = tv.tv_sec + secs;
		tv_ust.tv_usec = tv.tv_usec + usecs;
		
		if (nanos < 0) {
		    nanos += 1000;
		    tv_ust.tv_usec--;
		}
		if (tv_ust.tv_usec < 0) {
		    tv_ust.tv_sec--;
		    tv_ust.tv_usec += 1000000;
		}
		if (tv_ust.tv_usec > 1000000) {
		    tv_ust.tv_sec++;
		    tv_ust.tv_usec -= 1000000;
		}
		if (tv_ust.tv_sec < 0)
		    fprintf(stderr, "BOGUS: ");

		timestr = ctime(&tv_ust.tv_sec);
		timestr[24] = NULL;	/* remove newline */
		fprintf(stderr, "UST %016llx (%s + %d usec + %d nanos)\n",
			    ustmsc.ust, timestr, tv_ust.tv_usec, nanos);
	    }
#endif
	    bl = dmBufferGetSize(buf);
	    nbytes += bl;
		if (checkpat) {
			addr = dmBufferMapData(buf);
			checkPattern ((int*)addr, nframes%poolsize, bl);
			dmBufferGetUserData(buf, userData, &bl);
			checkPattern((int*)userData, nframes%poolsize+1, bl); 
	    }
		if (!bPreAllocBuf)
	    	(void)dmBufferFree(buf);
		if (verbose) fprintf(stderr, "Received buffer %d\n", nframes);
	} /* for loop */
	if (bPreAllocBuf) (void)dmBufferFree(buf);
    }
out:
    if (errno)
	err("IO");
    (void)read_timer(stats,sizeof(stats));
    if (cput <= 0.0 )  cput = 0.001;
    if( realt <= 0.0 )  realt = 0.001;
    fprintf(stdout,
	    "dmttcp%s: %.0f bytes in %.2f real seconds = %s/sec +++\n",
	    trans?"-t":"-r",
	    nbytes, realt, outfmt(nbytes/realt));
    if (verbose) {
	fprintf(stdout,
	    "dmttcp%s: %.0f bytes in %.2f CPU seconds = %s/cpu sec\n",
	    trans?"-t":"-r",
	    nbytes, cput, outfmt(nbytes/cput));
    }
    fprintf(stdout,"dmttcp%s: %s\n", trans?"-t":"-r", stats);
    return 0;

usage:
    fprintf(stderr,Usage);
    return 1;
}

void
err(char* s)
{
	char errStr[256];
	int err;
	const char* e = dmGetError(&err, errStr);
    fprintf(stdout,"dmttcp%s ", trans?"-t":"-r");
	fprintf(stdout, "%s: %s %s\n", s, e, errStr);
    fprintf(stdout,"errno=%d\n",err);
    exit(1);
}

void
mes(char* s)
{
    fprintf(stderr,"dmttcp%s: %s\n", trans?"-t":"-r", s);
}

#define NUMPATTERNS 5
/*
 * PATTERNLEFTOVER patterns the 1-3 bytes at the end of the buffer
 * when the buffer size is not a multiple of 4
 */
#define PATTERNLEFTOVER \
    clp = (char*)cp + leftover - 1;\
    for (c >>= (4 - leftover) * 8; leftover--; c >>= 8) \
	*clp-- = c;


void
pattern(register int* cp, register int pat, register int cnt)
{
    register unsigned int c=0;
    char *clp;
    register int leftover=cnt%4;
    
    pat %= NUMPATTERNS;
    cnt /= 4;
    switch (pat) {
	case 0:
	c = 0x00000000; /* black */
	while(cnt--) *cp++ = c;
	PATTERNLEFTOVER;
	break;
	case 1:
	/* c = 0x0000FF00; 32 bit red */
	c = 0x7C007C00; /* 16 bit red */
	while(cnt--) *cp++ = c;
	PATTERNLEFTOVER;
	break;
    case 2 :
	/* c = 0x00FF0000; 32 bit green */
	c = 0x03E003E0; /* 16 bit green */
	while(cnt--) *cp++ = c;
	PATTERNLEFTOVER;
	break;
    case 3 : 
	/* c = 0xFF000000; 32 bit blue */
	c = 0x001F001F; /* 16 bit blue */
	while(cnt--) *cp++ = c;
	PATTERNLEFTOVER;
	break;
    case 4 : 
	c = 0xFFFFFFFF; /* white */
	while(cnt--) *cp++ = c;
	PATTERNLEFTOVER;
	break;
    default :
	/* while(cnt--) *cp++ = c++; */
	fprintf(stderr, "pattern : NUMPATTERN > 5 error\n");
    }
}

/* 
 * CHECKPATTERN prints an error message if x is not equal to y. x
 * and y may be ints or chars
 */

#define CHECKPATTERN(x, y) \
if(x ^ y) { \
    fprintf(stderr,"dmttcp-r: Corrupt frame %d, byte value = %x\n",\
	    nframes, *(cp-1));\
	    return; }

/*
 * CHECKLEFTOVERPATTERN checks the last 1-3 bytes of a buffer not
 * divisible by 4. It does this by right shifting the contents of
 * register variable c in the for loop, and comparing the least
 * significant byte of c to *clp
 */

#define CHECKLEFTOVERPATTERN \
clp = (char*)cp+leftover-1;\
for(c>>=(4-leftover)*8; leftover--; c>>=8)\
    CHECKPATTERN(*clp--, (c & 0xFF));


void
checkPattern(register int* cp, register int pat, register int cnt)
{
    register unsigned int c=0;
    char *clp;
    register int leftover=cnt%4;

    pat %= NUMPATTERNS;
    cnt /= 4;
    switch(pat) {
    case 0 :
	c = 0x00000000;
	while(cnt--)
	    CHECKPATTERN(*cp++, c);
	CHECKLEFTOVERPATTERN;
	break;
    case 1 :
	c = 0x7C007C00;
	while(cnt--)
	    CHECKPATTERN(*cp++, c);
	CHECKLEFTOVERPATTERN;
	break;
    case 2 :
	c = 0x03E003E0;
	while(cnt--)
	    CHECKPATTERN(*cp++, c);
	CHECKLEFTOVERPATTERN;
	break;
    case 3 :
	c = 0x001F001F;
	while(cnt--)
		CHECKPATTERN(*cp++, c);
	CHECKLEFTOVERPATTERN;
	break;
    case 4 :
	c = 0xFFFFFFFF;
	while(cnt--)
	    CHECKPATTERN(*cp++, c);
	CHECKLEFTOVERPATTERN;
	break;
    default :
	fprintf(stderr, "pattern : NUMPATTERN > 5 error\n");
    }
}


char *
outfmt(double b)
{
    static char obuf[50];
    switch (fmt) {
        case 'G':
	    sprintf(obuf, "%.2f GB", b / 1024.0 / 1024.0 / 1024.0);
	    break;
        default:
	case 'K':
	    sprintf(obuf, "%.2f KB", b / 1024.0);
	    break;
	case 'M':
	    sprintf(obuf, "%.2f MB", b / 1024.0 / 1024.0);
	    break;
	case 'g':
	    sprintf(obuf, "%.2f Gbit", b * 8.0 / 1024.0 / 1024.0 / 1024.0);
	    break;
	case 'k':
	    sprintf(obuf, "%.2f Kbit", b * 8.0 / 1024.0);
	    break;
	case 'm':
	    sprintf(obuf, "%.2f Mbit", b * 8.0 / 1024.0 / 1024.0);
	    break;
    }
    return obuf;
}

#if defined(SYSV)
/*ARGSUSED*/
static
getrusage(ignored, ru)
    int ignored;
    register struct rusage *ru;
{
    struct tms buf;

    times(&buf);

    /* Assumption: HZ <= 2147 (LONG_MAX/1000000) */
    ru->ru_stime.tv_sec  = buf.tms_stime / HZ;
    ru->ru_stime.tv_usec = ((buf.tms_stime % HZ) * 1000000) / HZ;
    ru->ru_utime.tv_sec  = buf.tms_utime / HZ;
    ru->ru_utime.tv_usec = ((buf.tms_utime % HZ) * 1000000) / HZ;
}

/*ARGSUSED*/
static void
gettimeofday(struct timeval* tp, struct timeval* zp)
{
    tp->tv_sec = time(0);
    tp->tv_usec = 0;
}
#endif /* SYSV */

/*
 *			P R E P _ T I M E R
 */
void
prep_timer()
{
    gettimeofday(&time0, (struct timezone *)0);
    getrusage(RUSAGE_SELF, &ru0);
}

/*
 *			R E A D _ T I M E R
 * 
 */
double
read_timer(char* str, int len)
{
    struct timeval timedol;
    struct rusage ru1;
    struct timeval td;
    struct timeval tend, tstart;
    char line[132];

    getrusage(RUSAGE_SELF, &ru1);
    gettimeofday(&timedol, (struct timezone *)0);
    prusage(&ru0, &ru1, &timedol, &time0, line);
    (void)strncpy( str, line, len );

    /* Get real time */
    tvsub( &td, &timedol, &time0 );
    realt = td.tv_sec + ((double)td.tv_usec) / 1000000;

    /* Get CPU time (user+sys) */
    tvadd( &tend, &ru1.ru_utime, &ru1.ru_stime );
    tvadd( &tstart, &ru0.ru_utime, &ru0.ru_stime );
    tvsub( &td, &tend, &tstart );
    cput = td.tv_sec + ((double)td.tv_usec) / 1000000;
    if( cput < 0.00001 )  cput = 0.00001;
    return( cput );
}

static void
prusage(register struct rusage* r0, register struct rusage* r1,
	struct timeval* e, struct timeval* b, char* outp)
{
    struct timeval tdiff;
    register time_t t;
    register char *cp;
    register int i;
    int ms;

    t = (r1->ru_utime.tv_sec-r0->ru_utime.tv_sec)*100+
	(r1->ru_utime.tv_usec-r0->ru_utime.tv_usec)/10000+
	(r1->ru_stime.tv_sec-r0->ru_stime.tv_sec)*100+
	(r1->ru_stime.tv_usec-r0->ru_stime.tv_usec)/10000;
    ms =  (e->tv_sec-b->tv_sec)*100 + (e->tv_usec-b->tv_usec)/10000;

#define END(x)	{while(*x) x++;}
#if defined(SYSV)
    cp = "%Uuser %Ssys %Ereal %P";
#else
#if defined(sgi)		/* IRIX 3.3 will show 0 for %M,%F,%R,%C */
    cp = "%Uuser %Ssys %Ereal %P %Mmaxrss %F+%Rpf %Ccsw";
#else
    cp = "%Uuser %Ssys %Ereal %P %Xi+%Dd %Mmaxrss %F+%Rpf %Ccsw";
#endif
#endif
    for (; *cp; cp++)  {
	if (*cp != '%')
	    *outp++ = *cp;
	else if (cp[1]) switch(*++cp) {

	case 'U':
	    tvsub(&tdiff, &r1->ru_utime, &r0->ru_utime);
	    sprintf(outp,"%d.%01d", tdiff.tv_sec, tdiff.tv_usec/100000);
	    END(outp);
	    break;

	case 'S':
	    tvsub(&tdiff, &r1->ru_stime, &r0->ru_stime);
	    sprintf(outp,"%d.%01d", tdiff.tv_sec, tdiff.tv_usec/100000);
	    END(outp);
	    break;

	case 'E':
	    psecs(ms / 100, outp);
	    END(outp);
	    break;

	case 'P':
	    sprintf(outp,"%d%%", (int) (t*100 / ((ms ? ms : 1))));
	    END(outp);
	    break;

#if !defined(SYSV)
	case 'W':
	    i = r1->ru_nswap - r0->ru_nswap;
	    sprintf(outp,"%d", i);
	    END(outp);
	    break;

	case 'X':
	    sprintf(outp,"%d", t == 0 ? 0 : (r1->ru_ixrss-r0->ru_ixrss)/t);
	    END(outp);
	    break;

	case 'D':
	    sprintf(outp,"%d", t == 0 ? 0 :
		(r1->ru_idrss+r1->ru_isrss-(r0->ru_idrss+r0->ru_isrss))/t);
	    END(outp);
	    break;

	case 'K':
	    sprintf(outp,"%d", t == 0 ? 0 :
		((r1->ru_ixrss+r1->ru_isrss+r1->ru_idrss) -
		(r0->ru_ixrss+r0->ru_idrss+r0->ru_isrss))/t);
	    END(outp);
	    break;

	case 'M':
	    sprintf(outp,"%d", r1->ru_maxrss/2);
	    END(outp);
	    break;

	case 'F':
	    sprintf(outp,"%d", r1->ru_majflt-r0->ru_majflt);
	    END(outp);
	    break;

	case 'R':
	    sprintf(outp,"%d", r1->ru_minflt-r0->ru_minflt);
	    END(outp);
	    break;

	case 'I':
	    sprintf(outp,"%d", r1->ru_inblock-r0->ru_inblock);
	    END(outp);
	    break;

	case 'O':
	    sprintf(outp,"%d", r1->ru_oublock-r0->ru_oublock);
	    END(outp);
	    break;
	case 'C':
	    sprintf(outp,"%d+%d", r1->ru_nvcsw-r0->ru_nvcsw,
		    r1->ru_nivcsw-r0->ru_nivcsw );
	    END(outp);
	    break;
#endif /* !SYSV */
	}
    }
    *outp = '\0';
}

static void
tvadd(struct timeval* tsum, struct timeval* t0, struct timeval* t1)
{

    tsum->tv_sec = t0->tv_sec + t1->tv_sec;
    tsum->tv_usec = t0->tv_usec + t1->tv_usec;
    if (tsum->tv_usec > 1000000)
	    tsum->tv_sec++, tsum->tv_usec -= 1000000;
}

static void
tvsub(struct timeval* tdiff, struct timeval* t1, struct timeval* t0)
{

    tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
    tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
    if (tdiff->tv_usec < 0)
	    tdiff->tv_sec--, tdiff->tv_usec += 1000000;
}

static void
psecs(long l, register char* cp)
{
    register int i;

    i = l / 3600;
    if (i) {
	sprintf(cp,"%d:", i);
	END(cp);
	i = l % 3600;
	sprintf(cp,"%d%d", (i/60) / 10, (i/60) % 10);
	END(cp);
    } else {
	i = l;
	sprintf(cp,"%d", i / 60);
	END(cp);
    }
    i %= 60;
    *cp++ = ':';
    sprintf(cp,"%d%d", i / 10, i % 10);
}
