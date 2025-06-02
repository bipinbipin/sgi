/****************************************************************\
 * 	File:		recdvaudio.c				*
 *								*
 *	Simple test program which attempts to rec DV	        *
 *	audio using the dmDV interface.				*
 *								*
 * cc -o recdvaudio recdvaudio.c -laudio -ldmedia -laudiofile   *
 *                                                              *
\****************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dmedia/audio.h>
#include <audiofile.h>
#include <dmedia/dmedia.h>
#include <dmedia/dm_audioutil.h>


#define MAX_ALPARAMS 100

void display_params(  DMparams   *params)
{
  int val;

  printf("DM_DVAUDIO_TYPE: \t\t");
  switch (dmParamsGetInt(params, DM_DVAUDIO_TYPE)) {
  case DM_DVAUDIO_DV:
    printf("DM_DVAUDIO_DV\n");
    break;
  case DM_DVAUDIO_DVCPRO : 
    printf("DM_DVAUDIO_DVCPRO\n");
    break;
  default:
    printf("Illegal DM_DVAUDIO_TYPE" );
  }  

  printf("DM_DVAUDIO_FORMAT: \t\t");
  switch (dmParamsGetInt(params, DM_DVAUDIO_FORMAT)) {
  case DM_DVAUDIO_PAL:
    printf("DM_DVAUDIO_PAL\n");
    break;
  case DM_DVAUDIO_NTSC : 
    printf("DM_DVAUDIO_NTSC\n");
    break;
  default:
    printf("Illegal DM_DVAUDIO_FORMAT" );
  }

  printf("DM_AUDIO_CODEC_FRAMES_PER_BLOCK:%d\n",
	 dmParamsGetInt(params, DM_AUDIO_CODEC_FRAMES_PER_BLOCK));
  printf("DM_DVAUDIO_MIN_FRAMES_PER_BLOCK:%d\n",
	 dmParamsGetInt(params, DM_DVAUDIO_MIN_FRAMES_PER_BLOCK));
  printf("DM_AUDIO_CODEC_MAX_BYTES_PER_BLOCK:%d\n",
	 dmParamsGetInt(params, DM_AUDIO_CODEC_MAX_BYTES_PER_BLOCK));
  printf("DM_AUDIO_CODEC_FILTER_DELAY:\t%d\n",
	 dmParamsGetInt(params, DM_AUDIO_CODEC_FILTER_DELAY));
  printf("DM_DVAUDIO_VERBOSE:    \t\t%d\n",
	 dmParamsGetInt(params, DM_DVAUDIO_VERBOSE));
  printf("DM_DVAUDIO_LOCK_MODE: \t\t%d\n",
	 dmParamsGetInt(params, DM_DVAUDIO_LOCK_MODE));
  printf("DM_AUDIO_CHANNELS: \t\t%d\n",
	 dmParamsGetInt(params, DM_AUDIO_CHANNELS));  
  printf("DM_AUDIO_WIDTH:    \t\t%d\n",
	 dmParamsGetInt(params, DM_AUDIO_WIDTH));

  printf("DM_DVAUDIO_CHANNEL_MODE: \t");
  switch (dmParamsGetInt(params, DM_DVAUDIO_CHANNEL_MODE)) {
  case DM_DVAUDIO_SD_2CH:
    printf("DM_DVAUDIO_SD_2CH\n");
    break;
  case DM_DVAUDIO_SD_4CH : 
    printf("DM_DVAUDIO_SD_4CH\n");
    break;
  default:
    printf("Illegal DM_DVAUDIO_CHANNEL_MODE" );
  }

  printf("DM_DVAUDIO_CHANNEL_POLICY: \t");
  switch (dmParamsGetInt(params, DM_DVAUDIO_CHANNEL_POLICY)) {
  case DM_DVAUDIO_SD_2CH_STEREO:
    printf("DM_DVAUDIO_SD_2CH_STEREO\n");
    break;
  case DM_DVAUDIO_SD_2CH_2CH_MONO : 
    printf("DM_DVAUDIO_SD_2CH_2CH_MONO\n");
    break;
  case DM_DVAUDIO_SD_2CH_MONO :
    printf("DM_DVAUDIO_SD_2CH_MONO\n");
    break;

  case DM_DVAUDIO_SD_4CH_STEREO_STEREO :
    printf("DM_DVAUDIO_SD_4CH_STEREO_STEREO\n");
    break;
  case DM_DVAUDIO_SD_4CH_STEREO_2CH_MONO :
    printf("DM_DVAUDIO_SD_4CH_STEREO_2CH_MONO\n");
    break;
  case DM_DVAUDIO_SD_4CH_STEREO_1CH_MONO :
    printf("DM_DVAUDIO_SD_4CH_STEREO_1CH_MONO\n");
    break;
  case DM_DVAUDIO_SD_4CH_STEREO :
    printf("DM_DVAUDIO_SD_4CH_STEREO\n");
    break;
  case DM_DVAUDIO_SD_4CH_2CH_MONO_STEREO :
    printf("DM_DVAUDIO_SD_4CH_2CH_MONO_STEREO\n");
    break;
  case DM_DVAUDIO_SD_4CH_4CH_MONO :
    printf("DM_DVAUDIO_SD_4CH_4CH_MONO\n");
    break;
  case DM_DVAUDIO_SD_4CH_3CH_MONO_1 :
    printf("DM_DVAUDIO_SD_4CH_3CH_MONO_1\n");
    break;
  case DM_DVAUDIO_SD_4CH_2CH_MONO_1 :
    printf("DM_DVAUDIO_SD_4CH_2CH_MONO_1\n");
    break;
  case DM_DVAUDIO_SD_4CH_1CH_MONO_STEREO :
    printf("DM_DVAUDIO_SD_4CH_1CH_MONO_STEREO\n");
    break;
  case DM_DVAUDIO_SD_4CH_3CH_MONO_2 :
    printf("DM_DVAUDIO_SD_4CH_3CH_MONO_2\n");
    break;
  case DM_DVAUDIO_SD_4CH_2CH_MONO_2 :
    printf("DM_DVAUDIO_SD_4CH_2CH_MONO_2\n");
    break;
  case DM_DVAUDIO_SD_4CH_1CH_MONO :
    printf("DM_DVAUDIO_SD_4CH_1CH_MONO\n");
    break;
  case DM_DVAUDIO_SD_4CH_3_1_STEREO :
    printf("DM_DVAUDIO_SD_4CH_3_1_STEREO\n");
    break;
  case DM_DVAUDIO_SD_4CH_3_0_STEREO_1CH_MONO :
    printf("DM_DVAUDIO_SD_4CH_3_0_STEREO_1CH_MONO\n");
    break;
  case DM_DVAUDIO_SD_4CH_3_0_STEREO :
    printf("DM_DVAUDIO_SD_4CH_3_0_STEREO\n");
    break;
  case DM_DVAUDIO_SD_4CH_2_2_STEREO :
    printf("DM_DVAUDIO_SD_4CH_2_2_STEREO\n");
    break;
  default:
    printf("Illegal DM_DVAUDIO_CHANNEL_POLICY" );
  }

  printf("DM_MEDIUM: \t\t\t");
  switch (dmParamsGetEnum(params, DM_MEDIUM)) {
  case DM_IMAGE:
    printf("DM_IMAGE\n");
    break;
  case DM_AUDIO : 
    printf("DM_AUDIO\n");
    break;
  case DM_TIMECODE : 
    printf("DM_TIMECODE\n");
    break;
  case DM_TEXT : 
    printf("DM_TEXT\n");
    break;
  default:
    printf("Illegal DM_MEDIUM" );
  }
    

  printf("DM_AUDIO_BYTE_ORDER: \t\t");
  switch (dmParamsGetEnum(params, DM_AUDIO_BYTE_ORDER)) {
  case DM_AUDIO_BIG_ENDIAN:
    printf("DM_AUDIO_BIG_ENDIAN\n");
    break;
  case DM_AUDIO_LITTLE_ENDIAN : 
    printf("DM_AUDIO_LITTLE_ENDIAN\n");
    break;
  default:
    printf("Illegal DM_AUDIO_BYTE_ORDER" );
  }
 
  printf("DM_AUDIO_FORMAT: \t\t");
  switch (dmParamsGetEnum(params, DM_AUDIO_FORMAT)) {
  case DM_AUDIO_TWOS_COMPLEMENT:
    printf("DM_AUDIO_TWOS_COMPLEMENT\n");
    break;
  case DM_AUDIO_UNSIGNED : 
    printf("DM_AUDIO_UNSIGNED\n");
    break;
  case DM_AUDIO_FLOAT : 
    printf("DM_AUDIO_FLOAT\n");
    break;
  case DM_AUDIO_DOUBLE : 
    printf("DM_AUDIO_DOUBLE\n");
    break;
  default:
    printf("Illegal DM_AUDIO_FORMAT" );
  }
 
  printf("DM_AUDIO_RATE:    \t\t%f\n",dmParamsGetFloat(params, DM_AUDIO_RATE));

  printf("----------------------------------------------------------\n");
  
}

void
main(int argc, char **argv)
{
  FILE         *fp;
  FILE         *fpo;
  FILE         *audio_fp;
  DIFframe	frame;
  int		numchans = 2;
  int		verbose  = 0;
  int		numsampframes;
  int		numDIFS;
  int		c;
  int		errflag = 0;
  int		dev = AL_DEFAULT_INPUT;
  float	        rate;
  ALconfig	config;
  ALport	port;
  ALpv	        pv[2];
  DMparams     *params;
  long          alparams[MAX_ALPARAMS];
  DMDVaudioencoder encoder;
  short        *samples;
  int           samplesPerFrame;
  char          outfilename[128];
  char          audiofilename[128];
  int           frame_count = 0;
  int           inputfile = 0;
  int           n;
  AFfilehandle  afile;
  AFfilesetup   setup = NULL;
  int           length = 0;
  int           errorNumber;
  char          detail[DM_MAX_ERROR_DETAIL];
  char         *msg;

  while ((c = getopt(argc, argv, "hvc:d:m:i:l:")) != -1) {
    switch (c) {
    case 'v':
      verbose = 1;
      break;

    case 'l':
      length = atoi(optarg);
      break;

    case 'i':
      strcpy(audiofilename,optarg);
      inputfile = 1;
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
				AL_INPUT_DEVICE_TYPE);
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
	fprintf(stderr, "ERROR: '%s' is not a valid audio input "
		"device.\n", optarg);
	exit(1);
      }
      break;

    case 'h':
    case '?':
      errflag++;
      break;
    }
  }
   
  if ((argc - optind) > 2)
    errflag++;
	 
  if (errflag) {
    fprintf(stderr, "Usage: %s [-v] [-c numchans] [-i audiofile] [DV raw infile] [DV raw outfile]\n",
	    argv[0]);
    fprintf(stderr, "           -v  = verbose\n");
    exit(1);
  }

  if (optind == (argc-1)) {
    fp = stdin;
    sprintf(outfilename,"%s",argv[optind]);
    if (verbose) printf("input  file = stdin\n");
    if (verbose) printf("output file = %s\n",outfilename);    
  } else {
    sprintf(outfilename,"%s",argv[optind+1]);
    if (verbose) printf("output file = %s\n",outfilename);
    if ((fp = fopen(argv[optind], "r")) == NULL) {
      fprintf(stderr, "Cannot open file '%s' ", argv[optind]);
      perror(NULL);
      exit(1);
    }
  }

  /* Set up the codec */
  if (dmDVAudioEncoderCreate(&encoder) == DM_FAILURE) {
    int error;
    char detail[1000];

    dmGetError(&error, detail);
    fprintf(stderr, "Audio Encoder Create failed: %s\n", detail);
  }

  if (inputfile) {
    afile = afOpenFile(audiofilename,"r",setup);
    rate = afGetRate(afile,AF_DEFAULT_TRACK);
    if (verbose) printf("rate=%f\n",rate);
  }
  else {
    /* Configure that audio port */
    config = alNewConfig();
    alSetSampFmt(config, AL_SAMPFMT_TWOSCOMP);
    alSetWidth(config, AL_SAMPLE_16);
    alSetChannels(config, numchans); 
    alSetDevice(config, dev);

    port = alOpenPort("recdvaudio", "r", config);
    if (port == NULL) {
      fprintf(stderr, "ERROR: Cannot open audio port, error code %d", 
	      oserror());
      exit(1);
    }
  }

  /* Now, we read a frame, check the sample rate, and change the 
   * device sample rate as necessary.
   */
  if (fread(frame, sizeof(DIFsequence), 12, fp) != 12) {
    fprintf(stderr, "ERROR: could not read the first frame.\n");
    exit(1);
  }

  dmParamsCreate(&params);

  if (dmDVAudioEncoderGetParams(encoder, params) == DM_FAILURE) {
    fprintf(stderr, "dmDVAudioEncoderGetParams failed.\n");
    exit(1);
  }

  if (verbose) {
    printf("default params:\n");
    display_params(params);
  }

  /* initialized params with frame default frame info. */
  dmDVAudioHeaderGetParams(frame, params, &samplesPerFrame);

  numDIFS = dmParamsGetInt(params, DM_DVAUDIO_FORMAT);
  numsampframes = dmParamsGetInt(params, DM_AUDIO_CODEC_FRAMES_PER_BLOCK);

  /* Fix up the file pointer if we're not in PAL mode */
  if (numDIFS == DM_DVAUDIO_NTSC) {
    fseek64(fp, (long long) numDIFS * sizeof(DIFsequence), SEEK_SET); 
    if (verbose) printf("encoding NTSC\n");
  } else {
    if (verbose) printf("encoding PAL\n");
  }


  if (!inputfile) {
    /* Get the rate from the audio device*/
    pv[0].param = AL_RATE;
    if (alGetParams(dev, pv, 1) < 0) {
      printf("alGetParams failed: %s\n", alGetErrorString(oserror()));
      exit(-1);
    }
    else {
      rate = (float) alFixedToDouble(pv[0].value.ll);
    }
  }

  /* Tell the codec what kind of output we want */
  dmParamsSetInt(params, DM_AUDIO_CHANNELS, numchans);
  dmParamsSetInt(params, DM_DVAUDIO_VERBOSE, verbose);
  dmDVAudioEncoderSetParams(encoder, params);
  dmParamsSetFloat(params, DM_AUDIO_RATE, rate);

  if ((samples = malloc(sizeof(short) * numsampframes * numchans)) == NULL) {
    fprintf(stderr, "Cannot malloc sample buffer\n");
    alClosePort(port);
    exit(1);	
  }

  fpo = fopen(outfilename,"wb");

  if (verbose) {
    printf("codec params:\n");
    display_params(params);
  }

  /* Okay, now, let 'er rip */
  do {

    dmDVAudioEncoderGetFrameSize(encoder, &samplesPerFrame);

    if (verbose) printf("Processing Frame: % 5d FrameSize=%d\n",
			frame_count,samplesPerFrame);
    else
      printf("Processing Frame: % 5d FrameSize=%d\r",
			frame_count,samplesPerFrame);

    if (inputfile) {
      n = AFreadframes(afile,AF_DEFAULT_TRACK,samples,
                     (int) samplesPerFrame);
    }
    else 
      alReadFrames(port, samples, samplesPerFrame);

    dmDVAudioEncode(encoder, samples, frame, &samplesPerFrame);

    fwrite(&frame,sizeof(char),sizeof(DIFsequence)*numDIFS,fpo);

    frame_count++;

    if ((length > 0) && (frame_count == length)) exit(0);

  } while (fread(frame, sizeof(DIFsequence), numDIFS, fp) == numDIFS);

  if (inputfile) 
    afCloseFile(afile);
  else 
    alClosePort(port);

  fclose(fp);
  fclose(fpo);

  printf("\n");
  exit(0);
}
