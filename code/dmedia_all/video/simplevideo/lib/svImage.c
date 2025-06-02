/* $Id: svImage.c,v 1.5 1994/07/25 18:29:35 dpb Exp $ */

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <gl/gl.h>
#define iopen junk1 
#include <gl/image.h>
#undef iopen

#include <vlUtil/vlUtil.h>
#include <sv/sv.h>
#include "svPriv.h"

/********************************************************/
/*** XXX why aren't these in image.h for real?        ***/
/*** End of prototypes that should be in <gl/image.h> ***/
/********************************************************/

int putrow(IMAGE *image,
	   unsigned short *buffer,
	   int y,
	   int z);

int getrow(IMAGE *image,
	   unsigned short *buffer,
	   int y,
	   int z);

IMAGE *iopen(char *file,
	     char *mode,
	     ...);

int iclose (IMAGE *);

/********************************************************/
/*** End of prototypes that should be in <gl/image.h> ***/
/********************************************************/


/***
 *** Forward declaration of local static functions;
 ***/

static unsigned long *
longimagedata(char *name);

typedef unsigned long ul;


/**********************************************************************
 *  
 * svNewImage - 
 *
 * Allocate and initialize a svImage record;
 **********************************************************************
 */
svImage *
svNewImage(void)
{
    svImage *frame;

    frame = malloc(sizeof(svImage));
    if (frame) {
	frame->width = 0;
	frame->height = 0;
	frame->packing = -1;
	frame->dataMalloced = FALSE;
	frame->data = NULL;
	frame->compressed = FALSE;
    }

    return frame;
}

/**********************************************************************
 *
 * svFreeImage -
 *
 * Free an image data structure;
 **********************************************************************
 */

void
svFreeImage(svImage **userFrame)
{
    svImage *frame;

    frame = *userFrame;

    if (frame->dataMalloced)
	free(frame->data);
    frame->data = NULL;

    free(frame);
    *userFrame = NULL;
}

/**********************************************************************
 *  
 * svSaveImage - 
 *
 * Save an image to a file
 **********************************************************************
 */
int
svSaveImage(char *filename, svImage *frame)
{
    IMAGE *image;
    ushort rbuf[1024];
    ushort gbuf[1024];
    ushort bbuf[1024];
    int x, y;

    char *data;
    int xsize, ysize;

    static char *compressFilter = "/usr/bsd/compress";
    static char *compFilterArgv0 = "compress";

    ConnectionState *sv;


    sv = _findContext();

    data  = frame->data;
    xsize = frame->width;
    ysize = frame->height;

    /*** RGB data ***/
    if (frame->packing == VL_PACKING_RGB_8 ||
	frame->packing == VL_PACKING_RGBA_8) {

	image = iopen(filename, "w", RLE(1), 3, xsize, ysize, 3);

	for (y=0;y<ysize;y++) {
	    for(x=0;x<xsize;x++) {
		data++;
		bbuf[x] = (ushort) *(data++);
		gbuf[x] = (ushort) *(data++);
		rbuf[x] = (ushort) *(data++);
	    }
	    putrow(image, rbuf, ysize - y - 1, 0);
	    putrow(image, gbuf, ysize - y - 1, 1);
	    putrow(image, bbuf, ysize - y - 1, 2);
	}

	iclose(image);
    }

    /* other */
    else {
	int image_fd;
	short magic;
	pid_t compress_pid = 0;
	int status;
	struct stat sb;

	/*** You would think you could use access(2) to figure
	 *** this stuff out, but with EX_OK, it doesn't do the 
	 *** checks against our uid and gid to see if the execute
	 *** bit is turned on in the right place!
	 ***/
	stat(compressFilter, &sb);
	if (sv->saveImagesCompressed &&
	    !(sb.st_mode & 0001) &&
	    !((sb.st_mode & 0100) && sb.st_uid == geteuid()) &&
	    !((sb.st_mode & 0010) && sb.st_gid == getegid()))
	    return svCompressMissing;

	if (sv->saveImagesCompressed)
	    frame->compressed = TRUE;

	/* open the file and write the header */
	image_fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC);
	if (image_fd < 0)
	    return(svBadFile);

	magic = RGB_SV_IMAGE_MAGIC;
	write(image_fd, &magic, 2);
	write(image_fd, frame, sizeof(*frame));

	/* Set up compression if called for */
	if (sv->saveImagesCompressed) {
	    int fdset[2];			/* [0] read, [1] write */

	    pipe(fdset);
	    if ((compress_pid=fork()) == 0) {
		/* child */
		close(0);
		close(1);

		/* setup fds for compress */
		close(fdset[1]);
		dup2(fdset[0], 0);	    /* clone fdset[0] into stdin */
		dup2(image_fd, 1);	    /* image_fd is the output    */
		execl(compressFilter, compFilterArgv0, (char *)0);
		exit(1);
	    }

	    /* aim image_fd at the pipe to compress */
	    close(fdset[0]);
	    close(image_fd);
	    image_fd = fdset[1];
	}

	/* write the data */
	if (write(image_fd, frame->data, (uint) frame->dataSize) < 0) {
	    unlink(filename);
	    if (sv->saveImagesCompressed)
		return svCompressFailed;
	    else
		return svFileWriteFailed;
	}

	close(image_fd);

	if (compress_pid)
	    waitpid(compress_pid, &status, 0);
    }

    return 0;
}

/**********************************************************************
 * 
 * svLoadImage - 
 *
 * Load an image from a file
 **********************************************************************
 */

int 
svLoadImage(char *filename, svImage **userFrame)
{
    int image_fd;
    int count, size;
    short magic;
    char *raster;
    char *ptr;
    svImage *frame;
    IMAGE *image;
    
    static char *uncompressFilter = "/usr/bsd/uncompress";
    static char *uncompFilterArgv0 = "uncompress";

    *userFrame = NULL;

    /* Open the file, try to read the magic number */
    image_fd = open(filename, O_RDONLY);
    if (image_fd < 0)
	return svBadFile;

    read(image_fd, &magic, 2);

    /*  If it's RGB image data */
    if (magic == RGB_MAGIC) {
	close(image_fd); 
	image = iopen(filename, "r");
	raster = (char *)longimagedata(filename);

	frame = (svImage *) malloc(sizeof(svImage));
	frame->width   = (int) image->xsize;
	frame->height  = (int) image->ysize;
	frame->packing = VL_PACKING_RGB_8;
	frame->data    =  raster;
	frame->dataSize = frame->width * frame->height * 4;
	frame->dataMalloced = TRUE;
    }
    else if (magic == RGB_SV_IMAGE_MAGIC) {
	frame = (svImage *) malloc(sizeof(svImage));
	read(image_fd, frame, sizeof(*frame));
	
	if (frame->compressed) {
	    int fdset[2];		/* [0] read, [1] write */
	    struct stat sb;

	    /*** If we used compression, now make sure we will be able
	     *** to exec the uncompress program.  See comments above 
	     *** about why I'm not using access(2).
	     ***/
	    stat(uncompressFilter, &sb);
	    if (!(sb.st_mode & 0001) &&
		!((sb.st_mode & 0100) && sb.st_uid == geteuid()) &&
		!((sb.st_mode & 0010) && sb.st_gid == getegid()))
		return svCompressMissing;
	    
	    pipe(fdset);
	    if (fork() == 0) {
		/* child */
		close(0);
		close(1);

		/* setup fds for uncompress */
		close(fdset[0]);
		dup2(image_fd, 0); 	/* image fd is uncompress input */
		dup2(fdset[1], 1); 	/* clone fdset[1] to stdout */
		execl(uncompressFilter, uncompFilterArgv0, (char *)0);
		exit(1);
	    }
	    
	    /* aim image_fd at the output from uncompress */
	    close(fdset[1]);
	    close(image_fd);
	    image_fd = fdset[0];
	}

	frame->data = malloc((uint)frame->dataSize);

	/* Read the frame data - we need the loop 'cause if we're
	 * reading from a pipe, we may not get all the data in
	 * one shot;
	 */

	size = frame->dataSize;
	ptr  = frame->data;
	while (size && (count=read(image_fd, ptr, (uint)size)) >= 0) {
	    size -= count;
	    ptr  += count;
	}
    }
    else
	return svBadFile;

    *userFrame = frame;
    
    return 0;
}

/**********************************************************************
 * 
 * svViewImage - 
 *
 * Use GL to display an image on screen
 **********************************************************************
 */

int
svViewImage(svImage *frame, int x, int y)
{
    int ymax;

    if (frame->packing != VL_PACKING_RGB_8 &&
	frame->packing != VL_PACKING_RGBA_8)
	return svBadImageType;

    foreground();
    ymax = (int) getgdesc(GD_YPMAX);
    prefsize((long)frame->width, (long)frame->height);
    prefposition((long)x, (long)x+(long)frame->width,
		 (long)(ymax-y), (long)(ymax-y-frame->height));
    winopen("");
    RGBmode();
    gconfig();
    pixmode(PM_TTOB, 1);
    lrectwrite(0, 0, (short)(frame->width-1), (short)(frame->height-1),
	       (ulong *)frame->data);

    return svSuccess;
}

/**********************************************************************
 * misc. routines used for reading rgb files;
 **********************************************************************
 */

static void
rgbatocpack(unsigned short *r,
	    unsigned short *g,
	    unsigned short *b,
	    unsigned short *a,
	    unsigned long *l,
	    int n)
{

    while(n>=8) {
	l[0] = (ul)r[0] | (ul)(g[0]<<8) | (ul)(b[0]<<16) | (ul)(a[0]<<24);
	l[1] = (ul)r[1] | (ul)(g[1]<<8) | (ul)(b[1]<<16) | (ul)(a[1]<<24);
	l[2] = (ul)r[2] | (ul)(g[2]<<8) | (ul)(b[2]<<16) | (ul)(a[2]<<24);
	l[3] = (ul)r[3] | (ul)(g[3]<<8) | (ul)(b[3]<<16) | (ul)(a[3]<<24);
	l[4] = (ul)r[4] | (ul)(g[4]<<8) | (ul)(b[4]<<16) | (ul)(a[4]<<24);
	l[5] = (ul)r[5] | (ul)(g[5]<<8) | (ul)(b[5]<<16) | (ul)(a[5]<<24);
	l[6] = (ul)r[6] | (ul)(g[6]<<8) | (ul)(b[6]<<16) | (ul)(a[6]<<24);
	l[7] = (ul)r[7] | (ul)(g[7]<<8) | (ul)(b[7]<<16) | (ul)(a[7]<<24);
	l += 8;
	r += 8;
	g += 8;
	b += 8;
	a += 8;
	n -= 8;
    }
    while(n--) 
        *l++ = (ul)*r++|(ul)((*g++)<<8)|(ul)((*b++)<<16)|(ul)((*a++)<<24);
}

static void
rgbtocpack(unsigned short *r,
	   unsigned short *g,
	   unsigned short *b,
	   unsigned long *l,
	   int n)
{
    while(n>=8) {
	l[0] = (ul)r[0] | (ul)(g[0]<<8) | (ul)(b[0]<<16) | (ul)(0xff<<24);
	l[1] = (ul)r[1] | (ul)(g[1]<<8) | (ul)(b[1]<<16) | (ul)(0xff<<24);
	l[2] = (ul)r[2] | (ul)(g[2]<<8) | (ul)(b[2]<<16) | (ul)(0xff<<24);
	l[3] = (ul)r[3] | (ul)(g[3]<<8) | (ul)(b[3]<<16) | (ul)(0xff<<24);
	l[4] = (ul)r[4] | (ul)(g[4]<<8) | (ul)(b[4]<<16) | (ul)(0xff<<24);
	l[5] = (ul)r[5] | (ul)(g[5]<<8) | (ul)(b[5]<<16) | (ul)(0xff<<24);
	l[6] = (ul)r[6] | (ul)(g[6]<<8) | (ul)(b[6]<<16) | (ul)(0xff<<24);
	l[7] = (ul)r[7] | (ul)(g[7]<<8) | (ul)(b[7]<<16) | (ul)(0xff<<24);
	l += 8;
	r += 8;
	g += 8;
	b += 8;
	n -= 8;
    }
    while(n--) 
        *l++ = (ul)*r++ | (ul)((*g++)<<8) | (ul)((*b++)<<16) | (ul)(0xff<<24);
}

/* 	cpack -
 *		Convert from and to cpack format.
 *	
 */
static void
bwtocpack(unsigned short *b,
	  unsigned long *l,
	  int n)
{
    while(n>=8) {
	l[0] = 0xff000000+0x00010101*(ulong)b[0];
	l[1] = 0xff000000+0x00010101*(ulong)b[1];
	l[2] = 0xff000000+0x00010101*(ulong)b[2];
	l[3] = 0xff000000+0x00010101*(ulong)b[3];
	l[4] = 0xff000000+0x00010101*(ulong)b[4];
	l[5] = 0xff000000+0x00010101*(ulong)b[5];
	l[6] = 0xff000000+0x00010101*(ulong)b[6];
	l[7] = 0xff000000+0x00010101*(ulong)b[7];
	l += 8;
	b += 8;
	n -= 8;
    }
    while(n--) 
	*l++ = (ul)0xff000000+(ul)(0x00010101*(*b++));
}

static unsigned long *
longimagedata(char *name)
{
    unsigned long *base, *lptr;
    unsigned short *rbuf, *gbuf, *bbuf, *abuf;
    IMAGE *image;
    ushort ysize;
    int y;

    image = iopen(name,"r");
    if(!image) {
	fprintf(stderr,"longimagedata: can't open image file %s\n",name);
	exit(1);
    }

    base = (ulong *)malloc((uint)image->xsize*(uint)image->ysize*sizeof(long));
    rbuf = (ushort *)malloc((uint)image->xsize*sizeof(short));
    gbuf = (ushort *)malloc((uint)image->xsize*sizeof(short));
    bbuf = (ushort *)malloc((uint)image->xsize*sizeof(short));
    abuf = (ushort *)malloc((uint)image->xsize*sizeof(short));
    if(!base || !rbuf || !gbuf || !bbuf) {
	fprintf(stderr,"longimagedata: can't malloc enough memory\n");
	exit(1);
    }
    lptr = base;
    ysize = image->ysize;
    for(y=0; y<ysize; y++) {
	if(image->zsize>=4) {
	    getrow(image,rbuf,ysize-y-1,0);
	    getrow(image,gbuf,ysize-y-1,1);
	    getrow(image,bbuf,ysize-y-1,2);
	    getrow(image,abuf,ysize-y-1,3);
	    rgbatocpack(rbuf,gbuf,bbuf,abuf,lptr,(int) image->xsize);
	    lptr += (unsigned long) image->xsize;
	} else if(image->zsize==3) {
	    getrow(image,rbuf,ysize-y-1,0);
	    getrow(image,gbuf,ysize-y-1,1);
	    getrow(image,bbuf,ysize-y-1,2);
	    rgbtocpack(rbuf,gbuf,bbuf,lptr,(int) image->xsize);
	    lptr += (unsigned long) image->xsize;
	} else {
	    getrow(image,rbuf,ysize-y-1,0);
	    bwtocpack(rbuf,lptr,(int)image->xsize);
	    lptr += (unsigned long) image->xsize;
	}
    }
    iclose(image);
    free(rbuf);
    free(gbuf);
    free(bbuf);
    free(abuf);
    return base;
}
