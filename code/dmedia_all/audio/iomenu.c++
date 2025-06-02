#include <stdio.h>
#include <malloc.h>
#include <unistd.h>
#include <dmedia/audio.h>

#include <Vk/VkApp.h>
#include <Vk/VkSimpleWindow.h>
#include <Vk/VkWindow.h>
#include <Vk/VkSubMenu.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkMenuItem.h>

/*
 * iomenu.c -- an app that that presents the choices in AL 2.0's two-tiered
 *		  data flow of device/interface to a GUI menu.
 *		  Compile with -laudio -lvk -lXm -lXt -lX11
 */

#define AUDPATH_SHOW_INTERFACE (1 << 0)
#define AUDPATH_SHOW_DEVICE (2 << 0)

typedef struct audio_path_s {
	char *label;	/* user-readable label to be associated with the menu entry */
	int   	dev;	/* device associated with path */
	int   iface;	/* interface associated with device on path */
	unsigned int   flags;
} audio_path_t;


static audio_path_t *audio_inlist;
static audio_path_t *audio_outlist;

/*
 * Generic routine to set the device/interface path
 * If a non-NULL ALconfig is given, the proper deivce is set
 * as the interface is setup.  If a NULL ALconfig is given,
 * the AL_DEFAULT_INPUT is changed accordingly.
 */ 

int setAudioPath(int dev, int iface, ALconfig cfg)
{
	int retval;
	ALpv pv;
	int param;

	if(dev <= 0)
		return -1;    /* Device no good */

 	if (alIsSubtype(AL_OUTPUT_DEVICE_TYPE, dev)) {
		param = AL_DEFAULT_OUTPUT;
	} else if (alIsSubtype(AL_INPUT_DEVICE_TYPE, dev)) {
		param = AL_DEFAULT_INPUT;
	} else {
		return -1; 	/* Device no good */
	}

/* 	
 *	Most apps really don't need to wrestle the Default Input/Output away from
 *	another audio application that might be using it.  Another way of
 *	getting audio i/o from where you want it, but that is also a little 
 *	less disruptive to other audio apps is to point future ALports to
 *	the specified device with the alSetDevice() call below.  Note that
 *	if a interface is specified, it doesn't necessarily get an app out 
 *	of changing that interface out from other apps.
 */

	if(cfg == NULL) {
		/* No config passed in, set the AL_DEFAULT_INPUT */
		pv.param = param;
		pv.value.i = dev;
		pv.sizeOut = 0;
		retval = alSetParams(AL_SYSTEM, &pv, 1);

		/* Check for success of alSetParams() */
		if((retval < 0) || (pv.sizeOut < 0))
			return -1;

	} else if(alSetDevice(cfg,dev) < 0) {
		return -1;
	}

	if (iface > 0) {
		/* Interface was specified */
		pv.param = AL_INTERFACE;
		pv.value.i = iface;
		pv.sizeOut = 0;
		retval = alSetParams(dev, &pv, 1);
	}

	return 0; /* success */
}


char *removePrefix(char *str)
{
char *retval;

	retval = strstr(str, ".");
	if(retval != NULL) {
		retval++;    /* Get past the "." */
	} else {
		retval = str;
	}
	return retval;
}


int getLabel(int res, char *label, int maxlen)
{
ALpv pv;

	pv.param = AL_LABEL;
	pv.value.ptr = label;
	pv.sizeIn = maxlen;
	alGetParams(res, &pv, 1);

	return pv.sizeOut;
}


/*
 * Make a list of audio device/interface paths.
 * Try to discern what the best possible name to display is for the complete path
 */
int makeAudioPathList(audio_path_t **list, char dir)
{
int numlist;
ALpv pv;
ALvalue dev[64];
ALvalue iface[64];
int ndevs,numifaces;
int i,listidx,j;
char ifacelabel[64];
char devlabel[64];
char label[64];
char *iface_nodot;
unsigned int devflags;
int realifaces;
char isoutput;
int param;

	isoutput = (dir == 'w');
	param = (isoutput) ? AL_DEFAULT_OUTPUT : AL_DEFAULT_INPUT;

	/* initialize the list to NULL */
	numlist = 0;
	*list = NULL;

	/* Find all the possible input or output devices */
	ndevs = alQueryValues(AL_SYSTEM,param,dev,64,NULL,0);

	/* Count how many possible device/interface combos there are */
	for(i=0; i<ndevs; i++) { 
		numlist += alQueryValues(dev[i].i,AL_INTERFACE, NULL, 0, NULL, 0);
	}

	/* 
	 * Allocate space for the list now that we know how many entries there are. 
	 * Save space for the list to be NULL terminated, as such.
	 */
	*list = (audio_path_t *)malloc(sizeof(audio_path_t) * (numlist+1));
	
	/* Fill in the list */
	for(listidx=i=0; i<ndevs; i++) {

		/* Get the label of the device for future use */
		getLabel(dev[i].i, devlabel, 64);

		/* Find all the interfaces associated with the device */
		numifaces = alQueryValues(dev[i].i,AL_INTERFACE, iface, 64, NULL, 0);

		/* 
		 *	BEGIN ARCANE RULES
		 */
		devflags = AUDPATH_SHOW_INTERFACE | AUDPATH_SHOW_DEVICE;

		/* 
		 * If all output interfaces are active at once, pretend there's 
		 * only 1 interface so only the device name gets displayed.
	  	 */
		pv.param = AL_INTERFACE;
		pv.value.i = 0;
		alGetParams( dev[i].i, &pv, 1);
		if((pv.value.i == AL_ALL_INTERFACES) && isoutput) {
			devflags = AUDPATH_SHOW_DEVICE;
			numifaces = 1;
		} else {

		    for(realifaces=j=0; j<numifaces; j++) {

			/* 
			 * If there's even a single analog input interface on a device, 
			 * assume it's baseline and forgo the device name.
		 	 */
			if((isoutput==0) && alIsSubtype(AL_ANALOG_IF_TYPE,iface[j].i))
				devflags = AUDPATH_SHOW_INTERFACE;

			/*
			 * count the non-null and non-test interfaces
			 */
			if( (alIsSubtype(AL_TEST_IF_TYPE,iface[j].i)==0) &&
			    (iface[j].i != AL_NULL_INTERFACE) )
					realifaces++;
		    }

		    /* 
		     * If there's only 1 interesting audio interface, 
		     * just show the device name 
		     */
	    	    if(realifaces <= 1) {
			devflags = AUDPATH_SHOW_DEVICE;
		    }
		}

		/*
		 * END ARCANE RULES
		 */

		for(j=0; j<numifaces; j++) {
		    
		    /* Skip over NULL and TEST interfaces */
		    if( (iface[j].i != AL_NULL_INTERFACE) && 
			(alIsSubtype(AL_TEST_IF_TYPE, iface[j].i) == 0))  {

			/* For each interface, find its label as well */
			getLabel(iface[j].i, ifacelabel, 64);

			/* 
			 * Concatenate the device + interface labels for display 
			 * in the form "<device>.<label>" if required.
			 */
			label[0] = 0;

			iface_nodot = ifacelabel;
			if( devflags & AUDPATH_SHOW_DEVICE ) {
				strcat(label,devlabel);

				/*
				 * See if the interface's label has a "." in it.  This
				 * implies that it is fully qualified with a subsystem
				 * name in front of it.  Since we're fully qualifying 
				 * our menu entries by way of having the device name 
				 * in front of all our interfaces, this is not really 
				 * needed.
				 */

				iface_nodot = removePrefix(ifacelabel);

				if( devflags & AUDPATH_SHOW_INTERFACE) {
					strcat(label,".");
				}
			}

			if( devflags & AUDPATH_SHOW_INTERFACE) {
				strcat(label,iface_nodot);
			}

			/* Fill in the list entry */
			(*list)[listidx].dev   = dev[i].i;
			(*list)[listidx].iface = iface[j].i;
			(*list)[listidx].label = strdup(label);
			(*list)[listidx].flags = devflags;
			listidx++;
		    }
		}
	}

	/* NULL terminate the list */
	(*list)[listidx].dev = 0;
	(*list)[listidx].iface = 0;
	(*list)[listidx].flags = 0;
	(*list)[listidx].label = NULL;

	return listidx;
}


/*
 * Free the resources allcoated by makeAudioInputList() above 
 */
void freeAudioPathList(audio_path_t **pathlist)
{
int i;
	if(pathlist == NULL)
		return;

	if((*pathlist) == NULL)
		return;

	for(i=0; (*pathlist)[i].dev != 0; i++)
		free( (*pathlist)[i].label );

	free(*pathlist);
	*pathlist = NULL;
}


/*
 * Callback to be handled by path selection.
 */
void setPathCallback(Widget w, XtPointer client_data, XtPointer call_data)
{
	audio_path_t *apath = (audio_path_t *)client_data;

	printf("Setting DEVICE.INTERFACE to %s\n",apath->label);
	setAudioPath(apath->dev, apath->iface, NULL);
}


/*
 * Exit app and free resources allocated
 */
void exitApp(Widget w, XtPointer client_data, XtPointer call_data)
{
	freeAudioPathList(&audio_inlist);
	freeAudioPathList(&audio_outlist);
        exit(0); /* Success */
}


void makeAudioPathMenu(VkSubMenu *pane, audio_path_t *list, int numlist)
{
VkSubMenu *rollover;
char label[64];
char *nopfx;
int i,starti;

	/* Go through the list of audio paths */
	for(i=0;i<numlist;i++) {

		/* 
		 * If we should be showing the whole <device>.<interface> name,
		 * make a rollover menu titled with the device name.
		 * Add the interfaces as children of this submenu.
		 */
		if( list[i].flags == (AUDPATH_SHOW_DEVICE|AUDPATH_SHOW_INTERFACE)) {

			/* Get the device label */
			getLabel(list[i].dev, label, 64);

			/* Add the device rollover menu */
			rollover = pane->addSubmenu( label );
			for(	starti = i; 
				list[starti].dev == list[i].dev;
				i++) 
			{

				/* Get the interface label */
				getLabel(list[i].iface, label, 64);

				/* Shorten it should it contain a prefix */
				nopfx = removePrefix(label);

				/* Add the new entry to the rollover menu */
				rollover->addAction( nopfx, setPathCallback,
					(XtPointer) &list[i]);
			} 
			i--; /* Adjust for i++ at end of for() loop */

		} else {

			/* 
			 * Add a path menu entry for each item in the path list 
			 */
			pane->addAction( list[i].label, setPathCallback,
					(XtPointer) &list[i]);
		}
	}
}


int main(int argc, char *argv[])
{
int numin;
int numout;
VkApp *app;
VkWindow *win;
VkSubMenu *inputPane;
VkSubMenu *outputPane;
VkSubMenu *filePane;


	/* Make lists of audio input and output paths */
	numin  = makeAudioPathList(&audio_inlist, 'r');
	numout = makeAudioPathList(&audio_outlist,'w');

	/* Make a view kit app and main window */
	app = new VkApp("IOmenu", &argc, argv, NULL, 0);
	win = new VkWindow("iomenu");

	/* add an exit button to the main menu bar */
	filePane = win->addMenuPane("File");
	filePane->addAction("Exit", exitApp, (XtPointer)(&audio_inlist));

	/* Add our own input and output menu */
	inputPane = win->addMenuPane("Input");
	outputPane = win->addMenuPane("Output");

	/* Populate the input and output menus */
	makeAudioPathMenu(inputPane,audio_inlist,numin);
	makeAudioPathMenu(outputPane,audio_outlist,numout);


	/* Realize the main window */
	win->show();

	/* Do viewkit main loop */
	app->run();
}
