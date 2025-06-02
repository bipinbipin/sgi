/*
 * events - Example program that handles selected Audio events.
 * events [-h][-r <resource>][-t <timeout>] param
 *              
 * 
 */
#include <stdio.h>
#include <unistd.h>
#include <stropts.h>
#include <poll.h>

#include <audio.h>

int getParam(char *);
int getResource(char *);
int handleEvent(ALevent);
void cleanup(ALeventQueue, ALevent);
void printHelp(char *);

void main(int argc, char **argv)
{
    ALeventQueue  eventq;
    ALevent       event;

    char          resName[50];
    int           resource = AL_DEFAULT_OUTPUT;
    char          *paramName;
    int           param;
    
    fd_set        rd_fds;
    int           eventqfd;
    struct pollfd fds;
    int           timeout;
    int           c;
    int           err;

    /*
     * Handle command line arguments.
     */
    strcpy(resName,"default.out");
    timeout = 120000; /* 2 minute timeout */

    while((c = getopt(argc,argv,"ht:r:")) != -1) {
	switch(c) {
	case 't':
	    timeout = atoi(optarg);
	    break;
	case 'r':
	    strcpy(resName,optarg);
	    break;
	case 'h':
	case '?':
	    printHelp(argv[0]);
	}
    }

    if(optind >= argc) {
	printHelp(argv[0]);
	exit(1);
    }
    
    paramName = argv[optind];
  
	    
    /*
     * Figure out what kind of resource we are dealing with
     */

    resource = getResource(resName);
    if(!resource) {
	fprintf(stderr,"%s: Error getting resource: %s\n",
		argv[0],alGetErrorString(oserror()));
	exit(1);
    }

    /*
     * Figure out what event we are dealing with
     */
    
    param = getParam(paramName);
    if(!param) {
	fprintf(stderr,"%s: Unsupported param\n",argv[0]);
	exit(1);
    }

    /*
     * Open event queue and allocate event structure
     */

    eventq = alOpenEventQueue("Event Test");
    if(eventq == NULL) {
	fprintf(stderr,"%s: Error Opening Event Queue\n",argv[0],
		alGetErrorString(oserror()));
	exit(1);
    }

    /*
     * Allocate event structure 
     */

    event = alNewEvent();
    if(event == NULL) {
	fprintf(stderr,"%s: Error Allocating Event\n",argv[0],
		alGetErrorString(oserror()));
	cleanup(eventq,NULL);
	exit(1);
    }
    
    /*
     * Select events to receive from event queue
     */
    err = alSelectEvents(eventq,resource,&param,1);
    if(err) {
	fprintf(stderr,"%s: Error Allocating Event\n",argv[0],
		alGetErrorString(oserror()));
	cleanup(eventq,event);
	exit(1);
    }

    /*
     * Get file descriptor from event queue to poll
     */
    eventqfd = alGetEventQueueFD(eventq);
    if(eventqfd == -1) {
	fprintf(stderr,"%s: Invalid file descriptor\n",argv[0],
		alGetErrorString(oserror()));
	exit(1);
    }

    /* 
     * Start handling events
     */
    fds.fd = eventqfd;
    fds.events = POLLIN;

    while(1) {
	err = poll(&fds,1,timeout);
	if(err == 0)  /* timeout occurred */
	    break;
	alNextEvent(eventq,event);
	handleEvent(event);
    }
    fprintf(stdout,"%s: Timeout occurred.  Exiting...\n",argv[0]);
    cleanup(eventq,event);

    exit(0);
}
    
    
int getParam(char *name)
{
    if(!strcmp(name,"AL_LABEL"))
	return AL_LABEL;
    if(!strcmp(name,"AL_PORTS"))
	return AL_PORTS;
    if(!strcmp(name,"AL_CONNECTIONS"))
	return AL_CONNECTIONS;
    if(!strcmp(name,"AL_RATE"))
	return AL_RATE;
    else 
	return -1;
}

/*
 * Right now only handle the following resources:
 * 1. System resource: AL_SYSTEM
 * 2. Device resource: AnalogOut, AnalogIn
 * 3. Default input and Default Output
 */
int getResource(char *name)
{
    if(!strcmp(name,"system"))
	return AL_SYSTEM;
    else if(!strcmp(name,"default.in"))
	return AL_DEFAULT_INPUT;
    else if(!strcmp(name,"default.out"))
	return AL_DEFAULT_OUTPUT;
    else 
	return alGetResourceByName(AL_SYSTEM,name,AL_DEVICE_TYPE);
}

#define MAX_LABEL_SIZE 64

int handleEvent(ALevent event)
{
    int             param;
    ALsetChangeInfo *set_info;
    ALvalue         value;
    ALpv            pv;
    char            label[MAX_LABEL_SIZE];
    int             ret;

    param = alGetEventParam(event);

    /*
     * The event data retrieval will vary depending on
     * the event param.
     */

    switch(param) {
    case AL_LABEL:
	fprintf(stdout,"Event Param: AL_LABEL\n");
	/*
	 * Use alGetParams to retrieve param, see alParams.3a
	 */
	pv.param = AL_LABEL;
	pv.value.ptr = label;
	pv.sizeIn = MAX_LABEL_SIZE;

	if(alGetParams(alGetEventSrcResource(event),&pv,1) < 0) {
	    fprintf(stdout,"handleEvent: alGetParams failed: %s\n",
		    alGetErrorString(oserror()));
	}
	else 
	    fprintf(stdout,"Event Label: %s\n",label);
	break;
    case AL_RATE:
	fprintf(stdout,"Event Param: AL_RATE\n");
	value = alGetEventValue(event);
	fprintf(stdout,"Event Rate: %lf\n",alFixedToDouble(value.ll));
	break;
    case AL_CONNECTIONS:
	fprintf(stdout,"Event Param: AL_CONNECTIONS\n");
    case AL_PORTS:
	if(param == AL_PORTS)
	    fprintf(stdout,"Event Param: AL_PORTS\n");
	set_info = (ALsetChangeInfo *)alGetEventData(event);
	fprintf(stdout,"Event Data: Resource:\t%d\n",set_info->val.i);
	fprintf(stdout,"Event Data: total:\t%d\n",set_info->total);
	fprintf(stdout,"Event Data: delta: %d\n",set_info->pchange);
	break;
    default:
	fprintf(stdout,"Don't know how to handle event\n");
	return -1;
    }
    
    fprintf(stdout,"Event Resource:\t\t%d\n",alGetEventResource(event));
    fprintf(stdout,"Event Source Resource:\t%d\n",alGetEventSrcResource(event));
    fprintf(stdout,"Event UST:\t\t%ld\n",alGetEventUST(event));
    fprintf(stdout,"Event TIME:\t\t%ld\n",alGetEventTime(event));

    return 0;
}

void cleanup(ALeventQueue eventq, ALevent event)
{
    if(eventq)
	alCloseEventQueue(eventq);
    if(event)
	alFreeEvent(event);
}

void printHelp(char *name)
{
    fprintf(stdout,"%s:[-h][-r <resource>][-t <timeout>] param",name);
}
