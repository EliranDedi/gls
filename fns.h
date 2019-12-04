typedef	void	Thread(void*);
typedef	int	Cmp(void*,void*);	/* shut up kencc */
typedef	int	Dircmp(Dir*,Dir*);

void	select(void);		/* regular functions */
void	redraw(void);
void	drawbottom(char*);
void	newdata(void);

Thread	mousethread;		/* thread functions */
Thread	resizethread;
Thread	kbdthread;
Thread	drawthread;

Dircmp	dirstrlencmp;		/* for qsort */
Dircmp	dirnamecmp;
