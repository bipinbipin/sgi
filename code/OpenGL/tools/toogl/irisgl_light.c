/*
 *  irisgl_light.c
 *  $Revision: 1.2 $
 *
 *	some simple functions to make porting lighting code from
 *	iris to open gl easier.
 *
 */

#include <GL/gl.h>
#include <gl/gl.h> /* for iris gl lighting constants */
#include <stdio.h>
#include <math.h>

/* exports */
void mylmdef(short deftype, short index, short np, float props[]);
void mylmbind(short target, short index);

/* internal */
static void mylmdef_material(short index, float props[]);
static void mylmdef_lmodel(short index, float props[]);
static void mylmdef_light(short index, float props[]);
static void mylmbind_material(int index);
static void mylmbind_lmodel(int index);
static void mylmbind_light(int target,int index);


#define MAXMAT 10
#define MAXLM 2
#define MAXLT 10

typedef float _GLfloat;

typedef struct {
   _GLfloat x, y, z, w;
} _GLcoord;

typedef struct {
   _GLfloat r, g, b, a;
} _GLcolor;

typedef struct {
    int defined;
    _GLcolor ambient;
    _GLcolor diffuse;
    _GLcolor specular;
    _GLcolor emissive;
    _GLfloat specularExponent;
    _GLfloat cmapa, cmaps, cmapd; /* not used */
    int aflag,dflag,sflag,eflag,expflag; /* set if this prop is set by iris gl */
} Material;

static Material mat[MAXMAT];

typedef struct {
    int defined;
    _GLcolor ambient;
    int localViewer;
    int twoSided;
    int aflag,lflag,tflag; /* set if this prop is set by iris gl */
} LightModel;

static LightModel lm[MAXLM];

typedef struct {
    int defined;
    _GLcolor ambient;
    _GLcolor diffuse;
    _GLcolor specular; /* not used */
    _GLcoord position;
    _GLfloat spotLightExponent; /* not used */
    _GLfloat spotLightCutOffAngle; /* not used */
    _GLfloat constantAttenuation; /* not used */
    _GLfloat linearAttenuation; /* not used */
    _GLfloat quadraticAttenuation; /* not used */
    int aflag,dflag,sflag,pflag; /* set if this prop is set by iris gl */
} LightSource;

static LightSource lt[MAXLT];

static material_enable,lmodel_enable; /* used to determine if lighting is on/off */


void
mylmdef(short deftype, short index, short np, float props[])
{
    switch (deftype) {
      case DEFMATERIAL:
	mylmdef_material(index,props);
	break;
      case DEFLMODEL:
	mylmdef_lmodel(index,props);
	break;
      case DEFLIGHT:
	mylmdef_light(index,props);
	break;
    }
}

void
mylmbind(short target, short index)
{
    switch (target) {
      case MATERIAL:
	mylmbind_material(index);
	break;
      case LMODEL:
	mylmbind_lmodel(index);
	break;
      default: /* LIGHTn */
	mylmbind_light(target,index);
	break;
    }
}

/*
 *  XXX - defaults of iris and open gl may be different
 */
static void
mylmdef_material(short index, float props[])
{
int done;
float prop;
static char pre[] = "mylmdef_material:";

    if (index >= MAXMAT) {
	fprintf(stderr,"%s index %d too big\n",pre,index);
	return;
    }

    /* if not yet defined, set some defaults */
    if (!mat[index].defined) {
	mat[index].defined = 1;
    }


    /* parse iris gl props */
    done = 0;
    while (!done) {
	switch ((int)(prop = *props++)) {
	  case AMBIENT:
	    mat[index].ambient.r = *props++;
	    mat[index].ambient.g = *props++;
	    mat[index].ambient.b = *props++;
	    mat[index].ambient.a = 1.0;
	    mat[index].aflag = 1;
	    break;
	  case DIFFUSE:
	    mat[index].diffuse.r = *props++;
	    mat[index].diffuse.g = *props++;
	    mat[index].diffuse.b = *props++;
	    mat[index].diffuse.a = 1.0;
	    mat[index].dflag = 1;
	    break;
	  case SPECULAR:
	    mat[index].specular.r = *props++;
	    mat[index].specular.g = *props++;
	    mat[index].specular.b = *props++;
	    mat[index].specular.a = 1.0;
	    mat[index].sflag = 1;
	    break;
	  case EMISSION:
	    mat[index].emissive.r = *props++;
	    mat[index].emissive.g = *props++;
	    mat[index].emissive.b = *props++;
	    mat[index].emissive.a = 1.0;
	    mat[index].eflag = 1;
	    break;
	  case SHININESS:
	    mat[index].specularExponent = *props++;
	    mat[index].expflag = 1;
	    break;
	  case ALPHA:
	    props++;
	    fprintf(stderr,"%s ALPHA ignored\n",pre);
	    break;
	  case (int)LMNULL:
	    done = 1;
	    break;
	  default:
	    fprintf(stderr,"%s unexpected prop %g\n",pre,prop);
	    break;
	}
    }
}

static void
mylmbind_material(int index)
{
    material_enable = index;
    if (index == 0) {
	glDisable(GL_LIGHTING);
    }
    else {
	if (material_enable && lmodel_enable) {
	    glEnable(GL_LIGHTING);
	}
	if (mat[index].aflag) 
	    glMaterialfv(GL_FRONT,GL_AMBIENT,&mat[index].ambient.r);
	if (mat[index].dflag) 
	    glMaterialfv(GL_FRONT,GL_DIFFUSE,&mat[index].diffuse.r);
	if (mat[index].sflag) 
	    glMaterialfv(GL_FRONT,GL_SPECULAR,&mat[index].specular.r);
	if (mat[index].eflag) 
	    glMaterialfv(GL_FRONT,GL_EMISSION,&mat[index].emissive.r);
	if (mat[index].expflag) 
	    glMaterialf(GL_FRONT,GL_SHININESS,mat[index].specularExponent);
    }
}


static void
mylmdef_lmodel(short index, float props[])
{
int done;
float prop;
static char pre[] = "mylmdef_lmodel:";

    if (index >= MAXLM) {
	fprintf(stderr,"%s index %d too big\n",pre,index);
	return;
    }

    /* if not yet defined, set some defaults (XXX) */
    if (!lm[index].defined) {
	lm[index].defined = 1;
    }

    /* parse iris gl props */
    done = 0;
    while (!done) {
	switch ((int)(prop = *props++)) {
	  case AMBIENT:
	    lm[index].ambient.r = *props++;
	    lm[index].ambient.g = *props++;
	    lm[index].ambient.b = *props++;
	    lm[index].ambient.a = 1.0;
	    lm[index].aflag = 1;
	    break;
	  case LOCALVIEWER:
	    lm[index].localViewer = *props++;
	    lm[index].lflag = 1;
	    break;
	  case TWOSIDE:
	    lm[index].twoSided = *props++;
	    lm[index].tflag = 1;
	    break;
	  case (int)LMNULL:
	    done = 1;
	    break;
	  default:
	    fprintf(stderr,"%s unexpected prop %g\n",pre,prop);
	    break;
	}
    }
}

static void
mylmbind_lmodel(int index)
{
    lmodel_enable = index;
    if (index == 0) {
	glDisable(GL_LIGHTING);
    }
    else {
	if (material_enable && lmodel_enable) {
	    glEnable(GL_LIGHTING);
	}
	if (lm[index].aflag) 
	    glLightModelfv(GL_LIGHT_MODEL_AMBIENT,&lm[index].ambient.r);
	if (lm[index].lflag) 
	    glLightModelf(GL_LIGHT_MODEL_LOCAL_VIEWER,lm[index].localViewer);
	if (lm[index].tflag) 
	    glLightModelf(GL_LIGHT_MODEL_TWO_SIDE,lm[index].twoSided);
    }
}

static void
mylmdef_light(short index, float props[])
{
int done;
float prop;
static char pre[] = "mylmdef_light:";

    if (index >= MAXLT) {
	fprintf(stderr,"%s index %d too big\n",pre,index);
	return;
    }

    /* if not yet defined, set some defaults (XXX) */
    if (!lt[index].defined) {
	lt[index].defined = 1;
    }

    /* parse iris gl props */
    done = 0;
    while (!done) {
	switch ((int)(prop = *props++)) {
	  case AMBIENT:
	    lt[index].ambient.r = *props++;
	    lt[index].ambient.g = *props++;
	    lt[index].ambient.b = *props++;
	    lt[index].ambient.a = 1.0;
	    lt[index].aflag = 1;
	    break;
	  case LCOLOR:
	    lt[index].diffuse.r = *props++;
	    lt[index].diffuse.g = *props++;
	    lt[index].diffuse.b = *props++;
	    lt[index].diffuse.a = 1.0;
	    lt[index].dflag = 1;
	    break;
	  case POSITION:
	    lt[index].position.x = *props++;
	    lt[index].position.y = *props++;
	    lt[index].position.z = *props++;
	    lt[index].position.w = *props++;
	    lt[index].pflag = 1;
	    break;
	  case (int)LMNULL:
	    done = 1;
	    break;
	  default:
	    fprintf(stderr,"%s unexpected prop %g\n",pre,prop);
	    break;
	}
    }
}

static void 
mylmbind_light(int target,int index)
{
int num;

    num = target-LIGHT0;
    if (index == 0) { /* turn off this light */
	glDisable(GL_LIGHT0+num);
    }
    else { /* turn on lighting */
	glEnable(GL_LIGHT0+num);
	if (lt[index].aflag) 
	    glLightfv(GL_LIGHT0+num,GL_AMBIENT,&lt[index].ambient.r);
	/* iris gl has no specular light, so just use the same as diffuse */
	if (lt[index].dflag) {
	    glLightfv(GL_LIGHT0+num,GL_DIFFUSE,&lt[index].diffuse.r);
	    glLightfv(GL_LIGHT0+num,GL_SPECULAR,&lt[index].diffuse.r);
	}
	if (lt[index].pflag) 
	    glLightfv(GL_LIGHT0+num,GL_POSITION,&lt[index].position.x);
    }
}
