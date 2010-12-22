/*
	muLess v0.1 alpha
	hypertextual less - for muLinux
	(C) by Michele Andreoli, 1999 (GPL)
*/ 


/* #include "version.h" */
#define VERSION 266

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

/* app's */

int mono=FALSE;
int refr=FALSE, refr_time=1;
char *ifile=NULL;
char *prgname;
#define MAXLINES  10000 
FILE *in_fd;
int tty;
char buffer[512];
char *lines[MAXLINES];
char TopMsg[128]="";
char BarMsg[128]="";
char StatusMsg[128]="";
char Control[32]="";
char Name[16]="";
int child,pid;
char pid_arg[16];

int nlines,npages, slines,offset=0;

/* tab of strings */

struct Tab { int e; char *v; };


struct Link
{
int count;  /* count in row */	
int cur;  /* current in row */
int row;  /* abs pos. */
} c_link, n_link;

char current[128]; 	/* current link */

/* mode */

enum { LESS_MODE, LYNX_MODE, MUTT_MODE, REFR_MODE};
int mode=MUTT_MODE;

struct Tab mlist[]=
{
{LESS_MODE,"less"},
{LYNX_MODE,"hyperlink"},
{MUTT_MODE,"mutt"},
{REFR_MODE,"refr"},
{-1,NULL},
};

/* other */

enum { DO_PRINT, DO_COUNT};


/* functions */
 
static void Abort(char*);
static void finish(int);
static void quit(int);
static void load();
static void reload();
static void redraw();
int scan_line(int,int);
char *elem(); 

int next_link();
int previous_link();
int do_scroll();
int msg(char*);
int status(char*);
int print_bottom();
int do_callback(char);
int init_ncurses();

/* defines */

#define LOG(x) fprintf(stderr,"%s\n",x) 
#define XCENTER(x) (COLS -strlen(x))/2
#define MSG_ROW LINES-1 
#define HEAD_ROW 0
#define BAR_ROW LINES-2 
#define top(x) print_row(x,HEAD_ROW)
#define bar(x) print_row(x,BAR_ROW) 
#define msg1(f,x) sprintf(buffer,f,x); msg(buffer) 
#define msg2(f,x,y) sprintf(buffer,f,x,y); msg(buffer) 
#define msg3(f,x,y,z) sprintf(buffer,f,x,y,z); msg(buffer) 

#define SIGN(x) (x>=0? (x>0) :-1)


/* colors */

#define WHITE_ON_BLUE 1
#define RED_ON_BLACK 2 
#define WHITE_ON_RED 3 
#define GREEN_ON_BLACK 4 

chtype LINK, SELECT, BAR, MSG, STATUS;


/* link's escape code */

#define SLINK 0x1 
#define ELINK 0x2 

/* top and bottom rows */
 
int print_row(char*s, int r)
{
char FMT[32], row[128];
/*
sprintf(FMT,"%%%dc%%s%%%dc",XCENTER(s),XCENTER(s));
sprintf(row,FMT,' ',s,' ');
*/

sprintf(row,"%-*.*s",COLS,COLS,s);

attrset( BAR );
mvaddstr(r, 0, row);
clrtoeol();
}

/* messages bar */

int msg(char *m)
{
attrset( MSG );
sprintf(buffer,"%.80s",m);
mvaddstr(MSG_ROW,0,buffer);
clrtoeol();
attrset(A_NORMAL);
refresh();
}

/* status bar (right side) */

int status(char *m)
{
char buf[128]="";
sprintf(buf,"%*s",COLS/2,m);
attrset( STATUS );
mvaddstr(MSG_ROW,COLS/2,buf);
/*
clrtoeol();
*/
attrset(A_NORMAL);
}


int usage()
{
printf("%s v0.%d (C) 1999 by M.Andreoli\n",prgname, VERSION);
printf("Usage:   %s [options] [file]\n", prgname);
printf("options:\n");
printf("\t%-15s%s","-h","this cruft\n");
printf("\t%-15s%s","-M","monochrome mode (same HAS_COLOR=FALSE)\n");
printf("\t%-15s%s","-l","less mode\n");
printf("\t%-15s%s","-m","mutt mode\n");
printf("\t%-15s%s","-x","lynx mode\n");
printf("\t%-15s%s","-r seconds","refresh mode (no keyb events)\n");
printf("\t%-15s%s","-e prog","control program [with -x or -m]\n");
printf("\t%-15s%s","-t top", "top bar title\n");
printf("\t%-15s%s","-b bottom","bottom bar menu\n");
printf("\t%-15s%s","-s status","initial status msg\n");
printf("\t%-15s%s","-n name","instance name\n");
 
exit(0); 
}

int main(int argc, char *argv[])
{
int y,np,rem;
int j,i,n;
int c;
char *s=argv[0];
char *v=NULL;

/* process pid */

pid=(int) getpid();
sprintf(pid_arg,"%d",pid);

/* ------------------- 
parse parameters
----------------------*/

/* prg name */

prgname=argv[0];

        while ( *s != '\0' ) {
                if ( *s++ == '/' )
                        prgname = s;
        }

i=1;

/* default mode */

if ( strcmp(prgname,"less" )==0 )
		mode=LESS_MODE;
else
		mode=MUTT_MODE;


/* colors */

if ( (v=getenv("HAS_COLOR")) != NULL )
	if ( strcmp(v,"FALSE")==0) mono=TRUE;
	
while (i<=argc-1)
{


if ( argv[i][0] == '-' )
	switch( argv[i][1] )
	{
	case 'n': { i++; strcpy(Name,argv[i]); break;}
	case 't': { i++; strcpy(TopMsg,argv[i]); break;}
	case 'b': { i++; strcpy(BarMsg,argv[i]); break;}
	case 's': { i++; strcpy(StatusMsg,argv[i]); break;}
	case 'e': { i++; strcpy(Control,argv[i]); break;}
	case 'l': { mode=LESS_MODE; break;}
	case 'x': { mode=LYNX_MODE; break;}
	case 'm': { mode=MUTT_MODE; break;}
	case 'M': { mono=TRUE; break;}
	case 'r': { mode=REFR_MODE; 
			refr=TRUE; i++; refr_time=atoi(argv[i]); break;}
	default: usage();
	}
else
	ifile=argv[i];
i++;
}


/* interrupts handlers */

    (void) signal(SIGINT,quit);      /* interrupts */
    (void) signal(SIGUSR1, reload);      /* interrupts */


/* init ncurses */

init_ncurses();
reload(0);


/* some defaults */

if ( StatusMsg[0]=='\0' )
        {
        sprintf(buffer,"cursor up/down, pag up/down to move.");
        msg(buffer);

        sprintf(buffer,"v0.%d, %s mode",VERSION,elem(mlist,mode) );
        status(buffer);
        }


/* refresh mode */

if ( refr)
	{
	strcpy(StatusMsg," ");
	for(;;) {
		/*
		nodelay(stdscr, TRUE);
		nocbreak();
		nonl();
		*/
		cbreak();
		timeout(50);
		c=getch();
		if (c=='q') finish(0); 
		do_callback('r'); reload(0); 
		sleep(refr_time); 
		}
	}

/* normal mode */

for(;;)
{
c=getch();

        switch(c) {
	case 'q':
		{
		do_callback(c);
		finish(0);
		break;
		} 
        case KEY_NPAGE:
		{
		n_link.row+=slines; if(!do_scroll(c)) n_link.row-=slines;
		redraw();
           	break;
		}
        case KEY_PPAGE:
		{
		
		n_link.row-=slines; if (!do_scroll(c)) n_link.row+=slines;
		
		redraw();
           	break;
		}
        case KEY_DOWN:
                {
		if (next_link(+1))  do_scroll(c) ; redraw(); 
		break;
                }
        case KEY_UP:
                {
		if (next_link(-1)) do_scroll(c); redraw(); 
		break;
                }
	default:
		{
		do_callback(c);
		}
        }
}

finish(0);
}


/* 
interrupt handlers 
*/

/* stop program */

static void finish(int sig)
{
	msg(" ");
	endwin();
	printf("%.60s","bye!\n");
	exit(0);
/*
	exit(sig != 0);
*/
}

/* fatal error */

static void Abort(char *m)
{
	endwin();
	printf("%s: %s, Abort.\n",prgname,m);
	exit(1);
}

/* control-C */

static void quit(int sig)
{
int c;

if (refr) finish(0);

msg("quit really(y/n)?");
move(MSG_ROW,18); refresh(); 
echo(); 
c=getch();
noecho();

switch(c)
{
case 'y':
case 'Y':
	{
	finish(0); break;
	}
default:
	{
	msg("Excellent!"); refresh();
	}
}

}

/* load input file */

static void load()
{
char id;
int n,i,j;
int c;

if (ifile!=NULL) 
	in_fd=fopen(ifile,"r");
else
	in_fd=stdin;

if (in_fd==NULL)
	{
	Abort("missing input file");
	}

/* read info */

#define SKIP_NL fscanf(in_fd,"%*c")

while ( fscanf(in_fd,":%c:",&id) > 0   )
switch(id)
	{
	case 'T':
		 {fscanf(in_fd,"%[^\n]",TopMsg);
		 SKIP_NL;
		 break;}
	case 'B':
		{fscanf(in_fd,"%[^\n]",BarMsg);
		 SKIP_NL;
		break;}
	case 'S':
		{fscanf(in_fd,"%[^\n]",StatusMsg);
		 SKIP_NL;
		break;}
	}

/* read rows */

buffer[0]='\0';

nlines=0; j=COLS;

while( (c=fgetc(in_fd)) !=EOF )
{
	if ( (j==COLS) || (c=='\n') )
	{
	nlines++; lines[nlines]=(char*)malloc(COLS+1);
	lines[nlines][0]='\0';
	j=0;
	}
	if ( (c!='\n') && (c!='\r') ) 
		{
		lines[nlines][j]=c; 
		lines[nlines][j+1]='\0'; 
		j++;}

}
if (nlines>0) nlines--;

fclose(in_fd);

if ( in_fd==stdin )
{
	tty=open("/dev/tty",O_RDONLY);
	if (tty<0)
	{ fprintf(stderr,"%s: cannot open tty.",prgname), exit(1);}
	dup(tty);
}

/* some initialization */

slines=LINES-3;
npages=(nlines - nlines%slines)/slines +1 ;

/*
offset=0;
c_link.row=0;
c_link.cur=0;
c_link.count=0;
*/
}

/* SIGUSR1 (-10) handler */

/* Reload */

static void reload(int sig)
{
int rc;

erase();
load();


/* some defaults */

offset=0;
switch (mode)
{
case REFR_MODE:
case MUTT_MODE:
if(TopMsg[0]=='\0') sprintf(TopMsg,"%s", ifile==NULL?"(stdin)":ifile);
if(BarMsg[0]=='\0') strcpy(BarMsg,"q for quit");
        c_link.row=1;
        c_link.cur=0;
        c_link.count=0;
        break;
case LYNX_MODE:
	/*
        c_link.row=0;
	*/
        c_link.row=1;
        c_link.cur=0;
        c_link.count=0;
}

/* default actions */

switch (mode)
{
/*
case MUTT_MODE:
*/
case LYNX_MODE:
        if (next_link(+1) ) 
		{
		if (n_link.row < slines) do_scroll();
		}
        break;
}

redraw();

if (sig!=0)
{
	waitpid(child, &rc,0);
	signal(SIGUSR1,reload);
}

}

/* Redraw */

static void redraw()
{
int i;

if (TopMsg[0]!='\0') top(TopMsg);
if (BarMsg[0]!='\0') bar(BarMsg);
if (StatusMsg[0]!='\0') msg(StatusMsg);

attrset(A_NORMAL);

for (i=1; (i<=slines) && (i+offset<=nlines); i++)
	{
	move(i,0);
	scan_line(i+offset,DO_PRINT);
	}
for (; i<=slines; i++)
	{
	move(i,0);
	printw("%s",""); clrtoeol();
	}

refresh();
}

/* scanline: print and search link */

int scan_line(int i, int opt)
{
int printed=1;
int count=0;
int active=0;
int link_begin=0,link_end=0;
chtype ch,link_attr,line_attr;
char c;
int v=0,j;

v=0;
current[0]='\0';

link_attr=LINK;
line_attr=A_NORMAL;

switch(mode)
{
case REFR_MODE:
case MUTT_MODE:
	if ( i== c_link.row ) {line_attr=SELECT; break;}
case LYNX_MODE:
	if ( i== c_link.row ) {link_attr=SELECT; break;}
case LESS_MODE:
	{ break;}
default:
	{ LOG("scan_line: unk mode"); exit(1);}
}

	clrtoeol();


	/* put single char and fill row with blanks */

	for (j=0; printed<=COLS; j++)
	{
		c=*(lines[i]+j);


		if (c==0) break;
		if (c==SLINK) 
			 {link_begin=TRUE ; v=0; count++; continue;} 

		if (c==ELINK) 
			 {link_end=link_begin?TRUE:FALSE;continue;}

		ch=c|line_attr;
			
		active=(c_link.cur==count) \
                && (c_link.cur!=0) \
                && (link_attr==SELECT) \
			;

		if (link_begin)
		{
			ch=c|LINK;
			if (active)
				{
				ch=c|SELECT;
				current[v]=c; current[v+1]='\0';
                                v++;
				}
			if (line_attr==SELECT)
				ch=c|SELECT;
		}

		if (link_end)
		{
			if(active) { current[v-1]='\0'; v=0;}

		ch=c|line_attr; 
		link_begin=FALSE; link_end=FALSE;
		}


		/* if (c == '\n') continue */;
		if ( (opt==DO_PRINT)  && c!='\n') 
				if ( (ch & A_CHARTEXT) < 128 )
						addch(ch);
				else
					addch( (ch & A_CHARTEXT) | line_attr);

	/* printed count */	
	printed++;
	if (c=='\t') printed+=7;
	} /* for */

	for (; printed<=COLS; printed++)
		if (opt==DO_PRINT) addch(' '|line_attr);

	/* attrset(A_NORMAL); */
	return count;
}


/* NextLink */

int next_link(int dir)
{
int i,count=0;
int in_row=0;
int t;

int row;

switch(mode)
{
case LESS_MODE:
case MUTT_MODE:
                {
		row=c_link.row+dir;
		if ( (row > nlines) || (row<1) )
        		{
       			 printf("\a");
       			 sprintf(StatusMsg,"%s","no more");
       			 return (FALSE);
       			 }
		n_link.row=row;
		return(TRUE);
                }
}

/* search in row */

t=c_link.cur + dir;

if ( (t>=1) && (t<=c_link.count)  )
{
	in_row=TRUE ;
}

if ( in_row )
	{
	n_link.row=c_link.row;
	n_link.cur=c_link.cur+dir;
	n_link.count=c_link.count;
	return (TRUE);
	}

/* forward/backward search */

for (i=c_link.row + dir ; (i<=nlines) && (i>=1); i+=dir)
{
	count=scan_line(i,DO_COUNT);
	if (count>0)
		{n_link.count=count;n_link.row=i;
		if (dir>0) n_link.cur=1; else n_link.cur=count;
		return(TRUE);
		}
}
/*
n_link.count=0;
n_link.cur=0;
*/
return(FALSE);
}

/* extract elem in Tab */

char *elem(struct Tab t[], int p)
{
int i;

for (i=0; t[i].v != NULL; i++)
	if ( p==t[i].e) return t[i].v;

return NULL;

}

/* DoScroll */

int do_scroll(int key)
{
int d,shift;
int np,rem;
int y;

	y=n_link.row-1;

/* overflow */

if ( (n_link.row > nlines) || (n_link.row<1) )
	{
	/*
	printf("\a");
	sprintf(StatusMsg,"%s","no more");
	*/
	return (FALSE);
	}

/* n. of pages and remainder */

rem=y % slines;
np=(y-rem)/slines ;

/* ok, operate */


switch(mode)
{
case LYNX_MODE:
	if (n_link.count==0  && (key!=KEY_PPAGE  && key!=KEY_NPAGE) ) 
		{
		printf("\a");
		sprintf(StatusMsg,"%s","no more");
		return(FALSE);
		}	
case MUTT_MODE:
	{offset=np*slines; break;}
case REFR_MODE:
case LESS_MODE:
	{ offset=n_link.row; break;}
}

c_link.row=n_link.row;
c_link.cur=n_link.cur;
c_link.count=n_link.count;
		

print_bottom();
return(TRUE);
}


int print_bottom()
{
int rem,np;

scan_line(c_link.row,DO_COUNT);
rem=c_link.row % slines;
np=(c_link.row-rem)/slines ;

buffer[0]='\0';

switch(mode)
{
case LESS_MODE:
{
sprintf(buffer,"%s: page %d/%d, line %d/%d",\
ifile==NULL?"(stdin)":ifile,np+1,npages,
c_link.row,nlines);
break;
}
case MUTT_MODE:
sprintf(buffer,"page %d/%d, line %d/%d",\
np+1,npages,
c_link.row,nlines);
break;
case LYNX_MODE:
{
sprintf(buffer,"%s",\
current);
break;
}
case REFR_MODE: break;
}

strcpy(StatusMsg,buffer);
clrtoeol();
refresh(); 
}

/* docallback */

int do_callback ( char c)
{
int i=0;
int rc;
char *name=NULL;

msg("");
refresh();

switch(mode)
{
case LESS_MODE:
	        {
		sprintf(buffer,"%c %d %s",c,0,ifile==NULL?"(stdin)":ifile);
		break;
                }
case REFR_MODE:
case MUTT_MODE:
	        {
		sprintf(buffer,"%c %d %s",c,c_link.row,lines[c_link.row]);
		break;
                }
case LYNX_MODE:
		{
                scan_line(c_link.row,DO_COUNT);
		sprintf(buffer,"%c %d %s",c,c_link.row,current);
		}
}

/* instance name */

if ( Name[0]!='\0') name=Name;


if (Control[0]!='\0')
{
	child=fork();

	if ( child ) {
	waitpid(child,&rc,0);
	endwin();
	init_ncurses();
	redraw();
	;}
		else
		{
		;
		endwin();
       		execl (Control,Control,buffer,pid_arg,name,NULL);
		}
}

	else { msg("key unbound -- q for quit");}


}


int init_ncurses()
{
/* ncurses init */

    initscr();      /* initialize  */
    keypad(stdscr, TRUE);
    nonl();         /* no NL->CR/NL on output */
    noecho();       /* don't echo input */
    meta(stdscr,TRUE);

if (refr) nocbreak();
	else cbreak();

/* apps init */

if (has_colors() && (!mono) )
{
start_color();

init_pair(WHITE_ON_BLUE,COLOR_WHITE,COLOR_BLUE);
init_pair(WHITE_ON_RED,COLOR_WHITE,COLOR_RED);
init_pair(RED_ON_BLACK,COLOR_RED,COLOR_BLACK);
init_pair(GREEN_ON_BLACK,COLOR_GREEN,COLOR_BLACK);

	LINK=COLOR_PAIR(GREEN_ON_BLACK);
	SELECT=COLOR_PAIR(WHITE_ON_RED);
	BAR=COLOR_PAIR(WHITE_ON_BLUE)|A_BOLD;
	MSG=COLOR_PAIR(RED_ON_BLACK)| A_BOLD;
	STATUS=COLOR_PAIR(GREEN_ON_BLACK);
}
else
	{
	LINK=A_UNDERLINE;
	SELECT=A_REVERSE;
	BAR=A_REVERSE;
	MSG=A_BOLD;
	STATUS=A_BOLD;
	}


}
