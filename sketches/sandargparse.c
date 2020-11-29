#include <u.h>
#include <libc.h>

/*
term% sandargparse.out -a 1 -a 2
val: 1
val: 2
*/

void
main(int argc, char *argv[])
{
	char *a;

	ARGBEGIN{
	case 'a':
		a = ARGF();
		if(a == nil)
			print("no val\n");
		else
			print("val: %s\n", a);
		break;
	default:
		print("nine\n");
	}ARGEND;

	exits(nil);
}
