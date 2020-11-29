#include <u.h>
#include <libc.h>
#include <thread.h>
#include <draw.h>
#include <mouse.h>
#include <keyboard.h>

Mousectl *mctl;
Keyboardctl *kctl;

void
terminate(char *err)
{
	closekeyboard(kctl);
	closemouse(mctl);
	closedisplay(display);
	threadexitsall(err);
}


void
threadmain(int, char *argv[])
{
	static const uint INITX = 410, INITY = 300;
	char *cmdnewwindow;
	cmdnewwindow = smprint("-dx %d -dy %d",
		// account for rio window border
		INITX + 8, INITY + 8);
	newwindow(cmdnewwindow);
	free(cmdnewwindow);

	static char *mitems[] = {
		"exit",
		nil
	};
	static Menu menu = {
		mitems,
		nil,
		-1
	};

	Mouse m;

	int menuhitres;

//	mctl = initmouse("/dev/mouse", nil);
	mctl = initmouse(nil, screen);
	if(mctl == nil)
		sysfatal("initmouse: %r");

	if(initdraw(nil, nil, argv[0]) < 0)
		sysfatal("initdraw: %r");


	for(;;) {
		recv(mctl->c, &m);
		if(m.buttons & 2) {
			if((menuhitres = menuhit(2, mctl, &menu, nil)) == 0) {
				print("selected first item\n");
				terminate(nil);
			} else
				print("selected %d item\n", menuhitres);
		}
	}
}
