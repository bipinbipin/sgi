
/*
 * Conversion from GL to OpenGL...
 * $Revision: 1.3 $
 */

#include <stdlib.h>
#include <iostream.h>
#include <assert.h>
#include <getopt.h>

#include "perlclass.h"
#include "search.h"

static char *revision = "$Revision: 1.3 $";

static int lineno = 0;
static int errors = 0;
static int debug = 0;
static int no_queue = 0;
static int no_window = 0;
static int no_comments = 0;
static int no_lighting = 0;
static int emulate_lighting = 0;

int matching(const char *, int offset = 0);
PerlStringList split_args(PerlString &, int &ok);
void replace_args(PerlString &in, const PerlStringList &args);

PerlString instr, ostr;
PerlStringList comments;

static void error(char *err)
{
    cerr << "Error: " << err << " at line " << lineno << " of input.\n";
    cerr << instr << "\n";
    errors++;
}

static void
options(int argc, char **argv)
{
    int c;
    
    while ((c = getopt(argc,  argv, "dclLqvw")) != -1) {
	switch(c) {
	default:
	    cerr << "Usage: toogl [-clLqwv] < infile > outfile\n" ;
	    cerr << "	-c  don't put comments with OGLXXX into program\n";
	    cerr << "	-l  don't translate lighting calls (e.g. lmdef, lmbind, #defines) \n";
	    cerr << "	-L  translate lighting calls for emulation library (mylmdef, mylmbind) (implies -l) \n";
	    cerr << "	-q  don't translate event queue calls (e.g. qread, setvaluator) \n";
	    cerr << "	-v  print revision number.\n";
	    cerr << "	-w  don't translate window manager calls (e.g. winopen, mapcolor) \n";
	    exit (1);
	case 'd':
	    debug = 1;
	    break;
	case 'q':
	    no_queue = 1;
	    break;
	case 'w':
	    no_window = 1;
	    break;
	case 'c':
	    no_comments = 1;
	    break;
	case 'l':
	    no_lighting = 1;
	    break;
	case 'L':
	    emulate_lighting = 1;
	    no_lighting = 1;
	    break;
	case 'v':
	    cerr << "toogl " << revision << "\n";
	    break;
	}
    }
}

int read_line()
{
    cin >> instr;
    instr = " " + instr + " ";	// add a space before and after string so regular expressions that require non-alphanumeric before and after will work.
    lineno++;
    return (!cin.eof()); 
}


class glThing;
static glThing
     *first_glThing[MAXPATTERN], **last_glThing[MAXPATTERN];

     
Search search[MAXPATTERN];

class glThing {
    public:
	glThing(const PerlString &n) :name(n) {
	    // link into list of all glFuncs.
	    int len = n.length();
	    if(len > MAXPATTERN-1)
		len = MAXPATTERN-1;
	    prev = last_glThing[len];
	    if(!prev) {
		prev = last_glThing[len] = &first_glThing[len];
	    }
	    last_glThing[len] = &(this->nextp);
	    nextp = 0;
	    *prev = this;
	    search[len].add((const char *)n);
	};
	~glThing() {	// can't undo search.add()
	    *prev = nextp;
	    if(nextp)
		nextp->prev = prev;
	};
	inline glThing *next() {return nextp;};
	virtual int m(PerlString &,  PerlStringList &) = 0;
	virtual void replace(PerlString &in, PerlStringList &s) = 0 ;
    private:
	const PerlString name;
        glThing *nextp, **prev;

};

//const static PerlString pre("^(.*[^a-zA-Z_0-9]+)*(");
const static PerlString pre("^(.*[^a-zA-Z_0-9]+)(");  // XXX won't work at beginning of line
const static PerlString post("[ \t]*)(\\(.*)$");


class glFunc :public glThing {
protected:   
    Regexp re;
    const PerlString restr;
    const PerlString quick;
public:
    glFunc( const PerlString & name ) : glThing(name), re(pre+name+post), restr(pre+name+post), quick(name) {
    };
    
    ~glFunc() {
    };
    
    virtual int m( PerlString & f, PerlStringList & s) {
	int ret = 0;
	if(f.length() && f[0] && (-1 != f.index(quick))) {    // don't try to match null strings
	    /* The re breaks it into:
	     * (stuff before name) -- verify no alphanumeric prefixed to name
	     * (name including whitespace up to '('))
	     * ('(' plus stuff after '(' to end of line)))
	     * 
	     * further processing breaks it into:
	     * (stuff before name)			[0]
	     * (name including whitespace up to '('))	[1]
	     * a '('					[2]
	     * arg1					[3]
	     * arg2
	     * ...				
	     * argn					[nargs+2]	    
	     * closing ')'				[nargs+3]
	     * rest of line				[nargs+4]
	     * note:there is ALWAYS at least 1 arg string
	     */
	    int i = f.m(re, s);
	    s.shift();	// drop match of whole line
	    i--;
//	    assert(i == -1 || i == 3);	// re either matches or doesn't
	    if(i == 3) {
		int ok;
		PerlString rest(s[2]);
		s.pop();
		s.push(split_args(rest, ok));
		if(ok)
		    ret = s.scalar();
	    } else if (i != -1) {
		cerr << "Internal Error, wierd re match:(" << i << ")\n" << s.join("\n") << "\nRE:" << restr << '\n';
	    }
	} 
	return ret;
	};
	
    virtual void replace(PerlString &in, PerlStringList &s) = 0 ;
    
};



// gl functions with no args -- only whitespace allowed inside ()'s
class glSimple:public glFunc {
public:
    glSimple(const PerlString &s, const PerlString &r)
	:glFunc(s), rep(r), comments(PerlString("")) { 
    };

    glSimple(const PerlString &s, const PerlString &r, const PerlString &c)
	:glFunc(s), rep(r), comments(c){ 
    };
    
    
    ~glSimple() {};

   	
    virtual void replace(PerlString &in, PerlStringList &s) {
	s[1] = rep;
	int nargs = s.scalar() - 5;
	s.splice(2, nargs+2);	    // remove "( )" elements
	in = s.join("");
	::comments.push(comments.split("#"));
    };


private:
    const PerlString rep;
    PerlString comments;
};

// gl functions, possibly with args, to be deleted
// These are special: entire function is copied into comments.
class glDelete: public glFunc {
public:
    glDelete(const PerlString &s, const PerlString &c) 
	: glFunc(s), comments(c) {
    };
    ~glDelete() {};


    virtual void replace(PerlString &in, PerlStringList &s) {
	::comments.push(comments.split("#"));
	int nargs = s.scalar() - 5;
	PerlString f(s[1]);
	PerlStringList args(6);
	args = s.splice(3, nargs);
	f += "(" + args.join(",") + ")";    // move args to f 
	::comments.push(f);
	s.splice(2, 2);	// remove "()"
	s[1] = "/*DELETED*/";	// replace name
	in = s.join("");
    };

private:
    PerlString comments;
};

// gl Functions with args
class glArgs: public glFunc {
public:
    glArgs(const PerlString &s, const PerlString &r, const PerlString &c) 
	: glFunc(s), rep(r), comments(c) {
    };
    glArgs(const PerlString &s, const PerlString &r) 
	: glFunc(s), rep(r), comments("") {
    };
    ~glArgs() {};

   
    virtual void replace(PerlString &in, PerlStringList &s) {
	PerlStringList argz(6);
	int nargs = s.scalar() - 5;
	s[1] = rep;
	argz.push((s.splice(2, nargs+2)));
	argz.shift();
	argz.pop();	    // drop '(' and ')'
	replace_args(s[1], argz);
	in = s.join("");
	::comments.push(comments.split("#"));
    };


private:
    const PerlString rep;
    PerlString comments;
};

const static PerlString defpre("^(.*[^a-zA-Z_0-9]+)(");  // XXX won't work at beginning of line
const static PerlString defpost(")([^a-zA-Z_0-9]+.*)$"); // XXX won't work at end of line

class glDefine :public glThing {
private:   
    Regexp re;
    const PerlString restr;
    const PerlString quick;
    const PerlString rep;
    PerlString comments;
public:
    glDefine( const PerlString & name, const PerlString &repl, const PerlString &com ) : glThing(name), re(defpre+name+defpost), restr(defpre+name+defpost), quick(name), rep(repl), comments(com) {
    };
    glDefine( const PerlString & name, const PerlString &repl ) : glThing(name), re(defpre+name+defpost), restr(defpre+name+defpost), quick(name), rep(repl), comments("") {
    };
    
    ~glDefine() {
    };
    
    virtual int m( PerlString & f, PerlStringList & s) {
	if(f.length() && f[0] && (-1 != f.index(quick))) {    // don't try to match null strings
	    /* The re breaks it into:
	     * (stuff before name)
	     * (name)
	     * (stuff after name)
	     */
	    int i = f.m(re, s);
	    if(i) {
		s.shift();	// drop match of whole line
		return s.scalar();
	    } else
		return 0;
	} else
	    return 0;
    };
	
    virtual void replace(PerlString &in, PerlStringList &s) {
	s[1] = rep;
	in = s.join("");
	::comments.push(comments.split("#"));
    };
    
};


glSimple simples[] = {
    glSimple("bgnqstrip", "glBegin(GL_QUAD_STRIP)"),
    glSimple("endqstrip", "glEnd()"),
    glSimple("bgntmesh", "glBegin(GL_TRIANGLE_STRIP)"),
    glSimple("endtmesh", "glEnd()"),
    glSimple("bgnline", "glBegin(GL_LINE_STRIP)", "for multiple, independent line segments: use GL_LINES"),
    glSimple("endline", "glEnd()"),
    glSimple("bgnclosedline", "glBegin(GL_LINE_LOOP)"),
    glSimple("endclosedline", "glEnd()"),
    glSimple("bgnpoint", "glBegin(GL_POINTS)"),
    glSimple("endpoint", "glEnd()"),
    glSimple("bgnpolygon", "glBegin(GL_POLYGON)", "special cases for polygons:#\tindependant quads: use GL_QUADS#\tindependent triangles: use GL_TRIANGLES"),
    glSimple("endpolygon", "glEnd()"),
    glSimple("pushmatrix", "glPushMatrix()"),
    glSimple("popmatrix", "glPopMatrix()"),
    glSimple("clear", "glClearIndex(index);glClearColor(r, g, b, a); glClear(GL_COLOR_BUFFER_BIT)", "clear: use only one of glCLearIndex or glClearColor,#and change index or r, g, b, a to correct values"),
    glSimple("zclear", "glClearDepth(1.); glClear(GL_DEPTH_BUFFER_BIT)"),
    glSimple("getshade", "(glGetIntegerv(GL_CURRENT_INDEX, &gctmp),  gctmp)", "getcolor:#GLint gctmp;"),  
    glSimple("getcolor", "(glGetIntegerv(GL_CURRENT_INDEX, &gstmp), gstmp)", "getshade:#GLint gctmp;"),  
    glSimple("bgncurve", "gluBeginCurve( obj )", "replace obj with your GLUnurbsObj*"),  
    glSimple("bgnsurface", "gluBeginSurface( obj )", "replace obj with your GLUnurbsObj*"),  
    glSimple("bgntrim", "gluBeginTrim( obj )", "replace obj with your GLUnurbsObj*"),  
    glSimple("closeobj", "glEndList()"),
    glSimple("endcurve", "gluEndCurve( obj )", "replace obj with your GLUnurbsObj*"),  
    glSimple("endfeedback", "glRenderMode(GL_RENDER)"),  
    glSimple("endselect", "glRenderMode(GL_RENDER)"), 
    glSimple("endsurface", "gluEndSurface( obj )", "replace obj with your GLUnurbsObj*"),  
    glSimple("endtrim", "gluEndTrim( obj )", "replace obj with your GLUnurbsObj*"),  
    glSimple("finish", "glFinish()"),  
    glSimple("genobj", "glGenLists(1)", "glGenLists: change range param to get more than one"),  
    glSimple("getdcm", "glIsEnabled(GL_FOG)", "depthcue supported by GL_FOG"),  
    glSimple("initnames", "glInitNames()"),  
    glSimple("pclos", "glEnd()"),  
    glSimple("popattributes", "glPopAttrib()"),
    glSimple("popname", "glPopName()"),
    glSimple("popviewport", "glPopAttrib()", "popviewport: see glPopAttrib man page"),
    glSimple("pushattributes", "glPushAttrib(GL_ALL_ATTRIB_BITS)"),
    glSimple("pushviewport", "glPushAttrib(GL_VIEWPORT_BIT)"),
};

glDelete deletes[] = {
    glDelete("addtopup", "addtopup not supported"), 
    glDelete("subpixel", "always subpixel"), 
    glDelete("gconfig",  "gconfig not supported:#\tcollect glxChooseVisual attributes into one list for visual selection"), 
    glDelete("concave", "use gluBeginPolygon(tobj) to draw concave polys"),  
    glDelete("endpupmode", "endpupmode obsolete"),  
    glDelete("getothermonitor", "getothermonitor obsolete"),  
    glDelete("gRGBcursor", "gRGBcursor obsolete"),  
    glDelete("ismex", "ismex obsolete -- (always TRUE)"),  
    glDelete("pupmode", "pupmode obsolete"),
    glDelete("spclos", "spclos obsolete"),  
    glDelete("swaptmesh", "swaptmesh not supported, maybe glBegin(GL_TRIANGLE_FAN)"),  
    glDelete("RGBrange", "RGBrange not supported, see glFog()"),  // XXX
    glDelete("lRGBrange", "lRGBrange not supported, see glFog()"),  // XXX
    glDelete("shaderange", "shaderange not supported, see glFog()"),  // XXX
    glDelete("lshaderange", "lshaderange not supported, see glFog()"),  // XXX
    glDelete("callfunc", "callfunc not supported"),
    glDelete("chunksize", "chunksize not supported"),
    glDelete("clearhitcode", "clearhitcode not supported"),
    glDelete("compactify", "compactify not supported"),
    glDelete("curveit", "curveit not supported, see glEvalMesh()"),
    glDelete("defbasis", "defbasis not supported, see glEvalMesh()"),
    glDelete("deflfont", "deflfont not supported, see glCallLists()"),
    glDelete("defpup", "defpup not supported"),
    glDelete("deltag", "deltag not supported"),
    glDelete("depthcue", "depthcue not supported, See glFog()"),// XXX do better
    glDelete("dopup", "dopup not supported"),
    glDelete("editobj", "editobj not supported# -- use display list hierarchy"),
    glDelete("foreground", "foreground not supported -- see fork man page"),
    glDelete("freepup", "freepup not supported"),
    glDelete("bbox2", "bbox2 not supported"),
    glDelete("bbox2i", "bbox2i not supported"),
    glDelete("bbox2s", "bbox2s not supported"),
    glDelete("gentag", "gentag not supported -- use display list hierarchy"),
    glDelete("getdescender", "getdescender not supported"),
    glDelete("getfont", "getfont not supported"),
    glDelete("getheight", "getheight not supported"),
    glDelete("gethitcode", "gethitcode not supported"),
    glDelete("getlsbackup", "getlsbackup not supported"),
    glDelete("getlstyle", "getlstyle not supported"),
    glDelete("getresetls", "getresetls not supported"),
    glDelete("getscrbox", "getscrbox not supported"),
    glDelete("glcompat", "glcompat not supported"),
    glDelete("gsync", "gsync not supported"),
    glDelete("gversion", "gversion not supported -- how about \"OpenGL\"?"),
    glDelete("istag", "istag not supported -- use display list hierarchy"),
    glDelete("lsbackup", "lsbackup not supported"),
    glDelete("maketag", "maketag not supported -- use display list hierarchy"),
    glDelete("newpup", "newpup not supported"),
    glDelete("newtag", "newtag not supported -- use display list hierarchy"),
    glDelete("objinsert", "objinsert not supported -- use display list hierarchy"),
    glDelete("objreplace", "objreplace not supported -- use display list hierarchy"),
    glDelete("pagecolor", "pagecolor not supported"),
    glDelete("resetls", "resetls not supported"),
    glDelete("scrbox", "scrbox not supported"),
    glDelete("setpup", "setpup not supported -- See Window Manager"),
    glDelete("strwidth", "strwidth not supported -- See Fonts"),
    glDelete("lstrwidth", "lstrwidth not supported -- See Fonts"),
    glDelete("swapinterval", "swapinterval not supported -- See MBX"),
    glDelete("zdraw", "zdraw not supported -- See stencil for similar functions"),
    glDelete("zsource", "zsource not supported"),
    glDelete("getgpos", "getgpos -- graphics position not supported"),
    glDelete("patch", "patch not supported, see gluNurbsSurface man page"),
    glDelete("rpatch", "rpatch not supported, see gluNurbsSurface man page"),
    glDelete("patchbasis", "patchbassis not supported, see gluNurbsSurface man page"),
    glDelete("patchcurves", "patchcurves not supported, see gluNurbsSurface man page"),
    glDelete("patchprecision", "patchprecision not supported, see gluNurbsSurface man page"),
    glDelete("rcrv", "rcrv not supported -- see gluNurbsCurve man page"),
    glDelete("rcrvn", "rcrvn not supported -- see gluNurbsCurve man page"),
    glDelete("xfpt", "xfpt not supported -- see gluProject man page"),
    glDelete("xfpti", "xfpti not supported -- see gluProject man page"),
    glDelete("xfpts", "xfpts not supported -- see gluProject man page"),
    glDelete("xfpt2", "xfpt2 not supported -- see gluProject man page"),
    glDelete("xfpt2i", "xfpt2i not supported -- see gluProject man page"),
    glDelete("xfpt2s", "xfpt2s not supported -- see gluProject man page"),
    glDelete("xfpt4", "xfpt4 not supported -- see gluProject man page"),
    glDelete("xfpt4i", "xfpt4i not supported -- see gluProject man page"),
    glDelete("xfpt4s", "xfpt4s not supported -- see gluProject man page"),
    glDelete("scrsubdivide", "scrsubdivide not needed."),
    glDelete("Tag", "Object tags not supported"),
    glDelete("Offset", "Object tags not supported"),
    glDelete("Cursor", "Cursor -- use Window Manager"),
    glDelete("Device", "Device -- use Window Manager"),
};


glArgs args[] = {
    glArgs("c3f", "glColor3fv($1)"),
    glArgs("c3s", "glColor3sv($1)", "color values need to be scaled"),  // XXX
    glArgs("c3i", "glColor3iv($1)", "color values need to be scaled"),  // XXX
    glArgs("c4f", "glColor4fv($1)"),
    glArgs("c4s", "glColor4sv($1)", "color values need to be scaled"),  // XXX
    glArgs("c4i", "glColor4iv($1)", "color values need to be scaled"),  // XXX
    glArgs("v2f", "glVertex2fv($1)"), 
    glArgs("v2s", "glVertex2sv($1)"), 
    glArgs("v2i", "glVertex2iv($1)"), 
    glArgs("v2d", "glVertex2dv($1)"), 
    glArgs("v3f", "glVertex3fv($1)"), 
    glArgs("v3s", "glVertex3sv($1)"), 
    glArgs("v3i", "glVertex3iv($1)"), 
    glArgs("v3d", "glVertex3dv($1)"), 
    glArgs("v4f", "glVertex4fv($1)"), 
    glArgs("v4s", "glVertex4sv($1)"), 
    glArgs("v4i", "glVertex4iv($1)"), 
    glArgs("v4d", "glVertex4dv($1)"), 
    glArgs("n3f", "glNormal3fv($1)"), 
    glArgs("t2f", "glTexCoord2fv($1)"), 
    glArgs("t3f", "glTexCoord3fv($1)"), 
    glArgs("t2s", "glTexCoord2sv($1)"), 
    glArgs("t2i", "glTexCoord2iv($1)"), 
    glArgs("t2d", "glTexCoord2dv($1)"), 
    glArgs("translate", "glTranslatef($1, $2, $3)"), 
    glArgs("loadmatrix", "glLoadMatrixf($1)"), 
    glArgs("multmatrix", "glMultMatrixf($1)"), 
    glArgs("RGBcolor", "glColor3ub($1, $2, $3)"), 
    glArgs("normal", "glNormal3fv($1)"),
    glArgs("blendfunction", "glBlendFunc($1, $2); if(($1) == GL_ONE && ($2) == GL_ZERO) glDisable(GL_BLEND); else glEnable(GL_BLEND)"),
    glArgs("callobj", "glCallList($1)", "check list numbering"), 
    glArgs("charstr", "glCallLists(strlen($1), GL_UNSIGNED_BYTE, $1)", "charstr: check list numbering"), // XXX what list definitions need to be done before this works?
    glArgs("cmov", "glRasterPos3f($1, $2, $3)"),
    glArgs("cmov2", "glRasterPos2f($1, $2)"),
    glArgs("cmov2i", "glRasterPos2i($1, $2)"),
    glArgs("cmov2s", "glRasterPos2s($1, $2)"),
    glArgs("cmovi", "glRasterPos3i($1, $2, $3)"),
    glArgs("cmovs", "glRasterPos3s($1, $2, $3)"),
    glArgs("color", "glIndexi($1)"),
    glArgs("colorf", "glIndexf($1)"),
    glArgs("setshade", "glIndexi($1)"),
    glArgs("font", "glListBase(int n)", "see glListBase info"),
    glArgs("afunction", "glAlphaFunc($2, ($1)/255.); if(($2)==GL_ALWAYS) glDisable(GL_ALPHA_TEST); else glEnable(GL_ALPHA_TEST)"),
    glArgs("acbuf", "glAccum($1, $2)"),	// XXX translate params?
    glArgs("getdepth", "{GLint get_depth_tmp[2];glGetIntegerv(GL_DEPTH_RANGE, get_depth_tmp);*($1)=get_depth_tmp[0];*($2)=get_depth_tmp[1];}", "You can probably do better than this."),  
    glArgs("smoothline", "if($1) glEnable(GL_LINE_SMOOTH); else glDisable(GL_LINE_SMOOTH)"),    // args to smoothline?  
    glArgs("linesmooth", "if($1) glEnable(GL_LINE_SMOOTH); else glDisable(GL_LINE_SMOOTH)"),    // args to linesmooth?  
    glArgs("polysmooth", "if($1) glEnable(GL_POLYGON_SMOOTH); else glDisable(GL_POLYGON_SMOOTH)"),
    glArgs("arc", "{ GLUquadricObj *qobj = gluNewQuadric(); gluQuadricDrawStyle(qobj, GLU_SILHOUETTE); glPushMatrix(); glTranslatef($1, $2,  0.); gluPartialDisk( qobj, 0., $3, 32, 1, ($4)*.1, (($5)-($4))*.1); glPopMatrix(); gluDeleteQuadric(qobj); }", "See gluPartialDisk man page."),
    glArgs("arci", "{ GLUquadricObj *qobj = gluNewQuadric(); gluQuadricDrawStyle(qobj, GLU_SILHOUETTE); glPushMatrix(); glTranslatef($1, $2, 0.); gluPartialDisk( qobj, 0., $3, 32, 1, ($4)*.1, (($5)-($4))*.1); glPopMatrix(); gluDeleteQuadric(qobj); }", "See gluPartialDisk man page."),
    glArgs("arcs", "{ GLUquadricObj *qobj = gluNewQuadric(); gluQuadricDrawStyle(qobj, GLU_SILHOUETTE); glPushMatrix(); glTranslatef($1, $2, 0.); gluPartialDisk( qobj, 0., $3, 32, 1, ($4)*.1, (($5)-($4))*.1); glPopMatrix(); gluDeleteQuadric(qobj); }", "See gluPartialDisk man page."),
    glArgs("arcf", "{ GLUquadricObj *qobj = gluNewQuadric(); glPushMatrix(); glTranslatef($1, $2, 0.); gluPartialDisk( qobj, 0., $3, 32, 1, ($4)*.1, (($5)-($4))*.1); glPopMatrix(); gluDeleteQuadric(qobj); }", "See gluPartialDisk man page."),
    glArgs("arcfi", "{ GLUquadricObj *qobj = gluNewQuadric(); glPushMatrix(); glTranslatef($1, $2, 0.); gluPartialDisk( qobj, 0., $3, 32, 1, ($4)*.1, (($5)-($4))*.1); glPopMatrix(); gluDeleteQuadric(qobj); }", "See gluPartialDisk man page."),
    glArgs("arcfs", "{ GLUquadricObj *qobj = gluNewQuadric(); glPushMatrix(); glTranslatef($1, $2, 0.); gluPartialDisk( qobj, 0., $3, 32, 1, ($4)*.1, (($5)-($4))*.1); glPopMatrix(); gluDeleteQuadric(qobj); }", "See gluPartialDisk man page."),
    glArgs("circ", "{ GLUquadricObj *qobj = gluNewQuadric(); gluQuadricDrawStyle(qobj, GLU_SILHOUETTE); glPushMatrix(); glTranslate($1, $2, 0.); gluDisk( qobj, 0., $3, 32, 1); glPopMatrix(); gluDeleteQuadric(qobj); }", "See gluDisk man page."),
    glArgs("circi", "{ GLUquadricObj *qobj = gluNewQuadric(); gluQuadricDrawStyle(qobj, GLU_SILHOUETTE); glPushMatrix(); glTranslate($1, $2, 0.); gluDisk( qobj, 0., $3, 32, 1); glPopMatrix(); gluDeleteQuadric(qobj); }", "See gluDisk man page."),
    glArgs("circs", "{ GLUquadricObj *qobj = gluNewQuadric(); gluQuadricDrawStyle(qobj, GLU_SILHOUETTE); glPushMatrix(); glTranslate($1, $2, 0.); gluDisk( qobj, 0., $3, 32, 1); glPopMatrix(); gluDeleteQuadric(qobj); }", "See gluDisk man page."),
    glArgs("circf", "{ GLUquadricObj *qobj = gluNewQuadric(); glPushMatrix(); glTranslate($1, $2, 0.); gluDisk( qobj, 0., $3, 32, 1); glPopMatrix(); gluDeleteQuadric(qobj); }", "See gluDisk man page."),
    glArgs("circfi", "{ GLUquadricObj *qobj = gluNewQuadric(); glPushMatrix(); glTranslate($1, $2, 0.); gluDisk( qobj, 0., $3, 32, 1); glPopMatrix(); gluDeleteQuadric(qobj); }", "See gluDisk man page."),
    glArgs("circfs", "{ GLUquadricObj *qobj = gluNewQuadric(); glPushMatrix(); glTranslate($1, $2, 0.); gluDisk( qobj, 0., $3, 32, 1); glPopMatrix(); gluDeleteQuadric(qobj); }", "See gluDisk man page."),
    glArgs("callobj", "glCallList($1)", "check list numbering"),
    glArgs("clipplane", "glClipPlane( GL_CLIP_PLANE0+($1), *equation); if($2) glEnable(GL_CLIP_PLANE+($1)); else glDisable(GL_CLIP_PLANE0+($1))", "see man page for glClipPlane equation"),
    glArgs("cpack", "glColor4ubv(&($1))", "cpack: if argument is not a variable#might need to be:#\tglColor4b(($1)&0xff, ($1)>>8&0xff, ($1)>>16&0xff, ($1)>>24&0xff)"),
    glArgs("crv", "glEvalCoord1f( u )", "replace u with domain coordinate"),
    glArgs("crvn", "glEvalMesh1f(GL_LINE, 0, n-1 )", "replace n with domain coordinate index"),
    glArgs("curvebasis", "glMap1();glMap2();glMapGrid()", "curvebasis: see man pages"),
    glArgs("curveprecision", "glMap1();glMap2();glMapGrid()", "curveprecision:see man pages"),
    glArgs("czclear", "glClearDepth($2);glClearColor(((float)(($1)&0xff))/255., (float)(($1)>>8&0xff)/255., (float)(($1)>>16&0xff)/255., (float)(($1)>>24&0xff)/255. );glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT)", "change glClearDepth parameter to be in [0, 1]"),// pattern behavior?
    glArgs("deflinestyle", "glNewList($1, GL_COMPILE);glLineStipple(factor, $2); glEndList()", "LineStipple:#\tget factor from lsrepeat()#\tYou don't really need to make a display list.#\tCheck list numbering."),
    glArgs("setlinestyle", "if($1) {glCallList($1); glEnable(GL_LINE_STIPPLE);} else glDisable(GL_LINE_STIPPLE)", "setlinestyle: Check list numbering."),
    glArgs("defpattern", "glNewList($1, GL_COMPILE); glPolygonStipple(MASK($3)); glEndList()", "glPolygonStipple:#\tSee man page to change $3 into mask.#\tYou don't really need to make a display list.#\tCheck list numbering."),
    glArgs("defrasterfont", "glXUseXFont( font, first, count, listBase)", "glXUseFont: see man page"), 
    glArgs("delobj", "glDeleteLists( $1, 1)", "glDeleteLists: check object numbers"), 
    glArgs("dither", "if($1) glEnable(GL_DITHER); else glDisable(GL_DITHER)"), 
    glArgs("drawmode", "glxChooseVisual(*display, screen, *attriblist)", "glxChooseVisual: add $1 to attriblist"), 
    glArgs("acsize", "glxChooseVisual(*display, screen, *attriblist)", "glxChooseVisual: add GLX_ACCUM_RED_SIZE, $1, etc. to attriblist"), 
    glArgs("endpick", "glRenderMode(GL_RENDER); glMatrixMode(GL_PROJECTION); glLoadIdentity(); gluPerspective( fovy, aspect, znear, zfar ); glMatrixMode(GL_MODELVIEW);", "endpick:#\treplace gluPerspective args#\tor use glPopMatrix() to restore."), 
    glArgs("feedback", "glFeedbackBuffer($2, GL_3D_COLOR, $1); glRenderMode(GL_FEEDBACK);"), 
    glArgs("fogvertex", "glFogfv($1, $2); if($1) glEnable(GL_FOG); else glDisable(GL_FOG)", "Fog: have to translate params."), 
    glArgs("frontbuffer", "glDrawBuffer(($1) ? GL_FRONT : GL_BACK)", "frontbuffer: other possibilities include GL_FRONT_AND_BACK"),
    glArgs("backbuffer", "glDrawBuffer(($1) ? GL_BACK : GL_FRONT)", "backbuffer: other possibilities include GL_FRONT_AND_BACK"),
    glArgs("frontface", "glCullFace(($1) ? GL_FRONT : GL_BACK); ($1) ? glEnable(GL_CULL_FACE):glDisable(GL_CULL_FACE)"),
    glArgs("backface", "glCullFace(!($1) ? GL_FRONT : GL_BACK); ($1) ? glEnable(GL_CULL_FACE):glDisable(GL_CULL_FACE)"),
    glArgs("draw", "glVertex3f($1, $2, $3)", "Add glEnd() after these vertices,  before next glBegin()"), 
    glArgs("draw2", "glVertex2f($1, $2)","Add glEnd() after these vertices,  before next glBegin()"), 
    glArgs("draw2i", "glVertex2i($1, $2)","Add glEnd() after these vertices,  before next glBegin()"), 
    glArgs("draw2s", "glVertex2s($1, $2)","Add glEnd() after these vertices,  before next glBegin()"), 
    glArgs("drawi", "glVertex3i($1, $2, $3)","Add glEnd() after these vertices,  before next glBegin()"), 
    glArgs("draws", "glVertex3s($1, $2, $3)","Add glEnd() after these vertices,  before next glBegin()"), 
    glArgs("move", "glBegin(GL_LINE_STRIP); glVertex3f($1, $2, $3)", "glBegin: Use GL_LINES if only one line segment is desired."), 
    glArgs("move2", "glBegin(GL_LINE_STRIP); glVertex2f($1, $2)", "glBegin: Use GL_LINES if only one line segment is desired."),
    glArgs("move2i", "glBegin(GL_LINE_STRIP); glVertex2i($1, $2)", "glBegin: Use GL_LINES if only one line segment is desired."),
    glArgs("move2s", "glBegin(GL_LINE_STRIP); glVertex2s($1, $2)", "glBegin: Use GL_LINES if only one line segment is desired."),
    glArgs("movei", "glBegin(GL_LINE_STRIP); glVertex3i($1, $2, $3)", "glBegin: Use GL_LINES if only one line segment is desired."),
    glArgs("moves", "glBegin(GL_LINE_STRIP); glVertex3s($1, $2, $3)", "glBegin: Use GL_LINES if only one line segment is desired."),
    glArgs("getdisplaymode", "(glGetIntegerv(GL_INDEX_MODE, &dmtmp), dmtmp)", "get display mode:#\tHow to tell if doublebuffered?#\tYou can do better than this.#GLint dmtmp;"),
    glArgs("getdrawmode", "glxGetCurrentContext()", "see man page"),
    glArgs("getgconfig", "(glGetIntegerv($1, &gctmp), gctmp)", "getgconfig:#GLint gctmp;"), 
    glArgs("getgdesc", "(glGetIntegerv($1, &gdtmp), gdtmp)", "getgdesc other posiblilties:#\tglxGetConfig();#\tglxGetCurrentContext();#\tglxGetCurrentDrawable();#GLint gdtmp;"), 
    glArgs("getmatrix", "glGetFloatv(GL_MODELVIEW_MATRIX, $1)", "getmatrix: you might mean#glGetFloatv(GL_PROJECTION_MATRIX, $1)"),
    glArgs("getmmode", "(glGetIntegerv(GL_MATRIX_MODE, &gmtmp), gmtmp)", "getmmode: translate returned values#GLint mmtmp;"),
    glArgs("getnurbsproperty", "gluGetNurbsProperty(GL_MATRIX_MODE, &tmp)", "see man page for gluGetNurbsProperty#move results from tmp."),
    glArgs("getopenobj", "(glGetIntegerv(GL_LIST_INDEX, &tmp),  tmp)", "getopenobj: #int tmp;"),
    glArgs("getpattern", "glGetPolygonStipple(mask)", "glGetPolygonStipple:#\tmask is a 32x32 array (See man page).#\tGLuByte *mask;"),
    glArgs("getplanes", "(glGetIntegerv(GL_INDEX_BITS, &tmp),  tmp)", "getplanes:#int tmp;"),
    glArgs("getscrmask", "{ GLint tmp[4]; glGetIntegerv(GL_SCISSOR_BOX, &tmp);*($1)=tmp[0];*($2)=tmp[0]+tmp[2]-1;*($3)=tmp[1];*($4)=tmp[1]+tmp[3]-1;}", "get GL_SCISSOR_BOX:#You can probably do better than this."),
    glArgs("getsm", "(glGetIntegerv(GL_SHADE_MODEL, &tmp), tmp)", "getsm:#GLint tmp;"),
    glArgs("getviewport", "{GLint tmp[4];glGetIntegerv(GL_VIEWPORT, &tmp);*($1)=tmp[0];*($2)=tmp[0]+tmp[2]-1;*($3)=tmp[1];*($4)=tmp[1]+tmp[3]-1;}", "get GL_VIEWPORT:#You can probably do better than this."),
    glArgs("getwritemask", "(glGetIntegerv( (glGetIntegerv(GL_INDEX_MODE, &tmp), tmp) ? GL_INDEX_WRITEMASK : GL_COLOR_MASK, &tmp), tmp)", "getwritemask:#GLint tmp;"),
    glArgs("getzbuffer", "glIsEnabled(GL_DEPTH_TEST)"),
    glArgs("gflush", "glFlush()"),
    glArgs("gRGBcolor", "{int tmp[4]; glGetIntegerv(GL_CURRENT_COLOR, tmp);*($1)=tmp[0];*($2)=tmp[1];*($3)=tmp[2];}", "get GL_CURRENT_COLOR: scale color values"), 
    glArgs("gRGBmask", "{GLboolean tmp[4]; glGetBooleanv(GL_COLOR_WRITEMASK, tmp);*($1)=tmp[0];*($2)=tmp[1];*($3)=tmp[2];}"), 
    glArgs("gselect", "glSelectBuffer($2, $1); glRenderMode(GL_SELECT)"),
    glArgs("isobj", "glIsList($1)", "glIsList: check object numbering"), 
    glArgs("lcharstr", "glCallLists(lstrlen($2), $1, $2)", "lcharstr: replace lstrlen with strlen(string) like function"), 
    glArgs("linewidthf", "glLineWidth($1)"), 
    glArgs("linewidth", "glLineWidth((GLfloat)($1))"),
    glArgs("getlwidth", "(glGetIntegerv(GL_LINE_WIDTH, &tmp), tmp)", "line width:#Could also be:#float tmp;#glGetFloatv(GL_LINE_WIDTH, &tmp);"),
    glArgs("lmcolor", "glColorMaterial(GL_FRONT_AND_BACK, $1);glEnable(GL_COLOR_MATERIAL)", "lmcolor: if LMC_NULL,  use:#glDisable(GL_COLOR_MATERIAL);"),
    glArgs("loadmatrix", "glLoadMatrix($1)"),
    glArgs("loadname", "glLoadName($1)"),
    glArgs("logicop", "glLogicOp($1); if($1 == GL_COPY) glDisable(GL_LOGIC_OP); else glEnable(GL_LOGIC_OP)"),
    glArgs("lookat", "gluLookAt($1, $2, $3, $4, $5, $6, UPX($7), UPY($7), UPZ($7))", "lookat: replace UPx with vector"),
    glArgs("lrectread", "glReadPixels($1, $2, ($3)-($1)+1, ($4)-($2)+1, GL_RGBA, GL_BYTE, $5)", "lrectread: see man page for glReadPixels"),
    glArgs("rectread", "glReadPixels($1, $2, ($3)-($1)+1, ($4)-($2)+1, GL_COLOR_INDEX, GL_SHORT, $5)", "rectread: see man page for glReadPixels"),
    glArgs("lrectwrite", "glRasterPos2i($1, $2);glDrawPixels(($3)-($1)+1, ($4)-($2)+1, GL_RGBA, GL_BYTE, $5)", "lrectwrite: see man page for glDrawPixels"),
    glArgs("rectwrite", "glRasterPos2i($1, $2);glDrawPixels(($3)-($1)+1, ($4)-($2)+1, GL_COLOR_INDEX, GL_SHORT, $5)", "rectwrite: see man page for glDrawPixels"),
    glArgs("lsetdepth", "glDepthRange($1, $2)", "glDepthRange params must be scaled to [0, 1]"),
    glArgs("setdepth", "glDepthRange($1, $2)", "glDepthRange params must be scaled to [0, 1]"),
    glArgs("lsrepeat", "glLineStipple($1, pattern)", "lsrepeat: combine with pattern from deflinestyle"),
    glArgs("getlsrepeat", "glGetIntegerv(GL_LINE_STIPPLE_REPEAT, &tmp)", "getlsrepeat: move tmp into your variable."),
    glArgs("makeobj", "glNewList($1, GL_COMPILE)", "Check list numbering."),
    glArgs("mapw", "gluProject(XXX)", "XXX I think this is backwards"),
    glArgs("mapw2", "gluProject(XXX)", "XXX I think this is backwards"),
    glArgs("mmode", "glMatrixMode($1)"),
    glArgs("nmode", "if($1) glEnable(GL_NORMALIZE); else glDisable(GL_NORMALIZE)"),
    glArgs("noport", "glxCreateGLXPixmap(*display, *visual, pixmap)", "noport: see man page"),
    glArgs("nurbscurve", "gluNurbsCurve(*nobj, $1, $2, $3, $4, $5, $6)", "gluNurbsCurve: replace nobj with your object#See man page"),
    glArgs("nurbssurface", "gluNurbsSurface(*nobj, $1, $2, $3, $4, $5, $6, $7, $8, $9, $a)", "gluNurbsCurve: replace nobj with your object#See man page"),
    glArgs("objdelete", "glDeleteLists(LIST($1, $2), RANGE($1, $2))", "objdelete: tags not supported#See glDeleteLists man page."),
    glArgs("ortho", "{GLint mm; glGetIntegerv(GL_MATRIX_MODE, &mm);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho($1, $2, $3, $4, $5, $6);glMatrixMode(mm);}"),
    glArgs("ortho2", "{GLint mm; glGetIntegerv(GL_MATRIX_MODE, &mm);glMatrixMode(GL_PROJECTION);glLoadIdentity();gluOrtho2D($1, $2, $3, $4);glMatrixMode(mm);}"),
    glArgs("overlay",  "glxChooseVisual(*dpy, screen, *attriblist)", "overlay: use GLX_BUFFER_SIZE $1, GLX_LEVEL 1 in attriblist"), 
    glArgs("underlay",  "glxChooseVisual(*dpy, screen, *attriblist)", "underlay: use GLX_BUFFER_SIZE $1, GLX_LEVEL -1 in attriblist"), 
    glArgs("passthrough", "glPassThrough($1)"),
    glArgs("pdr", "glVertex3f($1, $2, $3)"),
    glArgs("pdri", "glVertex3i($1, $2, $3)"),
    glArgs("pdrs", "glVertex3s($1, $2, $3)"),
    glArgs("pdr2", "glVertex2f($1, $2)"),
    glArgs("pdr2i", "glVertex2i($1, $2)"),
    glArgs("pdr2s", "glVertex2s($1, $2)"),
    glArgs("pmv", "glBegin(GL_POLYGON);glVertex3f($1, $2, $3)"),
    glArgs("pmvi", "glBegin(GL_POLYGON);glVertex3i($1, $2, $3)"),
    glArgs("pmvs", "glBegin(GL_POLYGON);glVertex3s($1, $2, $3)"),
    glArgs("pmv2", "glBegin(GL_POLYGON);glVertex2f($1, $2)"),
    glArgs("pmv2i", "glBegin(GL_POLYGON);glVertex2i($1, $2)"),
    glArgs("pmv2s", "glBegin(GL_POLYGON);glVertex2s($1, $2)"),
    glArgs("perspective", "{GLint mm;glGetIntegerv(GL_MATRIX_MODE, &mm);glMatrixMode(GL_PROJECTION);glLoadIdentity();gluPerspective(.1*($1), $2, $3, $4);glMatrixMode(mm);}"),
    glArgs("pick", "glSelectBuffer($2, $1);glRenderMode(GL_SELECT);glMatrixMode(GL_PROJECTION);gluPickMatrix(x, y, w, h, viewport);glMatrixMode(GL_MODELVIEW)", "pick:#\tSelect buffer is type GLuint.#\tSet gluPickMatrix params.#See man pages.#\tMight want to push Projection matrix if you have endpick pop it."),
    glArgs("picksize", "gluPickMatrix(x, y, $1, $2, viewport)", "picksize: merge this with other gluPickMatrix call due to pick()"),
    glArgs("pixmode", "glPixelTransfer($1, $2)", "pixmode: see glPixelTransfer man page#Translate parameters."),
    glArgs("pixmodef", "glPixelTransfer($1, $2)", "pixmodef: see glPixelTransfer man page#Translate parameters."),
    glArgs("pnt", "glBegin(GL_POINTS);glVertex3f($1, $2, $3);glEnd()", "points: put as many vertices as possible between Begin and End"), 
    glArgs("pnti", "glBegin(GL_POINTS);glVertex3i($1, $2, $3);glEnd()", "points: put as many vertices as possible between Begin and End"), 
    glArgs("pnts", "glBegin(GL_POINTS);glVertex3s($1, $2, $3);glEnd()", "points: put as many vertices as possible between Begin and End"), 
    glArgs("pnt2", "glBegin(GL_POINTS);glVertex2f($1, $2);glEnd()", "points: put as many vertices as possible between Begin and End"), 
    glArgs("pnt2i", "glBegin(GL_POINTS);glVertex2i($1, $2);glEnd()", "points: put as many vertices as possible between Begin and End"), 
    glArgs("pnt2s", "glBegin(GL_POINTS);glVertex2s($1, $2);glEnd()", "points: put as many vertices as possible between Begin and End"), 
    glArgs("pntsize", "glPointSize((GLfloat)($1))"),
    glArgs("pntsizef", "glPointSize((GLfloat)($1))"),
    glArgs("pntsmooth", "{if($1) glEnable(GL_POINT_SMOOTH); else glDisable(GL_POINT_SMOOTH);}"),
    glArgs("polf", "{int i; glBegin(GL_POLYGON); for(i = 0; i < $1; i++) glVertex3f(($2)[i]); glEnd();}"),
    glArgs("polfi", "{int i; glBegin(GL_POLYGON); for(i = 0; i < $1; i++) glVertex3i(($2)[i]); glEnd();}"),
    glArgs("polfs", "{int i; glBegin(GL_POLYGON); for(i = 0; i < $1; i++) glVertex3s(($2)[i]); glEnd();}"),
    glArgs("polf2", "{int i; glBegin(GL_POLYGON); for(i = 0; i < $1; i++) glVertex2f(($2)[i]); glEnd();}"),
    glArgs("polf2i", "{int i; glBegin(GL_POLYGON); for(i = 0; i < $1; i++) glVertex2i(($2)[i]); glEnd();}"),
    glArgs("polf2s", "{int i; glBegin(GL_POLYGON); for(i = 0; i < $1; i++) glVertex2s(($2)[i]); glEnd();}"),
    glArgs("poly", "{int i; glBegin(GL_LINE_LOOP); for(i = 0; i < $1; i++) glVertex3f(($2)[i]); glEnd();}"),
    glArgs("polyi", "{int i; glBegin(GL_LINE_LOOP); for(i = 0; i < $1; i++) glVertex3i(($2)[i]); glEnd();}"),
    glArgs("polys", "{int i; glBegin(GL_LINE_LOOP); for(i = 0; i < $1; i++) glVertex3s(($2)[i]); glEnd();}"),
    glArgs("poly2", "{int i; glBegin(GL_LINE_LOOP); for(i = 0; i < $1; i++) glVertex2f(($2)[i]); glEnd();}"),
    glArgs("poly2i", "{int i; glBegin(GL_LINE_LOOP); for(i = 0; i < $1; i++) glVertex2i(($2)[i]); glEnd();}"),
    glArgs("poly2s", "{int i; glBegin(GL_LINE_LOOP); for(i = 0; i < $1; i++) glVertex2s(($2)[i]); glEnd();}"),
    glArgs("polymode", "glPolygonMode(GL_FRONT_AND_BACK, $1)"),
    glArgs("polymooth", "{if($1) glEnable(GL_POLYGON_SMOOTH); else glDisable(GL_POLYGON_SMOOTH);}"),
    glArgs("pushname", "glPushName($1)"),
    glArgs("pwlcurve", "gluPwlCurve(*nobj, $1, $2, $3, $4)", "gluPwlCurve: replace nobj with your object#See man page"),
    glArgs("rdr", "glVertex3f($1, $2, $3)", "Add glEnd() after these vertices,  before next glBegin().#Relative drawing not supported -- change"), 
    glArgs("rdr2", "glVertex2f($1, $2)","Add glEnd() after these vertices,  before next glBegin().#Relative drawing not supported -- change"), 
    glArgs("rdr2i", "glVertex2i($1, $2)","Add glEnd() after these vertices,  before next glBegin().#Relative drawing not supported -- change"), 
    glArgs("rdr2s", "glVertex2s($1, $2)","Add glEnd() after these vertices,  before next glBegin().#Relative drawing not supported -- change"), 
    glArgs("rdri", "glVertex3i($1, $2, $3)","Add glEnd() after these vertices,  before next glBegin().#Relative drawing not supported -- change"), 
    glArgs("rdrs", "glVertex3s($1, $2, $3)","Add glEnd() after these vertices,  before next glBegin().#Relative drawing not supported -- change"), 
    glArgs("readdisplay", "glReadPixels($1, $2, ($3)-($1)+1, ($4)-($2)+1, GL_RGBA, GL_BYTE, $5)", "readdisplay: see man page for glReadPixels"),
    glArgs("readRGB", "{ int tmp[4]; glGetIntegerv(GL_CURRENT_RASTER_POS, tmp);glReadPixels(tmp[0], tmp[1], $1, 1,  GL_RGBA, GL_BYTE, array);}", "readRGB: see man page for glReadPixels"),
    glArgs("readpixels", "{ GLint tmp[4]; glGetIntegerv(GL_CURRENT_RASTER_POS, tmp);glReadPixels(tmp[0], tmp[1], $1, 1,  GL_INDEX, GL_SHORT, $2);}", "readpixels: see man page for glReadPixels"),
    glArgs("readsource", "glReadBuffer($1)"),
    glArgs("rect", "glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glRectf($1, $2, $3, $4); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)", "rect: remove extra PolygonMode changes"),
    glArgs("recti", "glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glRecti($1, $2, $3, $4); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)", "rect: remove extra PolygonMode changes"),
    glArgs("rects", "glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glRects($1, $2, $3, $4); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)", "rect: remove extra PolygonMode changes"),
    glArgs("rectf", "glRectf($1, $2, $3, $4)"),
    glArgs("rectfi", "glRecti($1, $2, $3, $4)"),
    glArgs("rectfs", "glRects($1, $2, $3, $4)"),
    glArgs("sbox", "glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glRect($1, $2, $3, $4); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)", "sbox: remove extra PolygonMode changes.#not screen-aligned -- See glRect"),
    glArgs("sboxi", "glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glRecti($1, $2, $3, $4); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)", "sboxi: remove extra PolygonMode changes.#not screen-aligned -- See glRect"),
    glArgs("sboxs", "glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); glRects($1, $2, $3, $4); glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)", "sboxs: remove extra PolygonMode changes.#not screen-aligned -- See glRect"),
    glArgs("sboxf", "glRect($1, $2, $3, $4)", "sboxf not screen-aligned -- See glRect"),
    glArgs("sboxfi", "glRecti($1, $2, $3, $4)", "sboxfi not screen-aligned -- See glRect"),
    glArgs("sboxfs", "glRects($1, $2, $3, $4)", "sboxfs not screen-aligned -- See glRect"),
    glArgs("rectcopy", "glRasterPos2i($5, $6);glCopyPixels($1, $2, $3-$1+1, $4-$2+1, GL_COLOR)"),
    glArgs("rectzoom", "glPixelZoom($1, $2)"),
    glArgs("RGBwritemask", "glColorMask($1, $2, $3, GL_TRUE)", "glColorMask: only Boolean values allowed"),
    glArgs("rmv", "glBegin(GL_LINES); glVertex3f($1, $2, $3)", "Relative drawing not supported -- change"), 
    glArgs("rmv2", "glBegin(GL_LINES); glVertex2f($1, $2)", "Relative drawing not supported -- change"),
    glArgs("rmv2i", "glBegin(GL_LINES); glVertex2i($1, $2)", "Relative drawing not supported -- change"),
    glArgs("rmv2s", "glBegin(GL_LINES); glVertex2s($1, $2)", "Relative drawing not supported -- change"),
    glArgs("rmvi", "glBegin(GL_LINES); glVertex3i($1, $2, $3)", "Relative drawing not supported -- change"),
    glArgs("rmvs", "glBegin(GL_LINES); glVertex3s($1, $2, $3)", "Relative drawing not supported -- change"),
    glArgs("rot", "glRotatef($1, ($2)=='x'||($2)=='X', ($2)=='y'||($2)=='Y', ($2)=='z'||($2)=='Z')", "You can do better than this."),
    glArgs("rotate", "glRotatef(.1*($1), ($2)=='x'||($2)=='X', ($2)=='y'||($2)=='Y', ($2)=='z'||($2)=='Z')", "You can do better than this."),
    glArgs("rpdr", "glVertex3f($1, $2, $3)", "Relative drawing not supported -- change"),
    glArgs("rpdri", "glVertex3i($1, $2, $3)", "Relative drawing not supported -- change"),
    glArgs("rpdrs", "glVertex3s($1, $2, $3)", "Relative drawing not supported -- change"),
    glArgs("rpdr2", "glVertex2f($1, $2)", "Relative drawing not supported -- change"),
    glArgs("rpdr2i", "glVertex2i($1, $2)", "Relative drawing not supported -- change"),
    glArgs("rpdr2s", "glVertex2s($1, $2)", "Relative drawing not supported -- change"),
    glArgs("rpmv", "glBegin(GL_POLYGON);glVertex3f($1, $2, $3)", "Relative drawing not supported -- change"),
    glArgs("rpmvi", "glBegin(GL_POLYGON);glVertex3i($1, $2, $3)", "Relative drawing not supported -- change"),
    glArgs("rpmvs", "glBegin(GL_POLYGON);glVertex3s($1, $2, $3)", "Relative drawing not supported -- change"),
    glArgs("rpmv2", "glBegin(GL_POLYGON);glVertex2f($1, $2)", "Relative drawing not supported -- change"),
    glArgs("rpmv2i", "glBegin(GL_POLYGON);glVertex2i($1, $2)", "Relative drawing not supported -- change"),
    glArgs("rpmv2s", "glBegin(GL_POLYGON);glVertex2s($1, $2)", "Relative drawing not supported -- change"),
    glArgs("scale", "glScalef($1, $2, $3)"),
    glArgs("sclear", "glClearStencil($1);glClear(GL_STENCIL_BUFFER_BIT)"),
    glArgs("scrmask", "glScissor($1, $2, $3, $4); glEnable(GL_SCISSOR_TEST)"),
    glArgs("setnurbsproperty", "gluNurbsProperty(*nobj, $1, $2)", "Replace nobj with your object -- see man page"),
    glArgs("setpattern", "if($1) {glCallList($1); glEnable(GL_POLYGON_STIPPLE);} else glDisable(GL_POLYGON_STIPPLE)", "pattern: check list numbering.#See substitutions made for defpattern.#see glPolygonStipple man page."),     
    glArgs("shademodel", "glShadeModel($1)"),
    glArgs("splf", "{int i; glBegin(GL_POLYGON); for(i = 0; i < $1; i++) glVertex3f(($2)[i]); glEnd();}"),
    glArgs("splfi", "{int i; glBegin(GL_POLYGON); for(i = 0; i < $1; i++) glVertex3i(($2)[i]); glEnd();}"),
    glArgs("splfs", "{int i; glBegin(GL_POLYGON); for(i = 0; i < $1; i++) glVertex3s(($2)[i]); glEnd();}"),
    glArgs("splf2", "{int i; glBegin(GL_POLYGON); for(i = 0; i < $1; i++) glVertex2f(($2)[i]); glEnd();}"),
    glArgs("splf2i", "{int i; glBegin(GL_POLYGON); for(i = 0; i < $1; i++) glVertex2i(($2)[i]); glEnd();}"),
    glArgs("splf2s", "{int i; glBegin(GL_POLYGON); for(i = 0; i < $1; i++) glVertex2s(($2)[i]); glEnd();}"),
    glArgs("stencil", "if($1) { glEnable(GL_STENCIL_TEST);glStencilFunc($3, $2, $4); glStencilOp($5, $6, $7);} else glDisable(GL_STENCIL_TEST);"),
    glArgs("stensize", "glStencilMask(0xff>>(8-($1)))"),
    glArgs("swritemask", "glStencilMask($1)"),
    glArgs("viewport", "glViewport($1, $3, ($2)-($1)+1, ($4)-($3)+1); glScissor($1, $3, ($2)-($1)+1, ($4)-($3)+1)"),
    glArgs("window", "{GLint mm;glGetIntegerv(GL_MATRIX_MODE, &mm);glMatrixMode(GL_PROJECTION);glLoadIdentity();glFrustum($1, $2, $3, $4, $5, $6);glMatrixMode(mm);}"),
    glArgs("wmpack", "glColorMask(($1)&0xff, (($1)>>8)&0xff, (($1)>>16)&0xff, (($1)>>24)&0xff)"),
    glArgs("writemask", "glIndexMask($1)"),
    glArgs("writepixels", "glDrawPixels($1, 1, GL_COLOR_INDEX, GL_SHORT, $2)", "writepixels: see man page for glDrawPixels"),
    glArgs("writeRGB", "glDrawPixels($1, 1, GL_RGBA, GL_BYTE, PACK($2, $3, $4))", "writeRGB: see man page for glDrawPixels#\tYouhave to pack the arrays into RGBA"),
    glArgs("zbuffer", "if($1) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST)"),
    glArgs("zfunction", "glDepthFunc($1)"),
    glArgs("zwritemask", "glDepthMask((GLBoolean)($1))",  "glDepthMask is boolean only"),
    glArgs("getbackface", "(glIsEnabled(GL_CULL_FACE) && (glGetIntegerv(GL_CULL_FACE_MODE, &tmp), (tmp == GL_BACK)))", "You can probably do better than this.#long tmp;"),  
    glArgs("getbuffer", "(glGetIntegerv(GL_DRAW_BUFFERS, &tmp),  tmp)", "getbuffer: translate results.#long tmp;"),  
    glArgs("getcmmode", "(glGetBooleanv(GL_INDEX_MODE, &tmp), tmp)", "getcmmode: multimap mode not supported.#long tmp;"),  
    glArgs("getcpos", "{ GLint cpostmp[4]; glGetIntegerv(GL_CURRENT_RASTER_POSITION, cpostmp); *($1) = cpostmp[0]; *($2) = cpostmp[1];}", "getcpos: You can probably do better than this."),  
    glArgs("texgen", "glTexGenfv($1, $2, $3)", "texgen: translate parameters."),
    glArgs("tevdef", "glNewList($1); glTexEnvfv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, MODES($3)); glEndList()", "glTexEnvfv:#\tSee man page.#\tTranslate parameters.#\tCheck list numbering."),
    glArgs("texdef2d", "glNewList($1); glTexImage2D(GL_TEXTURE_2D, 0, $2, $3, $4, border, GL_RGBA, GL_UNSIGNED_BYTE, $5); glEndList()", "glTexImage2D:#\tSee man page.#\tIf MipMaps desired, use gluBuild2DMIPmaps().#\tTranslate parameters.#\tSee glTexParameterf().#\tCheck list numbering."),
    glArgs("texbind", "if($2) {glCallList($2); glEnable(GL_TEXTURE_2D);} else glDisable(GL_TEXTURE_2D)", "texbind: check list numbering"),
    glArgs("tevbind", "if($2) {glCallList($2); glEnable(GL_TEXTURE_2D);} else glDisable(GL_TEXTURE_2D)", "tevbind: check list numbering"),
    glArgs("polarview", "glTranslatef(0., 0., -($1)); glRotatef( -($4)*10., 0., 0., 1.); glRotatef( -($3)*10., 1., 0., 0.); glRotatef( -($2)*10., 0., 0., 1);"),
};


glDefine defines[] = {
    glDefine("SRC_AUTO", "GL_BACK", "SRC_AUTO not really supported -- see glReadBuffer man page"), 
    glDefine("SRC_BACK", "GL_BACK"), 
    glDefine("SRC_FRONT", "GL_FRONT"), 
    glDefine("SRC_ZBUFFER", "XXX", "SRC_ZBUFFER: use GL_DEPTH_COMPONENT in glReadPixels call"), 
    glDefine("SRC_PUP", "GL_AUX0", "XXX which aux buffer is pup planes?"), 
    glDefine("SRC_OVER", "GL_AUX1", "XXX which aux buffer is overlay planes?"), 
    glDefine("SRC_UNDER", "GL_AUX2", "XXX which aux buffer is underlay planes?"), 
    glDefine("SRC_FRAMEGRABBER", "XXX", "SRC_FRAMEGRABBER not supported"), 
    glDefine("BF_ZERO", "GL_ZERO"), 
    glDefine("BF_ONE", "GL_ONE"), 
    glDefine("BF_DC", "GL_DST_COLOR"), 
    glDefine("BF_SC", "GL_SRC_COLOR"), 
    glDefine("BF_MDC", "GL_ONE_MINUS_DST_COLOR"), 
    glDefine("BF_MSC", "GL_ONE_MINUS_SRC_COLOR"), 
    glDefine("BF_SA", "GL_SRC_ALPHA"), 
    glDefine("BF_MSA", "GL_ONE_MINUS_SRC_ALPHA"), 
    glDefine("BF_DA", "GL_DST_ALPHA"), 
    glDefine("BF_MDA", "GL_ONE_MINUS_DST_ALPHA"), 
    glDefine("BF_MIN_SA_MDA", "GL_SRC_ALPHA_SATURATE"), 
    glDefine("AF_NEVER", "GL_NEVER"), 
    glDefine("AF_LESS", "GL_LESS"), 
    glDefine("AF_EQUAL", "GL_EQUAL"), 
    glDefine("AF_LEQUAL", "GL_LEQUAL"), 
    glDefine("AF_GREATER", "GL_GREATER"), 
    glDefine("AF_NOTEQUAL", "GL_NOTEQUAL"), 
    glDefine("AF_GEQUAL", "GL_GEQUAL"), 
    glDefine("AF_ALWAYS", "GL_ALWAYS"), 
    glDefine("ZF_NEVER", "GL_NEVER"), 
    glDefine("ZF_LESS", "GL_LESS"), 
    glDefine("ZF_EQUAL", "GL_EQUAL"), 
    glDefine("ZF_LEQUAL", "GL_LEQUAL"), 
    glDefine("ZF_GREATER", "GL_GREATER"), 
    glDefine("ZF_NOTEQUAL", "GL_NOTEQUAL"), 
    glDefine("ZF_GEQUAL", "GL_GEQUAL"), 
    glDefine("ZF_ALWAYS", "GL_ALWAYS"), 
    glDefine("SMP_OFF", "0"), 
    glDefine("SMP_ON", "1"), 
    glDefine("SMP_SMOOTHER", "1"), 
    glDefine("SML_OFF", "0"), 
    glDefine("SML_ON", "1"), 
    glDefine("SML_SMOOTHER", "1"), 
    glDefine("SML_END_CORRECT", "1"), 
    glDefine("PYSM_OFF", "0"), 
    glDefine("PYSM_ON", "1"), 
    glDefine("PYSM_SHRINK", "1", "PYSM_SHRINK not really supported -- see glEnable man page"), 
    glDefine("DT_OFF", "0"), 
    glDefine("DT_ON", "1"), 
    glDefine("FLAT", "GL_FLAT"), 
    glDefine("GOURAUD", "GL_SMOOTH"), 
    glDefine("LO_ZERO", "GL_CLEAR"),
    glDefine("LO_AND", "GL_AND"),
    glDefine("LO_ANDR", "GL_AND_REVERSE"),
    glDefine("LO_SRC", "GL_COPY"),
    glDefine("LO_ANDI", "GL_AND_INVERTED"),
    glDefine("LO_DST", "GL_NOOP"),
    glDefine("LO_XOR", "GL_XOR"),
    glDefine("LO_OR", "GL_OR"),
    glDefine("LO_NOR", "GL_NOR"),
    glDefine("LO_XNOR", "GL_EQUIV"),
    glDefine("LO_NDST", "GL_INVERT"),
    glDefine("LO_ORR", "GL_OR_REVERSE"),
    glDefine("LO_NSRC", "GL_COPY_INVERTED"),
    glDefine("LO_ORI", "GL_OR_INVERTED"),
    glDefine("LO_NAND", "GL_NAND"),
    glDefine("LO_ONE", "GL_SET"),
    glDefine("ST_KEEP", "GL_KEEP"),
    glDefine("ST_ZERO", "GL_ZERO"),
    glDefine("ST_REPLACE", "GL_REPLACE"),
    glDefine("ST_INCR", "GL_INCR"),
    glDefine("ST_DECR", "GL_DECR"),
    glDefine("ST_INVERT", "GL_INVERT"),
    glDefine("SF_NEVER", "GL_NEVER"),
    glDefine("SF_LESS", "GL_LESS"),
    glDefine("SF_EQUAL", "GL_EQUAL"),
    glDefine("SF_LEQUAL", "GL_LEQUAL"),
    glDefine("SF_GREATER", "GL_GREATER"),
    glDefine("SF_NOTEQUAL", "GL_NOTEQUAL"),
    glDefine("SF_GEQUAL", "GL_GEQUAL"),
    glDefine("SF_ALWAYS", "GL_ALWAYS"),
    glDefine("PYM_FILL", "GL_FILL"),
    glDefine("PYM_POINT", "GL_POINT"),
    glDefine("PYM_LINE", "GL_LINE"),
    glDefine("PYM_HOLLOW", "GL_LINE", "polymode PYM_HOLLOW not supported"),
    glDefine("PYM_LINE_FAST", "GL_LINE"),
    glDefine("FG_OFF", "0"),
    glDefine("FG_ON", "1"),
    glDefine("FG_DEFINE", "XXX fg_define", "see glFogf man page"),
    glDefine("FG_VTX_EXP", "GL_FOG_EXP", "see glFogf man page"),
    glDefine("FG_VTX_LIN", "GL_LINEAR", "see glFogf man page"),
    glDefine("FG_PIX_EXP", "GL_EXP", "see glFogf man page"),
    glDefine("FG_PIX_LIN", "GL_LINEAR", "see glFogf man page"),
    glDefine("FG_VTX_EXP2", "GL_EXP2", "see glFogf man page"),
    glDefine("FG_PIX_EXP2", "GL_EXP2", "see glFogf man page"),
    glDefine("PM_SHIFT", "XXX_SHIFT", "pixmode: see glPixelTransfer man page"),
    glDefine("PM_EXPAND", "XXX_EXPAND", "pixmode: see glPixelTransfer man page"),
    glDefine("PM_C0", "XXX_C0", "pixmode: see glPixelTransfer man page"),
    glDefine("PM_C1", "XXX_C1", "pixmode: see glPixelTransfer man page"),
    glDefine("PM_ADD24", "XXX_ADD24", "pixmode: see glPixelTransfer man page"),
    glDefine("PM_SIZE", "XXX_SIZE", "pixmode: see glPixelTransfer man page"),
    glDefine("PM_OFFSET", "XXX_OFFSET", "pixmode: see glPixelTransfer man page"),
    glDefine("PM_STRIDE", "XXX_STRIDE", "pixmode: see glPixelTransfer man page"),
    glDefine("PM_TTOB", "XXX_TTOB", "pixmode: see glPixelTransfer man page"),
    glDefine("PM_RTOL", "XXX_RTOL", "pixmode: see glPixelTransfer man page"),
    glDefine("PM_ZDATA", "XXX_ZDATA", "pixmode: see glPixelTransfer man page"),
    glDefine("PM_WARP", "XXX_WARP", "pixmode: see glPixelTransfer man page"),
    glDefine("PM_RDX", "XXX_RDX", "pixmode: see glPixelTransfer man page"),
    glDefine("PM_RDY", "XXX_RDY", "pixmode: see glPixelTransfer man page"),
    glDefine("PM_CDX", "XXX_CDX", "pixmode: see glPixelTransfer man page"),
    glDefine("PM_CDY", "XXX_CDY", "pixmode: see glPixelTransfer man page"),
    glDefine("PM_XSTART", "XXX_XSTART", "pixmode: see glPixelTransfer man page"),
    glDefine("PM_YSTART", "XXX_YSTART", "pixmode: see glPixelTransfer man page"),
    glDefine("PM_VO1", "XXX_VO1", "pixmode: see glPixelTransfer man page"),
    glDefine("NAUTO", "0"),
    glDefine("NNORMALIZE", "1"),
    glDefine("AC_CLEAR", "XXX_CLEAR",  "see glAccum man page for how to clear:#glClearAccum(r, g, b, a);#glClear(GL_ACCUM_BUFFER_BIT);"),
    glDefine("AC_ACCUMULATE", "GL_ACCUM"),
    glDefine("AC_CLEAR_ACCUMULATE", "GL_LOAD"),
    glDefine("AC_RETURN", "GL_RETURN"),
    glDefine("AC_MULT", "GL_MULT"),
    glDefine("AC_ADD", "GL_ADD"),
    glDefine("CP_OFF", "0"),
    glDefine("CP_ON", "1"),
    glDefine("CP_DEFINE", "2"),
    glDefine("GD_XPMAX", "XXX_XPMAX", "see window manager"),
    glDefine("GD_YPMAX", "XXX_YPMAX", "see window manager"),
    glDefine("GD_XMMAX", "XXX_XMMAX", "see window manager"),
    glDefine("GD_YMMAX", "XXX_YMMAX", "see window manager"),
    glDefine("GD_ZMIN", "XXX_ZMIN", "ZMIN not needed -- always 0."),
    glDefine("GD_ZMAX", "XXX_ZMAX", "ZMAX not needed -- always 1."),
    glDefine("GD_BITS_NORM_SNG_RED", "GL_RED_BITS"),
    glDefine("GD_BITS_NORM_SNG_GREEN", "GL_GREEN_BITS"),
    glDefine("GD_BITS_NORM_SNG_BLUE", "GL_BLUE_BITS"),
    glDefine("GD_BITS_NORM_DBL_RED", "GL_RED_BITS"),
    glDefine("GD_BITS_NORM_DBL_GREEN", "GL_GREEN_BITS"),
    glDefine("GD_BITS_NORM_DBL_BLUE", "GL_BLUE_BITS"),
    glDefine("GD_BITS_NORM_SNG_CMODE", "GL_INDEX_BITS"),
    glDefine("GD_BITS_NORM_DBL_CMODE", "GL_INDEX_BITS"),
    glDefine("GD_BITS_NORM_SNG_MMAP", "GL_INDEX_BITS", "multi-map not supported"),
    glDefine("GD_BITS_NORM_DBL_MMAP", "GL_INDEX_BITS", "multi-map not supported"),
    glDefine("GD_BITS_NORM_ZBUFFER", "GL_DEPTH_BITS"),
    glDefine("GD_BITS_OVER_SNG_CMODE", "XXX_BITS_OVER_SNG_CMODE", "XXX what to do?"),
    glDefine("GD_BITS_UNDR_SNG_CMODE", "XXX_BITS_UNDR_SNG_CMODE", "XXX what to do?"),
    glDefine("GD_BITS_PUP_SNG_CMODE", "XXX_BITS_PUP_SNG_CMODE", "XXX what to do?"),
    glDefine("GD_BITS_NORM_SNG_ALPHA", "GL_ALPHA_BITS"),
    glDefine("GD_BITS_NORM_DBL_ALPHA", "GL_ALPHA_BITS"),
    glDefine("GD_BITS_CURSOR", "XXX_BITS_CURSOR", "cursor bits not supported"),
    glDefine("GD_OVERUNDER_SHARED", "XXX_OVERUNDER_SHARED", "overunder_shared: not supported"),
    glDefine("GD_BLEND", "GL_BLEND", "blending is ALWAYS supported.#This function returns whether it is enabled."),
    glDefine("GD_CIFRACT", "XXX_CIFRACT", "CIFRACT irrelevant"),
    glDefine("GD_CROSSHAIR_CINDEX", "XXX_CROSSHAIR_CINDEX", "cursors not supported -- see window manager"),
    glDefine("GD_DITHER", "GL_DITHER", "dither ALWAYS supported.#This function returns whether it is enabled."),
    glDefine("GD_LINESMOOTH_CMODE", "GL_LINE_SMOOTH", "smooth lines ALWAYS supported.#This function returns whether it is enabled."),
    glDefine("GD_LINESMOOTH_RGB", "GL_LINE_SMOOTH", "smooth lines ALWAYS supported.#This function returns whether it is enabled."),
    glDefine("GD_LOGICOP", "GL_LOGICOP", "logic ops ALWAYS supported.#This function returns whether it is enabled."),
    glDefine("GD_NSCRNS", "XXX_NSCRNS", "nscrns not supported -- see window manager"),
    glDefine("GD_NURBS_ORDER", "GL_MAX_EVAL_ORDER"),
    glDefine("GD_NBLINKS", "XXX_NBLINKS", "blink not supported -- see window manager"),
    glDefine("GD_NVERTEX_POLY", "XXX_NVERTEX_POLY", "There is NO limit to the number of vertices in a polygon."),
    glDefine("GD_PATSIZE_64", "XXX_PATSIZE_64", "all patterns are the same size"),
    glDefine("GD_PNTSMOOTH_CMODE", "GL_POINT_SMOOTH", "Smooth points ALWAYS supported.#This function returns whether it is enabled."),
    glDefine("GD_PNTSMOOTH_RGB", "GL_POINT_SMOOTH", "Smooth points ALWAYS supported.#This function returns whether it is enabled."),
    glDefine("GD_PUP_TO_OVERUNDER", "XXX_PUP_TO_OVERUNDER", "pup_to_overunder: not supported"),
    glDefine("GD_READSOURCE", "GL_READ_BUFFER", "read buffer ALWAYS supported.#This function returns whether it is enabled."),
    glDefine("GD_READSOURCE_ZBUFFER", "GL_READ_BUFFER", "read buffer ALWAYS supported.#This function returns whether it is enabled."),
    glDefine("GD_STEREO", "GL_STEREO"),
    glDefine("GD_SUBPIXEL_LINE", "XXX_SUBPIXEL_LINE", "subpixel ALWAYS supported"),
    glDefine("GD_SUBPIXEL_PNT", "GL_SUBPIXEL_PNT", "subpixel ALWAYS supported"),
    glDefine("GD_SUBPIXEL_POLY", "GL_SUBPIXEL_POLY", "subpixel ALWAYS supported"),
    glDefine("GD_TRIMCURVE_ORDER", "GL_MAX_EVAL_ORDER", "XXX see glu man pages"),
    glDefine("GD_WSYS", "XXX_WSYS",  "see window manager"),
    glDefine("GD_ZDRAW_GEOM", "XXX_ZDRAW_GEOM", "zdraw not supported"),
    glDefine("GD_ZDRAW_PIXELS", "XXX_ZDRAW_PIXELS", "zdraw not supported"),
    glDefine("GD_SCRNTYPE", "XXX_SCRNTYPE", "see window manager"),
    glDefine("GD_TEXTPORT", "XXX_TEXTPORT", "see window manager"),
    glDefine("GD_NMMAPS", "XXX_NMMAPS", "multimap not supported -- see window manager"),
    glDefine("GD_FRAMEGRABBER", "XXX_FRAMEGRABBER", "framegrabber not supported"),
    glDefine("GD_TIMERHZ", "XXX_TIMERHZ", "timers not supported -- use event functions."),
    glDefine("GD_DBBOX", "XXX_DBBOX", "devices not supported -- use event functions."),
    glDefine("GD_AFUNCTION", "GL_ALPHA_TEST", "alpha test ALWAYS supported.#This function returns whether is is enabled."),
    glDefine("GD_ALPHA_OVERUNDER", "XXX_ALPHA_OVERUNDER", "alpha_overunder: not supported"),
    glDefine("GD_BITS_ACBUF", "GL_ACCUM_RED_BITS", "see glAccum man page."),
    glDefine("GD_BITS_ACBUF_HW", "GL_ACCUM_RED_BITS", "see glAccum man page."),
    glDefine("GD_BITS_STENCIL", "GL_STENCIL_BITS"),
    glDefine("GD_CLIPPLANES", "GL_MAX_CLIP_PLANES"),
    glDefine("GD_FOGVERTEX", "GL_FOG", "Fog ALWAYS supported.#This function returns whether is is enabled."),
    glDefine("GD_LIGHTING_TWOSIDE", "XXX_LIGHTING_TWOSIDE", "Twosided lighting ALWAYS supported.#This function returns whether is is enabled."),
    glDefine("GD_POLYMODE", "GL_POLYGON_MODE", "polygon mode ALWAYS supported.#This function returns whether is is enabled."),
    glDefine("GD_POLYSMOOTH", "GL_POLYGON_SMOOTH", "Smooth polygons ALWAYS supported.#This function returns whether is is enabled."),
    glDefine("GD_SCRBOX", "XXX_SCRBOX", "scrbox not supported"),
    glDefine("GD_TEXTURE", "GL_TEXTURE_2D", "Texture ALWAYS supported.#This function returns whether is is enabled."),
    glDefine("GD_FOGPIXEL", "GL_FOG", "Fog ALWAYS supported.#This function returns whether is is enabled."),
    glDefine("GD_TEXTURE_PERSP", "GL_TEXTURE_2D", "Texture ALWAYS supported.#This function returns whether is is enabled."),
    glDefine("GD_MUXPIPES", "XXX_MUXPIPES", "muxpipes: not supported -- See Window Manager?"),
    glDefine("MSINGLE", "GL_MODELVIEW","MSINGLE not supported"),
    glDefine("MPROJECTION", "GL_PROJECTION"),
    glDefine("MVIEWING", "GL_MODELVIEW"),
    glDefine("MTEXTURE", "GL_TEXTURE"),
    glDefine("STR_B", "GL_UNSIGNED_BYTE"),
    glDefine("STR_2B", "GL_2_BYTES"),
    glDefine("STR_3B", "GL_3_BYTES"),
    glDefine("STR_4B", "GL_4_BYTES"),
    glDefine("STR_16", "GL_UNSIGNED_SHORT"),
    glDefine("STR_32", "GL_UNSIGNED_INT"),
    glDefine("LMC_COLOR", "XXX lmc_COLOR",  "LMC_COLOR: use glDisable(GL_COLOR_MATERIAL);"),
    glDefine("LMC_EMISSION", "GL_EMISSION"),
    glDefine("LMC_AMBIENT", "GL_AMBIENT"),
    glDefine("LMC_DIFFUSE", "GL_DIFFUSE"),
    glDefine("LMC_SPECULAR", "GL_SPECULAR"),
    glDefine("LMC_AD", "GL_AMBIENT_AND_DIFFUSE"),
    glDefine("LMC_NULL", "XXX_LMC_NULL",  "LMC_NULL: use glDisable(GL_COLOR_MATERIAL);"),
    glDefine("TX_MINFILTER", "GL_TEXTURE_MIN_FILTER"),
    glDefine("TX_MAGFILTER", "GL_TEXTURE_MAG_FILTER"),
    glDefine("TX_WRAP", "XXX_TX_WRAP", "TX_WRAP not supported: see glTexParameterfv man page."),
    glDefine("TX_WRAP_S", "GL_TEXTURE_WRAP_S"),
    glDefine("TX_WRAP_T", "GL_TEXTURE_WRAP_T"),
    glDefine("TX_TILE", "XXX_TX_TILE", "TX_TILE not supported: see glTexParamterfv man page."),
    glDefine("TX_BORDER", "XXX_TX_BORDER", "Border needs to be added to glTexImage2D parameters"),
    glDefine("TX_NULL", ""),
    glDefine("TV_NULL", ""),
    glDefine("TX_POINT", "GL_NEAREST"),
    glDefine("TX_BILINEAR", "GL_LINEAR"),
    glDefine("TX_MIPMAP", "GL_NEAREST_MIPMAP_LINEAR"),
    glDefine("TX_MIPMAP_POINT", "GL_NEAREST_MIPMAP_NEAREST"),
    glDefine("TX_MIPMAP_LINEAR", "GL_NEAREST_MIPMAP_LINEAR"),
    glDefine("TX_MIPMAP_BILINEAR", "GL_LINEAR_MIPMAP_NEAREST"),
    glDefine("TX_MIPMAP_TRILINEAR", "GL_LINEAR_MIPMAP_LINEAR"),
    glDefine("TV_MODULATE", "GL_MODULATE"),
    glDefine("TV_BLEND", "GL_BLEND", "XXX is there a problem with GL_BLEND?"),
    glDefine("TV_DECAL", "GL_DECAL"),
    glDefine("TV_COLOR", "GL_TEXTURE_ENV_COLOR",  "env_color: see glTexEnv man page"),
    glDefine("TX_S", "GL_S"),
    glDefine("TX_T", "GL_T"),
    glDefine("TX_REPEAT", "GL_REPEAT"),
    glDefine("TX_CLAMP", "GL_CLAMP"),
    glDefine("TX_SELECT", "XXX_SELECT", "TX_SELECT not supported"),
    glDefine("TG_OFF", "XXX_TG_OFF", "tg_off use: #glDisable(GL_TEXTURE_GEN_S);#glDisable(GL_TEXTURE_GEN_T);"),
    glDefine("TG_ON", "XXX_TG_ON", "tg_on use: #glEnable(GL_TEXTURE_GEN_S);#glEnable(GL_TEXTURE_GEN_T);"),
    glDefine("TG_CONTOUR", "GL_TG_CONTOUR"),
    glDefine("TG_LINEAR", "GL_EYE_LINEAR"),
    glDefine("TG_SPHEREMAP", "GL_SPHERE_MAP"),
    glDefine("TV_ENV0", "GL_TEXTURE_ENV"),
    glDefine("TX_TEXTURE_0", "GL_TEXTURE_2D"),
    glDefine("GC_BITS_CMODE", "GL_INDEX_BITS"),
    glDefine("GC_BITS_RED", "GL_RED_BITS"),
    glDefine("GC_BITS_GREEN", "GL_GREEN_BITS"),
    glDefine("GC_BITS_BLUE", "GL_BLUE_BITS"),
    glDefine("GC_BITS_ALPHA", "GL_ALPHA_BITS"),
    glDefine("GC_BITS_ZBUFFER", "GL_DEPTH_BITS"),
    glDefine("GC_ZMIN", "XXX_ZMIN", "getgconfig: ZMIN always 0"),
    glDefine("GC_ZMAX", "XXX_Z_MAX", "getgconfig: ZMAX always 1"),
    glDefine("GC_BITS_STENCIL", "GL_STENCIL_BITS"),
    glDefine("GC_BITS_ACBUF", "GL_ACCUM_RED_BITS", "BITS_ACBUF: add together all red, grn, blu, alp bits."),
    glDefine("GC_MS_SAMPLES", "XXX_MS_SAMPLES", "multisample not supported in base OpenGL"),
    glDefine("GC_BITS_MS_ZBUFFER", "XXX_MS_ZBUFFER", "multisample not supported in base OpenGL"),
    glDefine("GC_MS_ZMIN", "XXX_MS_ZMIN", "zmin always 0.#multisample not supported in base OpenGL"),
    glDefine("GC_MS_ZMAX", "XXX_MS_ZMAX", "zmax always 1.#multisample not supported in base OpenGL"),
    glDefine("GC_BITS_MS_STENCIL", "GL_STENCIL_BITS", "multisample not supported in base OpenGL"),
    glDefine("GC_STEREO", "GL_STEREO"),
    glDefine("GC_DOUBLE", "GL_DOUBLEBUFFER"),
    glDefine("Byte", "GLbyte"),
    glDefine("Boolean", "GLboolean"),
    glDefine("String", "GLbyte *"),
    glDefine("Lstring", "GLint *"),
    glDefine("Angle", "GLfloat"),
    glDefine("Screencoord", "GLuint"),
    glDefine("Scoord", "GLshort"),
    glDefine("Icoord", "GLint"),
    glDefine("Coord", "GLfloat"),
    glDefine("Matrix", "GLfloat *",  "XXX fix this."),
    glDefine("Colorindex", "GLshort"),
    glDefine("RGBvalue", "GLuint"),
    glDefine("Pattern16", "GLuint *", "XXX see glPolygonStipple man page"),
    glDefine("Pattern32", "GLuint *", "XXX see glPolygonStipple man page"),
    glDefine("Pattern64", "GLuint *", "XXX see glPolygonStipple man page"),
    glDefine("Linestyle", "GLuint *", "XXX see glLineStipple man page"),
    glDefine("Object", "GLuint"),
    

};
 
static int possible_hits[MAXPATTERN];
static int replacements[MAXPATTERN];

static void
print_hits()
{
    int i;
    cerr << "Possible hits & replacements made for " << lineno << " lines:\n";
    for(i = 0; i < MAXPATTERN; i++) {
	cerr << i << ":\t" << possible_hits[i] ;
	cerr << "\t" << replacements[i] << "\n";
    }
}
 
void
process_line()
{
    int i;
    PerlStringList s;
    glThing *p;
    
    ostr = instr;
    
    for(i = 0; i < MAXPATTERN; i++) {
	if(first_glThing[i] && search[i].check(ostr)) { // if we have a possible match here...
	    possible_hits[i]++;
	    for(p = first_glThing[i];p;p = p->next()) {
		int junk;   // junk not used -- avoids a compiler bug
		while(junk = p->m(ostr, s)) {
		    p->replace(ostr, s);
		    s.reset();
		    replacements[i]++;
		}
	    }
	}
    }
}

void
print_line()
{
    if(!comments.isempty()) {
	if(!no_comments) {
	    if(comments.scalar() == 1) {
		cout << "\t/* OGLXXX " << comments[0] << " */\n";
	    } else {
		cout << "\t/* OGLXXX\n\t * " << comments.join("\n\t * ") << "\n\t */\n";
	    }
	}
	comments.reset();
    }
    ostr = ostr.substr(1, ostr.length() - 2);	// remove first and last " " added by read_line
    cout << ostr << '\n';
    
    if(debug)
	cout.flush();
}

void
init_optional_functions()
{
    if(!no_window) {
	new     glDelete("wintitle", "wintitle not supported -- See Window Manager");
	new     glDelete("winset", "winset not supported -- See Window Manager");
	new     glDelete("winpush", "winpush not supported -- See Window Manager");
	new     glDelete("winposition", "winposition not supported -- See Window Manager");
	new     glDelete("winpop", "winpop not supported -- See Window Manager");
	new     glDelete("winopen", "winopen not supported -- See Window Manager");
	new     glDelete("winmove", "winmove not supported -- See Window Manager");
	new     glDelete("winget", "winget not supported -- See Window Manager");
	new     glDelete("windepth", "windepth not supported -- See Window Manager");
	new     glDelete("winconstraints", "winconstraints not supported -- See Window Manager");
	new     glDelete("winclose", "winclose not supported -- See Window Manager");
	new     glDelete("videocmd", "videocmd not supported");
	new     glDelete("tpon", "tpon not supported");
	new     glDelete("tpoff", "tpoff not supported");
	new     glDelete("textport", "textport not supported");
	new     glDelete("textinit", "textinit not supported");
	new     glDelete("textcolor", "textcolor not supported -- See Fonts");
	new     glDelete("swinopen", "swinopen not supported -- See Window Manager");
	new     glDelete("stepunit", "stepunit not supported -- See Window Manager");
	new     glDelete("setvideo", "setvideo not supported");
	new     glDelete("setmonitor", "setmonitor not supported");
	new     glDelete("setmap", "setmap not supported -- See Window Manager");
	new     glDelete("setdblights", "setdblights not supported -- See Window Manager");
	new     glDelete("setbell", "setbell not supported -- See Window Manager");
	new     glDelete("scrnselect", "scrnselect not supported -- See Window Manager");
	new     glDelete("scrnattach", "scrnattach not supported -- See Window Manager");
	new     glDelete("screenspace", "screenspace not supported");
	new     glDelete("ringbell", "ringbell not supported -- See Window Manager");
	new     glDelete("reshapeviewport", "reshapeviewport not supported -- See Window Manager");
	new     glDelete("prefposition", "prefposition not supported -- See Window Manager");
	new     glDelete("prefsize", "prefsize not supported -- See Window Manager");
	new     glDelete("onemap", "onemap not supported -- See Window Manager");
	new     glDelete("noborder", "noborder not supported -- See Window Manager");
	new     glDelete("multimap", "multimap not supported -- See Window Manager?");
	new     glDelete("mswapbuffers", "mswapbuffers not supported -- See Window Manager");
	new     glDelete("mapcolor", "mapcolor not supported -- See Window Manager");
	new     glDelete("imakebackground", "imakebackground not supported -- See Window Manager");
	new     glDelete("icontitle", "icontitle not supported -- See Window Manager");
	new     glDelete("iconsize", "iconsize not supported -- See Window Manager");
	new     glDelete("greset", "greset not supported -- See Window Manager");
	new     glDelete("ginit", "ginit not supported -- See Window Manager");
	new     glDelete("gexit", "gexit not supported -- See Window Manager#\tThere's bound to be something you have to do!");
	new     glDelete("getwscrn", "getwscrn not supported -- See Window Manager");
	new     glDelete("getvideo", "getvideo not supported");
	new     glDelete("getsize", "getsize not supported -- See Window Manager");
	new     glDelete("getorigin", "getorigin not supported -- See Window Manager");
	new     glDelete("getmcolor", "getmcolor not supported -- See Window Manager");
	new     glDelete("getmonitor", "getmonitor not supported");
	new     glDelete("getmap", "getmap not supported");
	new     glDelete("getbutton", "getbutton not supported -- See Window Manager");
	new     glDelete("gammaramp", "gammaramp not supported");
	new     glDelete("fullscrn", "fullscrn not supported");
	new     glDelete("fudge", "fudge not supported");
	new     glDelete("endfullscrn", "endfullscrn not supported");
	new     glDelete("dglopen", "dglopen not supported");
	new     glDelete("dglclose", "dglclose not supported");
	new     glDelete("dbtext", "dbtext not supported, see Window Manager");
	new     glDelete("defcursor", "defcursor not supported, see Window Manager");
	new     glDelete("cyclemap", "cyclemap not supported, see Window Manager?");
	new     glDelete("curstype", "curstype not supported");
	new     glDelete("curson", "curson not supported");
	new     glDelete("cursoff", "cursoff not supported");
	new     glDelete("curorigin", "curorigin not supported");
	new     glDelete("blink", "blink not supported, see Window Manager");
	new     glDelete("blanktime", "blanktime not supported, see Window Manager");
	new     glDelete("blankscreen", "blankscreen not supported, see Window Manager");
	new     glDelete("winattach", "winattach obsolete -- see Window Manager");
	new     glDelete("setcursor", "setcursor not supported -- see Window Manager");
	new     glDelete("RGBcursor", "RGBcursor obsolete -- see Window Manager");
	new     glDelete("getcursor", "getcursor not supported -- see Window Manager?");
	new     glSimple("swapbuffers", "glXSwapBuffers(*display, window)", "swapbuffers: replace display and window");
	new     glSimple("singlebuffer", "glxChooseVisual(*dpy, screen, *attriblist)", "singlebuffer: don't use GLX_DOUBLEBUFFER in attriblist");
	new     glSimple("doublebuffer", "glxChooseVisual(*dpy, screen, *attriblist)", "doublebuffer: use GLX_DOUBLEBUFFER in attriblist");
	new     glSimple("cmode", "glxChooseVisual(*dpy, screen, *attriblist)", "cmode: use GLX_BUFFER_SIZE, 1024 in attriblist");
	new     glSimple("RGBmode",  "glxChooseVisual(*dpy, screen, *attriblist)", "RGBmode: use GLX_RGBA in attriblist");
	new	glDelete("gbegin", "gbegin not supported -- See Window Manager");
	new	glDelete("getport", "getport obsolete -- see Window Manager");
	new	glDelete("keepaspect", "keepaspect not supported -- See Window Manager");
	new	glDelete("maxsize", "maxsize not supported -- See Window Manager");
	new	glDelete("minsize", "minsize not supported -- See Window Manager");
	new     glDelete("clkon", "clkon not supported");
	new     glDelete("clkoff", "clkoff not supported");
	new     glDelete("lampoff", "lampoff not supported -- See Window Manager");
	new     glDelete("lampon", "lampon not supported -- See Window Manager");
	new     glDelete("attachcursor", "attachcursor not supported -- See Window Manager");
    }
    if(!no_queue) {
	new     glDelete("blkqread", "blkqread not supported, see Events"); // XXX X equivalent?
	new     glDelete("getvaluator", "getvaluator not supported -- See Events");
	new     glDelete("isqueued", "isqueued not supported -- See Events");
	new     glDelete("noise", "noise not supported -- See Events");
	new     glDelete("qdevice", "qdevice not supported -- See Events");
	new     glDelete("qenter", "qenter not supported -- See Events");
	new     glDelete("qgetfd", "qgetfd not supported -- See Events");
	new     glDelete("qread", "qread not supported -- See Events");
	new     glDelete("qreset", "qreset not supported -- See Events");
	new     glDelete("qtest", "qtest not supported -- See Events");
	new     glDelete("setvaluator", "setvaluator not supported -- See Events");
	new     glDelete("tie", "tie not supported -- See Events");
	new     glDelete("unqdevice", "unqdevice not supported -- See Events");
	new     glDelete("unqdevice", "unqdevice not supported -- See Events");
	new	glDelete("getdev", "getdev not supported -- See Events");
	new	glDelete("qcontrol", "qcontrol not supported -- See Events");
    }
    if(!no_lighting) {
	new glDefine("MAXLIGHTS", "(glGetIntegerv(GL_MAX_LIGHTS, &tmp), tmp)", "maxlights:#GLint tmp;");
	new glDefine("MATERIAL", "GL_FRONT", "Use GL_FRONT in call to glMaterialf.");
	new glDefine("BACKMATERIAL", "GL_BACK", "Use GL_BACK in call to glMaterialf.");
	new glDefine("LIGHT7", "GL_LIGHT7");
	new glDefine("LIGHT6", "GL_LIGHT6");
	new glDefine("LIGHT5", "GL_LIGHT5");
	new glDefine("LIGHT4", "GL_LIGHT4");
	new glDefine("LIGHT3", "GL_LIGHT3");
	new glDefine("LIGHT2", "GL_LIGHT2");
	new glDefine("LIGHT1", "GL_LIGHT1");
	new glDefine("LIGHT0", "GL_LIGHT0");
	new glDefine("LMNULL", "");
	new glDefine("TWOSIDE", "GL_LIGHT_MODEL_TWO_SIDE", "light model parameters need to be moved into a glLightModelf call.");
	new glDefine("ATTENUATION2", "GL_QUADRATIC_ATTENUATION");
	new glDefine("ATTENUATION", "GL_CONSTANT_ATTENUATION", "attenuation: see glLightf man page:#Add GL_LINEAR_ATTENUATION.");
	new glDefine("LOCALVIEWER", "GL_LIGHT_MODEL_LOCAL_VIEWER", "light model parameters need to be moved into a glLightModelf call.");
	new glDefine("SPOTLIGHT", "XXX_SPOTLIGHT", "see glLightf man page#Add GL_SPOT_EXPONENT and GL_SPOT_CUTOFF parameters.");
	new glDefine("POSITION", "GL_POSITION");
	new glDefine("LCOLOR", "GL_DIFFUSE", "light color: need to add GL_AMBIENT and GL_SPECULAR components.#ALPHA needs to be included in parameters.");
	new glDefine("COLORINDEXES", "GL_COLOR_INDEXES");
	new glDefine("ALPHA", "XXX_ALPHA", "alpha is to be included with other parameters");
	new glDefine("SHININESS", "GL_SHININESS");
	new glDefine("SPECULAR", "GL_SPECULAR", "include ALPHA parameter with specular");
	new glDefine("DIFFUSE", "GL_DIFFUSE", "include ALPHA parameter with diffuse");
	new glDefine("AMBIENT", "GL_AMBIENT", "Ambient:#\tIf this is a light model lmdef, then use glLightModelf and GL_LIGHT_MODEL_AMBIENT.#Include ALPHA parameter with ambient");
	new glDefine("EMISSION", "GL_EMISSION", "include ALPHA parameter with emission");
	new glDefine("SPOTDIRECTION", "GL_SPOT_DIRECTION");
	new glArgs("lmbind", "if($2) {glCallList($2); glEnable($1);} else glDisable($1)",  "lmbind: check object numbering."), 
	new glArgs("lmdef", "glNewList($2, GL_COMPILE); glMaterialf(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, *$4); glEndList();", "lmdef other possibilities include:#\tglLightf(light, pname, *params);#\tglLightModelf(pname, param);#Check list numbering.#Translate params as needed.");
     }
    if(emulate_lighting) {
	new glArgs("lmbind", "mylmbind($1, $2)"), 
	new glArgs("lmdef", "mylmdef($1, $2, $3, $4)");
     }
}


int 
main(int argc, char **argv)
{
    options(argc, argv);
    init_optional_functions();
        
    while(read_line()) {
	process_line();
	print_line();
    }
    
    if(debug) 
	print_hits();
	
    return errors; 
}


/*
 * return offset to matching parenthesis or quote characters
 * returns 0 if none found.
 * paren or quote must be first char.
 */
int matching(const char *in, int offset)
{
    const char *s = &in[offset];
    int r;
    int c, m;
    
    if(*s == '(')
	m = ')';
    else if(*s == '"') 
	m = '"';
    else {
	return 0;
    }
    
    s++;
    
    while((c = *s)&& (c != m)) {
	if(c == '(' && m != '"') {  // don't look at stuff in quotes
	    if(!(r = matching(s)))
		return 0;
	    s += r + 1;    
	} else if(c == '"') {
	    if(!(r = matching(s)))
		return 0;
	    s += r + 1;
	} else
	    s += 1;
    }
    if(!c || c != m)
	return 0;
    else
	return s - &in[offset];
}

PerlStringList split_args( PerlString &in,  int &ok)
{
    PerlStringList results(10);
    ok = 1;
    
    int r = matching(in);
    if(r == 0) {		// no args possible
	error( "un-matched parenthesis or quote");
	ok = 0;
	results.reset();
	return results;
    }
	
    results.push("(");
    
    PerlString s = in.substr(1, r-1);	// get arg list without ()'s
    PerlString rest = in.substr(r+1); // rest of input after ')'
    
    int i = 0;
    int j = 0;
    int c;
    
    while(i < s.length()) {
	c = s[i];
	if(c == '(' || c == '"') {
	    r = matching(s, i);
	    if(!r) {
		error("un-matched paren or quote");
		ok = 0;
		results.reset();
		break;
	    }
	    i += r + 1;
	} else if(c == ',') {
	    results.push(s.substr(j, i-j));
	    i++;
	    j = i;
	} else
	    i++;
    }
    
    results.push(s.substr(j, i-j));
    results.push(")");
    results.push(rest);
    
    return results;
}

#if TEST_MATCH

main() {
    PerlString istr;
    do {
	cin >> istr;
	PerlStringList a = split_args (istr);
	
	cout << a.join("\n") << '\n';
    } while(!cin.eof());
}
#endif

// replace "$n" in input string with args[n] (1 based)
// $1 - $9, $a - $f, or $A - $F work
// no check is made for $<anything else> or $ at end of string!

void replace_args(PerlString &in, const PerlStringList &args)
{
    int j, n;    
    
    while((j = in.index("$")) >= 0) {
	n = in[j+1];
	if(n >= '1' && n <= '9') 
	    n -= '1';
	else if (n >= 'a' && n <= 'f') 
	    n = n - 'a' + 9;
	else if (n >= 'A' && n <= 'F') 
	    n = n - 'A' + 9;
	if(n >= args.scalar()) {
	    error("Not enough arguments for function or other wierdness");
	    return;
	}
	      
	in.substr(j, 2) = args[n];
    }
}
