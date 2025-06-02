/*
** blendicomp example program header file
*/

#ifndef BLENDIC_H_
#define BLENDIC_H_

#include    <stdio.h>

/*
** system headers
*/
#include    <sys/types.h>
#include    <sys/resource.h>
#include    <sys/prctl.h>
#include    <sys/signal.h>

/*
** dmedia headers
*/
#include    <dmedia/dm_image.h>
#include    <dmedia/cl.h>
#include    <dmedia/cl_impactcomp.h>
#include    <movie.h>
#include    <dmedia/vl.h>
#include    <dmedia/vl_impact.h>
#include    <dmedia/vl_mgv.h>
#include    <dmedia/vl_mgc.h>

/*
** global structures
*/
typedef struct {
    int		    source_height;	/* compressed image height */
    int		    source_width;	/* compressed image width */
} image_t;

typedef struct {
    int		    interleave;		/* interleave on display? */
    int		    verbose;		/* verbosity */
} options_t;

typedef struct {
    CLcompressorHdl hdl;		/* compressor handle */
    CLbufferHdl	    compRB;		/* compressed data ring buffer */
    int		    vlcodec;		/* vl codec node cookie */
} codec_t;

typedef struct {
    char	  * filename;		/* movie file name */
    MVid	    mv;			/* movie */
    MVid	    imgTrack;		/* image track of movie */
    int		    numberFrames;	/* # of mv frames in movie */
} movie_t;

typedef struct {
    VLServer	    vlSrv;		/* connection to the VL Server */
    VLNode	    node_codec;		/* codec source node */
    VLNode	    node_video;		/* video output node */
    VLNode	    node_screen;	/* screen source node */
    VLNode	    node_blend;		/* blender internal node */
    VLPath	    vlPath;		/* the path, eh? */
} video_t;


/*
** declair the globals used everywhere
*/
extern image_t	    image;
extern options_t    options;
extern codec_t	    codec;
extern movie_t	    movie;
extern video_t	    video;


int parseargs( int argc, char * argv[] );
void stopAllThreads( void );
int clInit( void );
int clStartDecompressor( void );
int mvInit( void );

#endif	/* BLENDIC_H_ */
