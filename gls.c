#include <u.h>
#include <libc.h>
#include <draw.h>
#include <thread.h>
#include <mouse.h>
#include <keyboard.h>
#include "fns.h"
#include "dat.h"

/*
DESCRIPTION
	gls - graphical file browser
NOTES

TODO
	everything
*/

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
		select();
	goto Loop;
}

void
newdata(void)
{
	if(dir != nil)
		free(dir);
	if(fd > 0)
		close(fd);
	dir = nil;
	fd = -1;
//	print("opening %s\n", workdir);
	fd = open(workdir, OREAD);
	if(fd < 0)
		sysfatal("open: %r");
	nfile = dirreadall(fd, &dir);
	if(nfile < 0)
		sysfatal("dirreadall: %r");
	qsort(dir, nfile, sizeof *dir, (Cmp*)dirstrlencmp);
	maxwid = stringwidth(font, dir[0].name) + stringwidth(font, " ");
	if(dir[0].mode & DMDIR)
		maxwid += stringwidth(font, "/");
	ncol = Dx(screen->r)/maxwid;
	nline = 0;
	strpt = screen->r.min;
	qsort(dir, nfile, sizeof *dir, (Cmp*)dirnamecmp);
	for(i = nfile; i > ncol; i -= ncol)
		nline++;
	if(i > 0)
		nline++;
	redraw();
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
	case 2:
		redraw();
	}
	flushimage(display, 1);
	ul = 0;
	goto Loop;
}

void
redraw(void)
{
	draw(screen, screen->r, display->white, nil, ZP);
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
select(void)
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
		drawbottom(smprint("out of boundaries sel=%R gridmax=%P", sel, gridmax));
		goto Sendul;
	}
	draw(screen, sel, grey, nil, ZP);
	string(screen, sel.min, display->black, sel.min, font, dir[p].name);
	drawbottom(smprint("x=%d y=%d xy=%d dir[%d] sel=%R name=%s", x, y, x*y, p, sel, dir[p].name));
	if(dir[p].mode & DMDIR){
		snprint(workdir, sizeof workdir, "%s/%s", workdir, dir[p].name);
		if(chdir(workdir) < 0)
			sysfatal("chdir: %r");
		newdata();
	}
Sendul:
	sendul(drawchan, 1);
}

void
threadmain(int argc, char *argv[])
{
	getwd(workdir, sizeof workdir);
	fd = open(workdir, OREAD);
	if(fd < 0)
		sysfatal("open: %r");
	nfile = dirreadall(fd, &dir);
	if(nfile < 0)
		sysfatal("dirreadall: %r");
	qsort(dir, nfile, sizeof *dir, (Cmp*)dirstrlencmp);
	if(initdraw(0,0,"gls") < 0)
		sysfatal("initdraw: %r");
	maxwid = stringwidth(font, dir[0].name) + stringwidth(font, " ");
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
	qsort(dir, nfile, sizeof *dir, (Cmp*)dirnamecmp);
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
