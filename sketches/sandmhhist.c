#include <u.h>
#include <libc.h>
#include <draw.h>
#include <bio.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>

Mousectl *mctl;


void
histogram(void)
{
	Keyboardctl *kc;
	Mouse mm;
	Rune km;
	Alt a[] = {
		/* c	v	op */
		{nil,	&mm,	CHANRCV},	/* mouse message */
		{nil,	&km,	CHANRCV},	/* keyboard runes */
		{nil,	nil,	CHANEND},
	};
	static char *mitems[] = {
		"exit",
		nil
	};
	static Menu menu = {
		mitems,
		nil,
		-1
	};

	memset(&mm, 0, sizeof mm);
	memset(&km, 0, sizeof km);


	mctl = initmouse(nil, screen);
	if(!mctl)
		sysfatal("initmouse: %r");


	if(initdraw(nil, nil, "sandmhhist") < 0)
		sysfatal("initdraw: %r");

	kc = initkeyboard(nil);
	if(!kc)
		sysfatal("initkeyboard: %r");

	a[0].c = mctl->c;
	a[1].c = kc->c;

	for(;;)
		switch(alt(a)){
		case 0:
			if(mm.buttons & 4 && menuhit(3, mctl, &menu, nil) == 0)
				goto done;
			break;
		case 1:
			if(km == Kdel)
				goto done;
			break;
		default:
			sysfatal("shouldn't happen");
		}
done:
	closekeyboard(kc);
	closemouse(mctl);
	chanfree(a[0].c);
	threadexitsall(nil);
}

void
threadmain(int argc, char **argv)
{
	static const uint INITX = 410, INITY = 300;
	char *cmdnewwindow;
	cmdnewwindow = smprint("-dx %d -dy %d",
		// account for rio window border
		INITX + 8, INITY + 8);
	newwindow(cmdnewwindow);
	free(cmdnewwindow);



	histogram();
}