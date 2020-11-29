#include <u.h>
#include <libc.h>

struct namedbuf {
	char *name;
	char *buf;
	struct namedbuf *next;
};

struct vec2d {
	double x;
	double y;
};

void
main(void)
{

}

char *
valinnamedbuf(struct namedbuf *nb, char *name)
{
	struct namedbuf *cnb;

	for(cnb = nb; cnb != nil; cnb = cnb->next)
		if(!strcmp(cnb->name, name))
			return cnb->buf;
	return nil;
}

int
signalonnamedbufs(struct namedbuf *nb, char *names[])
{
	char *name;
	struct namedbuf *cnb;

	for(name = *names; name - *names < nelem(names); name++) {
		for(cnb = nb; cnb != nil; cnb = cnb->next)
			if(!strcmp(cnb->name, name))
				if(cnb->buf == nil)
					/* no signal */
					return 0;
				else break;
		if(cnb == nil)
			/* missing name */
			return -1;
	}
	/* signal on all names */
	return 1;
}

int
setnb(struct namedbuf *nb, char name[])
{
	while(nb != nil && strcmp(nb->name, name))
		nb = nb->next;
	if(nb == nil)
		return -1;
	free(nb->buf);
	nb->buf = strdup(name);
	return 0;
}



/* static vars version */
int
avoidsv(struct namedbuf *in, struct namedbuf *out)
{
	struct vec2d selectdirection(char *, char *);
	int significantforcep(struct vec2d, double);

	enum avoidstate {NIL, PLAN, GO, START};
	static struct vec2d resf; /* result force */
	static enum avoidstate currstate = NIL;

	static char nmforce[] = "force";
	static char nmheading[] = "heading";
	static char *nms[] = {nmforce, nmheading};
	
	switch(currstate) {
		case NIL:
			/* event dispatch */
			if(signalonnamedbufs(in,
					nms)) {
				currstate = PLAN;
				return 1;
			}
			return 0;
		case PLAN:
			/* set instance var */
			resf = selectdirection(
				valinnamedbuf(in, nmforce),
				valinnamedbuf(in, nmheading));
			currstate = GO;
			return 0;
		case GO:
			/* conditional dispatch */
			if(significantforcep(resf, 1.0))
				currstate = START;
			else
				currstate = NIL;
			return 1;
		case START:
			return -1;
		default:
			return -1;
	}
}

struct vec2d
selectdirection(char *forcestr, char *headingstr)
{
	struct vec2d parsevec2d(char *);
	double force = atof(forcestr);
	struct vec2d heading = parsevec2d(headingstr);
	struct vec2d resf = heading;
	
	resf.x *= force;
	resf.y *= force;
	return resf;
}

int
significantforcep(struct vec2d fvec, double scale)
{
	return hypot(fvec.x, fvec.y) > 1.0 * scale;
}

char *
followforce(struct vec2d fvec)
{
	return nil;
}

struct vec2d
parsevec2d(char *buf)
{
	char pbuf[64]; /* parse buffer */
	struct vec2d rv;
	char *t = strchr(buf, ',');

	if(t == nil)
		exits("bad vec str fmt");
	strecpy(pbuf, t - 1, buf);
	rv.x = atof(pbuf);
	rv.y = atof(t + 1);
	return rv;
}

void
serializevec2d(struct vec2d v, char *buf, int bufsz)
{
	snprint(buf, bufsz, "%g,%g", v.x, v.y);
}


