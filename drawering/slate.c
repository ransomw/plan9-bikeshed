#include <u.h>
#include <libc.h>
#include <stdio.h>
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




#define C_BLACK		1
#define MY_COLOR	3

void
mydrawpal(int height, Image *color)
{
	Rectangle r;
	r = screen->r;
	r.min.y = r.max.y - height;
	draw(screen, r, color, nil, ZP);
}

void
mydrawrect(Rectangle r, Image *color)
{
	Rectangle sr = screen->r;
	r.min.x += sr.min.x;
	r.min.y += sr.min.y;
	r.max.x += sr.min.x;
	r.max.y += sr.min.y;

	printf("mydrawrect (%d, %d)-(%d, %d)\n",
		r.min.x, r.min.y,
		r.max.x, r.max.y);

	draw(screen, r, color, nil, ZP);
}


void
main(void)
{
	Rectangle r, rr;
	Image *ca, *cb, *cc;

	newwindow("-dx 300 -dy 300");

	if(initdraw(0, 0, "paint") < 0)
		sysfatal("initdraw: %r");


	ca = allocimage(display, Rect(0, 0, 1, 1), RGB24, 1,
			c64[MY_COLOR % nelem(c64)]<<8 | 0xFF);
	cb = allocimage(display, Rect(0, 0, 1, 1), RGB24, 1,
			c64[5 % nelem(c64)]<<8 | 0xFF);
	cc = allocimage(display, Rect(0, 0, 1, 1), RGB24, 1,
			c64[7 % nelem(c64)]<<8 | 0xFF);



	r = screen->r;

	printf("(%d, %d)-(%d, %d)\n",
		r.min.x, r.min.y,
		r.max.x, r.max.y);

	draw(screen, r, cc, nil, ZP);
	flushimage(display, 1);

	sleep(1000);

	mydrawrect(Rect(0, 0, 20, 20), cb);

	mydrawpal(40, ca);
	flushimage(display, 1);

	sleep(1000);

	mydrawpal(20, cb);
	flushimage(display, 1);


	sleep(1000);
	exits(0);


}
