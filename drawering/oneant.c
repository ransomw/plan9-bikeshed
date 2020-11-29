#include <u.h>
#include <libc.h>
#include <draw.h>
#define MAX_CMD_NEW_WINDOW	100 /* strlen for newwindow() */
#define NUM_C64_COLORS	16
#define COLOR_IDX_BG	1 /* background color index */

typedef Image *Color;

struct {
	Color bg;
	Color nest;
	Color food;
	Color ant;
	Color foodant;
} thiscolors;

typedef struct ant Ant;

struct ant {
	Point p; /* current position */
	Point v; /* direction of motion */
	int steps; /* pedometer */
	unsigned char foundfood; /* boolean */
};

typedef struct ecosystem EcoSystem;

struct ecosystem {
	Rectangle food;
	Rectangle nest;
	Ant ant;
};

void init_drawering(Point);
Image *get_color_idx(int);
void updateecosystem(EcoSystem *es, Point drawmaxes);
void drawecosystem(EcoSystem es);

void
main(void)
{
	EcoSystem es = {
		{150, 150, 165, 165},
		{10, 10, 20, 20},
		{{20, 20}, {10, 0}, 0, 0}
	};
	Point drawmaxes = {310, 300};

	init_drawering(drawmaxes);
	thiscolors.bg = get_color_idx(1);
	thiscolors.nest = get_color_idx(2);
	thiscolors.food = get_color_idx(5);
	thiscolors.ant = get_color_idx(0);
	thiscolors.foodant = get_color_idx(6);

	for(;;) {
		updateecosystem(&es, drawmaxes);
		drawecosystem(es);
		sleep(500);
	}
}

void
updateecosystem(EcoSystem *es, Point drawmaxes)
{
	static const double antaccuracy = 0.5;
	double afuzz = PI * antaccuracy * (frand() - 0.5);
//	Point antv = es->ant.v;
	Ant *ant = &es->ant;
	Point *antpos = &ant->p;
	Point *antvec = &ant->v;
	Point aantv;

	if(antpos->x <= 0 || antpos->x >= drawmaxes.x ||
			antpos->y <= 0 || antpos->y >= drawmaxes.y) {
		antvec->x *= -1;
		antvec->y *= -1;
	}

	aantv.x = antvec->x * cos(afuzz) - antvec->y * sin(afuzz);
	aantv.y = antvec->x * sin(afuzz) + antvec->y * cos(afuzz);

	antpos->x += aantv.x;
	antpos->y += aantv.y;
	antpos->x = antpos->x < 0 ? 0 : antpos->x;
	antpos->y = antpos->y < 0 ? 0 : antpos->y;
	antpos->x = antpos->x > drawmaxes.x ? drawmaxes.x : antpos->x;
	antpos->y = antpos->y > drawmaxes.y ? drawmaxes.y : antpos->y;
}

/** drawing */

void
drawecosystem(EcoSystem es)
{
	void blankscreen(void);
	static int anthalfsize = 2;
	void drawrect(Rectangle, Image *);
	Rectangle antrect;
	
	antrect.min = es.ant.p;
	antrect.max = es.ant.p;
	antrect.min.x -= anthalfsize;
	antrect.min.y -= anthalfsize;
	antrect.max.x += anthalfsize;
	antrect.max.y += anthalfsize;

	blankscreen();
	drawrect(es.nest, thiscolors.nest);
	drawrect(es.food, thiscolors.food);
	if(es.ant.foundfood)
		drawrect(antrect, thiscolors.foodant);	
	else
		drawrect(antrect, thiscolors.ant);
}

void
init_drawering(Point p)
{
	void init_color_pallet(void);

	char cmdnewwindow[MAX_CMD_NEW_WINDOW];
	seprint(cmdnewwindow, cmdnewwindow+sizeof(cmdnewwindow),
		"-dx %d -dy %d",
		/* account for rio window border */
		p.x + 8, p.y + 8);
	newwindow(cmdnewwindow);
	if(initdraw(0, 0, "paint") < 0)
		sysfatal("initdraw: %r");
	init_color_pallet();
	flushimage(display, 1);
}

void
blankscreen(void)
{
	draw(screen, screen->r, thiscolors.bg,
		nil, ZP);
	flushimage(display, 1);
}

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
