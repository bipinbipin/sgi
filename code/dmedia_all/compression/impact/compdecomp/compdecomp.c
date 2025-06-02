/*
** compdecomp
** Indigo2 IMPACT Compression example program
**
** Silicon Graphics, Inc, 1996
*/

#include "compdecomp.h"
#include <bstring.h>
#include <signal.h>
#include <stdio.h>

#define MIN(a,b)    (((a)>(b))?(b):(a))
struct global_s global;

main(int argc, char * argv[])
{
    CLimageInfo imageInfo;
    char * from_ptr, * to_ptr;
    int from_prewrap, to_prewrap;
    int ignored;
    int do_copy, to_transfer;

    if (cl_comp_open()) exit(-1);
    if (cl_decomp_open()) exit(-1);

    if (vl_capture_init()) exit(-1);
    if (vl_play_init()) exit(-1);

    gui_init( &argc, argv );

    if (cl_decomp_init()) exit(-1);
    if (cl_comp_init()) exit(-1);

    while(1) {
	while (clGetNextImageInfo(global.comp.handle, &imageInfo, 
		sizeof(CLimageInfo)) == CL_NEXT_NOT_AVAILABLE) {
	    sginap(1);
	}


	to_transfer = imageInfo.size;

	from_prewrap = clQueryValid( global.comp.buffer, 
	    to_transfer, (void **)&from_ptr, &ignored );

	to_prewrap = clQueryFree( global.decomp.buffer,
	    to_transfer, (void **)&to_ptr, &ignored );

	do_copy = MIN(to_prewrap, to_transfer);
	do_copy = MIN(from_prewrap, do_copy);
	bcopy( from_ptr, to_ptr, do_copy );

	clUpdateTail( global.comp.buffer, do_copy );
	clUpdateHead( global.decomp.buffer, do_copy );

	to_transfer -= do_copy;
	if (to_transfer == 0) continue;

	from_ptr += do_copy;
	to_ptr += do_copy;
	from_prewrap -= do_copy;
	to_prewrap -= do_copy;

	if (from_prewrap == 0) {
	    from_prewrap = clQueryValid( global.comp.buffer, 
		to_transfer, (void **)&from_ptr, &ignored );
	}

	if (to_prewrap == 0) {
	    to_prewrap = clQueryFree( global.decomp.buffer,
		to_transfer, (void **)&to_ptr, &ignored );
	}

	do_copy = MIN( to_prewrap, to_transfer );
	do_copy = MIN( from_prewrap, do_copy );
	bcopy( from_ptr, to_ptr, do_copy );

	clUpdateTail( global.comp.buffer, do_copy );
	clUpdateHead( global.decomp.buffer, do_copy );

	to_transfer -= do_copy;
	if (to_transfer == 0) continue;

	from_ptr += do_copy;
	to_ptr += do_copy;
	from_prewrap -= do_copy;
	to_prewrap -= do_copy;

	if (from_prewrap == 0) {
	    from_prewrap = clQueryValid( global.comp.buffer, 
		to_transfer, (void **)&from_ptr, &ignored );
	}

	if (to_prewrap == 0) {
	    to_prewrap = clQueryFree( global.decomp.buffer,
		to_transfer, (void **)&to_ptr, &ignored );
	}

	do_copy = MIN( to_prewrap, to_transfer );
	do_copy = MIN( from_prewrap, do_copy );
	bcopy( from_ptr, to_ptr, do_copy );

	clUpdateTail( global.comp.buffer, do_copy );
	clUpdateHead( global.decomp.buffer, do_copy );
    }
}
