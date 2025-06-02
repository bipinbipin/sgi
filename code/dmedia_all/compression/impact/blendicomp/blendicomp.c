
#include <bstring.h>
#include "blendicomp.h"

image_t	    image;
options_t   options;
codec_t	    codec;
movie_t	    movie;
video_t	    video;

main(int argc, char * argv[])
{
    int mvInit( void );
    int clInit( void );
    int vlInit( void );
    int vlGo( void );
    int clStartDecompressor( void );

    options.verbose = 0;

    if (argc != 2) {
	fprintf(stderr,"blendicomp file.mv\n");
	exit(1);
    }

    movie.filename = argv[1];

    /*
    ** get the movie and decompressed image sizes
    */
    if (mvInit() < 0) exit(1);

    /*
    ** get the codec node and initialize the codec
    */
    if (clInit() < 0) exit(1);

    /*
    ** get the vl stuff rolling
    */
    if (vlInit() < 0) exit(1);


    /*
    ** start things rolling ...
    */
    vlBeginTransfer( video.vlSrv, video.vlPath, 0, NULL );
    clStartDecompressor();


    /*
    ** and drop into the loop reading the movie file forever
    */
    while (1) {
	int i;
	int tmpBufSize = 0;
	unsigned char * tmpBuf = NULL;
	int size;
	int preWrap;
	int wrap;
	unsigned char * freeData;
	int consumed;

	for (i=0; i<movie.numberFrames; i++) {
	    size = mvGetCompressedImageSize( movie.imgTrack, i );
	    if (tmpBufSize < size) {
		if (tmpBuf != NULL) free(tmpBuf);
		tmpBufSize = size + 4096;
		tmpBuf = (unsigned char *)malloc( tmpBufSize );
	    }
	    mvReadCompressedImage( movie.imgTrack, i, tmpBufSize, tmpBuf );

	    preWrap = clQueryFree( codec.compRB, size, 
						(void **)&freeData, &wrap );
	    if (preWrap > size) preWrap = size;
	    bcopy( tmpBuf, freeData, preWrap );
	    clUpdateHead( codec.compRB, preWrap );
	    consumed = preWrap;
	    if (consumed < size) {
		preWrap = clQueryFree( codec.compRB, size-consumed,
						(void **)&freeData, &wrap );
		bcopy( tmpBuf+consumed, freeData, size-consumed );
		clUpdateHead( codec.compRB, (size-consumed) );
	    }
	}
    }
}
