#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>

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
Mouse	*m;
Point	gridmax;
Channel	*drawchan;
Mousectl	*mctl;
Keyboardctl	*kctl;

void	select4(void);
void	redraw(void);

void
mousethread(void *v)
{
	Mousectl *mctl = v;
	Mouse mouse;
	threadsetname("mouse thread");
Loop:
	recv(mctl->c, &mouse);
	m = &mouse;
	if(m == nil)
		sysfatal("mthread mouse");
	if(mctl == nil)
		sysfatal("mthread mctl");
	if(m->buttons == 1)
		select4();
	goto Loop;
}

void
resizethread(void *v)
{
	Mousectl *mctl = v;
	
	threadsetname("resizethread");
Loop:				
	recvul(mctl->resizec);
	if(getwindow(display, Refnone) < 0)
		threadexitsall("getwindow: %r");
	sendul(drawchan, 2);
	goto Loop;
}

void
kbdthread(void *v)
{
	Keyboardctl *kbd = v;
	Rune r, rr;
	int nval;

	threadsetname("kbdthread");
Loop:
	recv(kctl->c, &r);
	switch(r){
	case Kdel: case Kesc: case 'q':
		threadexitsall("quit");
		break;
	}
	goto Loop;

}

void
drawthread(void *v)
{
	Image *i;
	ulong ul;

	flushimage(display, 1);
Loop:
	ul = recvul(drawchan);
	switch(ul){
	case 1:
		flushimage(display, 1);
		break;
	case 2:
		redraw();
		flushimage(display, 1);
	}
	ul = 0;
	goto Loop;
}

void
redraw(void)
{
	strpt = screen->r.min;
	for(i = 1; i < nfile+1; ++i){
		drawp = strpt;
		drawp.x += maxwid;
		drawp.y += font->height;
		if(dir[i-1].mode & DMDIR){
			draw(screen, Rpt(strpt,drawp), yellow, nil, ZP);
			drawp = string(screen, strpt, display->black, strpt, font, dir[i-1].name);
			string(screen, drawp, display->black, ZP, font, "/");
		}
		else if(dir[i-1].mode & 0100){
			draw(screen, Rpt(strpt,drawp), green, nil, ZP);
			string(screen, strpt, display->black, strpt, font, dir[i-1].name);
		}
		else{
			string(screen, strpt, display->black, strpt, font, dir[i-1].name);
		}
		strpt.y += font->height;
		if(i % nline == 0){
			strpt.y = screen->r.min.y;
			strpt.x += maxwid;
		}
	}
}

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
//	if(ptinrect(m->xy, Rpt(screen->r.min, gridmax)) == 0){
	if(p >= nfile || y >= nline || x >= ncol){
		drawbottom(smprint("nil %R %P", sel, gridmax));
		return;
	}
	draw(screen, sel, grey, nil, ZP);
	string(screen, sel.min, display->black, sel.min, font, dir[p].name);
	drawbottom(smprint("x=%d y=%d xy=%d dir[%d] sel=%R name=%s", x, y, x*y, p, sel, dir[p].name));
	strcpy(workdir, dir[p].name);
	sendul(drawchan, 1);
}

void
gfxsetup(void)
{
}

void
threadmain(int argc, char *argv[])
{
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
	if(dir[0].mode & DMDIR)
		maxwid += stringwidth(font, "/");
	ncol = Dx(screen->r)/maxwid;
	strpt = screen->r.min;
	yellow = allocimagemix(display, DYellow, DWhite);
	grey = allocimagemix(display, DBlack, DWhite);
	red = allocimagemix(display, DRed, DWhite);
	green = allocimagemix(display, DGreen, DWhite);
	gridmax.x = screen->r.min.x + ncol * maxwid;
	gridmax.y = screen->r.min.y + nline * font->height;
	qsort(dir, nfile, sizeof *dir, (int(*)(void*,void*))dirnamecmp);
	mctl = initmouse("/dev/mouse", nil);
	if(mctl == nil)
		sysfatal("initmouse: %r");
	kctl = initkeyboard("/dev/cons");
	if(kctl == nil)
		sysfatal("initmouse: %r");
	for(i = nfile; i > ncol; i -= ncol)
		nline++;
	if(i > 0)
		nline++;
	if(yellow == nil)
		sysfatal("yellow: nil");
	drawchan = chancreate(sizeof(ulong), 0);
	threadcreate(mousethread, mctl, 8192);
	threadcreate(kbdthread, kctl, 8192);
	threadcreate(resizethread, mctl, 8192);
	threadcreate(drawthread, nil, 8192);
	redraw();
	threadexits("main");
	sleep(100000);
}
