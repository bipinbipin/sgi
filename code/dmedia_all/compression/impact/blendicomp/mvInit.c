/*
** open the movie file and set it up
*/

#include <string.h>
#include "blendicomp.h"

mvInit()
{
    const char * compressionScheme;
    int ilace_movie;

    if (mvOpenFile(movie.filename, O_RDONLY, &movie.mv) != DM_SUCCESS) {
	fprintf(stderr,"failed to open movie file >%s<\n",movie.filename);
	return -1;
    }

    if (mvFindTrackByMedium(movie.mv,DM_IMAGE,&movie.imgTrack) != DM_SUCCESS) {
	fprintf(stderr,"movie >%s< doesn't have an image track\n",
							movie.filename);
	return -1;
    }

    /*
    ** extract info from the movie file and put it into the image global
    */
    image.source_width	= mvGetImageWidth(movie.imgTrack);
    image.source_height = mvGetImageHeight(movie.imgTrack);
    movie.numberFrames	= mvGetTrackLength(movie.imgTrack);

    ilace_movie = mvGetImageInterlacing(movie.imgTrack);
    if (ilace_movie != DM_IMAGE_NONINTERLACED) {
	image.source_height /= 2;   /* interlaced, so really 1/2 height */
    }

    /*
    ** make sure that it's a jpeg movie (duh)
    */
    compressionScheme = mvGetImageCompression( movie.imgTrack );
    if ( strcmp(compressionScheme, DM_IMAGE_JPEG) != 0 ) {
	fprintf(stderr,"movie file is not JPEG compressed\n");
	return -1;
    }

    return 0;
}
