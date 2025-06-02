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
_Audio		audio;
_PlayState	playstate;
_Video		video;
_Options	options; /* initialized by parseargs() */


extern void setDefaults(_Options *);


/*
 * set non-degrading priority
 * use high priority if running as root
 */
void
setscheduling(void)
{
    /*
     * swap permissions so that we become root for a moment. 
     * setreuid() should let us set real uid to effective uid = root
     * if the program is setuid root 
     * 
     */
    setreuid(geteuid(), getuid());
    setregid(getegid(), getgid());

    if (schedctl(NDPRI, 0, NDPHIMIN)< 0) {
         fprintf(stderr, "Run as root to enable real time scheduling\n");
    }

    /* swap permissions back, now we're just "joe user" */
    setreuid(geteuid(), getuid());
    setregid(getegid(), getgid());

}

void stopAllThreads
    (
    void
    )
{
    exit(0);
}


static void sigint(void)
{
    exit(1);
}


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

    signal(SIGINT, sigint);

    setDefaults(&options);
    parseargs(argc, argv);
    mvInit();

    alInit();

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

    /* These 4 parameters can only be set after we know the type
     * of the movie and the display, unless the user actually
     * set them in the command line */

    if (dmID.inbufs == -1) {
	if (DO_FRAME) {
	    if (image.display == GRAPHICS)
		dmID.inbufs = FRAME_GFX_INBUFS;
	    else
		dmID.inbufs = FRAME_VIDEO_INBUFS;
	} else {
	    if (image.display == GRAPHICS)
		dmID.inbufs = FIELD_GFX_INBUFS;
	    else
		dmID.inbufs = FIELD_VIDEO_INBUFS;
	}
    }

    /* When displaying to video, using a small number for dmID.outbufs 
     * results in bad video display while a large number tends to cause
     * graphics display to run ahead of the video display. The values
     * we use here creat good video display. The audio is sync'ed to the
     * video display, so it will be out of sync with graphics display.
     */
    if (dmID.outbufs == -1) {
	if (DO_FRAME) {
	    if (image.display == GRAPHICS)
		dmID.outbufs = FRAME_GFX_OUTBUFS;
	    else
		dmID.outbufs = FRAME_VIDEO_OUTBUFS;
	} else {
	    if (image.display == GRAPHICS)
		dmID.outbufs = FIELD_GFX_OUTBUFS;
	    else
		dmID.outbufs = FIELD_VIDEO_OUTBUFS;
	}
    }

    if (video.preroll == -1) {
	if (DO_FRAME) {
	    if (image.display == GRAPHICS)
		video.preroll = FRAME_GFX_VIDEOPREROLL;
	    else
		video.preroll = FRAME_VIDEO_VIDEOPREROLL;
	} else {
	    if (image.display == GRAPHICS)
		video.preroll = FIELD_GFX_VIDEOPREROLL;
	    else
		video.preroll = FIELD_VIDEO_VIDEOPREROLL;
	}
    }

    if (audio.preroll == -1) {
	if (DO_FRAME) {
	    if (image.display == GRAPHICS)
		audio.preroll = FRAME_GFX_AUDIOPREROLL;
	    else
		audio.preroll = FRAME_VIDEO_AUDIOPREROLL;
	} else {
	    if (image.display == GRAPHICS)
		audio.preroll = FIELD_GFX_AUDIOPREROLL;
	    else
		audio.preroll = FIELD_VIDEO_AUDIOPREROLL;
	}
    }

    streamDecompress();
    return(0);
}
