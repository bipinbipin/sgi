/*****************************************************************************
 * audio file library example code
 * 
 * this file is actually the source code for four programs:
 *    1) aiff2aifc convert an AIFF file to an AIFF-C file 
 *    2) aifc2aiff convert an AIFF-C file to an AIFF file (will decompress)
 *    3) aiffcdecompress - decompress audio data in an AIFF-C file
 *    4) aiffccompress -  compress audio data in an AIFF-C file
 *
 * Scott Porter, april 92
 *****************************************************************************/
#ident "$Revision: 1.19 $"

#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <bstring.h>
#include <sys/types.h>
#include <fcntl.h>
#ifdef AWARE
#include <malloc.h>
#include <stdlib.h>
#endif /*AWARE*/
#include <dmedia/audioutil.h>
#include <dmedia/audiofile.h>




static char aiff2aifc_usage[] 
    = "[-verbose] <infile.aiff> <outfile.aifc>";
static char aifc2aiff_usage[] 
    = "[-verbose] <infile.aifc> <outfile.aiff>";
#ifdef  AWARE
/*
 * NOTE: take out psycho1alpha and NMR
 */
static char compressaifc_usage[] =
"[-v]  <-c algorithm> [options] <in.aifc> <out.aifc>\n\
\n\
    standard algorithms: g722    ulaw    alaw    none \n\
    Aware algorithms:    awmpeg1 awmpeg2 awmulti awlsls \n\
    Aware options: \n\
        <-aw_targ     bits per sec>             \n\
        <-aw_chanpol  stereo|joint|indep>       \n\
        <-aw_bitpol   fixed|constqual|lossless> \n\
        <-aw_nmr      +/- dB's>                 \n\
\n";
#else  /* not AWARE*/
static char compressaifc_usage[]
    = "[-verbose] <-comp g722|ulaw|alaw|none> <infile.aifc> <outfile.aifc>";
#endif /*AWARE*/
static char decompressaifc_usage[]
    = "[-verbose] <infile.aifc> <outfile.aifc>";

static char aiff2aifc[]      = "aiff2aifc";
static char aifc2aiff[]      = "aifc2aiff";
static char compressaifc[]   = "aifccompress";
static char decompressaifc[] = "aifcdecompress";
static char valid_names[]
    = "aiff2aifc | aifc2aiff | aifccompress | aifcdecompress";

#define AIFF2AIFC       1
#define AIFC2AIFF       2
#define COMPRESSAIFC    3
#define DECOMPRESSAIFC  4

#ifdef AWARE
#define MAX_AWARE_OPTS 5
typedef struct {
    long   layer;
    long   defaultconfig;
    int    bitratetarget_seen;  /* bitrate target option */
    long   bitratetarget;
    int    channelpolicy_seen;  /* channel policy option */
    long   channelpolicy;
    int    bitratepolicy_seen;  /* bitrate policy option */
    long   bitratepolicy;
    int    constqualnmr_seen;   /* constant quality NMR option */
    double constqualnmr;
} aware_opts_type;
#endif /*AWARE*/

/*
 * this structure describes the command line options
 */
typedef struct {
    char *myname;                /* tail part of argv[0] */
    int  myspecialpurpose;       /* what program is supposed to do */
    char *myusage;               /* usage string */
    char *infilename;            /* input file name argument */
    char *outfilename;           /* output file name */
    int   verbose;               /* flag */
    int   outcompression;        /* type of compression for output file */
#ifdef AWARE
    aware_opts_type aware_opts; /* aware compression parameters */
#endif /*AWARE*/
} opts_type;

/*
 * this structure describes an audio track in a disk file
 */
typedef struct {
    int nmarks;                      /* number of markers */
    int *markids;                    /* list of marker ids */
    int *markpos;                   /* list of marker positions */
    char **markname;                 /* list of marker names */
    int  haveaesdata;                /* was there AES data for the track? */
    unsigned char aesdata[24];       /* contains the AES data */
    int sampwidth;                  /* sample width */
    int sampfmt;                    /* sample type */
    int nframes;                     /* total number of sample frames */
    int nchannels;                  /* number of audio channels */
    double rate;                     /* sample rate */
    int compression;            /* compression type */
    char *compressionname;           /* compression name */
} track_desc_type;

/*
 * this structure contains sampler configuration parameters (some redundancy)
 */
typedef struct {
    int midi_basenote;             /* base note for sample playback */
    int midi_lonote;               /* lowest note for sample playback */
    int midi_hinote;               /* highest note for sample playback */
    int midi_lovelocity;           /* lowest velocity for sample playback */
    int midi_hivelocity;           /* highest velocity for sample playback */
    int numdbs_gain;               /* number of DB's of gain suggested */
    int numcents_detune;           /* number of cents of detune suggested */
    int sustain_loop_id;                 /* sustain loop id */
    int sustain_loop_mode;               /* sustain loop mode */
    int sustain_loop_start_mark_id;      /* sustain loop start mark */
    int sustain_loop_end_mark_id;        /* sustain loop end mark */
    int release_loop_id;                 /* release loop id */
    int release_loop_mode;               /* release loop mode */
    int release_loop_start_mark_id;      /* release loop start mark */
    int release_loop_end_mark_id;        /* release loop end mark */
    int nloops;
    int *loopids;
} sampler_desc_type;

/*
 * this structure describes a "miscellaneous data chunk" in an audio file
 */
typedef struct {
    int miscid;                   /* id for the miscellaneous chunk */
    int miscsize;                 /* size of the miscellaneous chunk */
    int misctype;                 /* type of miscellaneous data */
} misc_desc_type;

/*
 * this structure describes an audio file
 */
typedef struct {
    int fd;                                /* unix file descriptor */
    char *filename;                        /* file name */
    AFfilehandle file;                     /* AF file handle */
    AFfilesetup setup;                     /* AF file setup struct */
    int filefmt;                          /* file format */
    int filefmtvers;                      /* fmt version, if applicable */
    track_desc_type track_desc;            /* audio track description */
    int have_sampler_data;                /* was there sampler data */
    sampler_desc_type sampler_desc;        /* sampler data description */
    int nmisc;                         /* number of misc data chunks */
    int *miscids;
    misc_desc_type  *misc_desc_list;       /* misc data chunks descriptions */
} file_desc_type;

static void on_sigint();

static void init_error_reporting(opts_type *);
static void print_error(opts_type *, const char *);
static void catchAFerror(long, const char *);

static void getmyname(char *, opts_type *);
static void parse_cmd_line(int, char **, opts_type *);
static int check_encoder_license(opts_type *);
static int check_decoder_license(file_desc_type *, opts_type *);

/* open input file */

static int open_infile(file_desc_type *, opts_type *);  

/* read input file configuration */

static void read_file_desc(file_desc_type *, opts_type *);
static void read_track_desc(file_desc_type *, opts_type *);
static void read_sampler_desc(file_desc_type *, opts_type *);
static void read_misc_desc(file_desc_type *, opts_type *);

/* setup output file configuration */

static void setup_file(file_desc_type *, file_desc_type *, opts_type *);
static void setup_track(file_desc_type *, file_desc_type *, opts_type *);
static void setup_sampler(file_desc_type *, file_desc_type *, opts_type *);
static void setup_misc(file_desc_type *, file_desc_type *, opts_type *);

/* open output file */

static int open_outfile(file_desc_type *, opts_type *);

/* copy data from input from to output file */

static int  copy_track(file_desc_type *, file_desc_type *, opts_type *); 
static void copy_sampler(file_desc_type *, file_desc_type *, opts_type *);
static int  copy_misc(file_desc_type *, file_desc_type *, opts_type *);

/* cleanup */

static void cleanup_infile(file_desc_type *, opts_type *);
static int  cleanup_outfile(file_desc_type *, opts_type *);

int caught_sigint = 0;

static void 
on_sigint()
{
    caught_sigint++;
}

main(int argc, char **argv)
{
   opts_type opts;               /* describes the command options */
   file_desc_type infile_desc;   /* describes the input file */
   file_desc_type outfile_desc;  /* describes the output file */

   bzero(&opts, sizeof(opts_type));
   parse_cmd_line(argc, argv, &opts);
  
#ifdef AWARE
   init_error_reporting(&opts);

   if (check_encoder_license(&opts) < 0)
   {
        exit(1);
   }
#endif /*AWARE*/

   if (open_infile(&infile_desc, &opts) < 0)
   {
        exit(1);
   }

   read_file_desc(&infile_desc, &opts);
   read_track_desc(&infile_desc, &opts);
   read_sampler_desc(&infile_desc, &opts);
   read_misc_desc(&infile_desc, &opts);

#ifdef AWARE
   if (check_decoder_license(&infile_desc, &opts) < 0)
   {
       exit(1);
   }
#endif /*AWARE*/

   setup_file(&infile_desc, &outfile_desc, &opts);
   setup_track(&infile_desc, &outfile_desc, &opts);
   setup_sampler(&infile_desc, &outfile_desc, &opts);
   setup_misc(&infile_desc, &outfile_desc, &opts);

   sigset(SIGINT, on_sigint);

   /*
    * open the new file
    */
   if (open_outfile(&outfile_desc, &opts) < 0)
   {
      exit(1);
   }

   /*
    * copy data from input file to output file
    */ 
   if (copy_misc(&infile_desc, &outfile_desc, &opts) < 0)
   {
      exit(1);
   }
   copy_sampler(&infile_desc, &outfile_desc, &opts);
   
   if (copy_track(&infile_desc, &outfile_desc, &opts) < 0)
   {
      exit(1);
   }

   /*
    * cleanup 
    */
   cleanup_infile(&infile_desc, &opts);
   if (cleanup_outfile(&outfile_desc, &opts) < 0)
   {
       exit(1);
   }

   exit(0);
}

/****************************************************************************
 *
 * P A R S E _ C M D _ L I N E
 *
 *****************************************************************************/
static void
parse_cmd_line(int argc, char **argv, opts_type *opts)
{
    int i;
    char *s;
    int compression_seen = 0;
    
    getmyname(argv[0], opts);

    if (!strncmp(opts->myname, aiff2aifc, strlen(aiff2aifc))) {
        opts->myspecialpurpose = AIFF2AIFC;
        opts->myusage = aiff2aifc_usage;
    }
    else if (!strncmp(opts->myname, aifc2aiff, strlen(aifc2aiff))) {
        opts->myspecialpurpose = AIFC2AIFF;
        opts->myusage = aifc2aiff_usage;
    }
    else if (!strncmp(opts->myname, compressaifc, strlen(compressaifc))) {
        opts->myspecialpurpose = COMPRESSAIFC;
        opts->myusage = compressaifc_usage;
    }
    else if (!strncmp(opts->myname, decompressaifc, strlen(decompressaifc))) {
        opts->myspecialpurpose = DECOMPRESSAIFC;
        opts->myusage = decompressaifc_usage;
    }
    else {
        fprintf(stderr, "%s: hey, who changed my name??!\n", opts->myname);
        fprintf(stderr, "%s: valid names -- %s\n", opts->myname,valid_names);
        exit(1);
    }

    if (argc < 3)
    {
       fprintf(stderr, "Usage: %s %s\n", opts->myname, opts->myusage);
       exit(1);
    }

    opts->infilename = argv[argc-2];
    opts->outfilename = argv[argc-1];

    opts->outcompression = AF_COMPRESSION_NONE;

    for (i=1; i<argc-2; i++)
    {
        s = argv[i];
        if (!strncmp(s, "-c", 2)) /* compression */
        {
            compression_seen++;
            i++;
            s=argv[i];

            if (!strcmp(s, "g722"))
            {
                opts->outcompression = AF_COMPRESSION_G722;
            }
            else if (!strcmp(s, "ulaw"))
            {
               opts->outcompression = AF_COMPRESSION_G711_ULAW;
            }
            else if (!strcmp(s, "alaw"))
            {
               opts->outcompression = AF_COMPRESSION_G711_ALAW;
            }
#ifdef AWARE
            else if (!strcmp(s, "awmpeg1"))
            {
               opts->outcompression       = AF_COMPRESSION_MPEG1;
               opts->aware_opts.layer     = AF_MPEG_LAYER_I;
               opts->aware_opts.defaultconfig 
                                          = AF_COMPRESSION_DEFAULT_MPEG_I;
               opts->aware_opts.bitratetarget_seen     = 0;
               opts->aware_opts.channelpolicy_seen     = 0;
               opts->aware_opts.bitratepolicy_seen     = 0;
               opts->aware_opts.constqualnmr_seen      = 0;
            }
            else if (!strcmp(s, "awmpeg2"))
            {
               opts->outcompression     = AF_COMPRESSION_MPEG1;
               opts->aware_opts.layer     = AF_MPEG_LAYER_II;
               opts->aware_opts.defaultconfig 
                                        = AF_COMPRESSION_DEFAULT_MPEG_II;
               opts->aware_opts.bitratetarget_seen     = 0;
               opts->aware_opts.channelpolicy_seen     = 0;
               opts->aware_opts.bitratepolicy_seen     = 0;
               opts->aware_opts.constqualnmr_seen      = 0;
            }
            else if (!strcmp(s, "awlsls"))
            {
               opts->outcompression      = AF_COMPRESSION_AWARE_MULTIRATE;
               opts->aware_opts.layer     = AF_MPEG_LAYER_I; /* whatever */
               opts->aware_opts.defaultconfig 
                                         = AF_COMPRESSION_AWARE_DEFAULT_LOSSLESS;
               opts->aware_opts.bitratetarget_seen     = 0;
               opts->aware_opts.channelpolicy_seen     = 0;
               opts->aware_opts.bitratepolicy_seen     = 0;
               opts->aware_opts.constqualnmr_seen      = 0;
            }
            else if (!strcmp(s, "awmulti"))
            {
               opts->outcompression     = AF_COMPRESSION_AWARE_MULTIRATE;
               opts->aware_opts.layer     = AF_MPEG_LAYER_I; /* whatever */
               opts->aware_opts.defaultconfig
                                        = AF_COMPRESSION_AWARE_DEFAULT_MULTIRATE;
               opts->aware_opts.bitratetarget_seen     = 0;
               opts->aware_opts.channelpolicy_seen     = 0;
               opts->aware_opts.bitratepolicy_seen     = 0;
               opts->aware_opts.constqualnmr_seen      = 0;
            }
#endif /*AWARE*/
            else if (!strcmp(s, "none"))
            {
               opts->outcompression = AF_COMPRESSION_NONE;
            }
            else 
            {
               fprintf(stderr, "%s: invalid compression %s\n",
                        opts->myname, s);
               exit(1);
            }
        }
        else if (!strncmp(s, "-v", 2))    /* verbose */
        {
            opts->verbose = 1;
        }
#ifdef AWARE
        else if (!strncmp(s, "-aw", 3))  /* Aware compression parameter */
        {
            if (!strcmp(s, "-aw_targ"))          /* bitrate target */
            {
               int b;

               opts->aware_opts.bitratetarget_seen++;
               i++;
               s=argv[i];
               switch (b = atoi(s))
               {
                   case 32000:  /* I, II */
                   case 48000:  /*    II */
                   case 56000:  /*    II */
                   case 64000:  /* I, II */
                   case 96000:  /* I, II */
                   case 112000: /*    II */
                   case 128000: /* I, II */
                   case 160000: /* I, II */
                   case 192000: /* I, II */
                   case 224000: /* I, II */
                   case 256000: /* I, II */
                   case 228000: /* I     */
                   case 320000: /* I, II */
                   case 352000: /* I     */
                   case 384000: /* I, II */
                   case 416000: /* I     */
                   case 448000: /* I     */
                       opts->aware_opts.bitratetarget = b;
                       break;
                   default:
                       fprintf(stderr, "%s: invalid -aw_targ '%s'\n", 
                           opts->myname, s);
                       fprintf(stderr, "Usage: %s %s\n", 
                           opts->myname,opts->myusage);
                       exit(1);
               }
            }
            else if (!strcmp(s, "-aw_chanpol"))       /* channel policy */
            {
               opts->aware_opts.channelpolicy_seen++;
               i++;
               s=argv[i];
               if (!strcmp(s, "stereo")) {
                   opts->aware_opts.channelpolicy = AF_AWARE_STEREO;
               }
               else if (!strcmp(s, "indep")) {
                   opts->aware_opts.channelpolicy = AF_AWARE_INDEPENDENT;
               }
               else if (!strcmp(s, "joint")) {
                   opts->aware_opts.channelpolicy = AF_MPEG_JOINT_STEREO;
               }
               else {
                       fprintf(stderr, "%s: invalid -aw_chanpol '%s'\n", 
                           opts->myname, s);
                       fprintf(stderr, "Usage: %s %s\n", 
                           opts->myname,opts->myusage);
                       exit(1);
               }
            }
            else if (!strcmp(s, "-aw_bitpol"))        /* bitrate policy */
            {
               opts->aware_opts.bitratepolicy_seen++;
               i++;
               s=argv[i];
               if (!strcmp(s, "fixed")) {
                   opts->aware_opts.bitratepolicy = AF_AWARE_FIXED_RATE; 
               }
               else if (!strcmp(s, "constqual")) {
                   opts->aware_opts.bitratepolicy = AF_AWARE_CONST_QUAL;
               }
               else if (!strcmp(s, "lossless")) {
                   opts->aware_opts.bitratepolicy = AF_AWARE_LOSSLESS;
               }
               else {
                   fprintf(stderr, "%s: invalid -aw_bitpol '%s'\n", 
                           opts->myname, s);
                   fprintf(stderr, "Usage: %s %s\n", 
                           opts->myname,opts->myusage);
                   exit(1);
               }
            }
            else if (!strcmp(s, "-aw_nmr"))           /*const qual NMR */
            {
               double nmr;
               opts->aware_opts.constqualnmr_seen++;
               i++;
               s=argv[i];
               nmr = atof(s);
               if ((nmr < -200.0) || (nmr > 200.0)) {
                   fprintf(stderr, "%s: invalid -aw_nmr '%s'\n", 
                           opts->myname, s);
                   fprintf(stderr, "Usage: %s %s\n", 
                           opts->myname,opts->myusage);
                   exit(1);
               }
               opts->aware_opts.constqualnmr = nmr;
    
            }
            else 
            {
                fprintf(stderr, "%s: invalid option '%s'\n", opts->myname, s);
                fprintf(stderr, "Usage: %s %s\n", opts->myname,opts->myusage);
                exit(1);
            }
        }
#endif /*AWARE*/
        else 
        {
            fprintf(stderr, "Usage: %s %s\n", opts->myname,opts->myusage);
            exit(1);
        }
    } /* for */

    if (opts->verbose)
    {
        if (compression_seen)
        {
            if (opts->outcompression == AF_COMPRESSION_G711_ULAW)
            {
                fprintf(stderr,"%s: compression = g.711 ulaw\n", opts->myname);
            }
            else if (opts->outcompression == AF_COMPRESSION_G711_ALAW)
            {
                fprintf(stderr,"%s: compression = g.711 Alaw\n", opts->myname);
            }
            else if (opts->outcompression == AF_COMPRESSION_G722)
            {
                fprintf(stderr,"%s: compression = g.722\n", opts->myname);
            }
#ifdef AWARE
            else if (opts->outcompression == AF_COMPRESSION_MPEG1) {
                if (opts->aware_opts.layer == AF_MPEG_LAYER_I) {
                    fprintf(stderr, "%s: MPEG audio layer I\n",
                                        opts->myname);
                }
                else {
                    fprintf(stderr, "%s: MPEG audio layer II\n",
                                        opts->myname);
                }
            }
            else if (opts->outcompression == AF_COMPRESSION_AWARE_MULTIRATE) {
                fprintf(stderr, "%s: compression = Aware MultiRate\n", 
                                opts->myname);
            }
#endif /*AWARE*/
        } 
        else 
        {
            fprintf(stderr,"%s: compression off\n", opts->myname);

        }
    }
} /* parse_cmd_line */


/****************************************************************************
 *
 * C H E C K _ E N C O D E R _ L I C E N S E
 *
 *****************************************************************************/
static int
check_encoder_license(opts_type *opts)
{
   char *msg;
   int license;

#ifdef AWARE
   if (opts->myspecialpurpose == COMPRESSAIFC)
   {
       if (opts->outcompression == AF_COMPRESSION_MPEG1)
       {
           if (opts->verbose) {
               fprintf(stderr, "%s: Checking for MPEG encoder license...",
                         opts->myname);
           }
           AUchecklicense(AU_LICENSE_AWARE_MPEG_ENCODER, &license, &msg);
           if (opts->verbose) {
               fprintf(stderr, "done\n");
           }
           if (license != AU_LICENSE_OK)
           {
              fprintf(stderr, 
              "%s: Could not obtain permission to use MPEG audio encoder\n",
                     opts->myname);
              fprintf(stderr, "%s", msg);
              return(-1);
           }
       }
       else if (opts->outcompression == AF_COMPRESSION_AWARE_MULTIRATE)
       {
           if (opts->verbose) {
               fprintf(stderr, 
                         "%s: Checking for Aware MultiRate encoder license...",
                         opts->myname);
           }
           AUchecklicense(AU_LICENSE_AWARE_MULTIRATE_ENCODER, &license, &msg);
           if (opts->verbose) {
               fprintf(stderr, "done\n");
           }
           if (license != AU_LICENSE_OK)
           {
              fprintf(stderr, 
              "%s: Could not obtain permission to use Aware MultiRate encoder\n",
                     opts->myname);
              fprintf(stderr, "%s", msg);
              return(-1);
           }
       }
   }
   return(0);
#endif /*AWARE*/ 
}


/****************************************************************************
 *
 * C H E C K _ D E C O D E R _ L I C E N S E
 *
 *****************************************************************************/
static int
check_decoder_license(file_desc_type *infile, opts_type *opts)
{
   char *msg;
   int license;

#ifdef AWARE
   if (opts->myspecialpurpose == DECOMPRESSAIFC)
   {
       /*
        * No longer necessary to check MPEG decoder license.
        * MPEG decode is now bundled with the system.
        */
       if (infile->track_desc.compression==AF_COMPRESSION_AWARE_MULTIRATE)
       {
           if (opts->verbose) {
               fprintf(stderr, 
                         "%s: Checking for Aware MultiRate decoder license...",
                         opts->myname);
           }
           AUchecklicense(AU_LICENSE_AWARE_MULTIRATE_DECODER, &license, &msg);
           if (opts->verbose) {
               fprintf(stderr, "done\n");
           }
           if (license != AU_LICENSE_OK)
           {
              fprintf(stderr, 
              "%s: Could not obtain permission to use Aware MultiRate decoder\n",
                     opts->myname);
              fprintf(stderr, "%s", msg);
              return(-1);
           }
       }
   }
   return(0);
#endif /*AWARE*/ 
}



/****************************************************************************
 *
 * G E T M Y N A M E
 *
 *****************************************************************************/
static void
getmyname(char *argv0, opts_type *opts)
{  
    char *p;

    p = strrchr(argv0, '/');
    if (p) {
        p++; 
    } 
    else {
        p = argv0;
    }
    
    opts->myname = (char *)malloc(strlen(p) + 1);
    strcpy(opts->myname, p);
} 

static int
open_infile(file_desc_type *infile_desc, opts_type *opts)
{
    infile_desc->filename = (char *)malloc(strlen(opts->infilename) + 1);
    strcpy(infile_desc->filename, opts->infilename);

    infile_desc->fd = open(infile_desc->filename, O_RDONLY);

    if (infile_desc->fd <= 0)
    {
        fprintf(stderr, "%s: failed to open %s\n",
              opts->myname, infile_desc->filename);
        return(-1);
    }

    infile_desc->filefmt = afIdentifyFD(infile_desc->fd);

    switch (infile_desc->filefmt)
    {
        case AF_FILE_AIFFC:
        case AF_FILE_AIFF:
            break;

        default:
            fprintf(stderr, "%s: failed to recognize %s as AIFF-C or AIFF\n",
               opts->myname, infile_desc->filename);
            return(-1);
    }

    infile_desc->file = afOpenFD(infile_desc->fd, "r", AF_NULL_FILESETUP);

    if (infile_desc->file == AF_NULL_FILEHANDLE)
    {
        fprintf(stderr, "%s: failed to attach an audio file struct to %s\n",
                 opts->myname, infile_desc->filename);
        return(-1);
    }
    return(0);
}

/****************************************************************************
 *
 * R E A D _ F I L E _ D E S C
 *
 *****************************************************************************/
static void
read_file_desc(file_desc_type *infile_desc, opts_type *opts)
{
    AFfilehandle file;

    file = infile_desc->file;
    infile_desc->filefmt = afGetFileFormat(file, &infile_desc->filefmtvers);
    infile_desc->nmisc = afGetMiscIDs(file, (int *)0);


    infile_desc->misc_desc_list
       = (misc_desc_type *)calloc(infile_desc->nmisc, sizeof(misc_desc_type));
    infile_desc->have_sampler_data = afGetInstIDs(file, (int *)0);


}

/****************************************************************************
 *
 * R E A D _ T R A C K _ D E S C
 *
 *****************************************************************************/
static void
read_track_desc(file_desc_type *file_desc, opts_type *opts)
{
    track_desc_type *track_desc;
    AFfilehandle file;
    char *s;
    int i;

    track_desc = &(file_desc->track_desc);
    file = file_desc->file;

    track_desc->nframes = afGetFrameCount(file, AF_DEFAULT_TRACK);
    track_desc->nchannels = afGetChannels(file, AF_DEFAULT_TRACK);

    afGetSampleFormat(file, AF_DEFAULT_TRACK,
        &track_desc->sampfmt, &track_desc->sampwidth);

    track_desc->haveaesdata
       = afGetAESChannelData(file, AF_DEFAULT_TRACK, track_desc->aesdata);

    track_desc->rate = afGetRate(file, AF_DEFAULT_TRACK);
    track_desc->compression = afGetCompression(file, AF_DEFAULT_TRACK);

    s = afGetCompressionName(file, AF_DEFAULT_TRACK);
    track_desc->compressionname = (char *)malloc(strlen(s)+1);
    strcpy(track_desc->compressionname, s);

    track_desc->nmarks = afGetMarkIDs(file, AF_DEFAULT_TRACK, (int *)0);

    track_desc->markids  = (int *)calloc(track_desc->nmarks, sizeof(int));
    track_desc->markpos = (int *)calloc(track_desc->nmarks, sizeof(int));
    track_desc->markname = (char **)calloc(track_desc->nmarks, sizeof(char *));

    afGetMarkIDs(file, AF_DEFAULT_TRACK, track_desc->markids);

    for (i=0; i<track_desc->nmarks; i++)
    {
       track_desc->markpos[i]
          = afGetMarkPosition(file, AF_DEFAULT_TRACK, track_desc->markids[i]);
       s = afGetMarkName(file, AF_DEFAULT_TRACK, track_desc->markids[i]);
       track_desc->markname[i] = (char *)malloc(strlen(s)+1);
       strcpy(track_desc->markname[i], s);
    }
    
}

/****************************************************************************
 *
 * R E A D _ S A M P L E R _ D E S C
 *
 *****************************************************************************/
static void
read_sampler_desc(file_desc_type *file_desc, opts_type *opts)
{
     sampler_desc_type *sampler_desc;
     AFfilehandle file;


     if (file_desc->have_sampler_data)
     {
       sampler_desc = &(file_desc->sampler_desc);
       file = file_desc->file;
  
       /*
        * get sampler parameters
        */
       sampler_desc->midi_basenote =
           afGetInstParamLong(file, AF_DEFAULT_INST, AF_INST_MIDI_BASENOTE);
       sampler_desc->midi_lonote =
           afGetInstParamLong(file, AF_DEFAULT_INST, AF_INST_MIDI_LONOTE);
       sampler_desc->midi_hinote =
           afGetInstParamLong(file, AF_DEFAULT_INST, AF_INST_MIDI_HINOTE);
       sampler_desc->midi_lovelocity =
           afGetInstParamLong(file, AF_DEFAULT_INST, AF_INST_MIDI_LOVELOCITY);
       sampler_desc->midi_hivelocity =
           afGetInstParamLong(file, AF_DEFAULT_INST, AF_INST_MIDI_HIVELOCITY);
       sampler_desc->numdbs_gain =
           afGetInstParamLong(file, AF_DEFAULT_INST, AF_INST_NUMDBS_GAIN);
       sampler_desc->numcents_detune =
           afGetInstParamLong(file, AF_DEFAULT_INST, AF_INST_NUMCENTS_DETUNE);
       sampler_desc->sustain_loop_id =
           afGetInstParamLong(file, AF_DEFAULT_INST, AF_INST_SUSLOOPID);
       sampler_desc->release_loop_id =
           afGetInstParamLong(file, AF_DEFAULT_INST, AF_INST_RELLOOPID);
  
       sampler_desc->sustain_loop_start_mark_id =
           afGetLoopStart(file, AF_DEFAULT_INST, sampler_desc->sustain_loop_id);
       sampler_desc->sustain_loop_end_mark_id =
           afGetLoopEnd(file, AF_DEFAULT_INST, sampler_desc->sustain_loop_id);
       sampler_desc->sustain_loop_mode =
           afGetLoopMode(file, AF_DEFAULT_INST, sampler_desc->sustain_loop_id);
  
       sampler_desc->release_loop_start_mark_id =
           afGetLoopStart(file, AF_DEFAULT_INST, sampler_desc->release_loop_id);
       sampler_desc->release_loop_end_mark_id =
           afGetLoopEnd(file, AF_DEFAULT_INST, sampler_desc->release_loop_id);
       sampler_desc->release_loop_mode =
           afGetLoopMode(file, AF_DEFAULT_INST, sampler_desc->release_loop_id);
  
       sampler_desc->nloops = afGetLoopIDs(file, AF_DEFAULT_INST, (int *)0);
       sampler_desc->loopids = (int *)calloc(sampler_desc->nloops, 
                                                               sizeof(int));
       afGetLoopIDs(file, AF_DEFAULT_INST, sampler_desc->loopids);
     }
}

/****************************************************************************
 *
 * R E A D _ M I S C _ D E S C
 *
 *****************************************************************************/
static void
read_misc_desc(file_desc_type *file_desc, opts_type *opts)
{
    int i;
    misc_desc_type *misc_desc;
    AFfilehandle file;

    file = file_desc->file;
    file_desc->miscids = (int *)calloc(file_desc->nmisc, sizeof(int));
    afGetMiscIDs(file, file_desc->miscids);

    for (i=0; i<file_desc->nmisc; i++)
    {
        misc_desc = &file_desc->misc_desc_list[i];

        misc_desc->miscid   = file_desc->miscids[i]; /* a little redundant */
        misc_desc->miscsize = afGetMiscSize(file, misc_desc->miscid);
        misc_desc->misctype = afGetMiscType(file, misc_desc->miscid);
    }
}

/****************************************************************************
 *
 *  S E T U P _ F I L E 
 *
 *****************************************************************************/
static void 
setup_file(file_desc_type *infile_desc, file_desc_type *outfile_desc,
                                                      opts_type *opts)
{
   outfile_desc->setup = afNewFileSetup();

   switch (opts->myspecialpurpose)
   {
        case AIFF2AIFC:
            afInitFileFormat(outfile_desc->setup, AF_FILE_AIFFC);
            break;
        case AIFC2AIFF:
            afInitFileFormat(outfile_desc->setup, AF_FILE_AIFF);
            break;
        case COMPRESSAIFC:
            afInitFileFormat(outfile_desc->setup, AF_FILE_AIFFC);
            break;
        default:
        case DECOMPRESSAIFC:
            afInitFileFormat(outfile_desc->setup, AF_FILE_AIFFC);
            break;
   }

}

/****************************************************************************
 *
 * S E T U P _ T R A C K
 *
 *****************************************************************************/
static void 
setup_track(file_desc_type *infile_desc, file_desc_type *outfile_desc,
                                               opts_type *opts)
{
    int nchannels;
    double rate;
    int sampfmt, sampres, nmarks, haveaesdata; 
    int *markids;
    int i;
    char *s;
    int n;
    AUpvlist pvlist;

    switch (opts->myspecialpurpose)
    {
        case AIFF2AIFC:
            afInitCompression(outfile_desc->setup, AF_DEFAULT_TRACK,
                       AF_COMPRESSION_NONE); 
            break;
        case AIFC2AIFF:
            afInitCompression(outfile_desc->setup, AF_DEFAULT_TRACK,
                       AF_COMPRESSION_NONE);
            break;
        case COMPRESSAIFC: /*get compression from command line*/
#ifdef AWARE
            if ((opts->outcompression == AF_COMPRESSION_AWARE_MULTIRATE) ||
                (opts->outcompression == AF_COMPRESSION_MPEG1))
            {
                pvlist = AUpvnew(MAX_AWARE_OPTS);
               
                n = 0;

                if (opts->outcompression == AF_COMPRESSION_MPEG1) {
                     AUpvsetparam(pvlist, n, AF_MPEG_PARAM_LAYER);
                     AUpvsetvaltype(pvlist, n, AU_PVTYPE_LONG);
                     AUpvsetval(pvlist, n, &opts->aware_opts.layer);
                     n++;
                }
                if (opts->aware_opts.bitratetarget_seen) {
                     AUpvsetparam(pvlist, n, AF_MPEG_PARAM_BITRATE_TARGET);
                     AUpvsetvaltype(pvlist, n, AU_PVTYPE_LONG);
                     AUpvsetval(pvlist, n, &opts->aware_opts.bitratetarget);
                     n++;
                }
                if (opts->aware_opts.channelpolicy_seen) {
                     AUpvsetparam(pvlist, n, AF_AWARE_PARAM_CHANNEL_POLICY);
                     AUpvsetvaltype(pvlist, n, AU_PVTYPE_LONG);
                     AUpvsetval(pvlist, n, &opts->aware_opts.channelpolicy);
                     n++;
                }
                if (opts->aware_opts.bitratepolicy_seen) {
                     AUpvsetparam(pvlist, n, AF_AWARE_PARAM_BITRATE_POLICY);
                     AUpvsetvaltype(pvlist, n, AU_PVTYPE_LONG);
                     AUpvsetval(pvlist, n, &opts->aware_opts.bitratepolicy);
                     n++;
                }
                if (opts->aware_opts.constqualnmr_seen) {
                     AUpvsetparam(pvlist,n,AF_MPEG_PARAM_CONST_QUAL_NMR);
                     AUpvsetvaltype(pvlist,n,AU_PVTYPE_DOUBLE);
                     AUpvsetval(pvlist,n,&opts->aware_opts.constqualnmr);
                     n++;
                }
                afInitCompressionParams(outfile_desc->setup,
                                        AF_DEFAULT_TRACK,
                                        opts->aware_opts.defaultconfig,
                                        pvlist,
                                        n);
            }
            else
#endif /*AWARE*/
            afInitCompression(outfile_desc->setup, AF_DEFAULT_TRACK,
                                  opts->outcompression);
            break;
        default:
        case DECOMPRESSAIFC:
            afInitCompression(outfile_desc->setup, AF_DEFAULT_TRACK,
                       AF_COMPRESSION_NONE);
            break;
    }

    nchannels   = afGetChannels(infile_desc->file, AF_DEFAULT_TRACK);
    afGetSampleFormat(infile_desc->file, AF_DEFAULT_TRACK, &sampfmt, &sampres);
    rate    = afGetRate(infile_desc->file, AF_DEFAULT_TRACK);
    haveaesdata = afGetAESChannelData(infile_desc->file, AF_DEFAULT_TRACK,0);
    nmarks      = afGetMarkIDs(infile_desc->file, AF_DEFAULT_TRACK,(int *)0);
    if (nmarks > 0)
    {
        markids     = (int *)calloc(nmarks, sizeof(int)); 
        afGetMarkIDs(infile_desc->file, AF_DEFAULT_TRACK, markids);
    }
    afInitChannels(outfile_desc->setup, AF_DEFAULT_TRACK, nchannels);
    afInitSampleFormat(outfile_desc->setup, AF_DEFAULT_TRACK, sampfmt, sampres);
    afInitRate(outfile_desc->setup, AF_DEFAULT_TRACK, rate);
    afInitMarkIDs(outfile_desc->setup, AF_DEFAULT_TRACK, markids, nmarks);

    if (haveaesdata)
    {
        afInitAESChannelData(outfile_desc->setup, AF_DEFAULT_TRACK);
    }
   
    for (i=0; i<nmarks; i++)
    {
      s = afGetMarkName(infile_desc->file, AF_DEFAULT_TRACK, markids[i]);
      afInitMarkName(outfile_desc->setup, AF_DEFAULT_TRACK, markids[i], s);
    } 
}

/****************************************************************************
 *
 *  S E T U P _ S A M P L E R
 *
 *****************************************************************************/
static void 
setup_sampler(file_desc_type *infile_desc, file_desc_type *outfile_desc,
                                            opts_type *opts)
{
    int instid;

    if (infile_desc->have_sampler_data) 
    {
       outfile_desc->have_sampler_data = 1;
       instid = AF_DEFAULT_INST;
       afInitInstIDs(outfile_desc->setup, &instid, 1);
  
       afInitLoopIDs(outfile_desc->setup, AF_DEFAULT_INST,
         infile_desc->sampler_desc.loopids, infile_desc->sampler_desc.nloops);
    }
    else 
    {
       outfile_desc->have_sampler_data =0;
       afInitInstIDs(outfile_desc->setup, (int *)0, 0);
    }
}

/****************************************************************************
 *
 * S E T U P _ M I S C
 *
 *****************************************************************************/
static void
setup_misc(file_desc_type *infile_desc, file_desc_type *outfile_desc, 
                                            opts_type *opts)
{
    int i;
    int *miscids;
  
    miscids = (int *)calloc(infile_desc->nmisc, sizeof(int));

    for (i=0; i<infile_desc->nmisc; i++)
    {
        miscids[i] = infile_desc->misc_desc_list[i].miscid;
    }

    afInitMiscIDs(outfile_desc->setup, miscids, infile_desc->nmisc);
    
    for (i=0; i<infile_desc->nmisc; i++)
    {
      afInitMiscSize(outfile_desc->setup, infile_desc->misc_desc_list[i].miscid,
                  infile_desc->misc_desc_list[i].miscsize);
      afInitMiscType(outfile_desc->setup, infile_desc->misc_desc_list[i].miscid,
                  infile_desc->misc_desc_list[i].misctype);
    }
}

/****************************************************************************
 *
 * O P E N _ O U T F I L E
 *
 *****************************************************************************/
static int
open_outfile(file_desc_type *outfile_desc, opts_type *opts)
{
   outfile_desc->filename = (char *)malloc(strlen(opts->outfilename) + 1);
   strcpy(outfile_desc->filename, opts->outfilename);

   outfile_desc->file =
      afOpenFile(outfile_desc->filename, "w", outfile_desc->setup);

   if (outfile_desc->file == AF_NULL_FILEHANDLE)
   {
      fprintf(stderr, "%s: failed to acquire audio file descriptor for %s\n",
         opts->myname, outfile_desc->filename);      
      return(-1);
   }  

   /*
    * test: read back the configuration parameters 
    */
   outfile_desc->track_desc.haveaesdata 
       = afGetAESChannelData(outfile_desc->file,AF_DEFAULT_TRACK,
                                                    (unsigned char *)0);
   outfile_desc->track_desc.nchannels 
       = afGetChannels(outfile_desc->file, AF_DEFAULT_TRACK);
   outfile_desc->track_desc.rate
       = afGetRate(outfile_desc->file, AF_DEFAULT_TRACK);

#ifndef AWARE
   outfile_desc->track_desc.compression 
       = afGetCompression(outfile_desc->file, AF_DEFAULT_TRACK);
#else /*AWARE*/
   afGetCompressionParams(outfile_desc->file, AF_DEFAULT_TRACK,
         &(outfile_desc->track_desc.compression), AU_NULL_PVLIST, 0);   
#endif /*AWARE*/

   afGetSampleFormat(outfile_desc->file, AF_DEFAULT_TRACK,
         &outfile_desc->track_desc.sampfmt, 
         &outfile_desc->track_desc.sampwidth);

   outfile_desc->filefmt = afGetFileFormat(outfile_desc->file, 
       &outfile_desc->filefmtvers);

   outfile_desc->have_sampler_data 
       = afGetInstIDs(outfile_desc->file, (int *)0);

   outfile_desc->nmisc 
       = afGetMiscIDs(outfile_desc->file, (int *)0);
   outfile_desc->miscids 
       = (int *)calloc(outfile_desc->nmisc, sizeof(int));

   return(0);
}

/****************************************************************************
 *
 * C O P Y _ T R A C K 
 *
 *****************************************************************************/
static int
copy_track(file_desc_type *infile_desc, file_desc_type *outfile_desc, 
                                                         opts_type *opts)
{
    int frames_per_chunk = 16384;
    int bytes_per_frame, bytes_per_sample;
    void *buf;
    int nframesxfered;
    int nframesread, nframes2read;
    int nframeswrote, nframes2write;
    int done = 0;
    unsigned char c[24];
    int i;
    int err;
    float percentdone;

    if (outfile_desc->track_desc.sampwidth <= 8) {
       bytes_per_sample = 1;
    }
    else if (outfile_desc->track_desc.sampwidth <= 16) {
       bytes_per_sample = 2;
    }
    else {
       bytes_per_sample = 4;
    }

    bytes_per_frame = bytes_per_sample * outfile_desc->track_desc.nchannels;

    buf = (char *)calloc(frames_per_chunk, bytes_per_frame);

    nframes2read  = frames_per_chunk;
    nframesxfered = 0;
    percentdone   = 0.0;
    err           = 0;
    
    if (opts->verbose) 
    {
        printf("%s: input file    = %s\n",opts->myname,infile_desc->filename);
        printf("%s: output file   = %s\n",opts->myname,outfile_desc->filename);
        printf("%s: transfer size = %d audio sample frames / block\n",
                                                   opts->myname, nframes2read);
    }

    while (!done & !caught_sigint) 
    {
        nframesread = afReadFrames(infile_desc->file, AF_DEFAULT_TRACK,
                                     buf, nframes2read);
        if (nframesread < nframes2read)
        {
            done=1;
        }
        nframes2write  = nframesread; 
        nframeswrote   = afWriteFrames(outfile_desc->file, AF_DEFAULT_TRACK,
                                      buf, nframes2write);

        nframesxfered += nframeswrote;
        percentdone = ((float)nframesxfered / 
                         (float)infile_desc->track_desc.nframes) * 100.0;
        if (opts->verbose)
        {
           printf("%s: %6.2f%% audio data copied\n", 
                                      opts->myname, percentdone);
        }
        if (nframeswrote < nframes2write)
        {
            fprintf(stderr, "%s: short write copying audio track data\n",
                 opts->myname);
            done=1;
            err=1;
        }
    }
    for (i=0; i<infile_desc->track_desc.nmarks; i++)
    {
        afSetMarkPosition(outfile_desc->file, AF_DEFAULT_TRACK,
            infile_desc->track_desc.markids[i], 
            infile_desc->track_desc.markpos[i]); 
    }

    if (infile_desc->track_desc.haveaesdata)
    {
        afGetAESChannelData(infile_desc->file, AF_DEFAULT_TRACK, c);
        afSetAESChannelData(outfile_desc->file, AF_DEFAULT_TRACK, c);
    }

    if (err)
    {
        return(-1);
    }
    return(0);
}

/****************************************************************************
 *
 * C O P Y _ S A M P L E R
 *
 *****************************************************************************/

static void 
copy_sampler(file_desc_type *infile_desc, file_desc_type *outfile_desc,
                                                 opts_type *opts)
{
     AFfilehandle file;
     sampler_desc_type *sampler_desc;  

     if (infile_desc->have_sampler_data)
     {     
       file         = outfile_desc->file;
       sampler_desc = &infile_desc->sampler_desc;
  
       /*
        * make sampler parameters in new file identical to sampler parameters
        * in old file
        */
       afSetInstParamLong(file, AF_DEFAULT_INST, AF_INST_MIDI_BASENOTE,
                        sampler_desc->midi_basenote);
       afSetInstParamLong(file, AF_DEFAULT_INST, AF_INST_MIDI_LONOTE,
                        sampler_desc->midi_lonote);
       afSetInstParamLong(file, AF_DEFAULT_INST, AF_INST_MIDI_HINOTE,
                        sampler_desc->midi_hinote);
       afSetInstParamLong(file, AF_DEFAULT_INST, AF_INST_MIDI_LOVELOCITY,
                        sampler_desc->midi_lovelocity);
       afSetInstParamLong(file, AF_DEFAULT_INST, AF_INST_MIDI_HIVELOCITY,
                        sampler_desc->midi_hivelocity);
       afSetInstParamLong(file, AF_DEFAULT_INST, AF_INST_NUMDBS_GAIN,
                        sampler_desc->numdbs_gain);
       afSetInstParamLong(file, AF_DEFAULT_INST, AF_INST_NUMCENTS_DETUNE,
                        sampler_desc->numcents_detune);
       afSetInstParamLong(file, AF_DEFAULT_INST, AF_INST_SUSLOOPID,
                        sampler_desc->sustain_loop_id);
       afSetInstParamLong(file, AF_DEFAULT_INST, AF_INST_RELLOOPID,
                        sampler_desc->release_loop_id);
  
       afSetLoopStart(file, AF_DEFAULT_INST, sampler_desc->sustain_loop_id,
                        sampler_desc->sustain_loop_start_mark_id);
       afSetLoopEnd(file, AF_DEFAULT_INST, sampler_desc->sustain_loop_id,
                        sampler_desc->sustain_loop_end_mark_id);
       afSetLoopMode(file, AF_DEFAULT_INST, sampler_desc->sustain_loop_id,
                        sampler_desc->sustain_loop_mode);
  
       afSetLoopStart(file, AF_DEFAULT_INST, sampler_desc->release_loop_id,
                        sampler_desc->release_loop_start_mark_id);
       afSetLoopEnd(file, AF_DEFAULT_INST, sampler_desc->release_loop_id,
                        sampler_desc->release_loop_end_mark_id);
       afSetLoopMode(file, AF_DEFAULT_INST, sampler_desc->release_loop_id,
                        sampler_desc->release_loop_mode);
     }
}


/****************************************************************************
 *
 * C O P Y _ M I S C
 *
 *****************************************************************************/
static int
copy_misc(file_desc_type *infile_desc, 
                  file_desc_type *outfile_desc, opts_type *opts)
{
    int chunksize = 32768;
    int i;
    int nbytes2read, nbytesread;
    int nbytes2write, nbyteswrote;
    int done=0;
    unsigned char *buf;
    int err;

    buf = (unsigned char *)malloc(chunksize);
    nbytes2read = chunksize;

    err=0;

    for (i=0; i<infile_desc->nmisc; i++)
    { 
        done = 0;

        while (!done)
        {
            nbytesread = afReadMisc(infile_desc->file, 
                             infile_desc->miscids[i], buf, nbytes2read);
               
            if (nbytesread < nbytes2read)
            {
                done = 1;
            }
            nbytes2write = nbytesread;
            nbyteswrote = afWriteMisc(outfile_desc->file, 
                             infile_desc->miscids[i], buf, nbytes2write);
 
            if (nbyteswrote != nbytes2write)
            {
                fprintf(stderr, "%s: short write on miscellaneous data\n",
                            opts->myname);
                done=1;
                err=1;
            }
        }
    }

    if (err)
    {
       return(-1);
    }
    return(0);
}

/****************************************************************************
 *
 * C L E A N U P _ I N F I L E
 *
 *****************************************************************************/
static void
cleanup_infile(file_desc_type *infile_desc, opts_type *opts)
{
    afCloseFile(infile_desc->file);
}

/****************************************************************************
 *
 * C L E A N U P _ O U T F I L E
 *
 *****************************************************************************/
static int
cleanup_outfile(file_desc_type *outfile_desc, opts_type *opts)
{
    int err;

    afFreeFileSetup(outfile_desc->setup);
    err = afCloseFile(outfile_desc->file);

    if (err < 0)
    {
        return(-1);
    }
    return(0);
}

static void
init_error_reporting(opts_type *opts)
{
    afSetErrorHandler(catchAFerror);
    print_error(opts, (char *)0); 
}

/*
 * error-reporting callback routine for audio file library
 */
static void
catchAFerror(long code, const char *s)
{
    static char t[512];

     sprintf(t, "Audio File Library error (%d) %s", code, s);
     print_error((opts_type *)0, t);
}


/*
 * print out error message
 */
static void
print_error(opts_type *opts, const char *t)
{
    static char myname[80];

    if (opts != (opts_type *)0)
    {
        strcpy(myname, opts->myname);
    }
    else if (t != (char *)0)
    {
        fprintf(stderr, "%s: %s\n", myname, t); 
    } 
}
