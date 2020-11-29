#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <draw.h>
#define MAXX	310
#define MAXY	300
#define MAX_CMD_NEW_WINDOW	100 /* strlen for newwindow() */
#define NUM_C64_COLORS	16
#define COLOR_IDX_BG	1 /* background color index */

/* ??? nelem() */

struct firework {
	Point p;
	char t;
	Image *color;
	struct firework *next;
};

void init_drawering(void);
void drawfirework(struct firework);
Image *get_color_idx(int i);
void blankscreen(void);

void drawray(Point p, Point q, Image *color);

struct firework newfirework(void);
void updatefirework(struct firework *);
void addfirework(struct firework *fp);

void
main(void)
{

	struct firework f;
	int i;

//	Point p = {MAXX / 2, MAXY / 2};
//	Point q = {3 * MAXX / 4, MAXY / 2};


	init_drawering();

	f = newfirework();

	for(i = 0;
		i < 32;
		i++) {
		blankscreen();
		drawfirework(f);
		sleep(250);
		updatefirework(&f);
		if(i % 2 == 0)
			addfirework(&f);
	}


	printf("\n");

	exits(0);
}

struct firework
newfirework(void)
{
	struct firework f;
	double randscale;
	double randcolor;
	int idxcolor;

	f.p.x = frand() * (double) MAXX;
	f.p.y = frand() * (double) MAXY;

	randcolor = frand();
	idxcolor = (((int) ((NUM_C64_COLORS - 1) * randcolor))
				+ COLOR_IDX_BG + 1) %
			NUM_C64_COLORS;
	f.color = get_color_idx(idxcolor);

	f.t = 0;
	f.next = nil;
	return f;
}

void
addfirework(struct firework *fp)
{
	struct firework *fpn;

	fpn = (struct firework *) malloc(sizeof(*fpn));
	*fpn = newfirework();
	while(fp->next != nil)
		fp = fp->next;
	fp->next = fpn;
}

void
incfireworktime(struct firework *fp)
{
	static const int maxtime = 7;

	fp->t++;
	if(fp->t > maxtime)
		fp->t = -1;
	if(fp->next != nil)
		incfireworktime(fp->next);
}

void
prunededfireworksh(struct firework *fp, struct firework *fpn)
{
	if(fpn == nil)
		return;
	if(fpn->t == -1) {
		free(fp->next);
		fp->next = fpn->next;
		prunededfireworksh(fp, fp->next);
	} else
		prunededfireworksh(fpn, fpn->next);
}

void
prunededfireworks(struct firework *fp)
{
	struct firework f;

	prunededfireworksh(fp, fp->next);
	if(fp->t != -1)
		return;
	f = newfirework();
	f.next = fp->next;
	*fp = f;
}

void
updatefirework(struct firework *fp)
{
	incfireworktime(fp);
	prunededfireworks(fp);
}

/* drawing things */

void
init_drawering(void)
{
	void init_color_pallet(void);

	char cmdnewwindow[MAX_CMD_NEW_WINDOW];
	sprintf(cmdnewwindow, "-dx %d -dy %d",
		/* account for rio window border */
		MAXX + 8, MAXY + 8);
	newwindow(cmdnewwindow);
	if(initdraw(0, 0, "paint") < 0)
		sysfatal("initdraw: %r");
	init_color_pallet();
	flushimage(display, 1);
}

void
drawrect(Rectangle r, Image *color)
{
	Rectangle sr = screen->r;
	r.min.x += sr.min.x;
	r.min.y += sr.min.y;
	r.max.x += sr.min.x;
	r.max.y += sr.min.y;
	draw(screen, r, color, nil, ZP);
	flushimage(display, 1);
}

Rectangle
translaterect(Rectangle r, Point p)
{
	r.min.x += p.x;
	r.min.y += p.y;
	r.max.x += p.x;
	r.max.y += p.y;
	return r;
}

void
printrect(Rectangle r)
{
	printf("(%d, %d) - (%d, %d)\n",
		r.min.x, r.min.y, r.max.x, r.max.y);
}

void
drawray(Point p, Point q, Image *color)
{
	static const sqsz = 3;
	Point v, u;
	Rectangle r, rs, re;
	int dist;
	int i;
//	double sf;

	v.x = q.x - p.x;
	v.y = q.y - p.y;
	rs.min = p;
	rs.max.x = rs.min.x + sqsz;
	rs.max.y = rs.min.y + sqsz;


	if(v.x < 0) {
		rs.min.x -= sqsz;
		rs.max.x -= sqsz;
	}
	if(v.y < 0) {
		rs.min.y -= sqsz;
		rs.max.y -= sqsz;
	}
	re.min = q;
	re.max.x = re.min.x + sqsz;
	re.max.y = re.min.y + sqsz;

	r = rs;
	dist = hypot(v.x, v.y);
	for(i = 1;
		(v.x > 0 ? r.min.x < q.x : r.min.x > q.x) ||
		(v.y > 0 ? r.min.y < q.y : r.min.y > q.y);
		i++) {
		u = v;
		u.x /= dist / (sqsz + i);
		u.y /= dist / (sqsz + i);
		r = translaterect(rs, u);
		drawrect(r, color);
		u.x *= -1;
		u.y *= -1;
		drawrect(translaterect(re, u), color);
	}
}

void
drawfirework(struct firework f)
{
	static const unsigned char nrays = 6;
	int raylen = 8 * f.t;
	int i;
	double a;
	Point rayq;
	

	for(i = 0; i < nrays; i++) {
		rayq.x = raylen;
		rayq.y = raylen;
		a = i * 2 * PI / nrays;
		rayq.x *= sin(a);
		rayq.y *= cos(a);
		rayq.x += f.p.x;
		rayq.y += f.p.y;
		drawray(f.p, rayq, f.color);

/*
		printf("%.0g (%d, %d) - (%d, %d)\n",
			360 * a/(2*PI),
			f.p.x, f.p.y, rayq.x, rayq.y);
*/
	}
	if(f.next != nil)
		drawfirework(*f.next);
}

void
blankscreen(void)
{
	draw(screen, screen->r, get_color_idx(COLOR_IDX_BG),
		nil, ZP);
	flushimage(display, 1);
}

/* must be */

Image *c64_colors[NUM_C64_COLORS];

Image *
get_color_idx(int i)
{
	return c64_colors[i % NUM_C64_COLORS];
}

void
init_color_pallet(void)
{
	int c64[] = {		/* c64 color palette */
		0x000000,
		0xFFFFFF,
		0x68372B,
		0x70A4B2,
		0x6F3D86,
		0x588D43,
		0x352879,
		0xB8C76F,
		0x6F4F25,
		0x433900,
		0x9A6759,
		0x444444,
		0x6C6C6C,
		0x9AD284,
		0x6C5EB5,
		0x959595,
	};
	int i;

	for(i = 0; i < NUM_C64_COLORS; i++)
		c64_colors[i] = allocimage(
			display, Rect(0, 0, 1, 1), RGB24, 1,
			c64[i]<<8 | 0xFF);
}

