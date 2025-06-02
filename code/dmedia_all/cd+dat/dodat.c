/*
 * A simple program to build a DAT tape 
 * from disk files.
 *
 * This program accepts a simple scripting language to
 * decide where to place files onto DAT, how much silence
 * to insert between tracks, etc.
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
 *
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/tpsc.h>
#include <sys/mtio.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <dmedia/dataudio.h>
#include <dmedia/audio.h>
#include <dmedia/audiofile.h>
#include <stdio.h>

#define OP_READ		1
#define OP_WRITE 	2
#define OP_SEEK 	4

int dat;            /* dat tape fd */

static char *datdev = "/dev/nrtape";

/*
 * tape_pos_unknown indicates that the current value of timecode(s) and
 * program number/index number on the tape (i.e. the current position) is not 
 * known. This is normally the case immediately following a seek 
 * operation (for example, if you seek by a-time, the ptime and program number
 * are unknown). We need to know the position before we can do a write, so
 * we can put the correct timecodes and program information in the frames we 
 * write. 
 */
static int tape_pos_unknown = 1;
/*
 * last_cmd stores the last command that was issued to the tape. In theory,
 * this should not matter. In practice, however, we need to work around a
 * DAT firmware bug. Writes which immediately follow a read do not append
 * correctly. Writes which immediately follow a seek function perfectly.
 * If the previous command was a read, we must therefore issue a
 * seek before we can do a write. 
 */
static int last_cmd = OP_SEEK;

static int startidframes = 0;
static int verbose = 1;		/* default is VERBOSE mode */
static int samps_per_frame = DTDA_NUMSAMPS44K;
static int program = 1, index = 1;
int smart = 1;		/* smart = 1 means try to workaround read->write bug */

/*
 * outframe is our "template" frame. We modify the fields we need and
 * insert the appropriate audio each time we write a frame to the tape.
 */
static DTFRAME outframe;
/*
 * inframe is the last frame that we read from the tape. We can use this
 * to determine our position if last_cmd = OP_READ.
 */
static DTFRAME inframe;

/*
 * we include our own DTframetohmsf because IRIX versions prior to
 * and including 5.2 have a slight bug in this function. We also include
 * DTframetotc since it depends upon correct functioning of frametohmsf.
 */

void
myDTframetohmsf(unsigned long fr, int *h,int *m, int *s, int *f)
{
    unsigned long mrem,hrem;
    *h = fr / 120000;
    hrem = fr % 120000;
    *m = hrem / 2000;
    mrem = hrem % 2000;
    *s = (2+(mrem * 3)) / 100;
    *f = mrem - (100 * *s) / 3;
}

void
myDTframetotc(unsigned long fr, struct dttimecode* tc)
{
    int h,m,s,f;
    myDTframetohmsf(fr,&h,&m,&s,&f);
    /*
     * now set the msf fields in the timecode. These are in BCD;
     */
    tc->hhi = h / 10;
    tc->hlo = h % 10;
    tc->mhi = m / 10;
    tc->mlo = m % 10;
    tc->shi = s / 10;
    tc->slo = s % 10;
    tc->fhi = f / 10;
    tc->flo = f % 10;
}

int 
get_firmware_revision(int fd, int *maj, int *min)
{
    ct_g0inq_data_t info;
    char revbuf1[MAX_INQ_PRL + 1];

    /*
     * get the DAT drive firmware revision.
     */
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

void
reset_timecodes()
{
    struct dttimecode *tcp;
    struct dttimepack *tpp;
    struct dttimepack *tpp1;

    /*
     * set up the subcode bits
     */
    tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_ATIME-1];
    bzero(&tpp->tc,sizeof(struct dttimecode));
    tpp->id = DTP_ATIME;

    tpp->index.dhi = 0;
    tpp->index.dlo = 1;
    tpp->pno1 = outframe.subcode.sid.pno1 = 0;
    tpp->pno2 = outframe.subcode.sid.pno2 = 0;
    tpp->pno3 = outframe.subcode.sid.pno3 = 1;
    tpp1 = (struct dttimepack *)&outframe.subcode.packs[DTP_PTIME-1];
    *tpp1 = *tpp;
    tpp1->id = DTP_PTIME;

    /* numpacks tells how many packs are written onto the DAT */
    outframe.subcode.sid.numpacks = 2;	/* ATIME is 2, PTIME is 1 */
}

void
copy_prohibit(int p)
{
    outframe.subcode.mid.copy = (p == 0) ? DTM_COPY_PERMITTED : DTM_COPY_PROHIBITED;
}

int
write_eot(int fd)
{
    int i;
    struct dttimepack *tpp;
    struct dttimecode *tcp;

    if (verbose) {
        printf("writing lead-out...");
        fflush(stdout);
    }

    /*
     * Set the lead-out subcodes in our template frame
     */
    outframe.subcode.sid.pno1 = 0;
    outframe.subcode.sid.pno2 = outframe.subcode.sid.pno3 = 0xE;   /* EOT */
    outframe.subcode.sid.ctrlid =  DTS_START;

    /*
     * Mark the timecodes as invalid. 
     */
    tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_ATIME-1];
    tcp = &tpp->tc;
    tcp->hhi = tcp->hlo = 0;
    tcp->mhi = tcp->mlo = tcp->shi = tcp->slo = tcp->fhi = tcp->flo = 0xA;
    tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_PTIME-1];
    tcp = &tpp->tc;
    tcp->hhi = tcp->hlo = 0;
    tcp->mhi = tcp->mlo = tcp->shi = tcp->slo = tcp->fhi = tcp->flo = 0xA;

    /*
     * zero the audio data in the template frame
     */
    bzero(&outframe.audio,DTDA_DATASIZE);

    /*
     * Write this dummy frame to the tape to make the trailer
     * DAT spec sez lead-out must be >=300 frames
     */
    for (i = 0; i < 300; i++) {
        if ((write(fd, &outframe, sizeof(DTFRAME))) < 0) {
            perror("tape write failed on trailer");
            return 0;
        }
    }

    if (verbose) {
        printf("done.\n");
    }
}

/*
 * Due to a bug in the DAT firmware, the following method of getting
 * time from the tape does NOT work. However, reading a frame and decoding
 * the timecode works fine. 
 */
int
get_wrongDATtime(fd)
{
    struct mtaudio mt_aud;
    int pn;
    char *s = "--:--:--:--";

    if (ioctl(fd, MTGETAUDIO, &mt_aud) >= 0) {
	pn = DTpnotodec(mt_aud.pno1, mt_aud.pno2, mt_aud.pno3);

	DTtimetoa(s,&mt_aud.atime);
	printf("DAT pos (read position) prog %d atime %s",pn,s);
	DTtimetoa(s,&mt_aud.ptime);
	printf(" ptime %s\n",s);
    }
}

int
seek_time(int fd,int atime,int h,int m, int s, int f)
{
    struct mtaudio mt_aud;
    struct mtget mt_get;

    mt_aud.zero1 = 0;	/* one must clear the zero fields, or pno is invalid */
    mt_aud.zero2 = 0;
    mt_aud.zero3 = 0;

    /* convert hmsf to tc using two DAT lib calls. */
    if (atime) {
        char *st = "--:--:--:--";
        mt_aud.seektype = MTAUDPOSN_ABS;
        myDTframetotc(DThmsftoframe(h,m,s,f),&mt_aud.atime);
        if (verbose) {
	    DTtimetoa(st,&mt_aud.atime);
    	    printf("seeking to atime %s... ",st);
            fflush(stdout);
        }
    }
    else {
        char *st = "--:--:--:--";
        mt_aud.seektype = MTAUDPOSN_PTIME;
        myDTframetotc(DThmsftoframe(h,m,s,f),&mt_aud.ptime);
        if (verbose) {
	    DTtimetoa(st,&mt_aud.ptime);
    	    printf("seeking to ptime %s... ",st);
            fflush(stdout);
        }
    }


    /*
     * start the seek.
     */
    if (ioctl(fd, MTSETAUDIO, &mt_aud) < 0) {
        perror("DAT seek failed");
        return 0;
    }

    tape_pos_unknown = 1;
    last_cmd = OP_SEEK;

    /*
     * wait for the seek to complete.
     */
    do {
	if (ioctl(fd, MTIOCGET, &mt_get) < 0) {
	    perror("DAT seek failed");
	    return 0;
	}
    } while (mt_get.mt_erreg & (CT_SEEKING >> 16));

    if (verbose) {
	printf("done\n");
    }

    return 1;
}

int
seek_to_last_read(int fd, int writeback)
{
    struct mtaudio mt_aud;
    struct mtget mt_get;
    struct dttimepack *tpp;

    mt_aud.seektype = MTAUDPOSN_ABS;
    tpp = (struct dttimepack *)&inframe.subcode.packs[DTP_ATIME-1];
    mt_aud.atime = tpp->tc;

    if (verbose) {
        char *st = "--:--:--:--";
        DTtimetoa(st,&mt_aud.atime);
	printf("seeking to atime %s... ",st);
        fflush(stdout);
    }

    /*
     * seek to the location of the frame, since reading the position
     * moved the tape.
     */
    if (ioctl(fd, MTSETAUDIO, &mt_aud) < 0) {
        perror("DAT seek failed");
        return 0;
    }

    /*
     * wait for the seek to complete.
     */
    do {
	if (ioctl(fd, MTIOCGET, &mt_get) < 0) {
	    perror("DAT seek failed");
	    return 0;
	}
    } while (mt_get.mt_erreg & (CT_SEEKING >> 16));

    sleep(1);
    tape_pos_unknown = 0;
    last_cmd = OP_SEEK;

    if (writeback) {
	/*
	 * in some cases we need to write back the last-read
	 * frame. This occurs when we want to append to the tape:
	 * we have read the last good frame on the tape, we need to
	 * seek back to the beginning of that frame, write it back,
	 * and then write the new data. This tries to work around the 
	 * back-to-back read/write firmware bug.
	 */
        last_cmd = OP_WRITE;
	if (verbose) printf("writing back frame...\n");
        if ((write(fd, &inframe, sizeof(DTFRAME))) < 0) {
	    perror("can't write back last read frame:");
	    return 0;
	}
    }
    return 1;
}

int
get_tape_pos(int fd, int reseek)
{
    struct mtaudio mt_aud;
    struct mtget mt_get;
    struct dttimepack *tpp;
    struct dttimepack *tpp1;
    struct dttimecode *tcp;
    struct dttimecode *tcp1;
    int i;
    int need_writeback = 1;

    /*
     * if we just did a read, we don't need a new frame of data.
     * Otherwise, to figure out where we are, we must read a frame.
     * reseek set to 1 will force a read (and later re-seek to the
     * beginning of the frame it just read).
     */
    if (reseek == 0 || last_cmd != OP_READ) {
        /*
         * Now read the timecodes, program, index, etc.
         * Alas! to do this accurately we must move the tape.
         * We will therefore have to seek again after we do this.
         */
	if (verbose) printf("reading a frame...\n");
        if ((read(fd, &inframe, sizeof(DTFRAME))) < 0) {
            perror("tape read failed");
            return 0;
        }
	need_writeback = 0;
	last_cmd = OP_READ;
    }
    else if (verbose) printf("using the frame I just read...\n");

    /*
     * get the program number from the frame
     */
    switch(inframe.subcode.sid.pno3) {
	case 0xa :
	    /* unused program number -- ignore */
	    break;
	case 0xb :
	    /* bot */
	    fprintf(stderr, "at BOT\n");
	    return 0;
	case 0xe :
	    /* eot */
	    fprintf(stderr, "at EOT\n");
	    return 0;
	default:
            program = DTpnotodec(inframe.subcode.sid.pno1, 
	        inframe.subcode.sid.pno2, inframe.subcode.sid.pno3);

	    outframe.subcode.sid.pno1 = inframe.subcode.sid.pno1;
	    outframe.subcode.sid.pno2 = inframe.subcode.sid.pno2;
	    outframe.subcode.sid.pno3 = inframe.subcode.sid.pno3;
	    break;
    }
    tpp1 = (struct dttimepack *)&inframe.subcode.packs[DTP_PTIME-1];
    if (DTtcvalid(&(tpp1->tc))) {
        tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_PTIME-1];
	*tpp = *tpp1;
    }
    else {
	fprintf(stderr,"No valid ptime found at location -- setting ptime to 0\n");
        tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_PTIME-1];
        tpp1 = (struct dttimepack *)&inframe.subcode.packs[DTP_PTIME-1];
	tpp->pno1 = tpp1->pno1;
	tpp->pno2 = tpp1->pno2;
	tpp->pno3 = tpp1->pno3;
	tpp->index = tpp1->index;
	bzero(&tpp->tc,sizeof(struct dttimecode));
    }
    if ((i = DTbcdtodec(tpp1->index.dhi,tpp1->index.dlo)) < 100) {
	index = i;
    }
    tpp1 = (struct dttimepack *)&inframe.subcode.packs[DTP_ATIME-1];
    if (DTtcvalid(&(tpp1->tc))) {
        tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_ATIME-1];
	*tpp = *tpp1;
    }
    else {
	fprintf(stderr,"No valid atime found at location\n");
        tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_PTIME-1];
        tpp1 = (struct dttimepack *)&inframe.subcode.packs[DTP_PTIME-1];
	tpp->pno1 = tpp1->pno1;
	tpp->pno2 = tpp1->pno2;
	tpp->pno3 = tpp1->pno3;
	tpp->index = tpp1->index;
	/*
	 * If there's no valid A-time then we fail. We need valid
	 * a-time, if not just for re-seeking to this frame.
	 */
        return 0;
    }
    if ((i = DTbcdtodec(tpp1->index.dhi,tpp1->index.dlo)) < 100) {
	index = i;
    }
    if (verbose) {
        char *str="--:--:--:--";
        tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_ATIME-1];
	DTtimetoa(str,&tpp->tc);
	printf("DAT pos (read frame): prog %d index %d atime %s",program,index,str);
        tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_PTIME-1];
	DTtimetoa(str,&tpp->tc);
	printf(" ptime %s\n",str);
    }

    if (smart == 0 || reseek == 0 || need_writeback) {
	if (verbose) {
	    printf("correcting time for moved tape...\n");
	}
	/*
	 * if we are not going to move the tape back to where it was,
	 * at least adjust the time to reflect the correct timecode where
	 * we are.
	 */
        tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_ATIME-1];
        tcp = &tpp->tc;
        DTinctime(tcp);
        /*
         * The DAT spec states that when index=0, ptime stays
         * at 0. We therefore only increment for a nonzero index.
         */
        if (index != 0) {
            tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_PTIME-1];
            tcp = &tpp->tc;
            DTinctime(tcp);
        }
    }

    return (reseek && smart) ? seek_to_last_read(fd,need_writeback) : 1;
}

/*
 * set_atime
 *
 * This allows you to set the atime to be written to the tape. Use this
 * with caution. In most cases, you want the program to figure out what
 * atime to write by making it contiguous with what's already on the tape.
 * Discontinuous atimes make for an unhappy tape.
 */
void
set_atime(int h, int m, int s, int f)
{
    struct dttimepack *tpp;
    struct dttimecode *tcp;

    tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_ATIME-1];
    tcp = &(tpp->tc);

    /* warning! no validity check here */
    myDTframetotc(DThmsftoframe(h,m,s,f),tcp);
    if (verbose) {
        char *str="--:--:--:--";
	DTtimetoa(str,tcp);
	printf("set atime to %s\n",str);
    }
    tape_pos_unknown = 0;	/* we "know" where we are now, we just set it*/
}

void
set_ptime(int h, int m, int s, int f)
{
    struct dttimepack *tpp;
    struct dttimecode *tcp;

    tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_PTIME-1];
    tcp = &(tpp->tc);

    /* warning! no validity check here */
    myDTframetotc(DThmsftoframe(h,m,s,f),tcp);
    if (verbose) {
        char *str="--:--:--:--";
	DTtimetoa(str,tcp);
	printf("set ptime to %s\n",str);
    }
}

int
seek_prog(int fd,int p)
{
    struct mtaudio mt_aud;
    struct mtget mt_get;
    int pt;

    if (p == 0) {
	p = program;
    }

    if (verbose) {
	printf("seeking to program %d ... ", p);
        fflush(stdout);
    }

    if (p > 799 || p < 0) {
	fprintf(stderr,"Invalid location: program %d\n",p);
	return 0;
    }

    mt_aud.zero1 = 0;	/* one must clear the zero fields, or pno is invalid */
    mt_aud.zero2 = 0;
    mt_aud.zero3 = 0;
    mt_aud.pno1 = p / 100;
    pt = p % 100;
    mt_aud.pno2 = pt / 10;
    mt_aud.pno3 = pt % 10;
    mt_aud.indexhi = 0;	/* seek ignores index */
    mt_aud.index = 0;
    mt_aud.seektype = MTAUDPOSN_PROG;

    /*
     * start the seek.
     */
    if (ioctl(fd, MTSETAUDIO, &mt_aud) < 0) {
        perror("DAT seek failed");
        return 0;
    }

    tape_pos_unknown = 1;
    last_cmd = OP_SEEK;

    /*
     * wait for the seek to complete.
     */
    do {
	if (ioctl(fd, MTIOCGET, &mt_get) < 0) {
	    perror("DAT query failed");
	    return 0;
	}
    } while (mt_get.mt_erreg & (CT_SEEKING >> 16));


    if (verbose) {
	printf("done\n");
    }

    return 1;
}

void
seek_eot(int fd)
{
}

int
seek_bot(int fd)
{
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

    if (verbose) {
        printf("rewinding...");
        fflush(stdout);
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
    if ((mtg.mt_erreg & (CT_AUD_MED >> 16)) == 0 && 
        (mtg.mt_dsreg & CT_BOT)) {
        /*
         * firmware bug workaround. Tapes which have previously 
         * been written with non-audio data are considered "data
         * tapes." Their format differs from that of audio tapes. 
         * If the tape in the drive  is considered a data tape, and
         * the drive thinks it's at BOT, we want to move it off 
         * BOT so that the drive will really rewind it when we issue  
         * MTREW. Can't do a read, since a read of a data tape in 
         * audio mode will fail. The write will force the medium to  
         * become an audio tape.
         */
	if (verbose)
            printf("fixing a data tape at BOT to be an audio tape at BOT\n");
        write(fd,&outframe,sizeof(DTFRAME));
    }
    mtc.mt_op = MTREW;
    mtc.mt_count = 1;
    ioctl(fd, MTIOCTOP, &mtc);
    if (verbose) {
	printf("done.\n");
    }

    tape_pos_unknown = 0;
    last_cmd = OP_SEEK;
    reset_timecodes();
}

int
write_bot(int fd)
{
    int i;
    struct dttimepack *tpp;
    struct dttimecode *tcp;

    if (verbose) {
        printf("writing lead-in...");
        fflush(stdout);
    }

    /*
     * Set the lead-in subcodes in our template frame
     * Program no. of 0xB denotes lead-in.
     */
    outframe.subcode.sid.pno1 = 0;
    outframe.subcode.sid.pno2 = outframe.subcode.sid.pno3 = 0xB;   /* BOT */
    outframe.subcode.sid.ctrlid =  DTS_START;

    /*
     * Mark the timecodes as invalid. 
     */
    tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_ATIME-1];
    tcp = &tpp->tc;
    tcp->hhi = tcp->hlo = 0;
    tcp->mhi = tcp->mlo = tcp->shi = tcp->slo = tcp->fhi = tcp->flo = 0xA;
    tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_PTIME-1];
    tcp = &tpp->tc;
    tcp->hhi = tcp->hlo = 0;
    tcp->mhi = tcp->mlo = tcp->shi = tcp->slo = tcp->fhi = tcp->flo = 0xA;
   
    /* clear the audio in the template frame */
    bzero(&outframe.audio,DTDA_DATASIZE);

    /*
     * Write the template frame to the tape to make the leader
     */
    for (i = 0; i < 100; i++) {
        if ((write(fd, &outframe, sizeof(DTFRAME))) < 0) {
            perror("tape write failed on leader");
            return 0;
        }
    }

    last_cmd = OP_WRITE;
    tape_pos_unknown = 0;
    reset_timecodes();		/* set timecodes to 0:0:0:0 and pno to 1 */

    if (verbose) {
        printf("done.\n");
    }
}

void
set_index(int i)
{
    struct dttimepack *tpp;

    index = i;
    if (verbose) {
        printf("setting index to %d\n",index);
    }

    /*
     * set up the subcode bits
     */
    tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_ATIME-1];
    tpp->index.dhi = index / 10;
    tpp->index.dlo = index % 10;
    tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_PTIME-1];
    tpp->index.dhi = index / 10;
    tpp->index.dlo = index % 10;
    /* ...and we don't bother with r-time */
}


void
set_program(int p)
{ 
    int rem;
    struct dttimepack *tpp;
    struct dttimecode *tcp;

    if (verbose) {
        printf("setting program to %d\n",p);
    }

    program = p;

    /*
     * set up the subcode bits
     * first we do a-time
     */
    tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_ATIME-1];

    tpp->pno1 = outframe.subcode.sid.pno1 = p / 100;
    rem = p % 100;
    tpp->pno2 = outframe.subcode.sid.pno2 = rem / 10;
    tpp->pno3 = outframe.subcode.sid.pno3 = rem % 10;

    /* 
     * now we do p-time 
     * we zero p-time when we change program numbers.
     * The DAT spec sez that program time is set to zero at start-of-
     * program, and increments throughout the program. The start-of-
     * program is where the first nonzero index occurs within the program.
     */
    tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_PTIME-1];
    tcp = &tpp->tc;

    tcp->hhi = 0;
    tcp->hlo = 0;
    tcp->mhi = 0;
    tcp->mlo = 0;
    tcp->shi = 0;
    tcp->slo = 0;
    tcp->fhi = 0;
    tcp->flo = 0;
    tpp->pno1 = outframe.subcode.sid.pno1;
    tpp->pno2 = outframe.subcode.sid.pno2;
    tpp->pno3 = outframe.subcode.sid.pno3;

    /* ...and we don't bother with r-time */

    /*
     * now we set it up so that the next 300 frames
     * have the start id bit set, indicating start-of-program.
     * DAT spec sez START must be set for 300 frames at start-of-program
     * and PRIORITYID must be set whenever program # is valid.
     */
    outframe.subcode.sid.ctrlid |= (DTS_PRIORITYID | DTS_START);
    startidframes = 300;
}

void
inc_program()
{
    set_program(program+1);
}

void
inc_index()
{
    set_index(index+1);
}

void
fix_parity()
{
    volatile unchar *pp;
    int i;

    /*
     * calculate the parity bytes for the packs we are using.
     */
    for(i = 0; i<outframe.subcode.sid.numpacks; i++) {
        pp = (volatile unchar *)&outframe.subcode.packs[i];
        outframe.subcode.packs[i].parity
            = pp[0] ^ pp[1] ^ pp[2] ^ pp[3] ^ pp[4] ^ pp[5] ^ pp[6];
    }
}

void
increment_time()
{
    struct dttimepack *tpp;
    struct dttimecode *tcp;

    tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_ATIME-1];
    tcp = &tpp->tc;
    DTinctime(tcp);
    /*
     * The DAT spec states that when index=0, ptime stays
     * at 0. We therefore only increment for a nonzero index.
     */
    if (index != 0) {
        tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_PTIME-1];
        tcp = &tpp->tc;
        DTinctime(tcp);
    }

    if (startidframes > 0) {
        startidframes--;
        if(startidframes==0) {
            /* shut off start ID bit after startidframes */
            outframe.subcode.sid.ctrlid &= ~DTS_START;
        }
    }
}

void
pause()
{
    char x[80];

    printf("Program paused. Hit return to continue.\n");
    gets(x);
}

void
swapcopy(ushort *a, ushort *b, int n)
{
    ushort *done = a + n;
    while(a != done) {
        /* DAT audio frames must have the audio byte-swapped */
        *b++ = ((*a & 0xff) << 8)+(*a >> 8);
        a++;
    }
}

int
read_file(int fd, char *filename, int endprog, int h, int m, int s, int f)
{
    AFfilehandle file=0;
    AFfilesetup fset;
    int sz = 0,once=0;
    int i;
    struct dttimepack *tpp;
    struct dttimecode *tcp;


    if (verbose) {
	printf("writing to %s ...",filename);
	fflush(stdout);
    }

    while (1) {
        if ((read(fd, &inframe, sizeof(DTFRAME))) < 0) {
            perror("tape write failed");
            return 0;
	}
        last_cmd = OP_READ;
        tape_pos_unknown = 1;

        /*
         * get the program number from the frame
         */
        switch(inframe.subcode.sid.pno3) {
	    case 0xa :
	        /* unused program number -- ignore */
	        break;
	    case 0xb :
	        /* bot */
	        if (!once && verbose) {
		    printf("skipping BOT...");
		    fflush(stdout);
	 	}
		once=1;
		continue;
	    case 0xe :
	        /* eot */
	        if (verbose) printf("stopped at EOT\n");
		if (file) AFclosefile(file);
	        return 0;
	    default:
                program = DTpnotodec(inframe.subcode.sid.pno1, 
	            inframe.subcode.sid.pno2, inframe.subcode.sid.pno3);
    
	        outframe.subcode.sid.pno1 = inframe.subcode.sid.pno1;
	        outframe.subcode.sid.pno2 = inframe.subcode.sid.pno2;
	        outframe.subcode.sid.pno3 = inframe.subcode.sid.pno3;
		if (endprog && program == endprog) {
		    if (file) AFclosefile(file);
		    if (verbose) printf("found program %d, stopping\n",endprog);
		    return 0;
		}
	        break;
        }
	if (endprog == 0 ) {
            tpp = (struct dttimepack *)&inframe.subcode.packs[DTP_ATIME-1];
	    tcp = &tpp->tc;
            if (DTtcvalid(tcp)) {
	        if (f == (tcp->fhi * 10 + tcp->flo) && 
		    s == (tcp->shi * 10 + tcp->slo) &&
		    m == (tcp->mhi * 10 + tcp->mlo) &&
		    h == (tcp->hhi * 10 + tcp->hlo)) {
		    if (file) AFclosefile(file);
		    if (verbose) printf("found timecode %d:%d:%d:%d, stopping\n",h,m,s,f);
		    return 0;
		}
	    }
	}
     
	if (!file) {
    	    fset = AFnewfilesetup();
    	    AFinitchannels(fset,AF_DEFAULT_TRACK,2);
    	    switch (inframe.subcode.mid.sampfreq) {
		case DT_FREQ32000 :
		    sz = DTDA_DATASIZE32K/sizeof(short);
                    AFinitrate(fset,AF_DEFAULT_TRACK,32000);
 	            break;
	        case DT_FREQ44100 :
		    sz = DTDA_DATASIZE44K/sizeof(short);
                    AFinitrate(fset,AF_DEFAULT_TRACK,44100);
 	            break;
		case DT_FREQ48000 :
		    sz = DTDA_DATASIZE48K/sizeof(short);
            	    AFinitrate(fset,AF_DEFAULT_TRACK,48000);
 	    	    break;
    	    }
	
    	    file = AFopenfile(filename, "w", fset);
	    AFfreefilesetup(fset);
    	    if (file == 0) {
		fprintf(stderr,"Couldn't open output file %s\n",filename);
		return 0;
    	    }
	}

	swapcopy((ushort *)&inframe.audio,(ushort *)&inframe.audio,sz);
        if (AFwriteframes(file, AF_DEFAULT_TRACK, &inframe.audio,
	    sz / 2) != sz/2) {
            fprintf(stderr,"file write failed (is your disk full?)\n");
            return 0;
	}
	
    }

    if (verbose) {
	printf("done.\n");
    }
}

int
write_file(int fd, char *filename)
{
    AFfilehandle file;
    int i;

    file = AFopenfile(filename, "r", 0);
    if (file == 0) {
	fprintf(stderr,"Couldn't open input file %s\n",filename);
	return 0;
    }
    if (AFgetchannels(file,AF_DEFAULT_TRACK) != 2) {
	fprintf(stderr,"Must be a stereo file\n");
	return 0;
    }

    if (tape_pos_unknown) {
	if (verbose) {
	    printf("getting tape position...");
            fflush(stdout);
	}
	if (get_tape_pos(fd,1) == 0) {
	    fprintf(stderr,"Couldn't get tape position: silence failed\n");
	    return 0;
	}
    }
    if (last_cmd == OP_READ && smart) {
	/* 
	 * workaround a firmware bug. writes can't immediately follow
	 * reads, so we seek to the location of the last read, and 
	 * write it back to the tape, then continue.
	 */
	seek_to_last_read(fd,1);
    }

    if (verbose) {
	printf("writing %s ...",filename);
	fflush(stdout);
    }

    while (i = AFreadframes(file, AF_DEFAULT_TRACK, &outframe.audio,
	samps_per_frame / 2)) {
	if (i) {
            fix_parity();
	    if (i < samps_per_frame / 2) {
	        bzero(((short *)&outframe.audio)+2*i,samps_per_frame - 2 * i);
	    }
	    swapcopy((ushort *)&outframe.audio,(ushort *)&outframe.audio,2*i);
            if ((write(fd, &outframe, sizeof(DTFRAME))) < 0) {
                perror("tape write failed");
                return 0;
	    }
	}
        last_cmd = OP_WRITE;
        tape_pos_unknown = 0;
	increment_time();
    }

    if (verbose) {
	printf("done.\n");
    }
}

int
write_testpat(int fd, int nframes)
{
    unsigned long i,j;
    struct dttimecode *tcp;
    struct dttimepack *tpp;
    unsigned long x;
    unsigned long *p;

    if (verbose) {
        printf("writing %d frames of test-pattern...",nframes);
        fflush(stdout);
    }

    if (tape_pos_unknown) {
	if (verbose) {
	    printf("getting tape position...");
            fflush(stdout);
	}
	if (get_tape_pos(fd,1) == 0) {
	    fprintf(stderr,"Couldn't get tape position: test-pattern failed\n");
	    return 0;
	}
    }
    if (last_cmd == OP_READ && smart) {
	/* 
	 * workaround a firmware bug. writes can't immediately follow
	 * reads, so we seek to the location of the last read, and 
	 * write it back to the tape, then continue.
	 */
	seek_to_last_read(fd,1);
    }

    for (i = 0; i < nframes; i++) {
        fix_parity();
	/*
	 * make the data a function of timecode & position within frame.
	 * this allows us to verify the data on a subsequent read.
	 */
        tpp = (struct dttimepack *)&outframe.subcode.packs[DTP_ATIME-1];
        tcp = &tpp->tc;
	x = (tcp->hhi << 28) | (tcp->hlo << 24) 
	  | (tcp->mhi << 20) | (tcp->mlo << 16) 
	  | (tcp->shi << 12) | (tcp->mlo << 8) 
	  | (tcp->fhi << 4) | tcp->flo;
	p = (unsigned long *) &outframe.audio;
	for (j = 0; j < DTDA_DATASIZE; j += sizeof(long)) {
	    *p++ = x++;
	}

        if ((write(fd, &outframe, sizeof(DTFRAME))) < 0) {
            perror("tape write failed");
            return 0;
        }
        last_cmd = OP_WRITE;
        tape_pos_unknown = 0;
	increment_time();
    }


    if (verbose) {
        printf("done.\n");
    }
}

int
write_silence(int fd, int nframes)
{
    unsigned long i;

    if (verbose) {
        printf("writing %d frames of silence...",nframes);
        fflush(stdout);
    }

    if (tape_pos_unknown) {
	if (verbose) {
	    printf("getting tape position...");
            fflush(stdout);
	}
	if (get_tape_pos(fd,1) == 0) {
	    fprintf(stderr,"Couldn't get tape position: silence failed\n");
	    return 0;
	}
    }
    if (last_cmd == OP_READ && smart) {
	/* 
	 * workaround a firmware bug. writes can't immediately follow
	 * reads, so we seek to the location of the last read, and 
	 * write it back to the tape, then continue.
	 */
	seek_to_last_read(fd,1);
    }

    /* set the audio in the template frame to zero */
    bzero(&outframe.audio,DTDA_DATASIZE);

    for (i = 0; i < nframes; i++) {
        fix_parity();
        if ((write(fd, &outframe, sizeof(DTFRAME))) < 0) {
            perror("tape write failed");
            return 0;
        }
        last_cmd = OP_WRITE;
        tape_pos_unknown = 0;
	increment_time();
    }


    if (verbose) {
        printf("done.\n");
    }
}

void
set_rate(int r)
{
    /*
     * set up the rate bits
     */
    if (verbose) {
	printf("setting rate to %d\n",r);
    }
    switch(r) {
	case 32 :
            outframe.subcode.mid.sampfreq = DT_FREQ32000;
	    samps_per_frame = DTDA_NUMSAMPS32K;
	    break;
	case 48 :
            outframe.subcode.mid.sampfreq = DT_FREQ48000;
	    samps_per_frame = DTDA_NUMSAMPS48K;
	    break;
	case 44 :
	default :
            outframe.subcode.mid.sampfreq = DT_FREQ44100;
	    samps_per_frame = DTDA_NUMSAMPS44K;
	    break;
    }
}

void
quit()
{
    exit(0);
}

void 
dummy_error(long a, const char *b)
{
}

main(int argc, char **argv)
{
    int status;
    int n = 1;
    int i;
    int excl_id;            /* id as returned by mediad */
    int maj, min;            /* dat firmware rel major&minor # */
    struct mtop mtc;

    if (argc>3) {
	fprintf(stderr, "usage: %s [-q] [filename]\n",argv[0]);
	exit(-1);
    }
    while(--argc) {
        if (!strncmp(argv[argc],"-q", 2)) {
	    verbose=0;
	}
	else {
	    if (!freopen(argv[argc],"r", stdin)) {
	 	fprintf(stderr, "error: could not open %s for input\n",argv[argc]);
		exit(-1);
	    }
	}
    }

    AFseterrorhandler(dummy_error);
	
    /*
     * Ask mediad for exclusive access to the DAT drive.
     */
    excl_id = mediad_get_exclusiveuse(datdev, argv[0]);

    /*
     * Open the DAT drive
     */
    dat = open(datdev, O_RDWR);
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

    /*
     * Put the DAT drive in audio mode. 
     */
    mtc.mt_op = MTAUD;
    mtc.mt_count = 1;
    if (ioctl(dat, MTIOCTOP, &mtc) < 0) {
        if (oserror() != EAGAIN) {
            perror("Couldn't put DAT into audio mode");
            exit(-1);
        }
    }

    /*
     * initialize a DAT frame
     */ 
    bzero(&outframe,sizeof(DTFRAME));

    /*
     * set up the rate bits
     */
    outframe.subcode.mid.sampfreq = DT_FREQ44100;

    /*
     * set up all the timecode packs
     */
    reset_timecodes();

    if(verbose) {
        printf("DAT tape ready... lay it on me.\n");
    }

    while (1) {
	yyparse();
    }
}

yywrap()
{
	return 0;
}

yyerror(char *s)
{
	fprintf(stderr,"?%s\n",s);
}
