#include <u.h>
#include <libc.h>
#include <stdio.h>
#include <draw.h>
#define MAXX	320
#define MAXY	300
#define MAX_CMD_NEW_WINDOW	100 /* strlen for newwindow() */
#define NUM_C64_COLORS	16
#define COLOR_IDX_BG	1 /* background color index */

/* ??? nelem() */

Image *c64_colors[NUM_C64_COLORS];


void init_drawering(void);
Image *get_color_idx(int i);
void drawrect(Rectangle, Image *);
void blankscreen(void);

void
main(void)
{
	const static int size = 20, seconds = 17;
	const int squares = (MAXX * MAXY)
				/
				(size * size);
	int i, skips, offset, coordx, coordy;
	Image *color;
	Rectangle r;

	init_drawering();

	skips = 0;
	for(i = 0;
		i < squares;
		i++) {
		if(i % NUM_C64_COLORS == COLOR_IDX_BG) {
			skips++;
			continue;
		}
		color = get_color_idx(i);
		offset = i - skips;
		coordx = (offset * size) % (MAXX - size);
		coordy = (offset * size) % (MAXY - size);		
		r = Rect(coordx, coordy,
				coordx + size, coordy + size);
		blankscreen();
		drawrect(r, color);
		sleep((1000 * seconds) / squares);
	}

	printf("\n");

	exits(0);
}

void
init_drawering(void)
{
	void init_color_pallet(void);

	char cmdnewwindow[MAX_CMD_NEW_WINDOW];
	sprintf(cmdnewwindow, "-dx %d -dy %d", MAXX, MAXY);
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

