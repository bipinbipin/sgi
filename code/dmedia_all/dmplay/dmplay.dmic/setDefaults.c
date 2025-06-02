#include "dmplay.h"
#include <invent.h>

#define DMICJPEG 'jpeg'

/* 1 for mvp, 2 for ev1, 3 for imactcomp, 0 for neither */
int hasVideoDevice;

/* 1 for mvp, 2 for ev1, 3 for imactcomp, 0 for neither (graphics) */
int useVideoDevice;

/* index of Vice JPEG decompressor for dmICCreate() */
int hasViceDec = -1;

/* index of Impact(Octane) JPEG decompressor for dmICCreate() */
int hasImpactDec = -1;

/* index of software JPEG decompressor for dmICCreate() */
int hasdmICSWDec = -1;

/* 1 for vice, 2 for dmicsw, 3 for impactcomp, 0 for neither */
int useEngine = 0;

int UseThisdmICDec;
_dmIC dmID;


/*
 * setDefaults
 */

void setDefaults
    (
    _Options *opts
    )
{
    int		i;
    int numIC;
    DMimageconverter ic;
    DMparams *p;
    inventory_t *inv;
    int octaneVideoFound = 0;

    opts->use_default_ports = 1;
    opts->audio_port_specified = 0;
    opts->playAudioIfPresent = 1;
    opts->display_port_specified = 0;
    opts->ifilename = NULL;
    opts->verbose    = 0;
    opts->initialLoopMode= LOOP_NOTSPEC;
    opts->autostop   = 1;
    opts->initialPlayMode= PLAY_CONT;
    opts->zoom = 1;  
    opts->videoOnly = 0;
    opts->hiPriority = 0;
    opts->forceSampleRate = 0;

    playstate.advanceAudio = 1;
    playstate.advanceVideo = 1;
    playstate.speedcontrol = 1;
    playstate.timing = 0;
    playstate.syncInterval = SYNCINTERVAL;
    playstate.gfx_lag = GFX_LAG;
    playstate.video_lag = VIDEO_LAG;

    /*
     * The following four items should be set after we know what kind of
     * of movie we are playing and whether to graphics or video. We
     * set them to -1 here so that we will know later whether the user
     * has specified them in the command line.
     */
    video.preroll =  -1;
    audio.preroll = -1;
    dmID.inbufs = -1;
    dmID.outbufs = -1;

    /* Look for mvp */
    setinvent();
    while ((inv = getinvent()) != NULL) {
        if ((inv->inv_type != INV_VICE) &&
            (inv->inv_type != INV_IMPACTCOMP)) { 
            continue;
        }

        if (inv->inv_type == INV_VICE) {
            opts->vid_device = strdup(MVP);
            useVideoDevice = hasVideoDevice = 1;
        }
        else if (inv->inv_type == INV_IMPACTCOMP) {
            opts->vid_device = strdup(IMPACT);
            useVideoDevice = hasVideoDevice = 3;
	    /* search if Octane video is on the system */
	    while ((inv = getinvent()) != NULL) {
		if (inv->inv_type == INV_VIDEO_RACER) {
		    octaneVideoFound = 1;
		    break;
		}
	    }
        }
	
	image.display = VIDEO_2_FIELD;
	goto devFound;
    }

    /* No device is found */
    useVideoDevice = hasVideoDevice = 0;
    opts->vid_device = NULL;
    image.display = GRAPHICS;

devFound:

    /* Find jpeg decompressors and make sure they can be opened */
    numIC = dmICGetNum();
    dmParamsCreate(&p);
    for (i = 0; i < numIC; i++) {
	/* Get description of next converter */
	if (dmICGetDescription(i, p) != DM_SUCCESS)
	    continue;
	/* Look for the right combination */
	if ((dmParamsGetInt(p, DM_IC_ID) == DMICJPEG) &&
	    (dmParamsGetEnum(p, DM_IC_CODE_DIRECTION) == 
		DM_IC_CODE_DIRECTION_DECODE)) {
	    if (dmParamsGetEnum(p, DM_IC_SPEED) == DM_IC_SPEED_REALTIME) {
		if (dmICCreate(i, &ic) == DM_SUCCESS) {
		    if (HASMVP) hasViceDec = i;
		    else if (HASIMPACT) hasImpactDec = i;
		    dmICDestroy(ic);
		}
	    } else {
		if (dmICCreate(i, &ic) == DM_SUCCESS) {
		    hasdmICSWDec = i;
		    dmICDestroy(ic);
		}
	    }
	}
    }
    dmParamsDestroy(p);


    /* Set engine default in the preference given below */
    if (HASVICE) {
	opts->image_engine = strdup(VICE);
	useEngine = 1;
	UseThisdmICDec = hasViceDec;
	dmID.speed = DM_IC_SPEED_REALTIME;
	return;
    }
    else if (HASIMPACTCOMP) {
	opts->image_engine = strdup(IMPACT);
        useEngine = 3;
        UseThisdmICDec = hasImpactDec;
        dmID.speed = DM_IC_SPEED_REALTIME;
	if (!octaneVideoFound)
	    opts->videoOnly = 1;
        return;
    }
    else if (HASDMICSW) {
	opts->image_engine = strdup(DMICSW);
	useEngine = 2;
	UseThisdmICDec = hasdmICSWDec;
	dmID.speed = DM_IC_SPEED_NONREALTIME;
	return;
    }

    fprintf(stderr, "No JPEG decompressor available on this host.\n");
    fprintf(stderr, "dmplay cannot run without JPEG decompressor.\n");
    exit(1);


} /* setDefaults */
