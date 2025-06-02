/*
 * dioav.h
 *
 * This file defines structures for the dioav file format. dioav is an
 * interleaved audio/video format optimized for direct I/O and realtime
 * capture/playback using the SGI digital media libraries. It has the 
 * following structure:
 *
 *   -----------------------
 *   |                     |
 *   |    dioav header     |
 *   |                     |
 *   -----------------------   <--- Direct I/O block size boundry
 *   |     av chunk 1      |
 *   | ------------------- |
 *   | |   video data    | |
 *   | ------------------- |
 *   | |  audio header   | |
 *   | ------------------- |
 *   | |   audio data    | |
 *   | ------------------- |
 *   |                     | 
 *   -----------------------   <--- Direct I/O block size boundry
 *   |     av chunk 2      |
 *   | ------------------- |
 *   | |   video data    | |
 *   | ------------------- |
 *   | |  audio header   | |
 *   | ------------------- |
 *   | |   audio data    | |
 *   | ------------------- |
 *   |                     | 
 *   -----------------------   <--- Direct I/O block size boundry
 *   |         ...         |
 *
 * 
 * Note that video data is headerless; the video frame/field is assumed to
 * be of a constant fixed size, as specifed in the dioav header. The dioav
 * header is containe in the first direct I/O block. As it is less than 512
 * bytes long, this is true for all current direct I/O devices.
 *
 * The first av chunk starts at the second direct I/O block. Each av chunk
 * is rounded up to be an integral number of av blocks. Note it is not 
 * guaranteed that he audio header and audio data reside at block boundries.
 */

/*
 * The dioav_header provides information about the dioav file and resides
 * in block 0.
 */
typedef struct {
    long dio_size;		/* Direct I/O block size used for this file */
    long chunk_rate;		/* Same as video frame or field rate */
    long chunk_size;		/* Size of a chunk in bytes */
    long video_width;		/* Image width */
    long video_height;		/* Image height */
    long video_packing;		/* Video Library packing used */
    long video_format;		/* Video Library format used */
    long video_captype;		/* Video Library capture type used */
    long video_trans_size;	/* Transfer size from VL */
    long audio_frame_rate;	/* Audio frame rate */
    long audio_sample_format;	/* Twos-complement, float, double */
    long audio_num_channels;	/* AL_{MONO, STEREO, 4CHANNEL } */
    long audio_sample_width;	/* AL_SAMPLE_{8, 16, 24}*/
    long audio_frame_size;	/* Size in bytes */
    long audio_num_frames;	/* How much space reserved for audio data */
} dioav_header;
 
/*
 * The audio header provides information about the following data. As most
 * of this information is already in the dioav_header, this header only has
 * the frame count.
 */
typedef struct {
    long long frame_count;
} audio_header;

/*
 * Useful constants and macros
 */
#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC 1000000000LL
#endif

#ifndef min
#define min(x, y) (((x) < (y)) ? (x) : (y) )
#endif
