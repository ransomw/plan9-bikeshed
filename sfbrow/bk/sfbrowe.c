#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>
/** simple file browser */
/* ===========================
 * |  <up> |    <path>       |
 * ---------------------------
 * | ^ |       ...           |
 * | | |                     |
 * | | |     <name>          |
 * |\/ |       ...           |
 * ---------------------------
 * |  <name filter>          |
 * |==========================
 */
// maximum number of runes in name filter
#define FILTER_MAX 1024
/** layout */
static const ulong UP_WIDTH = 30;
static const ulong SLIDER_WIDTH = 10;

/* todos
a graphic for the parent directory button

bug: plumbing filenames with spaces (quote)
/n/griddisk/cpl/Screenshot from 2019-03-14 00-18-58.png
plumb rule is
	data	matches	'[a-zA-Z¡-￿0-9_\-.,/]+'
	data	matches	'([a-zA-Z¡-￿0-9_\-.,/]+)\.(jpe?g|JPE?G|gif|GIF|tiff?|TIFF?|ppm|PPM|bit|BIT|png|PNG|pgm|PGM|bmp|BMP|yuv|YUV)'
any reason to /not/ include spaces?

store Selent names as both string and runes
display 'sfbrow!<path>' in rio
enter directory windows have meaningful names

grep todo clean
warning clean
documentation

ui libs: control(2), elementile(amavect)
*/

Mousectl *mctl;
Keyboardctl *kctl;

struct {
	Image *scrollhandle;
	Image *scrollbg;
} thiscolors;

/* intermediate channels */
// path component for io process
Channel *namec;
// results of path process
Channel *selc;
// request and result of layout based on application state
// todo: replace layout derefs with application state derefs
//	i.e. -- rm layoutreqc, layoutresc
Channel *layoutreqc;
Channel *layoutresc;
Channel *statereqc;
Channel *stateresc;
Channel *statesetc;
/* mouse event channels */
// scroll bar input
Channel *scrollc;
// index of the name clicked on
Channel *nameidxc;
/* action event channels */
// updates to the sort order
Channel *sortc;
// directory jumps for io process
Channel *dirc;
// ent scripts for io process
Channel *entscriptc;
// filter text
Channel *namefilterc;

/** channel vals */
static const ulong SCROLL_UP = 101;
static const ulong SCROLL_DOWN = 102;
static const ulong SCROLL_STOP = 103;
enum sorts {
	SORT_NAME,
	SORT_MTIME,
};

typedef struct layout Layout;
struct layout {
	Rectangle uprect;
	Rectangle pathrect;
	Rectangle scrollrect;
	Rectangle namesrect;
	Rectangle filterrect;
	ulong nameheight;
};

Layout
currlayout(void)
{
	Layout layout;
	Layout *layoutp;
	sendp(layoutreqc, nil);
	layoutp = recvp(layoutresc);
	layout = *layoutp;
	return layout;
}

typedef struct selent *Selent;
struct selent {
	uint isdir;
	char *name;
	uint mtime;
	struct selent *next;
};

typedef struct sel *Sel;
struct sel {
	char *path;
	char *dirpath;
	Selent ents;
};

void
terminate(char *err)
{
	closekeyboard(kctl);
	closemouse(mctl);
	closedisplay(display);
	threadexitsall(err);
}

static void
channelsize(Channel *c, int sz)
{
	if(c->e != sz)
		terminate(smprint(
			"expected channel with elements of size %d, got size %d",
			sz, c->e));
}

int
sendrune(Channel *c, Rune v)
{
	channelsize(c, sizeof(Rune));
	return send(c, &v);
}

Rune
recvrune(Channel *c)
{
	Rune v;

	channelsize(c, sizeof(Rune));
	if(recv(c, &v) < 0)
		return Runeerror;
	return v;
}

/* debugging */
void
printselent(Selent ent)
{
	print("--ent: isdir: %d, has next: %s, name: %s\n",
		ent->isdir, ent->next == nil ? "no" : "yes", ent->name);
}

void
printsel(Sel sel)
{
	Selent current;
	print("----sel\n");
	print("path: %s\n", sel->path);
	for(current = sel->ents; current != nil; current = current->next)
		printselent(current);
	print("----\n");
}

Sel
pathsel(char *path)
{
	Dir *d;
	Dir *dents;
	Dir *dent;
	Sel sel;
	Selent nextent;
	Selent current;
	int ndents, fd, i;
	char *pr;
	/* pathname manipulation via rune(2) as in basename(1) */
	pr = utfrune(path, '/');
	if(pr != path)
		terminate(strdup("pathsel() expected absolute path"));
	sel = (Sel) malloc(sizeof(*sel));
	sel->path = strdup(path);
	sel->ents = nil; // ??? necessary?
	d = dirstat(path);
	sel->dirpath = strdup(path);
	if(!(d->qid.type & QTDIR)) {
		/* path is a file, not a directory */
		pr = utfrrune(sel->dirpath, '/');
		*pr = '\0';
	}
	fd = open(sel->dirpath, OREAD);
	if(fd < 0)
		sysfatal("open: %r");
	current = nil;
	while((ndents = dirread(fd, &dents)) != 0) {
		for(i = 0; i < ndents; i++) {
			nextent = (Selent) malloc(sizeof(*nextent));
			nextent->name = strdup(dents[i].name);
			nextent->mtime = dents[i].mtime;
			nextent->next = nil;
			if(dents[i].qid.type & QTDIR)
				nextent->isdir = 1;
			else
				nextent->isdir = 0;
			if(sel->ents == nil)
				sel->ents = nextent;
			else if(current != nil)
				current->next = nextent;
			else
				terminate("programming error building sel list");
			current = nextent;
		}
		free(dents);
	}
	close(fd);
	return sel;
}

uint
lensel(Sel s)
{
	Selent ce;
	uint num = 0;
	for(ce = s->ents; ce != nil; ce = ce->next)
		num++;
	return num;
}

Sel
copysel(Sel sel)
{
	Sel copy;
	Selent current, currcopyent, nextcopyent;

	copy = (Sel) malloc(sizeof(*copy));
	copy->path = strdup(sel->path);
	copy->dirpath = strdup(sel->dirpath);
	current = sel->ents;
	currcopyent = nil;
	copy->ents = nil;
	while(current != nil) {
		nextcopyent = (Selent) malloc(sizeof(*nextcopyent));
		nextcopyent->name = strdup(current->name);
		nextcopyent->mtime = current->mtime;
		nextcopyent->isdir = current->isdir;
		nextcopyent->next = nil;
		if(copy->ents == nil)
			copy->ents = nextcopyent;
		else if(currcopyent != nil)
			currcopyent->next = nextcopyent;
		else
			terminate("programming error copying sel");
		currcopyent = nextcopyent;
		current = current->next;
	}
	return copy;
}

Selent
nthselent(Selent s, int i)
{
	Selent si;
	
	si = s;
	for(; i > 0; i--) {
		if(si == nil)
			terminate("nth index out of bounds");
		si = si->next;
	}
	if(si == nil)
		terminate("nth index out of bounds");
	return si;
}

Selent
swapselents(Selent sel, int i, int j)
{
	Selent lli, llj;
	Selent lliprev, lljprev;
	Selent llinext, lljnext;

	if(i > j)
		terminate("swap indices in wrong order");
	if(i == j)
		return sel;

	if(i == 0)
		lliprev = nil;
	else
		lliprev = nthselent(sel, i-1);
	lljprev = nthselent(sel, j-1);
	lli = nthselent(sel, i);
	llj = nthselent(sel, j);
	llinext = lli->next;
	lljnext = llj->next;

	/* blue-gold (cycle free at all times) */
	if(lliprev != nil)
		lliprev->next = llj;
	else
		sel = llj;
	lli->next = lljnext;
	if(i + 1 == j) {
		llj->next = lli;
		return sel;
	}
	lljprev->next = lli;
	llj->next = llinext;
	return sel;
}

Selent
selentsort(Selent ll, int left, int right,
	int (*comp)(void *, void*))
{
	int i, last;

	if(left >= right)
		return ll;
	ll = swapselents(ll, left, (left + right)/2);
	last = left;
	for(i = left + 1; i <= right; i++)
		if((*comp)(nthselent(ll, i), nthselent(ll, left)) < 0)
			ll = swapselents(ll, ++last, i);
	ll = swapselents(ll, left, last);
	/* now everything left of last is < left
	 * and everything on the other side is >= left
	 */
	ll = selentsort(ll, left, last - 1, comp);
	ll = selentsort(ll, last + 1, right, comp);
	return ll;
}

Selent
sortselents(Selent ents, int (*comp)(void *, void*))
{
	Selent ce;
	uint len = 0;

	for(ce = ents; ce != nil; ce = ce->next)
		len++;
	return selentsort(ents, 0, len-1, comp);
}

int
compselentnames(Selent lla, Selent llb)
{
	// todo: runes?
	return strcmp(lla->name, llb->name);
}

int
compselentmtimes(Selent lla, Selent llb)
{
	return lla->mtime - llb->mtime;
}

Selent
freeselentandadvance(Selent ent)
{
	Selent nextent;

	nextent = ent->next;
	free(ent->name);
	free(ent);
	return nextent;
}

int
filterselentsprefixpred(Selent ent, Rune *rs)
{
	Rune *name, *namecurr;

	namecurr = name = runesmprint("%s", ent->name);
	for(; *namecurr != '\0' && *rs != '\0'; namecurr++, rs++) {
		if(*namecurr != *rs) {
			free(name);
			return 0;
		}
	}
	free(name);
	if(*rs == '\0')
		return 1;
	return 0;
}

Selent
filterselentsprefix(Selent ents, Rune *rs)
{
	Selent fents, current, currfents;

	fents = nil;
	current = ents;
	while(fents == nil && current != nil) {
		if(filterselentsprefixpred(current, rs)) {
			fents = current;
			current = current->next;
			fents->next = nil;
		} else
			current = freeselentandadvance(current);
	}
	if(fents == nil)
		return fents;
	currfents = fents;
	while(current != nil) {
		if(filterselentsprefixpred(current, rs)) {
			currfents->next = current;
			currfents = currfents->next;
			current = current->next;
			currfents->next = nil;
		} else
			current = freeselentandadvance(current);
	}
	return fents;
}

void
freesel(Sel s)
{
	Selent current;
	free(s->path);
	free(s->dirpath);
	current = s->ents;
	for(; current != nil;
		current = freeselentandadvance(current))
		;
	free(s);
}

void
resizethread(void *)
{
	for(;;) {
		recvul(mctl->resizec);
		if(getwindow(display, Refnone) < 0)
			sysfatal("getwindow: %r");
		sendp(selc, nil);
	}
}

// 0xa is return. ??? not in keyboard.h
static const Rune KSret = 0xa;

void
keyboardthread(void *)
{
	Rune r;
	for(;;) {
		recv(kctl->c, &r);
		switch(r) {
		case Kdel:
		case Kesc:
			terminate(nil);
			continue;
		case Kup:
			sendul(scrollc, SCROLL_UP);
			continue;
		case Kdown:
			sendul(scrollc, SCROLL_DOWN);
			continue;
		}
		/* not in private Unicode space */
		if(r < KF &&
			/* send visible chars, backspace, return to state thread */
			(r >= ' ' || r == Kbs || r == KSret)) {
			sendrune(namefilterc, r); // todo: handle errors
			continue;
		}
		// private unicode characters and some ascii still available
	}
}

int
isinrect(Rectangle r, Point p)
{
	return r.min.x < p.x && r.min.y < p.y &&
		r.max.x > p.x && r.max.y > p.y;
}

/* ??? thread sleep? */
void
smoothscrolltickthread(void *arg)
{
	Channel *tickc = arg;
	static const long sleepms = 100;
	for(;;) {
		sleep(sleepms);
		sendp(tickc, nil);
	}
}

void
smoothscrollthread(void *arg)
{
	Channel *scrollstatec = arg;
	Channel *tickc = chancreate(sizeof(char *), 0);
	ulong scrollstate;
	char *tick;
	static const char delayticksafterclick = 3;
	char delayticks;
	enum { TICK, SCROLL, NONE };
	Alt alts[] = {
		[TICK] = {tickc, &tick, CHANRCV},
		[SCROLL] = {scrollstatec, &scrollstate, CHANRCV},
		[NONE] = {nil, nil, CHANEND},
	};

	proccreate(smoothscrolltickthread, tickc, 8*1024);
	scrollstate = SCROLL_STOP;
	delayticks = delayticksafterclick;
	for(;;) {
		switch(alt(alts)) {
		case TICK:
			if(delayticks != 0) {
				delayticks--;
				continue;
			}
			if(scrollstate != SCROLL_STOP)
				sendul(scrollc, scrollstate);
			break;
		case SCROLL:
			delayticks = delayticksafterclick;
			break;
		}
	}
}

typedef struct sortmenuitem Sortmenuitem;
struct sortmenuitem {
	char *text;
	ulong action;
	int (*compfn)(void *, void *) ;
};

static const Sortmenuitem sortmenuitems[] = {
	{"name", SORT_NAME,
		(int (*)(void *, void *)) compselentnames},
	{"modified", SORT_MTIME,
		(int (*)(void *, void *)) compselentmtimes},
};

/* sortaction2compfn: map actions to
 * [pointers to] a comparision functions.
 * used to avoid channeling function pointers,
 * although removing the actions enum might not be a bad idea.
 */
int (*
sortaction2compfn(ulong action)
)(void *, void *)
{
	int i;

	for(i = 0; i < nelem(sortmenuitems); i++)
		if(sortmenuitems[i].action == action)
			return sortmenuitems[i].compfn;
	terminate("unknown sort action");
	return nil;
}

char *
sortmenugen(int i)
{
	if(i < nelem(sortmenuitems))
		return sortmenuitems[i].text;
	return 0;
}

Menu sortmenu = {
	nil,
	sortmenugen,
	-1,
};

typedef struct dirmenuitem Dirmenuitem;
struct dirmenuitem {
	char *name;
	char *dir;
};

static Dirmenuitem *dirmenuitems;

int
isblankdirmenuitem(Dirmenuitem dirmenuitem) {
	return dirmenuitem.name == nil && dirmenuitem.dir == nil;
}

char *
dirmenugen(int i)
{
	Dirmenuitem *dirmenuitem;

	for(dirmenuitem = dirmenuitems;
		!isblankdirmenuitem(*dirmenuitem) && i > 0;
		dirmenuitem++, i--)
		;
	if(dirmenuitem != nil)
		return dirmenuitem->name;
	return 0;
}

Menu dirmenu = {
	nil,
	dirmenugen,
	-1,
};

typedef struct entscriptmenuitem Entscriptmenuitem;
struct entscriptmenuitem {
	char *name;
	char *script;
};

static Entscriptmenuitem *entscriptmenuitems;

int
isblankentscriptmenuitem(Entscriptmenuitem entscriptmenuitem) {
	return entscriptmenuitem.name == nil && entscriptmenuitem.script == nil;
}

char *
entscriptmenugen(int i)
{
	Entscriptmenuitem *entscriptmenuitem;

	for(entscriptmenuitem = entscriptmenuitems;
		!isblankentscriptmenuitem(*entscriptmenuitem) && i > 0;
		entscriptmenuitem++, i--)
		;
	if(entscriptmenuitem != nil)
		return entscriptmenuitem->name;
	return 0;
}

Menu entscriptmenu = {
	nil,
	entscriptmenugen,
	-1,
};

void
handlesortmenures(int i)
{
	if(i >= nelem(sortmenuitems))
		terminate("invalid sel action index");
	if(i != -1) // click inside box
		sendul(sortc, sortmenuitems[i].action);
}

void
handledirmenures(int i)
{
	Dirmenuitem *dirmenuitem;

	if(i == -1)
		return;
	if(dirmenuitems == nil)
		return;
	for(dirmenuitem = dirmenuitems;
		!isblankdirmenuitem(*dirmenuitem) && i > 0;
		dirmenuitem++, i--)
		;
	if(dirmenuitem == nil)
		terminate("invalid dir menu index");
	sendp(dirc, strdup(dirmenuitem->dir));
}

void
entscriptmenures(int i)
{
	Entscriptmenuitem *entscriptmenuitem;

	if(i == -1)
		return;
	if(entscriptmenuitems == nil)
		return;
	for(entscriptmenuitem = entscriptmenuitems;
		!isblankentscriptmenuitem(*entscriptmenuitem) && i > 0;
		entscriptmenuitem++, i--)
		;
	if(entscriptmenuitem == nil)
		terminate("invalid ent script menu index");
	sendp(entscriptc, strdup(entscriptmenuitem->script));
}

ulong
calculatescrollperc(Point pos, Layout layout)
{
	uint scrollperc;

	scrollperc = (pos.y - layout.scrollrect.min.y) * 100
					/ Dy(layout.scrollrect);
	if(pos.y < layout.scrollrect.min.y)
		scrollperc = 0;
	if(scrollperc > 100)
		scrollperc = 100;
	return scrollperc;
}

void
mousethread(void *)
{
	Mouse m;
	Point pos;
	Layout layout;
	uint nameidx;
	uint scrollperc;
	Channel *scrollstatec;
	char scrolling;

	scrollstatec = chancreate(sizeof(ulong), 0);
	threadcreate(smoothscrollthread, scrollstatec, 8*1024);
	scrolling = 0;
	for(;;) {
		recv(mctl->c, &m);
		if(m.buttons == 0) {
			scrolling = 0;
			sendul(scrollstatec, SCROLL_STOP);
			continue;
		}
		pos = m.xy;
		layout = currlayout();
		if(scrolling == 1) {
			if(m.buttons == 2)
				sendul(scrollc, calculatescrollperc(pos, layout));
			continue;
		}
		if(m.buttons == 2 && !isinrect(layout.scrollrect, pos) &&
			!isblankentscriptmenuitem(*entscriptmenuitems)) {
			entscriptmenures(
				menuhit(2, mctl, &entscriptmenu, nil)
			);
			continue;
		}
		if(m.buttons == 4) {
			if(isinrect(layout.pathrect, pos) &&
				!isblankdirmenuitem(*dirmenuitems))
				handledirmenures(
					menuhit(3, mctl, &dirmenu, nil)
				);
			else if(isinrect(layout.namesrect, pos))
				handlesortmenures(
					menuhit(3, mctl, &sortmenu, nil)
				);
			if(!isinrect(layout.scrollrect, pos))
				continue;
		}
		if(isinrect(layout.uprect, pos)) {
			if(m.buttons != 1) { continue; }
			sendp(namec, strdup(".."));
		} else if(isinrect(layout.pathrect, pos)) {
			if(m.buttons != 1) { continue; }
			// todo: menu a-la page(1)
		} else if(isinrect(layout.scrollrect, pos)) {
			if(m.buttons == 1) {
				sendul(scrollc, SCROLL_UP);
				sendul(scrollstatec, SCROLL_UP);
				scrolling = 1;
				continue;
			}
			if(m.buttons == 4) {
				sendul(scrollc, SCROLL_DOWN);
				sendul(scrollstatec, SCROLL_DOWN);
				scrolling = 1;
				continue;
			}
			if(m.buttons != 2) { continue; }
			sendul(scrollc, calculatescrollperc(pos, layout));
			scrolling = 1;
		} else if(isinrect(layout.namesrect, pos)) {
			if(m.buttons != 1) { continue; }
			nameidx = (pos.y - layout.namesrect.min.y) / layout.nameheight;
			sendul(nameidxc, nameidx);
		}
	}
}

Selent
firstdisplayedselent(Selent ents, uint scrollidx)
{
	for(;ents != nil && scrollidx > 0; scrollidx -= 1)
		ents = ents->next;
	return ents;
}

Rune *ellipses = L"...";

Rune *
buildtruncatedname(
	Rune *namerunes,
	Rune *runes,
	Rune *runesend,
	long nr)
{
	Rune r;

	r = runes[nr];
	runes[nr] = '\0';
	runestrcpy(namerunes, runes);
	runes[nr] = r;
	runestrcat(namerunes, ellipses);
	runestrcat(namerunes, runesend - nr);
	return namerunes;
}

Rune *
truncatename(char *namestr, int maxwidth)
{
	Rune *runes, *runesend; // namestr converted to runes
	Rune *namerunes; // truncated name runes dest
	Point sz = {0, 0};
	long nr;

	runes = runesmprint("%s", namestr);
	if(runestrlen(runes) < 2)
		return runes;
	runesend = runes;
	for(; *runesend != '\0'; runesend++)
		;
	namerunes = (Rune *) malloc(sizeof(*runes) * (
		runestrlen(ellipses) + (runesend - runes) + 1
	));
	for(nr = 0;
		sz.x < maxwidth && nr < (runesend - runes) / 2 + 1;
		) {
		nr++;
		buildtruncatedname(namerunes, runes, runesend, nr);
		sz = runestringsize(font, namerunes);
	}
	nr--;
	buildtruncatedname(namerunes, runes, runesend, nr);
	free(runes);
	return namerunes;
}

void
drawname(Rectangle namerect, char *namestr)
{
	Point sz, pos;
	Rune *namerunes;

	sz = stringsize(font, namestr);
	pos = namerect.min;
	// todo: error on misfit height
	if(sz.x < Dx(namerect)) {
		string(screen, pos, display->black, ZP, font, namestr);
		return;
	}
	namerunes = truncatename(namestr, Dx(namerect));
	runestring(screen, pos, display->black, ZP, font, namerunes);
	free(namerunes);
}

void
drawnames(Layout layout, Selent ents, uint scrollidx)
{
	Rectangle namerect = layout.namesrect;
	char *namestr;
	namerect.max.y = namerect.min.y + layout.nameheight;
	ents = firstdisplayedselent(ents, scrollidx);
	draw(screen, layout.namesrect, display->white, nil, ZP);
	for(;;) {
		if(ents == nil || namerect.max.y > layout.namesrect.max.y)
			return;
		if(ents->isdir)
			namestr = smprint("%s/", ents->name);
		else
			namestr = strdup(ents->name);
		drawname(namerect, namestr);
		free(namestr);
		namerect.min.y += layout.nameheight;
		namerect.max.y += layout.nameheight;
		ents = ents->next;
	}
}

void
drawscroll(Layout layout, uint nents, uint scrollidx)
{
	uint totnameheight = nents * layout.nameheight;
	uint totbarheight = Dy(layout.scrollrect);
	uint percheight;
	uint percscroll;
	if(totnameheight > 0)
		percheight = Dy(layout.namesrect) * 100 / totnameheight;
	else
		percheight = 100;
	if(nents > 0)
		percscroll = scrollidx * 100 / nents;
	else
		percscroll = 0;
	Rectangle beforehandlerect = layout.scrollrect;
	Rectangle handlerect = layout.scrollrect;
	Rectangle afterhandlerect = layout.scrollrect;
	handlerect.min.y += totbarheight * percscroll / 100;
	handlerect.max.y = handlerect.min.y + totbarheight * percheight / 100;
	beforehandlerect.max.y = handlerect.min.y;
	afterhandlerect.min.y = handlerect.max.y;
	draw(screen, beforehandlerect, thiscolors.scrollbg, nil, ZP);
	draw(screen, handlerect, thiscolors.scrollhandle, nil, ZP);
	draw(screen, afterhandlerect, thiscolors.scrollbg, nil, ZP);
}

void
drawnamefilter(Layout layout, Rune *filterrs)
{
	Point pos;

	draw(screen, layout.filterrect, display->white, nil, ZP);
	// todo: left-truncate the one-line display
	pos = layout.filterrect.min;
	runestring(screen, pos, display->black, ZP, font, filterrs);
}

void
drawnamepanel(Layout layout, Sel sel, uint scrollidx, Rune *filterrs)
{
	drawnames(layout, sel->ents, scrollidx);
	drawscroll(layout, lensel(sel), scrollidx);
	if(filterrs != '\0')
		drawnamefilter(layout, filterrs);
	flushimage(display, 1);
}

char *
mstrsrcecpy(char *s2, char *s2e)
{
	char c;
	char *s1;

	s1 = malloc(s2e - s2 + 1);
	c = *s2e;
	*s2e = '\0';
	strcpy(s1, s2);
	*s2e = c;
	return s1;
}

// could use a nil-terminated list...
typedef struct wrappedtext Wrappedtext;
struct wrappedtext {
	char **lines;
	uint nlines;
};

Wrappedtext
wraptext(char *text, uint maxwidth, Font *font)
{
	Wrappedtext wt;

	// protect against programming bugs, mstrsrcecpy in particular
	char *textcp = strdup(text);

	Rune r;
	char *rp; // current rune
	char *rprev; // previous rune
	int w; // chars per current rune
	int lidx; // line index
	int left; // remaining space on current line

	wt.lines = (char **) malloc(sizeof(wt.lines[0]));
	lidx = 0;
	left = maxwidth;
	rprev = rp = textcp;
	for(;;) {
		left -= stringnwidth(font, (char*)rp, 1);
		if(left < 0 || *rp == '\0') {
			wt.lines = (char **) realloc(wt.lines,
				sizeof(wt.lines[0]) * (lidx + 1));
			wt.lines[lidx] = mstrsrcecpy(rprev, rp);
			lidx++;
			left = maxwidth;
			rprev = rp;
			if(*rp == '\0')
				break;
		}
		r = *rp;
		if(r < Runeself)
			w = 1;
		else
			w = chartorune(&r, (char*)rp);
		rp += w;
	}

	wt.nlines = lidx;
	free(textcp);
	return wt;
}

void
drawpath(Layout layout, char *pathstr)
{
	int i;
	Rectangle pathrect;
	Point sz, pos;
	Wrappedtext wrappedtext;

	wrappedtext = wraptext(pathstr, Dx(layout.pathrect), font);

	draw(screen, layout.pathrect, display->white, nil, ZP);
	pos = layout.pathrect.min;
	for(i = 0; i < wrappedtext.nlines; i++) {
		sz = stringsize(font, wrappedtext.lines[i]);
		if(layout.pathrect.max.y < pos.y + sz.y)
			break;
		string(screen,
			pos,
			display->black, ZP, font,
			wrappedtext.lines[i]);
		pos.y += sz.y;
	}

	for(i = 0; i < wrappedtext.nlines; i++) {
		free(wrappedtext.lines[i]);
	}
	free(wrappedtext.lines);
}

void
drawupbutton(Layout layout)
{
	// todo: program an Aliens-like graphic
	draw(screen, layout.uprect, display->black, nil, ZP);
}

void
drawtoppanel(Layout layout, char *pathstr)
{
	drawpath(layout, pathstr);
	drawupbutton(layout);
	flushimage(display, 1);
}

void
drawall(Layout layout, Sel sel, uint scrollidx, Rune *filterrs)
{
	drawtoppanel(layout, sel->path);
	drawnamepanel(layout, sel, scrollidx, filterrs);
}

typedef struct entscriptparams Entscriptarg;
struct entscriptparams {
	char *text;
	char *script;
	Channel *pidc;
};

void
entscriptthread(void *arg)
{
	Entscriptarg *esarg = arg;
	procexecl(esarg->pidc,
		esarg->script, "entscript", esarg->text,
		nil);
}

void
runentscript(char *text, char *script)
{
	Entscriptarg entscriptparams;

	entscriptparams.pidc = chancreate(sizeof(ulong), 0);
	entscriptparams.text = text;
	entscriptparams.script = script;
	proccreate(entscriptthread, &entscriptparams, 8*1024);
	recvul(entscriptparams.pidc);
	chanclose(entscriptparams.pidc);
}

Layout
computelayout(Sel sel, Rune *filterrs)
{
	static const ulong NAV_HEIGHT = 30;
	static const ulong NAME_HEIGHT = 15;
	Layout layout;
	Rectangle uprect, pathrect, scrollrect, namesrect, filterrect;
	ulong nav_height;
	Wrappedtext wrappedpathtext;
	Point sz;
	int i;
	Selent current;

	/* navigation bar rectangles (x-axis) */
	uprect = screen->r;
	uprect.max.x = uprect.min.x + UP_WIDTH;
	pathrect = screen->r;
	pathrect.min.x = uprect.max.x;
	/* compute nav height based on current path size */
	nav_height = 0;
	if(sel != nil) {
		wrappedpathtext = wraptext(sel->path, Dx(pathrect), font);
		for(i = 0; i < wrappedpathtext.nlines; i++) {
			sz = stringsize(font, wrappedpathtext.lines[i]);
			nav_height += sz.y;
		}
	}
	if(nav_height < NAV_HEIGHT)
		nav_height = NAV_HEIGHT;
	/* compute name height as maximum of heights of current names */
	layout.nameheight = 0;
	if(sel != nil) {
		current = sel->ents;
		while(current != nil) {
			sz = stringsize(font, current->name);
			if(layout.nameheight < sz.y)
				layout.nameheight = sz.y;
			current = current->next;
		}
	}
	if(layout.nameheight < NAME_HEIGHT)
		layout.nameheight = NAME_HEIGHT;
	/* navigation bar rectangles (y-axis) */
	uprect.max.y = uprect.min.y + nav_height;
	pathrect.max.y = pathrect.min.y + nav_height;
	/* application view rectangles */
	scrollrect = screen->r;
	scrollrect.min.y += nav_height;
	scrollrect.max.x = scrollrect.min.x + SLIDER_WIDTH;
	namesrect = screen->r;
	namesrect.min.x = scrollrect.max.x;
	namesrect.min.y = scrollrect.min.y;
	/* name filter rectangle */
	filterrect = screen->r;
	sz = runestringsize(font, filterrs);
	if(*filterrs == '\0')
		filterrect.min.y = filterrect.max.y;
	else
		filterrect.min.y = filterrect.max.y - sz.y;
	scrollrect.max.y -= Dy(filterrect);
	namesrect.max.y -= Dy(filterrect);
	// todo: cope with screens smaller than minimum layout (nav + 1 name)
	/* set layout and return */
	layout.uprect = uprect;
	layout.pathrect = pathrect;
	layout.scrollrect = scrollrect;
	layout.namesrect = namesrect;
	layout.filterrect = filterrect;
	return layout;
}

void
updatefilterrs(Rune *rs, Rune r)
{
	ulong len;
	for(len = 0;
		len < FILTER_MAX && *rs != '\0';
		rs++, len++)
		;
	if(r == KSret)
		return;
	if(r == Kbs) {
		if(len > 0)
			rs[-1] = '\0';
		return;
	}
	if(len == FILTER_MAX)
		return;
	*rs = r;
	rs[1] = '\0';
}

Sel
makedispsel(Sel currsel,
	int (*sortcompfn)(void *, void *),
	Rune *filterrs)
{
	Sel dispsel;

	dispsel = copysel(currsel);
	dispsel->ents = sortselents(dispsel->ents, sortcompfn);
	if(*filterrs != '\0')
		dispsel->ents = filterselentsprefix(dispsel->ents, filterrs);
	return dispsel;
}

typedef struct {
	uint scrollidx;
	Sel currsel;
	char makingsel;
	ulong sort;
	Layout layout;
	Rune filterrs[FILTER_MAX+1];
} AppState;

void
atomthread_state(void *)
{
	AppState s;
	char *statereq;
	enum { DEREF, RESET, NONE };
	Alt alts[] = {
		[DEREF] = {statereqc, &statereq, CHANRCV},
		[RESET] = {statesetc, &s, CHANRCV},
		[NONE] = {nil, nil, CHANEND},
	};
	for(;;) {
		switch(alt(alts)) {
		case DEREF:
			channelsize(stateresc, sizeof(s));
			send(stateresc, &s); // todo: handle errors
			break;
		case RESET:
			break;
		}
	}
}

AppState
deref_state(void)
{
	AppState s;

	sendp(statereqc, nil);
	channelsize(stateresc, sizeof(s));
	if(recv(stateresc, &s) < 0) {
		terminate("app state deref recv error");
	}
	s.layout = computelayout(s.currsel, s.filterrs);
	return s;
}

/* todo: following the Clojure ideal, swap_, a function
 * that changes state based on it's current value is less
 * likely to write conflicts into the atom
 */
void
reset_state(AppState s)
{
	channelsize(statesetc, sizeof(AppState));
	// todo: handle errors
	send(statesetc, &s);
}

// todo: separate atomic changes and derefs from application logic
void
statethread(void *)
{
	Layout layout;
	Selent nameent;
	/* application state */
	AppState appState;
	uint scrollidx;
	Sel currsel;
	char makingsel;
	int (*sortcompfn)(void *, void *) = sortaction2compfn(SORT_NAME);
	Rune filterrs[FILTER_MAX+1];
	/* incoming updates */
	uint scrollupdate, nameidx;
	Sel newsel;
	char *layoutreq;
	ulong newsort;
	Rune filterr;
	char *entscript;
	/* intermediates */
	Sel dispsel;
	/* Alt stuff */
	int altidx;
	enum { SCROLL, NAME_IDX, SEL, LAYOUT,
		SORT, ENTSCRIPT, FILTER, NONE };
	Alt alts[] = {
		[SCROLL] = {scrollc, &scrollupdate, CHANRCV},
		[NAME_IDX] = {nameidxc, &nameidx, CHANRCV},
		[SEL] = {selc, &newsel, CHANRCV},
		[LAYOUT] = {layoutreqc, &layoutreq, CHANRCV},
		[SORT] = {sortc, &newsort, CHANRCV},
		[ENTSCRIPT] = {entscriptc, &entscript, CHANRCV},
		[FILTER] = {namefilterc, &filterr, CHANRCV},
		[NONE] = {nil, nil, CHANEND},
	};
	// todo: display makingsel status in ui (up color change)
	makingsel = 0;
	scrollidx = 0;
	filterrs[0] = '\0';
	currsel = nil;

	appState.scrollidx = scrollidx;
	appState.currsel = currsel;
	appState.makingsel = makingsel;
	appState.sort = SORT_NAME;
	appState.filterrs[0] = '\0';

	for(;;) {
		reset_state(appState);
		altidx = alt(alts);
		appState = deref_state();
		layout = computelayout(currsel, filterrs);
		switch(altidx) {
		case SCROLL:
			if(currsel == nil)
				continue;
			dispsel = makedispsel(currsel, sortcompfn, filterrs);
			if(scrollupdate == SCROLL_UP) {
				if(scrollidx > 0)
					scrollidx--;
			} else if(scrollupdate == SCROLL_DOWN) {
				if(scrollidx < lensel(dispsel) - 1)
					scrollidx++;
			} else if(0 <= scrollupdate && scrollupdate <= 100) {
				scrollidx = scrollupdate * lensel(dispsel) / 100;
			}
			drawnamepanel(layout, dispsel, scrollidx, filterrs);
			freesel(dispsel);
			break;
		case NAME_IDX:
			if(makingsel)
				break;
			if(currsel == nil)
				continue;
			dispsel = makedispsel(currsel, sortcompfn, filterrs);
			nameent = firstdisplayedselent(dispsel->ents, scrollidx);
			while(nameidx > 0 && nameent != nil) {
				nameidx--;
				nameent = nameent->next;
			}
			if(nameent != nil) {
				makingsel = 1;
				sendp(namec, strdup(nameent->name));
			}
			freesel(dispsel);
			break;
		case SEL:
			if(newsel != nil) { /* not a resize */
				// todo: error if not makingsel
				if(currsel != nil) {
					if(strcmp(currsel->dirpath, newsel->dirpath) != 0)
						scrollidx = 0;
					freesel(currsel);
				} else
					scrollidx = 0;
				currsel = newsel;
				makingsel = 0;
				*filterrs = '\0';
			}
			dispsel = makedispsel(currsel, sortcompfn, filterrs);
			layout = computelayout(dispsel, filterrs);
			drawall(layout, dispsel, scrollidx, filterrs);
			freesel(dispsel);
			break;
		case SORT:
			if(makingsel)
				break;
			if(currsel == nil)
				continue;
			sortcompfn = sortaction2compfn(newsort);
			scrollidx = 0;
			dispsel = makedispsel(currsel, sortcompfn, filterrs);
			// sorting the entries list doesn't change the layout
			drawnamepanel(layout, dispsel, scrollidx, filterrs);
			freesel(dispsel);
			break;
		case ENTSCRIPT:
			if(makingsel)
				break;
			runentscript(currsel->path, entscript);
			break;
		case FILTER:
			updatefilterrs(filterrs, filterr);
			dispsel = makedispsel(currsel, sortcompfn, filterrs);
			layout = computelayout(dispsel, filterrs);
			drawnamepanel(layout, dispsel, scrollidx, filterrs);
			freesel(dispsel);
			break;
		case LAYOUT:
			sendp(layoutresc, &layout);
			break;
		}
	}
}

char *
abspath(char *path)
{
	int n, fd;
	char abspath[512];

	fd = open(path, OREAD);
	if(fd < 0)
		terminate(smprint(
			"error opening abspath file '%s'",
			path));
	n = fd2path(fd, abspath, sizeof(abspath));
	close(fd);
	if(n < 0)
		terminate(smprint(
			"error reading abspath file '%s'",
			path));
	return strdup(abspath);
}

/** run as a separate process, this thread does the io */
void
selthread(void *arg)
{
	Sel sel;
	char *name, *dir;
	char *pathcurr;
	char *pr;

	enum { NAME, DIR, NONE };
	Alt alts[] = {
		[NAME] = {namec, &name, CHANRCV},
		[DIR] = {dirc, &dir, CHANRCV},
		[NONE] = {nil, nil, CHANEND},
	};

	pathcurr = abspath((char *) arg);
	for(;;) {
		sel = pathsel(pathcurr);
		sendp(selc, sel);
		switch(alt(alts)) {
		case DIR:
			free(pathcurr);
			pathcurr = abspath(dir);
			break;
		case NAME:
// todo: cleanup flow
			free(pathcurr);
			if(strcmp(name, "..") == 0) {
				pathcurr = strdup(sel->dirpath);
				if(strcmp(pathcurr, "/") != 0) {
					pr = utfrrune(pathcurr, '/');
					if(pr != pathcurr)
						*pr = '\0';
					else if(strlen(pathcurr) > 1)
						pr[1] = '\0';
				}
			} else {
				if(strcmp(sel->dirpath, "/") != 0)
					pathcurr = smprint("%s/%s", sel->dirpath, name);
				else
					pathcurr = smprint("/%s", name);
			}
			free(name);
			break;
		}
	}
}

Dirmenuitem
parsedirmenuarg(char *a)
{
	char *as = strdup(a);
	char *pr, *dirinputstr;
	Dirmenuitem dirmenuitem;

	pr = utfrune(as, ':');
	*pr = '\0';
	dirmenuitem.name = strdup(as);
	// todo: move by runes
	dirinputstr = pr + 1;
	dirmenuitem.dir = abspath(dirinputstr);
	free(as);
	return dirmenuitem;
}

Entscriptmenuitem
parseentscriptmenuarg(char *a)
{
	char *as = strdup(a);
	char *pr, *entscriptinputstr;
	Entscriptmenuitem entscriptmenuitem;

	pr = utfrune(as, ':');
	*pr = '\0';
	entscriptmenuitem.name = strdup(as);
	// todo: move by runes
	entscriptinputstr = pr + 1;
	entscriptmenuitem.script = abspath(entscriptinputstr);
	free(as);
	return entscriptmenuitem;
}

void
threadmain(int argc, char *argv[])
{
	char *flagparam;
	uint ndirmenuitems, nentscriptmenuitems;
	Dirmenuitem blankdirmenuitem = {nil, nil};
	Entscriptmenuitem blankentscriptmenuitem = {nil, nil};

	nentscriptmenuitems = ndirmenuitems = 0;
	dirmenuitems = (Dirmenuitem *) realloc(dirmenuitems,
		sizeof(*dirmenuitems) * (ndirmenuitems + 1));
	entscriptmenuitems = (Entscriptmenuitem *) realloc(entscriptmenuitems,
		sizeof(*entscriptmenuitems) * (nentscriptmenuitems + 1));
	dirmenuitems[ndirmenuitems] = blankdirmenuitem;
	entscriptmenuitems[nentscriptmenuitems] = blankentscriptmenuitem;
	ARGBEGIN{
	case 's':
		flagparam = ARGF();
		if(flagparam == nil)
			terminate("designated directory flag takes a parameter");
		nentscriptmenuitems++;
		entscriptmenuitems = (Entscriptmenuitem *) realloc(entscriptmenuitems,
			sizeof(*entscriptmenuitems) * (nentscriptmenuitems + 1));
		entscriptmenuitems[nentscriptmenuitems] = blankdirmenuitem;
		entscriptmenuitems[nentscriptmenuitems-1] =
			parseentscriptmenuarg(flagparam);
		break;
	case 'd':
		flagparam = ARGF();
		if(flagparam == nil)
			terminate("designated directory flag takes a parameter");
		ndirmenuitems++;
		dirmenuitems = (Dirmenuitem *) realloc(dirmenuitems,
			sizeof(*dirmenuitems) * (ndirmenuitems + 1));
		dirmenuitems[ndirmenuitems] = blankdirmenuitem;
		dirmenuitems[ndirmenuitems-1] = parsedirmenuarg(flagparam);
		break;
	default:
		print("nine\n");
	}ARGEND;


	static const uint INITX = 410, INITY = 300;
	char *cmdnewwindow;
	cmdnewwindow = smprint("-dx %d -dy %d",
		// account for rio window border
		INITX + 8, INITY + 8);
	newwindow(cmdnewwindow);
	free(cmdnewwindow);

	fmtinstall('P', Pfmt);
	fmtinstall('R', Rfmt);

	if(initdraw(nil, nil, "sfbrow") < 0)
		sysfatal("initdraw: %r");

	mctl = initmouse(nil, screen);
	if(mctl == nil)
		sysfatal("initmouse: %r");
	kctl = initkeyboard("/dev/cons");
	if(kctl == nil)
		sysfatal("initkeyboard: %r");

	/* colors */
	thiscolors.scrollhandle = allocimagemix(display, DDarkblue, DWhite);
	thiscolors.scrollbg = allocimagemix(display, DPurpleblue, DWhite);
	/* channels */
	// todo: unglobalize at least some of these
	scrollc = chancreate(sizeof(ulong), 0);
	nameidxc = chancreate(sizeof(ulong), 0);
	selc = chancreate(sizeof(Sel), 0);
	namec = chancreate(sizeof(char *), 0);
	dirc = chancreate(sizeof(char *), 0);
	entscriptc = chancreate(sizeof(char *), 0);
	layoutreqc = chancreate(sizeof(char *), 0);
	layoutresc = chancreate(sizeof(Layout *), 0);
	statereqc = chancreate(sizeof(char *), 0);
	stateresc = chancreate(sizeof(AppState), 0);
	statesetc = chancreate(sizeof(AppState), 0);
	namefilterc = chancreate(sizeof(Rune), 0);
	sortc = chancreate(sizeof(ulong), 0);
	/* threads */
	threadcreate(atomthread_state, nil, 8*1024 +
		FILTER_MAX*sizeof(Rune) + sizeof(Layout));
	threadcreate(resizethread, nil, 8*1024);
	threadcreate(mousethread, nil, 8*1024);
	threadcreate(keyboardthread, nil, 8*1024);
	threadcreate(statethread, nil, 8*1024 +
		3*(FILTER_MAX*sizeof(Rune) + sizeof(Layout)));
	proccreate(selthread, ".", 8*1024);
	threadexits(nil);
}
