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

struct vec2d selectdirection(char *forcestr, char *headingstr);

/* static vars version */
char *
avoidsv(struct namedbuf *in, struct namedbuf *out)
{
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
					nms
				//	{"force", "heading"}
					))
				currstate = PLAN;
			break;
		case PLAN:
	//		resf = selectdirection(
				
			break;
		default:
			return "unknown state";
	}
	return nil;
}

struct vec2d
selectdirection(char *forcestr, char *headingstr)
{
	struct vec2d parsevec2d(char *buf);
	struct vec2d force = parsevec2d(forcestr);
	struct vec2d heading = parsevec2d(headingstr);
	return heading;
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


