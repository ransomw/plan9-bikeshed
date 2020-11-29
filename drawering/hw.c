#include <u.h>
#include <libc.h>
#include <draw.h>

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

/* globals in draw.h
	Display display; // connection 
	Image screen; // client's window
 * also available
void	draw(Image *dst, Rectangle r, Image *src,
		    Image *mask, Point p)
void	line(Image *dst, Point p0, Point p1, int end0, int end1,
		    int	radius,	Image *src, Point sp)
 */
	

void
main(void)
{
	Image piktur;
	Image *palpart; /* like pallet in paint.c */
	Image *color;
	Rectangle r;
	Point p0, p1, p2;

	p0.x = 0;
	p0.y = 0;
	p1.x = 100;
	p1.y = 100;
	p2.x = 200;
	p2.y = 200;
	r.min = p1; /* upper left */
	r.max = p2; /* lower right */

	newwindow("-dx 300 -dy 300");

	if(initdraw(0, 0, "helloworld") < 0)
		sysfatal("initdraw: %r");

	//
	r = screen->r;
	r.min.y = r.max.y - 40;
	color = allocimage(display, Rect(0, 0, 1, 1), RGB24, 1,
				c64[3 % nelem(c64)]<<8 | 0xFF);
	draw(screen, r, color, nil, ZP);

	//
	/*
	palpart = allocimage(display, Rect(0, 0, 1, 1), RGB24, 1,
		c64[3]<<8 | 0xFF);
	draw(screen, r, palpart, nil, p0);
	flushimage(display, 1);
	*/

	sleep(3000);


	exits(0);
}

