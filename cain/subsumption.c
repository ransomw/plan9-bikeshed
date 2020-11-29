#include <u.h>
#include <libc.h>

typedef struct namedbuf {
	char *name;
	char *buf;
	struct namedbuf *next;
};

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
char *
avoidsv(struct namedbuf *in, struct namedbuf *out)
{
	enum state {NIL, PLAN, GO, START};
	static double resf; /* result force */
	static enum state currstate = NIL;

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
			/* conditional dispatch */
		//	resf = 
			break;
		default:
			return "unknown state";
	}
	return nil;
}
