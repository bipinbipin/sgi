#include <audio.h>

long long device_delta_msc;	/* frame number delta: output - input */

/*
 * This is a simple example of synchronization between two audio
 * ports. 
 * 
 * This example schedules a fixed latency between input and output.
 */

/*
 * sync_ports
 *
 * drop or pad data until the next sample frame to be written on
 * output is "offset" sample frames after the next sample frame to
 * be read from the input.
 */
int
sync_ports(ALport ip, ALport op, long long offset)
{
	long long imsc, iust;
	long long omsc, oust, omsc1;
	double nanosec_per_frame = (1000000000.0)/(48000.0);
	long long buf_delta_msc;

	/*
	 * Now find out what the MSC offset is between the input device and
	 * the output device. This is constant as long as we have the ports
	 * open and as long as they are connected to the same devices, even
	 * if the ports underflow or overflow. Note that if we use default
	 * input or default output devices, when a default device is changed
	 * this offset will need to be recalculated.
	 */
	alGetFrameTime(ip, &imsc, &iust);
	alGetFrameTime(op, &omsc, &oust);

	/* 
	 * get the output sample frame number associated with the same time
	 * (iust) as the input sample frame number we have (imsc). Division
	 * here should yield more accurate results.
	 */
	omsc1 = omsc + (long long)((double) (iust - oust) / (nanosec_per_frame));

	/*
	 * Now just subtract imsc from omsc1, yielding the delta MSC between
	 * simultaneous sample frames at the input and output devices.
	 */
	device_delta_msc = omsc1 - imsc;

	do {
		/*
		 * Check for underflow/overflow. If we've lost data,
		 * the sample frame number will not have incremented by
		 * the same number of frames as we've read or written
		 */
		alGetFrameNumber(ip, &imsc);
		alGetFrameNumber(op, &omsc);

		/*
		 * Relate the two buffers.
		 * buf_delta_msc is how many sample frames elapse between when
		 * the input buffer reached the input jacks and when the output
		 * buffer will reach the output jacks. This number may be positive 
		 * or negative. 
		 */
		buf_delta_msc = (omsc - imsc) - device_delta_msc;

		if (buf_delta_msc > offset) {
			alDiscardFrames(ip, buf_delta_msc-offset);
		}
		else if (buf_delta_msc < offset) {
			alZeroFrames(op, offset-buf_delta_msc);
		}
		printf("bdm = %lld\n", buf_delta_msc);
	} while (buf_delta_msc != offset);

	return 0;
}

/*
 * check_sync
 *
 * Call this every once in a while to see if we've lost sync.
 *
 * Changing the sample rates on either port, or underflow or 
 * overflow on either port will cause us to lose sync.
 * If we lose sync, this function will automatically call sync_ports
 * to re-synchronize.
 */

int
check_sync(ALport ip, ALport op, long long offset)
{
	long long imsc;
	long long omsc;
	long long buf_delta_msc;

	/*
	 * Check for underflow/overflow. If we've lost data,
	 * the sample frame number will not have incremented by
	 * the same number of frames as we've read or written
	 */
	alGetFrameNumber(ip, &imsc);
	alGetFrameNumber(op, &omsc);

	/*
	 * Relate the two buffers.
	 * buf_delta_msc is how many sample frames elapse between when
	 * the input buffer reached the input jacks and when the output
	 * buffer will reach the output jacks. This number may be positive 
	 * or negative. 
	 */
	buf_delta_msc = (omsc - imsc) - device_delta_msc;

	if (buf_delta_msc != offset) {
		sync_ports(ip,op,offset);
		return 1;
	}
	return 0;
}

main(int argc, char **argv)
{
	int src, dest;
	ALport ip, op;
	ALconfig c;
	short ibuf[10000], obuf[10000];
	ALpv pv[2];

	if (argc != 3) {
		printf("usage: %s <src> <dest>\n",argv[0]);
		exit(-1);
	}

	/*
	 * Get the resource ID's associated with the input and output
	 * devices specified.
	 */
	src = alGetResourceByName(AL_SYSTEM, argv[1], AL_DEVICE_TYPE);
	if (!src) {
		printf("invalid device %s\n", argv[1]);
		exit(-1);
	}
	dest = alGetResourceByName(AL_SYSTEM, argv[2], AL_DEVICE_TYPE);
	if (!dest) {
		printf("invalid device %s\n", argv[2]);
		exit(-1);
	}

	c = alNewConfig();
	if (!c) {
		printf("alNewConfig failed: %s\n", alGetErrorString(oserror()));
		exit(-1);
	}

	/*
	 * open two ports, ip and op, associated with devices src and dest, 
	 * respectively.
	 */
	alSetDevice(c, src);
	alSetQueueSize(c, 10000);
	ip = alOpenPort("test input", "r", c);
	if (!ip) {
		printf("input openport failed: %s\n", alGetErrorString(oserror()));
		exit(-1);
	}

	alSetDevice(c, dest);
	op = alOpenPort("test output", "w", c);
	if (!op) {
		printf("output openport failed: %s\n", alGetErrorString(oserror()));
		exit(-1);
	}

	/*
	 * Set the sample-rates on both devices. This program later assumes that 
	 * the devices are using exactly the same rate, relative to the same
	 * master clock.
	 */
	pv[0].param = AL_RATE;
	pv[0].value.ll = alDoubleToFixed(48000.0);
	pv[1].param = AL_MASTER_CLOCK;
	pv[1].value.i = AL_CRYSTAL_MCLK_TYPE;
	alSetParams(src, pv, 2);
	alSetParams(dest, pv, 2);

	/*
	 * Prime the output with some data so that alGetFrameNumber returns
	 * a useful result. It doesn't work if we're in underflow. This call
	 * shouldn't block, since our queue size is 10000 frames.
	 */
	alZeroFrames(op, 5000);

	sync_ports(ip,op,512);

	/*
	 * Clear output buf so we write silence. We could use alZeroFrames
	 * instead but using alWriteFrames makes this more like a real application.
	 */
	bzero(obuf, sizeof(obuf));

	while (1) {

		alWriteFrames(op, obuf, 5000);
		alReadFrames(ip, obuf, 5000);
		check_sync(ip,op,512);
	}
}
