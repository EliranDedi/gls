#include <u.h>
#include <libc.h>
#include <draw.h>
#include <event.h>

void eresized(int new) { sysfatal("eresized"); }

/*
DESCRIPTION
	gls - graphical file browser
NOTES

TODO
	everything
*/

char	workdir[1024] = ".";
char	*tmp;
long	nfile;
Dir	*dir;
int	fd, i;
int	maxwid;
int	ncol;
int	nline;
Point	strpt;
Point	drawp;
Image	*yellow;
Image	*green;
Image	*red;
Image	*grey;
Event	e;
Mouse	*m = &e.mouse;
Rectangle selected;
Point gridmax;
char	*cdto;
int	inited;

#define Line(n)	(n*font->height)
#define Col(n)	(n)

int
dirstrlencmp(Dir *a, Dir *b)
{
	return -(strlen(a->name)-strlen(b->name));
}

int
dirnamecmp(Dir *a, Dir *b)
{
	return strcmp(a->name, b->name);
}

char msg2[] = "interrupt";

int interrupted(void *v, char *msg){ if(strcmp(msg,msg2) == 0) sysfatal(""); closedisplay(display); return 1;}

void
drawbottom(char *s)
{
	Point bottom;

	if(s == nil){
		drawbottom("nil");
		return;
	}
	bottom.x = screen->r.min.x;
	bottom.y = screen->r.max.y-font->height;
	draw(screen, Rpt(bottom, screen->r.max), display->white, nil, ZP);
	string(screen, bottom, display->black, bottom, font, s);
}

void
select3(void)
{
	int p, x, y;
	Point min, max;

	for(y = 0; screen->r.min.y + y * font->height < m->xy.y-font->height; ++y)
		;
	for(x = 0; screen->r.min.x + x * maxwid < m->xy.x-maxwid; ++x)
		;
//	if(x > ncol)
//		--x;
	p = nline*x+y;
	min = Pt(screen->r.min.x+x*maxwid,screen->r.min.y+y*font->height);
	max = Pt(screen->r.min.x+x*maxwid+maxwid,screen->r.min.y+y*font->height+font->height);
//	selected = Rpt(Pt(screen->r.min.x+x*maxwid,screen->r.min.y+y*font->height),Pt(screen->r.min.x+x*maxwid+maxwid,screen->r.min.y+y*font->height+font->height));
	selected = Rpt(min, max);
	if(p >= nfile || selected.min.x >= gridmax.x){
		drawbottom(smprint("nil %R %P", selected, gridmax));
		return;
	}
	draw(screen, selected, grey, nil, ZP);
	flushimage(display, 1);
	string(screen, selected.min, display->black, selected.min, font, dir[ncol*y+x].name);
//	drawbottom(smprint("%R", selected));
//	drawbottom(dir[p].name);
	drawbottom(smprint("%d %d %d %P %s\n", x, y, (ncol*y)+x, Pt(screen->r.min.x+x*maxwid,screen->r.min.y+y*font->height), dir[ncol*y+x].name));
}

void
select4(void)
{
	int i, x, y, p;
	Point min, max;
	Rectangle sel;

	for(y = screen->r.min.y; y + font->height < m->xy.y; y += font->height)
		;
	for(x = screen->r.min.x; x + maxwid < m->xy.x; x += maxwid)
		;
	min = Pt(x,y);
	max = Pt(x+maxwid, y+font->height);
	x = (x-screen->r.min.x)/maxwid;
	y = (y-screen->r.min.y)/font->height;
	p = nline*x+y;
	sel = Rpt(min,max);
	if(p >= nfile || y >= nline || x >= ncol){
		drawbottom(smprint("nil %R %P", selected, gridmax));
		//drawbottom("nil");
		return;
	}
	draw(screen, sel, grey, nil, ZP);
	string(screen, sel.min, display->black, sel.min, font, dir[p].name);
	drawbottom(smprint("%d %d %d %d %P %P %s", x, y, x*y, p, min, max, dir[p].name));
	//drawbottom(smprint("%d %d %d %s", x, y, x*y, dir[x*y].name));
	strcpy(workdir, dir[p].name);
}


void
main(int argc, char *argv[])
{
//	int atnotify(int (*f)(void*, char*), int in)
	if(atnotify(interrupted, 1) < 0)
		sysfatal("notify: %r");
//	strcpy(workdir, ".");
	//fd = open(".", OREAD);
	fd = open(workdir, OREAD);
	if(fd < 0)
		sysfatal("open: %r");
	nfile = dirreadall(fd, &dir);
	if(nfile < 0)
		sysfatal("dirreadall: %r");
	qsort(dir, nfile, sizeof *dir, (int(*)(void*,void*))dirstrlencmp);
	tmp = smprint("%s ", dir[0].name);
	if(tmp == nil)
		sysfatal("smprint: %r");
	if(initdraw(0,0,"gls") < 0)
		sysfatal("initdraw: %r");
	maxwid = stringwidth(font, tmp);
	ncol = Dx(screen->r)/maxwid;
	strpt = screen->r.min;
	yellow = allocimagemix(display, DYellow, DWhite);
	grey = allocimagemix(display, DBlack, DWhite);
	red = allocimagemix(display, DRed, DWhite);
	green = allocimagemix(display, DGreen, DWhite);
	gridmax.x = screen->r.min.x + ncol * maxwid;
	gridmax.y = screen->r.min.y + nline * font->height;
	qsort(dir, nfile, sizeof *dir, (int(*)(void*,void*))dirnamecmp);
	for(i = nfile; i > ncol; i -= ncol)
		nline++;
	if(i > 0)
		nline++;
	if(yellow == nil)
		sysfatal("yellow: nil");
	for(i = 1; i < nfile+1; ++i){
		drawp = strpt;
		drawp.x += maxwid;
		drawp.y += font->height;
		if(dir[i-1].mode & DMDIR){
			draw(screen, Rpt(strpt,drawp), yellow, nil, ZP);
			tmp = smprint("%s/", dir[i-1].name);
			if(tmp == nil)
				sysfatal("smprint: %r");
			string(screen, strpt, display->black, strpt, font, tmp);
			free(tmp);
		}
		else if(dir[i-1].mode & 0100){
			draw(screen, Rpt(strpt,drawp), green, nil, ZP);
			string(screen, strpt, display->black, strpt, font, dir[i-1].name);
		}
		else{
			string(screen, strpt, display->black, strpt, font, dir[i-1].name);
		}
//		strpt.x += maxwid;
		strpt.y += font->height;
		if(i % nline == 0){
//			strpt.y += font->height;
			strpt.y = screen->r.min.y;
			strpt.x += maxwid;
		}
		flushimage(display,1);
	}
	if(inited == 0){
		einit(Emouse);
		inited = 1;
	}
	for(;;){
		event(&e);
		if(m->buttons == 1)
			select4();
	}
	sleep(100000);
}
