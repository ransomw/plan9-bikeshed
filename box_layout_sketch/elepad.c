/*

a layout is a rectangle that contains other rectangles.
there can be blank spaces -- a layout needn't be a partition
of its containing rectangle.  although, the behavior of blank spaces
(let's call them backgrounds) is not well-understood or -defined.

while it generalizes the current sfbrow layout via OOP-style
function pointers for handlers,
libelementile includes layout-specific ideas such as vertical
and horizontal dividers.
this ele[ment][note]pad is a move toward further generalization
of layout objects that contain other layout objects in a way that
does not pre-define layout particulars other than the preceeding
definition of what layout is.

there is no one true way of defining boxes within a box.
possibilities include
- coordinates based on percentages (e.g. percent width)
- fixed sizes in pixels
- fitting content size
implementation is based on a two-pass recursion thru the tree
- desired: leaves up
- actual:

rather than events/callbacks, e.g. passing a Mouse to root,
as in elementile, the opinion here is have bundles fibrous
of channels tethering layouts together.  one bundle sends
impulses down through the tree of elements, and another bundle
bubbles events back up.  it's unclear how useful the latter will
be.  it's lifted from web browser rendering engines, after all.

*/

typedef struct {
	Channel *m;
	Channel *k;
} ActionChans;

ActionChans
newActionChans(void)
{
	ActionChans chans;

	chans.m = chancreate(sizeof(Mouse), 0);
	chans.k = chancreate(sizeof(Rune), 0);
	return chans;
}

typedef struct {
	ActionChans down /* pass to children */
	ActionChans up /* bubble up */
	Channel *close; /* signal layout removed */
} ActionBundle;

ActionBundle
newActionBundle(void)
{
	ActionBundle ab;

	ab.down = newActionChans();
	ab.up = newActionChans();
	ab.close = chancreate(sizeof(char *), 0);
	return ab;
}

//

enum BoxT { BOX_PER, BOX_PIX, BOX_FIT };

typedef struct {} LBkgnd; /* uncertainty */

enum LayoutT {L_CONTAINER, L_LEAF};

typedef struct {
	ActionBundle ab;
	BoxT boxT;
	Rectangle box;
	Rectangle boxEst;
	Rectangle boxR;
	Rectangle (*boxfn)(LayoutContainer l);
	Layout boxes;
	LBkgnd bkgnd;
} LayoutContainer;
typedef struct {
	ActionBundle ab;
	BoxT boxT;
	Rectangle box; // meaning according to BoxT
	Rectangle boxEst; // estimation pass box (px)
	Rectangle boxR; // real drawn box (px)
	Rectangle (*boxfn)(LayoutLeaf l);
	// XX what else???
	// access application state, draw, and ___
} LayoutLeaf;
typedef struct {
	v union {
		LayoutContainer lc;
		LayoutLeaf ll;
	};
	t LayoutT;
	next Layout; /* allow a layout to contain multiple layouts */
} *Layout;

void
layout_container_thread(args *void)
{
	LayoutContainer lc; // set from args
	ActionBundle ab = lc.ab;
	char *close_sig;
	Mouse m;
	Rune k;
	//
	Layout currbox;

	enum { MOUSE_DOWN, MOUSE_UP, KBD_DOWN, KBD_UP, CLOSE, NONE };
	Alt alts[] = {
		[MOUSE_DOWN] = {ab.down.m, &m, CHANRCV},
		[MOUSE_UP] = {ab.up.m, &m, CHANRCV},
		[KBD_DOWN] = {ab.down.k, &k, CHANRCV},
		[KBD_UP] = {ab.up.k, &k, CHANRCV},
		[CLOSE] = {ab.close, &close_sig, CHANRCV},
		[NONE] = {nil, nil, CHANEND},
	};
	for(;;) {
		switch(alt(alts)) {
		case MOUSE_DOWN:
			for(currbox = lc.boxes; currbox != nil; currbox->next)
				send(currbox->v.ab.down.m, &m);
			break;
		case MOUSE_UP:
			break;
		case KBD_DOWN:
			for(currbox = lc.boxes; currbox != nil; currbox->next)
				send(currbox->v.ab.down.k, &k);
			break;
		case KBD_UP:
			break;
		case CLOSE:
			threadexits(nil);
			break;
		}
	}
}

void
layout_leaf_thread(args *void)
{
	ab ActionBundle;
	char *close_sig;
	Mouse m;
	Rune k;

	enum { MOUSE_DOWN, MOUSE_UP, KBD_DOWN, KBD_UP, CLOSE, NONE };
	Alt alts[] = {
		[MOUSE_DOWN] = {ab.down.m, &m, CHANRCV},
		[MOUSE_UP] = {ab.up.m, &m, CHANRCV},
		[KBD_DOWN] = {ab.down.k, &k, CHANRCV},
		[KBD_UP] = {ab.up.k, &k, CHANRCV},
		[CLOSE] = {ab.close, &close_sig, CHANRCV},
		[NONE] = {nil, nil, CHANEND},
	};
	for(;;) {
		switch(alt(alts)) {
		case MOUSE_DOWN:
			break;
		case MOUSE_UP:
			break;
		case KBD_DOWN:
			break;
		case KBD_UP:
			break;
		case CLOSE:
			break;
		}
	}
}

int
layout_valid_box(l Layout)
{
	switch(layout.boxT) {
	case BOX_PERC:
		return
			l->v.boxfn == nil &&
			l->v.box.min.x >= 0 &&
			l->v.box.min.y >= 0 &&
			l->v.box.max.x <= 100 &&
			l->v.box.max.y <= 100;
		break;
	case BOX_PIX:
		return
			l->v.boxfn == nil &&
			l->v.box.min.x >= 0 &&
			l->v.box.min.y >= 0;
		break;
	case BOX_FIT:
		return
			l->v.boxfn != nil;
		break;
	}
}

/*
verify rectangles all fit for layouts other than the root.
 */
int
layout_valid_boxes(l Layout)
{

}

/*
compute the coordinates (in pixels?) of a layout within
the rectangle of its parent box
*/
Rectangle
layout_box_in_parent(Rectangle bp, Layout cl)
{
	Rectangle rr;
	Rectangle fr;
if(cl->t == L_LEAF) {
	switch(layout.boxT) {
	case BOX_PIX:
		rr.min.x = cl->v.box.min.x + pb.min.x;
		rr.min.y = cl->v.box.min.y + pb.min.y;
		rr.max.x = cl->v.box.max.x + pb.min.x;
		rr.max.y = cl->v.box.max.y + pb.min.y;
		return rr;
		break;
	case BOX_PERC:
		rr.min.x = cl->v.box.min.x * Dx(pb) + pb.min.x;
		rr.min.y = cl->v.box.min.y * Dy(pb) + pb.min.y;
		rr.max.x = cl->v.box.max.x * Dx(pb) + pb.min.x;
		rr.max.y = cl->v.box.max.y * Dy(pb) + pb.min.y;
		return rr;
		break;
	case BOX_FIT:
		fr = cl->v.boxf(cl->v);
		if(cl->v.box.min.x >= 0)
			rr.min.x = cl->v.box.min.x * Dx(pb) + pb.min.x;
		else
			rr.min.x = fr.min.x * Dx(pb) + pb.min.x;

			rr.min.y = cl->v.box.min.y * Dy(pb) + pb.min.y;
			rr.max.x = cl->v.box.max.x * Dx(pb) + pb.min.x;
			rr.max.y = cl->v.box.max.y * Dy(pb) + pb.min.y;

		return rr;
		break;
	}
} else {

}
}
