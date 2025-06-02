/*
 * dmplay.c: Plays back an SGI Movie JPEG file.
 *
 * 
 * Silicon Graphics Inc., June 1994
 */

#include "dmplay.h"

/*
 * Global definitions
 */
_Movie		movie;
_Image		image;
_Codec		codec;
_Audio		audio;
_PlayState	playstate;
_Video		video;
_Options	options; /* initialized by parseargs() */

extern void streamDecompressImpact( void );
extern void streamDecompressEv1( void );

static void
initglobals( void )
{
    playstate.advanceAudio = 1;
    playstate.advanceVideo = 1;
    playstate.playMode = PLAY_CONT;

    codec.engine = CL_JPEG_COSMO;

    image.display = VIDEO_2_FIELD;
}

/* 
 * set up initial play state variables based on information
 * from command-line options and movie file header
 */
void
initPlayState( void )
{
    if (audio.isTrack && options.playAudioIfPresent)
        playstate.audioActive = 1;
    else 
        playstate.audioActive = 0;

    playstate.playMode = options.initialPlayMode;
    playstate.autoStop = options.autostop;

    if (options.initialLoopMode == LOOP_NOTSPEC) {
        if (movie.loopMode == MV_LOOP_CONTINUOUSLY)
            playstate.loopMode = LOOP_REPEAT;
        else  /* XXX MV_LOOP_SWINGING not supported */
            playstate.loopMode = LOOP_NONE;
    } else {
        playstate.loopMode = options.initialLoopMode;
    }
}

/*
 * set non-degrading priority
 * use high priority if running as root
 */
void
setscheduling(void)
{

    if (!playstate.audioActive)
	return;

    /*
     * swap permissions so that we become root for a moment. 
     * setreuid() should let us set real uid to effective uid = root
     * if the program is setuid root 
     * 
     */
    setreuid(geteuid(), getuid());
    setregid(getegid(), getgid());

    if (schedctl(NDPRI, 0, NDPHIMIN)< 0) {
         fprintf(stderr, "%s: run as root to enable real time scheduling\n",
             options.myname);
    }

    /* swap permissions back, now we're just "joe user" */
    setreuid(geteuid(), getuid());
    setregid(getegid(), getgid());

}

void
main(int argc, char *argv[]) 
{
    /*
     * Drop permissions from setuid-root to the real user.  This
     * will prevent us from doing anything that the normal user
     * wouldn't have permission to do.  We can restore ourselves
     * to super user status later if we need to.
     */
    setreuid(geteuid(), getuid());
    setregid(getegid(), getgid());

    initglobals();
    Xinit(&argc, argv);
    parseargs(argc, argv);

    if (codec.singleFrame)
	singleFrameDecompress();
    else {
        if (!strcasecmp ("impact", options.image_engine))
		streamDecompressImpact();
	else
		streamDecompressEv1();
    }
    exit(0);
}
