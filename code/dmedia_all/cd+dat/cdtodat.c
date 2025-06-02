/*
 * A simple program to transfer CD data to DAT
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
#include <dmedia/cdaudio.h>
#include <dmedia/dataudio.h>
#include <dmedia/audio.h>
#include <stdio.h>

/*
 * #define AUDIO if you want to play the CD to the audio system
 * instead of writing to DAT. 
 */

#define NFRAMES 12

#define QUEUESIZE 68
#define HIGHWATER 34

static char *datdev = "/dev/nrtape";

static int verbose = 1;

static int startidframes;    /* # of frames left in DAT track start ID */

/*
 * we define a word-aligned DAT frame. Writes directly to a scsi
 * device can only come from word-aligned addresses. If we made an
 * array of regular DAT frames, every other one would not be word-aligned.
 */
typedef struct wadtframe {        /* word-aligned DAT frame */
    DTFRAME frame;
    short pad;
} WADTFRAME;


/*
 * We implement a circular queue of DAT audio frames. One process 
 * reads from the CD and writes data to the queue in DAT format.
 * when the size of the queue hits a high-water mark, another process
 * wakes up and begins to write data to the DAT from the other end
 * of the queue. This extra buffering is required because both CD and
 * DAT run at "exactly" 44.1kHz, but off of different clocks. They
 * may therefore generate or consume data at slightly different rates.
 */

static WADTFRAME dtq[QUEUESIZE];    /* queue of DAT audio frames */
static volatile int head = 0, tail = 0;    /* queue head & tail pointers */
static volatile int finish = 0;

/*
 * dtf is the "template" DAT frame. It is modified continually by the
 * process reading the CD, then copied into the queue. This prevents
 * us from having to update all the DAT fields all the time; we just
 * update those fields which change from frame to frame.
 */
static DTFRAME dtf;            
static int dtf_nsamps=0;    /* number of audio samps currently in dtf */
static DTFRAME empty;
static index; 		    /* current index being recorded */

void
cd_control_func(void *arg, CDDATATYPES type, unsigned char *flags)
{
    /*
     * Determine whether or not copy protect is enabled
     * on the CD. If so, we must pass it through to the
     * DAT.
     */
    dtf.subcode.mid.copy = 
        ((*flags & CDQ_COPY_MASK) == CDQ_COPY_PERMITTED) ?
         DTM_COPY_PERMITTED : DTM_COPY_PROHIBITED;
}

void
cd_index_func(void *arg, CDDATATYPES type, CDPROGNUM *inum)
{
    struct dttimepack *tpp;
    struct dttimecode *tcp;

    index = inum->value;

    if (verbose) {
        printf("index %d\n",index);
    }

    /*
     * set up the subcode bits
     */
    tpp = (struct dttimepack *)&dtf.subcode.packs[DTP_ATIME-1];
    tpp->index.dhi = index / 10;
    tpp->index.dlo = index % 10;
    tpp = (struct dttimepack *)&dtf.subcode.packs[DTP_PTIME-1];
    tcp = &tpp->tc;
    tpp->index.dhi = index / 10;
    tpp->index.dlo = index % 10;
    if (index == 0) {
	/* 
	 * The DAT spec states that when index=0, times stay 
	 * at 0.
	 */
	bzero(tcp,sizeof(struct dttimecode));
    }
    /* ...and we don't bother with r-time */
}

void
cd_pnum_func(void *arg, CDDATATYPES type, CDPROGNUM *pnum)
{
    int p = pnum->value;
    struct dttimepack *tpp;
    struct dttimecode *tcp;

    if (verbose) {
        printf("program %d\n",p);
    }
    /*
     * set up the subcode bits
     */
    tpp = (struct dttimepack *)&dtf.subcode.packs[DTP_ATIME-1];

    /* don't write pno1 because CD progs only go up to 99 */
    tpp->pno2 = dtf.subcode.sid.pno2 = p / 10;
    tpp->pno3 = dtf.subcode.sid.pno3 = p % 10;
    tpp = (struct dttimepack *)&dtf.subcode.packs[DTP_PTIME-1];
    tpp->pno2 = dtf.subcode.sid.pno2;
    tpp->pno3 = dtf.subcode.sid.pno3;
    /* ...and we don't bother with r-time */

    tcp = &tpp->tc;

    /* don't bother writing hours since they're always 0 */
    tcp->mhi = 0;
    tcp->mlo = 0;
    tcp->shi = 0;
    tcp->slo = 0;
    tcp->fhi = 0;
    tcp->flo = 0;

    /*
     * now we set it up so that the next 300 frames
     * have the start id bit set, indicating start-of-program.
     * DAT spec sez START must be set for 300 frames at start-of-program
     * and PRIORITYID must be set whenever program # is valid.
     */
    dtf.subcode.sid.ctrlid |= (DTS_PRIORITYID | DTS_START);
    startidframes = 300;
}

void
write_DAT(void *fd)
{
    int filled;
    struct dttimepack *tpp;
    struct dttimecode *tcp;
#ifdef AUDIO
    static ALport p = 0;
    ALconfig c;
#endif

    do {
        filled = tail - head;
        if (filled < 0) {
            filled += QUEUESIZE; 
        }
    } while (filled < HIGHWATER);

    while (1) {
        while (tail == head) {
            if (finish) return;
        }
#ifndef AUDIO
        if (write((int)fd,&(dtq[head].frame),sizeof(DTFRAME)) < 0) {
            perror("DAT write failed");
            exit(-1);
        }
#endif

#ifdef AUDIO
        if (!p) {
             c = ALnewconfig();
            ALsetqueuesize(c,DTDA_NUMSAMPS44K*2);
            p = ALopenport("CDtest","w",c);
            ALfreeconfig(c);
        }
        ALwritesamps(p, &dtq[head].frame.audio, DTDA_NUMSAMPS44K);
#endif
	/* we increment head carefully so it only takes 1 write to do so */
        if (head == QUEUESIZE-1) {
	    head = 0;
	}
	else {
	    head++;
	}
    }
}

void
fix_parity()
{
    volatile unchar *pp;
    int i;

    /*
     * calculate the parity bytes for the packs we are using.
     */
    for(i = 0; i<dtf.subcode.sid.numpacks; i++) {
        pp = (volatile unchar *)&dtf.subcode.packs[i];
        dtf.subcode.packs[i].parity
            = pp[0] ^ pp[1] ^ pp[2] ^ pp[3] ^ pp[4] ^ pp[5] ^ pp[6];
    }
}

void
adjust_time()
{
    struct dttimepack *tpp;
    struct dttimecode *tcp;

    /*
     * increment Atime normally.
     */
    tpp = (struct dttimepack *)&dtf.subcode.packs[DTP_ATIME-1];
    tcp = &tpp->tc;
    DTinctime(tcp);

    /* 
     * The DAT spec states that when index=0, ptime stays
     * at 0. Otherwise, we increment the ptime.
     */
    if (index != 0) {
        tpp = (struct dttimepack *)&dtf.subcode.packs[DTP_PTIME-1];
        tcp = &tpp->tc;
        DTinctime(tcp);
    }

    if (startidframes > 0) {  
        startidframes--;
        if(startidframes==0) {
            /* shut off start ID bit after startidframes */
            dtf.subcode.sid.ctrlid  &= ~DTS_START;
        }
    }
}

void
swapcopy(ushort *a, ushort *b, int n)
{
    ushort *done = a + n;
    while(a != done) {
#ifndef AUDIO
	/* DAT audio frames must have the audio byte-swapped */
        *b++ = ((*a & 0xff) << 8)+(*a >> 8);
#else
	/* AL audio frames do not byte-swap the audio */
        *b++ = *a;
#endif
        a++;
    }
}

void
cd_audio_func(void *arg, CDDATATYPES type, short *audio)
{
    int filled;
    int qfull = 0;

    /*
     * The size of a CD frame and the size of a DAT frame vastly differ.
     * The DAT frames are much larger. We have to accumulate enough CD
     * frames to fill a DAT frame before we can stick the DAT frame on
     * the queue to be written. There are two cases: (1) the CD frame
     * fills the DAT frame, in which case it must be split into two
     * pieces, the full DAT frame shoved onto the queue, and the remaining
     * piece used to partially fill the next DAT frame; and (2) the entire CD 
     * frame fits into the DAT frame, in which case we just stick it in.
     */
    if (dtf_nsamps + CDDA_NUMSAMPLES >= DTDA_NUMSAMPS44K) {
	/* 
	 * case 1: we must split the CD frame and write the DAT frame
	 * s is the split point.
	 */
        int s = DTDA_NUMSAMPS44K - dtf_nsamps;		
        swapcopy((ushort*)audio,((ushort *)&dtf.audio)+dtf_nsamps, s);

        /* if queue is full, wait until space is available */
        do {
            filled = tail - head;
            if (filled < 0) {
                filled += QUEUESIZE; 
            }
	
            if (filled == QUEUESIZE - 1 && qfull==0) {
                printf("queue is full! DAT may have glitches.\n");
		qfull = 1;
	    }

        } while (filled >= QUEUESIZE - 1);
	qfull = 0;

	/*
	 * calculate the parity in the subcode packs, write the DAT
	 * frame to the queue, and adjust the times in the subcode packs.
	 */
        fix_parity();		
        bcopy(&dtf,&dtq[tail].frame,sizeof(DTFRAME));	
        adjust_time();	

	/* we increment tail carefully so it only takes 1 write to do so */
        if (tail == QUEUESIZE-1) {
	    tail = 0;
	}
	else {
	    tail++;
	}

        swapcopy((ushort *)audio+s, (ushort *)&dtf.audio, (CDDA_NUMSAMPLES-s));
        dtf_nsamps = CDDA_NUMSAMPLES-s;
    }
    else {
	/* 
	 * case 2: the CD frame fits in the DAT frame
	 */
        swapcopy((ushort *)audio,((ushort *)&dtf.audio)+dtf_nsamps, CDDA_NUMSAMPLES);
        dtf_nsamps += CDDA_NUMSAMPLES;
    }
}

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

int
finish_dat(int fd)
{
    int i;

    /*
     * use the same dummy frame we used to write the leader,
     * except switch from lead-in to lead-out subcodes.
     */
    empty.subcode.sid.pno1 = 0;
    empty.subcode.sid.pno2 = empty.subcode.sid.pno3 = 0xE;   /* EOT */
    empty.subcode.sid.ctrlid =  DTS_START;

    /*
     * Write this dummy frame to the tape to make the trailer
     * DAT spec sez lead-out must be >=300 frames
     */
    for (i = 0; i < 300; i++) {
        if ((write(fd, &empty, sizeof(DTFRAME))) < 0) {
            perror("tape write failed on trailer");
            return 0;
        }
    }
}

int
prepare_dat(int fd)
{
    int i;
    struct mtop mtc;
    struct mtaudio mta;
    struct mtget mtg;
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
     
    /*
     * Now get some information about the DAT media
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
        write(fd,&empty,sizeof(DTFRAME));
    }
    mtc.mt_op = MTREW;
    mtc.mt_count = 1;
    ioctl(fd, MTIOCTOP, &mtc);

    /*
     * Now we stick 100mm guard area on the start
     * of the tape.
     */
    bzero(&empty, sizeof(DTFRAME));
    empty.subcode.sid.pno1 = 0;
    empty.subcode.sid.pno2 = empty.subcode.sid.pno3 = 0xB;   /* BOT */
    empty.subcode.sid.ctrlid =  DTS_START;

    /*
     * Write all timecode packs, and indicate values in
     * all are invalid (but present!)
     */
    empty.subcode.mid.sampfreq = DT_FREQ44100;
    empty.subcode.packs[DTP_ATIME-1].id = DTP_ATIME;
    empty.subcode.packs[DTP_PTIME-1].id = DTP_PTIME;
    empty.subcode.packs[DTP_RTIME-1].id = DTP_RTIME;
    tpp = (struct dttimepack *)&empty.subcode.packs[DTP_ATIME-1];
    tcp = &tpp->tc;
    tcp->hhi = tcp->hlo = 0;
    tcp->mhi = tcp->mlo = tcp->shi = tcp->slo = tcp->fhi = tcp->flo = 0xA;
    tpp = (struct dttimepack *)&empty.subcode.packs[DTP_PTIME-1];
    tcp = &tpp->tc;
    tcp->hhi = tcp->hlo = 0;
    tcp->mhi = tcp->mlo = tcp->shi = tcp->slo = tcp->fhi = tcp->flo = 0xA;
    tpp = (struct dttimepack *)&empty.subcode.packs[DTP_RTIME-1];
    tcp = &tpp->tc;
    tcp->hhi = tcp->hlo = 0;
    tcp->mhi = tcp->mlo = tcp->shi = tcp->slo = tcp->fhi = tcp->flo = 0xA;
    empty.subcode.sid.numpacks = 3;

    /*
     * Write this dummy frame to the tape to make the leader
     * (i've always wondered how dummies became leaders...)
     */
    for (i = 0; i < 100; i++) {
        if ((write(fd, &empty, sizeof(DTFRAME))) < 0) {
            perror("tape write failed on leader");
            return 0;
        }
    }

    /*
     * initialize a dat frame
     */ 
    bzero(&dtf,sizeof(DTFRAME));

    /*
     * set up the rate bits
     */
    dtf.subcode.mid.sampfreq = DT_FREQ44100;

    /*
     * set up the subcode bits
     */
    tpp = (struct dttimepack *)&dtf.subcode.packs[DTP_ATIME-1];
    tpp->id = DTP_ATIME;

    tpp->index.dhi = 0;
    tpp->index.dlo = 1;
    tpp->pno1 = dtf.subcode.sid.pno1 = 0;
    tpp->pno2 = dtf.subcode.sid.pno2 = 0;
    tpp->pno3 = dtf.subcode.sid.pno3 = 1;
    tpp1 = (struct dttimepack *)&dtf.subcode.packs[DTP_PTIME-1];
    *tpp1 = *tpp;
    tpp1->id = DTP_PTIME;

    /* mark R-time as invalid, since we don't really want to write it */
    tpp = (struct dttimepack *)&empty.subcode.packs[DTP_RTIME-1];
    tpp->id = DTP_RTIME;
    tcp = &tpp->tc;
    tcp->hhi = tcp->hlo = 0;
    tcp->mhi = tcp->mlo = tcp->shi = tcp->slo = tcp->fhi = tcp->flo = 0xA;
    dtf.subcode.sid.ctrlid = 0;

    /* numpacks tells how many packs are written onto the DAT */
    dtf.subcode.sid.numpacks = 2;	/* ATIME is 2, PTIME is 1 */
}

main(int argc, char **argv)
{
    CDPLAYER *cd;
    CDSTATUS cdstatus;
    CDPARSER *cdparser;
    CDFRAME buf[NFRAMES];        /* CD input buffer */
    int dat;            /* dat tape fd */
    int status;
    int n = 1;
    int i;
    int excl_id;            /* id as returned by mediad */
    int maj, min;            /* dat firmware rel major&minor # */

    if (argc == 2 && !strncmp(argv[1],"-q", 2)) {
	verbose=0;
    }
    else if (argc!=1) {
	fprintf(stderr, "usage: %s [-q]\n",argv[0]);
	exit(-1);
    }
	
    /*
     * open the default CD drive. This will get exclusive access
     * from mediad.
     */
    cd = CDopen(0, "r");
    if (!cd) {
        fprintf(stderr,"Couldn't open CD-ROM drive\n");
        exit(-1);
    }

    cdparser = CDcreateparser();
    if (!cdparser) {
        fprintf(stderr,"Couldn't create CD parser\n");
        exit(-1);
    }

    /* 
     * add callback functions to deal with events from the CD-ROM 
     * here's why:
     *	a.	we do NOT read ptime from the CD. We
     *		calculate it ourselves, starting from 0 at the beginning
     *		of a program, and incrementing from the start of the first
     *		non-zero index. Thus we have no ptime callback.
     *  b.	we do NOT read atime from the CD. We calculate it 
     *		ourselves. Thus we have no atime callback.
     *	c.	we read the control bits to pass through the copy
     *		prohibit bit. This is legally required. Don't remove it.
     *		(cd_control_func)
     *	d.	we transfer index and program number from CD to DAT.
     *		(cd_index_func, cd_pnum_func)
     *	e.	The cd_audio function transfers the audio data to a queue,
     *		accounting for the difference in frame size between the media.
     */
    CDaddcallback(cdparser, cd_control, (CDCALLBACKFUNC) cd_control_func, 0);
    CDaddcallback(cdparser, cd_index, (CDCALLBACKFUNC)  cd_index_func, 0);
    CDaddcallback(cdparser, cd_pnum, (CDCALLBACKFUNC) cd_pnum_func, 0);
    CDaddcallback(cdparser, cd_audio, (CDCALLBACKFUNC) cd_audio_func, 0);

    /*
     * initialize our idea of the state of the CD-ROM
     */
    CDgetstatus(cd, &cdstatus);

    /*
     * Make sure everything is kosher with the CD-ROM. Must
     * have an audio disc in before the program is run.
     */
    if (cdstatus.state == CD_NODISC) {
        fprintf(stderr,"No disc in CD-ROM drive\n");
        exit(-1);
    }
    else if (cdstatus.state == CD_CDROM) {
        fprintf(stderr,"Disc in CD-ROM drive is not an audio CD\n");
        exit(-1);
    }
    else if (cdstatus.state == CD_ERROR) {
        fprintf(stderr,"Error reading CD-ROM drive\n");
        exit(-1);
    }

    /*
     * Ask mediad for exclusive access to the DAT drive.
     */
    excl_id = mediad_get_exclusiveuse(datdev, "cdtodat");

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

    if (prepare_dat(dat) < 0) {
	exit(-1);
    }

    sproc(write_DAT,PR_SALL,dat);
    while(n) {
        n = CDreadda(cd, buf, NFRAMES);

        if (n < 0) {
            fprintf(stderr,"Error reading CD-ROM drive\n");
            finish = 1;
            exit(-1);
        }
        for (i = 0; i < n; i++) {
            CDparseframe(cdparser, &buf[i]);
        }
    }

    finish = 1;
    wait(&status);            /* wait for child to complete */

    /* 
     * now finish up the DAT tape with a lead-out section marking
     * EOT
     */
    if (finish_dat(dat) < 0) {
	exit(-1);
    }

    if(verbose) printf("DAT tape is complete\n");
}
