/****************************************************************\
 * 	File:		playdvaudio.c				*
 *								*
 *	Simple test program which attempts to play back DV	*
 *	audio using the dmDV interface.				*
 *								*
 * cc -o playdvaudio playdvaudio.c -laudio -ldmedia -laudiofile *
 *                                                              *
\****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dmedia/audio.h>
#include <dmedia/dmedia.h>
#include <dmedia/audiofile.h>
#include <dmedia/dm_audioutil.h>

void
main(int argc, char **argv)
{
  FILE       *fp;
  DIFframe	frame;
  int		numchans = 2;
  int		verbose  = 0;
  int		numsampframes;
  int		numDIFS;
  int		c;
  int		errflag = 0;
  int		dev = AL_DEFAULT_OUTPUT;
  float	rate;
  ALconfig	config;
  ALport	port;
  ALpv	pv[2];
  DMparams   *params;
  DMDVaudiodecoder decoder;
  short      *samples;
  int         samplesPerFrame;
  int         loopMode = 0;
  int         length = 0;
  int         i;
  int         num_frames;
  int         frame_count = 0;
  char        outfile[128];
  int         save_to_file = 0;
  AFfilehandle handle;
  int         fileformat;
  AFfilesetup filesetup;

  while ((c = getopt(argc, argv, "Lvc:d:m:l:o:")) != -1) {
    switch (c) {
    case 'v':
      verbose = 1;
      break;

    case 'L':
      loopMode = 1;
      break;

    case 'l':
      length = atoi(optarg);
      break;

    case 'o':
      strcpy (outfile,argv[optind-1]);
      save_to_file = 1;
      break;

    case 'c':
      numchans = atoi(optarg);
      if (numchans != 1 && numchans != 2 && numchans != 4) {
	fprintf(stderr, "Error: only 1, 2, and 4 channels are"
		" supported.\n");
	exit(1);
      }
      break;

    case 'd':
      dev = alGetResourceByName(AL_SYSTEM, optarg, 
				AL_OUTPUT_DEVICE_TYPE);
      if (dev) {
	int itf = alGetResourceByName(AL_SYSTEM, optarg, 
				      AL_INTERFACE_TYPE);
	if (itf) {
	  ALpv pv;
	  pv.param = AL_INTERFACE;
	  pv.value.i = itf;
	  alSetParams(dev, &pv, 1);
	}
      } else {
	fprintf(stderr, "ERROR: '%s' is not a valid audio output "
		"device.\n", optarg);
	exit(1);
      }
      break;

    case '?':
      errflag++;
      break;
    }
  }
   
  if ((argc - optind) > 1)
    errflag++;
	 
  if (errflag) {
    fprintf(stderr, "Usage: %s [-v] [-c numchans] [-L] [-l frames] [DV raw file]\n",argv[0]);
    exit(1);
  }

  if (optind == argc) {
    fp = stdin;
  } else {
    if ((fp = fopen(argv[optind], "r")) == NULL) {
      fprintf(stderr, "Cannot open file '%s' ", argv[optind]);
      perror(NULL);
      exit(1);
    }
  }



  /* Configure that audio port */
  config = alNewConfig();
  alSetSampFmt(config, AL_SAMPFMT_TWOSCOMP);
  alSetWidth(config, AL_SAMPLE_16);
  alSetChannels(config, numchans); 
  alSetDevice(config, dev);

  port = alOpenPort("playdvaudio", "w", config);
  if (port == NULL) {
    fprintf(stderr, "ERROR: Cannot open audio port, error code %d", 
	    oserror());
    exit(1);
  }


  /* Now, we read a frame, check the sample rate, and change the 
   * device sample rate as necessary.
   */
  if (fread(frame, sizeof(DIFsequence), 12, fp) != 12) {
    fprintf(stderr, "ERROR: could not read the first frame.\n");
    exit(1);
  }

  dmParamsCreate(&params);

  if (dmDVAudioHeaderGetParams(frame, params, &samplesPerFrame) == DM_FAILURE) {
    fprintf(stderr, "This DV file does not appear to contain any audio.\n");
    exit(1);
  }

  rate    = dmParamsGetFloat(params, DM_AUDIO_RATE);
  numDIFS = dmParamsGetInt(params, DM_DVAUDIO_FORMAT);
  numsampframes = dmParamsGetInt(params, DM_AUDIO_CODEC_FRAMES_PER_BLOCK);


  if (save_to_file) {
    filesetup = afNewFileSetup();
    afInitFormatParams( filesetup, AF_DEFAULT_TRACK, params);
    handle = afOpenFile(outfile,"w",filesetup);
  }

  /* Fix up the file pointer if we're not in PAL mode */
  if (numDIFS == DM_DVAUDIO_NTSC) {
    fseek64(fp, (long long) numDIFS * sizeof(DIFsequence), SEEK_SET); 
    printf("decoding NTSC, freq = %5.1f Hz\n",rate);

  } else {
    printf("decoding PAL , freq = %5.1f Hz\n",rate);
  }

  /* Set up the codec */
  if (dmDVAudioDecoderCreate(&decoder) == DM_FAILURE) {
    int error;
    char detail[1000];

    dmGetError(&error, detail);
    fprintf(stderr, "Decoder Create failed: %s\n", detail);
  }

  /* Set the rate */
  pv[0].param = AL_RATE;
  pv[0].value.ll = alDoubleToFixed(rate);
  pv[1].param = AL_MASTER_CLOCK;
  pv[1].value.i = AL_CRYSTAL_MCLK_TYPE;
  alSetParams(dev, pv, 2);

  /* Tell the codec what kind of output we want */
  dmParamsSetInt(params, DM_AUDIO_CHANNELS, numchans);
  dmParamsSetInt(params, DM_DVAUDIO_VERBOSE, verbose);
  dmDVAudioDecoderSetParams(decoder, params);

  if ((samples = malloc(sizeof(short) * numsampframes * numchans)) == NULL) {
    fprintf(stderr, "Cannot malloc sample buffer\n");
    alClosePort(port);
    exit(1);	
  }


  if (loopMode) {
    for (;;) {
      fseek(fp,0L,SEEK_SET);
      for (i=0;i<length;i++) {
	fread(frame, sizeof(DIFsequence), numDIFS, fp);
	dmDVAudioDecode(decoder, frame, samples, &num_frames);
	alWriteFrames(port, samples, num_frames);
      }
    }
  }
  else {
    /* Okay, now, let 'er rip */
    do {

      frame_count++;

      dmDVAudioDecode(decoder, frame, samples, &num_frames);
      alWriteFrames(port, samples, num_frames);

      if (save_to_file) afWriteFrames(handle, AF_DEFAULT_TRACK, samples,
				      num_frames);

      printf("decoding frame : % d \r",frame_count);
      fflush(stdout);
      if ((length>0) && (length == frame_count)) {
	printf("\nframe=%d exiting\n",frame_count);
	break;
      }
    } while (fread(frame, sizeof(DIFsequence), numDIFS, fp) == numDIFS);
  }

  printf("waiting for port to drain...\n");
  /* Wait for the port to drain */
  while(alGetFilled(port) > 0)
    usleep(20000);

  alClosePort(port);
  if (save_to_file) afCloseFile(handle);

  exit(0);
}
