/*
 * A simple program to verify that a DAT tape
 * has been correctly written.
 *
 * Doug Cook
 * Silicon Graphics, Inc., December 1993
 *
 * (c) Copyright 1993, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * Permission to use, copy, modify, and distribute this software for
 * any purpose and without fee is hereby granted, provided that the above
 * copyright notice appear in all copies and that both the copyright notice
 * and this permission notice appear in supporting documentation, and that
 * the name of Silicon Graphics, Inc. not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission.
 *
 * THE MATERIAL EMBODIED ON THIS SOFTWARE IS PROVIDED TO YOU "AS-IS"
 * AND WITHOUT WARRANTY OF ANY KIND, EXPRESS, IMPLIED OR OTHERWISE,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY OR
 * FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL SILICON
 * GRAPHICS, INC.  BE LIABLE TO YOU OR ANYONE ELSE FOR ANY DIRECT,
 * SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY
 * KIND, OR ANY DAMAGES WHATSOEVER, INCLUDING WITHOUT LIMITATION,
 * LOSS OF PROFIT, LOSS OF USE, SAVINGS OR REVENUE, OR THE CLAIMS OF
 * THIRD PARTIES, WHETHER OR NOT SILICON GRAPHICS, INC.  HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH LOSS, HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE
 * POSSESSION, USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * US Government Users Restricted Rights
 * Use, duplication, or disclosure by the Government is subject to
 * restrictions set forth in FAR 52.227.19(c)(2) or subparagraph
 * (c)(1)(ii) of the Rights in Technical Data and Computer Software
 * clause at DFARS 252.227-7013 and/or in similar or successor
 * clauses in the FAR or the DOD or NASA FAR Supplement.
 * Unpublished-- rights reserved under the copyright laws of the
 * United States.  Contractor/manufacturer is Silicon Graphics,
 * Inc., 2011 N.  Shoreline Blvd., Mountain View, CA 94039-7311.
 *
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/tpsc.h>
#include <sys/mtio.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <dmedia/dataudio.h>
#include <dmedia/audio.h>

int dat;            /* dat tape fd */
static char *datdev = "/dev/nrtape";
int testpat = 0;
int derrcnt = 0, terrcnt = 0;

int 
get_firmware_revision(int fd, int *maj, int *min)
{
    ct_g0inq_data_t info;
    char revbuf1[MAX_INQ_PRL + 1];

    if (ioctl(fd, MTSCSIINQ, &info) >= 0) {
        strncpy(revbuf1, (char *)info.id_prl, MAX_INQ_PRL);
        *maj = atoi(strtok(revbuf1, "."));
        *min = atoi(strtok(NULL, "."));
        return 0;
    }
    else {
        return -1;     /*ioctl failed */
    }
}

static DTFRAME dtf;
static int program, index;

int
rewind_dat(int fd)
{
    int i;
    struct mtop mtc;
    struct mtaudio mta;
    struct mtget mtg;

    /*
     * Get some information about the DAT media
     * We'll use this momentarily.
     */
    if (ioctl(fd, MTIOCGET, &mtg) < 0) {
        perror("Couldn't issue MTIOCGET");
        return(0);
    }

    /*
     * rewind the tape.
     * Note that a rewind does not block. The rewind goes on while
     * the program continues. The first write will block until
     * the rewind completes. 
     * We want the drive in audio mode here because BOT is different
     * between data and audio modes (data mode rewinds to logical
     * BOT, which is a little ways into the tape, and we want real
     * BOT). 
     */
    if ((mtg.mt_erreg & (CT_AUD_MED >> 16)) == 0) {
	printf("tape is not an audio tape\n");
        return 0;
    }
    mtc.mt_op = MTREW;
    mtc.mt_count = 1;
    if (ioctl(fd, MTIOCTOP, &mtc) < 0) {
        perror("MTREW failed");
	return 0;
    }
    return 1;
}

int
prepare_dat(int fd)
{
    struct mtop mtc;
    struct dttimecode *tcp;
    struct dttimepack *tpp;
    struct dttimepack *tpp1;

    /*
     * Put the DAT drive in audio mode. 
     */
    mtc.mt_op = MTAUD;
    mtc.mt_count = 1;
    if (ioctl(fd, MTIOCTOP, &mtc) < 0) {
        if (oserror() != EAGAIN) {
            perror("Couldn't put DAT into audio mode");
            return(0);
        }
    }
    return 1;
}

int
verify_dat(int fd)
{
    DTFRAME frame;
    struct dttimepack *tpp;
    struct dttimecode *tcp;
    struct dttimecode exp_ptime;
    struct dttimecode exp_atime;
    char *s1 = "--:--:--:--";
    char *s2 = "--:--:--:--";
    int program = -1;
    int printed = 0;
    int li = 0;				/* are we in lead-in area? */
    int x=1;
    int i1,i2,index = 0;
    int j,np;
    unsigned long y;
    unsigned long *p;
 
    while(x) {
	if ((x = read(fd,&frame,sizeof(DTFRAME))) < 0) {
	    perror("read failed");
	    return 0;
	}

        /*
         * get the program number from the frame
         */
        if (frame.subcode.sid.pno3 != 0xa
            && frame.subcode.sid.pno3 != 0xb
            && frame.subcode.sid.pno3 != 0xe) {
            np = DTpnotodec(frame.subcode.sid.pno1,
                frame.subcode.sid.pno2, frame.subcode.sid.pno3);
	    if (np != program) {
	        printf("\nprogram %d   \n",np);
	        bzero(&exp_ptime,sizeof(struct dttimecode));
		program = np;
	    }
        }
	else {
	    if (!li && frame.subcode.sid.pno3 == 0xb) {
		li = 1;
		printf("lead-in...\n");
	    }
	    if (frame.subcode.sid.pno3 == 0xe) {
		printf("\nlead-out...\n");
		return 1;
	    }
	    continue;
	}
        tpp = (struct dttimepack *)&frame.subcode.packs[DTP_PTIME-1];
	tcp = &(tpp->tc);
	if ((i1 = DTbcdtodec(tpp->index.dhi,tpp->index.dlo)) < 100) {
	    if (i1 != index) {
	        index = i1;
	        printf("\nindex %d   \n",i1);
	    }
	}
        if (DTtcvalid(tcp)) {
	    if (bcmp(tcp,&exp_ptime,sizeof(struct dttimecode))) {
		DTtimetoa(s1,&exp_ptime);
		DTtimetoa(s2,tcp);
	        printf("unexpected ptime: wanted %s got %s\n",s1,s2);
		terrcnt++;
		exp_ptime = *tcp;
	    }
        }
	else {
	    printf("invalid ptime\n");
	}
        tpp = (struct dttimepack *)&frame.subcode.packs[DTP_ATIME-1];
	tcp = &(tpp->tc);
	if ((i2 = DTbcdtodec(tpp->index.dhi,tpp->index.dlo)) < 100) {
	    index = i2; 
	}
        if (DTtcvalid(tcp)) {
	    if (bcmp(tcp,&exp_atime,sizeof(struct dttimecode))) {
		DTtimetoa(s1,&exp_atime);
		DTtimetoa(s2,tcp);
	        printf("unexpected atime: wanted %s got %s\n",s1,s2);
		terrcnt++;
		exp_atime = *tcp;
	    }
        }
	else {
	    printf("invalid atime\n");
	}
	DTtimetoa(s1,tcp);
	printf("%s",s1);
/*
	printf("%s\n",s1);
*/
	fflush(stdout);
	if (i1 != i2) {
	    printf("index numbers mismatch between atime and ptime\n");
	    terrcnt++;
	}
	DTinctime(&exp_atime);
	if (index != 0) {
	    /* index 0 ptime should stay at 0 */
	    DTinctime(&exp_ptime);
	}
	if (testpat && index != 0) {
	    /*
	     * attempt to match test-pattern on tape
	     */
            tpp = (struct dttimepack *)&frame.subcode.packs[DTP_ATIME-1];
            tcp = &tpp->tc;
            y = (tcp->hhi << 28) | (tcp->hlo << 24)
              | (tcp->mhi << 20) | (tcp->mlo << 16)
              | (tcp->shi << 12) | (tcp->mlo << 8)
              | (tcp->fhi << 4) | tcp->flo;
            p = (unsigned long *) &frame.audio;
            for (j = 0; j < DTDA_DATASIZE; j += sizeof(long)) {
                if (*p != y) {
	 	    if (!printed) {
		        printf("data error: got 0x%x at pos %d, wanted 0x%x\n",
				*p, j, y);
			printed=1;
		    }
		    derrcnt++;
		};
		p++;
		y++;
            }
	}
	printed=0;
    }
    return 1;
}

main(int argc, char **argv)
{
    int status;
    int n = 1;
    int i;
    int excl_id;            /* id as returned by mediad */
    int maj, min;            /* dat firmware rel major&minor # */

    if (argc>2) {
	fprintf(stderr, "usage: %s [-t]\n",argv[0]);
	exit(-1);
    }
    if (argc == 2) {
	if (!strcmp(argv[1],"-t")) {
	    printf("Checking test-patterns...\n");
	    testpat=1;
	}
	else {
	    fprintf(stderr, "usage: %s [-t]\n",argv[0]);
	    exit(-1);
	}
    }
	
    /*
     * Ask mediad for exclusive access to the DAT drive.
     */
    excl_id = mediad_get_exclusiveuse(datdev, argv[0]);

    /*
     * Open the DAT drive
     */
    dat = open(datdev, O_RDONLY);
    if (dat < 0) {
        perror("Could not open DAT drive:");
        exit(-1);
    }


    if (get_firmware_revision(dat, &maj, &min) < 0) {
        fprintf(stderr,"Couldn't get DAT firmware revision\n");
        exit(-1);
    }
    /* pre-2.63 DAT drives don't work with audio */
    if (maj < 2 || (maj == 2 && min < 63)) {
        fprintf(stderr,"DAT firmware rev %d.%d is too old -- you must have 2.63 or greater\n",maj,min);
        exit(-1);
    }

    if (prepare_dat(dat) < 0) {
	exit(-1);
    }
    if (rewind_dat(dat) < 0) {
	exit(-1);
    }
    if (verify_dat(dat) < 0) {
	exit(-1);
    }
    printf("%d timecode errors, %d data errors\n",terrcnt,derrcnt);
}
