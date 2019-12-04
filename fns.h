typedef	void	Thread(void*);
typedef	int	Cmp(void*,void*);	/* shut up kencc */
typedef	int	Dircmp(Dir*,Dir*);

void	select(void);
void	redraw(void);
void	drawbottom(char*);

Thread	mousethread;
Thread	resizethread;
Thread	kbdthread;
Thread	drawthread;

Dircmp	dirstrlencmp;
Dircmp	dirnamecmp;
