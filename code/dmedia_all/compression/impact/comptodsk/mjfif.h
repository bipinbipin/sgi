/*
** motion jfif file format
**
** this file defines the file format that is used to write motion
** jfif images to a file using directio, so that very high bandwidth
** sequences may be captured and played back.
*/

#ifndef	MJFIF_H_
#define	MJFIF_H_

/*
** file always starts with an mjfif_header_t
** while this structure isn't a full dio_size in length,
** we must do disk read/write operations based on that size,
** so the data actually begins at an offset of dio_size
**
** offsets are relative to the start of the file (offset 0)
*/
struct mjfif_header_s {
	char		magic[8];	/* mjfif-01 */
	__int32_t	dio_size;	/* direct i/o block size */
	__int32_t	field_rate;	/* fields per second */
	__int32_t	image_width;	/* image width */
	__int32_t	image_height;	/* image height */
	__int32_t	n_images;	/* total number of images */
	__int64_t	data_size;	/* total number of bytes captured */
};

typedef struct mjfif_header_s mjfif_header_t;

#endif	/* MJFIF_H_ */
