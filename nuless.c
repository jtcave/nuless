/*
  nuless -- programmable pager
  Original file copyright 1999 Michele Andreoli
  Subsequent changes copyright 2010 James Cave

  nuless is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  
  nuless is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with nuless.  If not, see <http://www.gnu.org/licenses/>.

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

/**** Globals ****/

/* TODO: split this entire section into a header */

/* Is nuless running in monochrome mode? */
int mono=FALSE;

/* Flag for refresh mode, as well as the interval */
int refr=FALSE, refr_time=1;

/* Input file name */
char *ifile=NULL;

/* name of this program */
char *prgname;

/* input file handle */
FILE *in_fd;

/* file descriptor for /dev/tty */
int tty;

/* general purpose character buffer */
char buffer[512];

/* stupid hard-coded limit! */
#define MAXLINES  50000 

/* each line of the file */
char *lines[MAXLINES];

/* messages */
char TopMsg[128]="";
char BarMsg[128]="";
char StatusMsg[128]="";

/* name of the control program */
char Control[32]="";

/* instance name */
char Name[16]="";

/* child and self PIDs */
int child,pid;

/* buffer for string-formatted PID, used in event strings */
char pid_arg[16];

/* number of lines and pages in the file */
int nlines,npages;

/* screen lines */
int slines;

/* TODO: document */
int offset=0;

/* Mapping of integer -> string
   cf. char *elem(struct tab*, int)
*/
struct Tab
{
  int k;
  char *v;
};

/* stores data for hyperlinks */
struct Link
{
  /* The number of links in the same row as this one. */
  int count;
  /* Which of those links is this one? XXX: may not be accurate */
  int cur;
  /* Which row contains this link? */
  int row;  /* abs pos. */
} c_link, n_link;

char current[128]; 	/* current link */

/* mode codes */

enum { LESS_MODE, LYNX_MODE, MUTT_MODE, REFR_MODE};

/* mutt is the default mode */
int mode=MUTT_MODE;

/* map modes to human-readable strings */
struct Tab mlist[]=
  {
    {LESS_MODE,"less"},
    {LYNX_MODE,"hyperlink"},
    {MUTT_MODE,"mutt"},
    {REFR_MODE,"refr"},
    {-1,NULL},
  };


/* actions to do in print_row */

enum { DO_PRINT, DO_COUNT};


/** function prototypes **/
 
/* Termination */
static void Abort(char*);
static void finish(int);
static void quit(int);

/* file loading */
static void load(void);
static void reload(int);

/* hyperlink manipulation */
int next_link(int);

/* display routines */
void msg(char*);
void status(char*);
void print_bottom(void);
void print_row(char*, int);
void do_callback(char);

/* rendering (high level display routines */
static void redraw(void);
int do_scroll(int);
int scan_line(int,int);
char *elem(struct Tab *, int); 

/* high-level control */
void init_ncurses(void);
void loop_refresh(void);
void loop_event(void);
void usage(void);
void arguments(int, char**);

/* macros */
/* TODO: document these. */
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


/* color schemes */

#define WHITE_ON_BLUE 1
#define RED_ON_BLACK 2 
#define WHITE_ON_RED 3 
#define GREEN_ON_BLACK 4 

/* formatting attributes for different purposes */
chtype LINK, SELECT, BAR, MSG, STATUS;


/* escape code for links */

#define SLINK 0x1 
#define ELINK 0x2 




/**** Begin procedure definitions ****/



/** display routines **/

/* top and bottom rows */

void print_row(char *s, int r) {
  char row[1024];
  /*
    char FMT[32];
    sprintf(FMT,"%%%dc%%s%%%dc",XCENTER(s),XCENTER(s));
    sprintf(row,FMT,' ',s,' ');
  */

  sprintf(row,"%-*.*s",COLS,COLS,s);

  attrset( BAR );
  mvaddstr(r, 0, row);
  clrtoeol();
}

/* messages bar */

void msg(char *m)
{
  attrset( MSG );
  sprintf(buffer,"%.80s",m);
  mvaddstr(MSG_ROW,0,buffer);
  clrtoeol();
  attrset(A_NORMAL);
  refresh();
}

/* status bar (right side) */

void status(char *m)
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

/** startup **/

/* usage message */

void usage() {
  printf("%s v0.%d\n",prgname, VERSION);
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

/* parse arguments--warning, colossus ahead */

void arguments(int argc, char** argv) {
  char *s=argv[0];
  char *v=NULL;
  /* get the name of this program's binary */

  prgname=argv[0];
  while ( *s != '\0' ) {
    if ( *s++ == '/' )
      prgname = s;
  }

  int i=1;

  /* figure out the default mode */

  if ( strcmp(prgname,"less" )==0 )
    mode=LESS_MODE;
  else
    mode=MUTT_MODE;


  /* colors */
  if ( (v=getenv("HAS_COLOR")) != NULL )
    if ( strcmp(v,"FALSE")==0) mono=TRUE;

  /* parse options */
  while (i<=argc-1)
    {
      if ( argv[i][0] == '-' )
	switch( argv[i][1] )
	  {
	  case 'n':
	    /* -n: set instance name */
	    i++;
	    strcpy(Name,argv[i]);
	    break;
	  
	  case 't':
	    /* -t: set top row message */
	    i++;
	    strcpy(TopMsg,argv[i]);
	    break;
	  
	  case 'b':
	    /* -b: set bottom bar message (not status message) */
	    i++;
	    strcpy(BarMsg,argv[i]);
	    break;
	  
	  case 's':
	    /* -s: set initial status message */
	    i++;
	    strcpy(StatusMsg,argv[i]);
	    break;
	  
	  case 'e':
	    /* -e: set control program */
	    i++;
	    strcpy(Control,argv[i]);
	    break;
	  
	  /* -l, -x, -m: set UI modes */
	  case 'l': mode=LESS_MODE; break;
	  case 'x': mode=LYNX_MODE; break;
	  case 'm': mode=MUTT_MODE; break;

	  case 'M': 
	    /* -M: monochrome mode */
	    mono=TRUE;
	    break;
	  
	  case 'r': 
	    /* -r: refresh mode */
	    mode=REFR_MODE; 
	    refr=TRUE;
	    i++;
	    refr_time=atoi(argv[i]);
	    break;
	  
	  /* -h or invalid: show usage message */
	  case 'h': default: usage();
	  
	  } /* end switch */
      else {
	ifile=argv[i];  /* set input file */
      }
      i++;
    }
}

int main(int argc, char *argv[]) {
  /* TODO: document */
  /*int np,rem;*/
  /*int j,i,n;*/


  /* get process pid, and format it for later */

  pid=(int) getpid();
  sprintf(pid_arg,"%d",pid);

  /* now to parse parameters */
	
  arguments(argc, argv);


  /* install signal handlers */

  (void) signal(SIGINT,quit);        /* quit on ^C */
  (void) signal(SIGUSR1, reload);    /* reload on USR1 */


  /* init ncurses */

  init_ncurses();
  reload(0);


  /* set some defaults */

  if ( StatusMsg[0]=='\0' )
    {
      sprintf(buffer,"cursor up/down, pag up/down to move.");
      msg(buffer);

      sprintf(buffer,"v0.%d, %s mode",VERSION,elem(mlist,mode) );
      status(buffer);
    }


  /* pick a loop*/
  if (refr)
    loop_refresh();
  else
    loop_event();

  /* shouldn't fall through here */
  finish(7);
  /* definitely not here, either */
  return 7;

} /* end int main(int, char**) */


/** interrupt handlers and data loading **/


/* quit program */

static void finish(int sig) {
  msg(" ");
  endwin();
  printf("%.60s","bye!\n");
  exit(0);
  /*
    exit(sig != 0);
  */
}

/* quit on fatal error */

static void Abort(char *m) {
  endwin();
  printf("%s: %s, Abort.\n",prgname,m);
  exit(1);
}

/* ask user to quit on q or ^C */

static void quit(int sig) {
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
#define SKIP_NL fscanf(in_fd,"%*c")
static void load()
{
  if (ifile!=NULL) 
    in_fd=fopen(ifile,"r");
  else
    in_fd=stdin;

  if (in_fd==NULL) {
      Abort("missing input file");
    }

  /* read the info headers */
  char id;
  while ( fscanf(in_fd,":%c:",&id) > 0   ) {
    switch(id)
      {
      case 'T':
	fscanf(in_fd,"%[^\n]",TopMsg);
	SKIP_NL;
	break;
      case 'B':
	fscanf(in_fd,"%[^\n]",BarMsg);
	SKIP_NL;
	break;
      case 'S':
	fscanf(in_fd,"%[^\n]",StatusMsg);
	SKIP_NL;
	break;
      }
  }

  /* read rows */

  buffer[0]='\0';
  nlines=0;
  int j=COLS;
  int c;
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
      if (tty<0) {
	fprintf(stderr,"%s: cannot open tty.", prgname);
	exit(1);
      }
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
/* Reload the file */
static void reload(int sig) {
  int rc;

  erase();
  load();


  /* apply default settings */

  offset=0;
  switch (mode)
    {
    case REFR_MODE:
    case MUTT_MODE:
      if(TopMsg[0]=='\0') {
	sprintf(TopMsg,"%s", ifile==NULL?"(stdin)":ifile);
      }
      if(BarMsg[0]=='\0') {
	strcpy(BarMsg,"q for quit");
      }
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
      break;
    }

  /* default actions */

  switch (mode)
    {
      /*
	case MUTT_MODE:
      */
    case LYNX_MODE:
      if (next_link(+1) ) {
	/* XXX: do_scroll() was called with no arguments before */
	if (n_link.row < slines) do_scroll(0);
      }
      break;
    }

  redraw();

  if (sig!=0) {
    waitpid(child, &rc,0);
    signal(SIGUSR1,reload);
  }
}

/** more display routines **/

/* Redraw the screen */
static void redraw() {
  /* print the messages if they exist*/
  if (TopMsg[0]!='\0') top(TopMsg);
  if (BarMsg[0]!='\0') bar(BarMsg);
  if (StatusMsg[0]!='\0') msg(StatusMsg);

  attrset(A_NORMAL);

  int i;
  for (i=1; (i<=slines) && (i+offset<=nlines); i++) {
    /* redraw this line of text */
    move(i,0);
    scan_line(i+offset,DO_PRINT);
  }
  for (; i<=slines; i++) {
    /* erase blank lines */
    move(i,0);
    printw("%s","");
    clrtoeol();
  }

  refresh();
}

/* scanline: print a line and format links */
int scan_line(int i, int opt) {
  int v=0;
  current[0]='\0';

  /* set attributes */
  chtype link_attr=LINK;
  chtype line_attr=A_NORMAL;
  switch(mode)
  {
    case REFR_MODE:
    case MUTT_MODE:
      if ( i == c_link.row ) {
	line_attr=SELECT;
	break;
      }
    case LYNX_MODE:
      if ( i == c_link.row ) {
	link_attr=SELECT;
	break;
      }
    case LESS_MODE:
      break;
    default:
      LOG("scan_line: undefined mode");
      exit(1);
  }
  clrtoeol();


  /* start printing the line */
  int printed = 1;
  int count = 0;
  int j;
  for (j=0; printed<=COLS; j++) {
    if (nlines == 0) break;
    int count=0;
    char c=*(lines[i]+j);
    int link_begin=0,link_end=0;
    int active=0;

    /* quit printing if there aren't any characters left */
    if (c==0) break;
    if (c==SLINK) {
      /* This is the start of a link. Don't print it. Do note it. */
      link_begin=TRUE;
      v = 0;
      count++;
      continue;
    } 

    if (c==ELINK) {
      /* This is the end of a link. Don't print it. Do note it. */
      link_end = link_begin ? TRUE : FALSE;
      continue;
    }

    /* apply attributes */
    chtype ch = c|line_attr;
			
    /* is this character part of the active link? */
    active=(c_link.cur==count) \
      && (c_link.cur!=0) \
      && (link_attr==SELECT) \
      ;

    /* If there's a link going on, render it as a link. */
    if (link_begin) {
      ch=c|LINK;
      if (active) {
	/* active links look different than inactive links */
	ch=c|SELECT;
	current[v]=c; current[v+1]='\0';
	v++;
      }
      if (line_attr==SELECT) {
	ch=c|SELECT;
      }
    }

    if (link_end) {
      if(active) {
	current[v-1]='\0';
	v=0;
      }

      ch=c|line_attr; 
      /* reset! */
      link_begin=FALSE; link_end=FALSE;
    }


    if ( (opt==DO_PRINT)  && c!='\n') {
      /* if we're printing, then print the rendered character */
      if ( (ch & A_CHARTEXT) < 128 ) {
	addch(ch);
      }
      else {
	addch( (ch & A_CHARTEXT) | line_attr);
      }
    }


    /* printed count */	
    printed++;
    /* tabs are 8 spaces */
    if (c=='\t') printed+=7;
  } /* for */

  /* fill rest of line with blanks */
  for (; printed<=COLS; printed++)
    if (opt==DO_PRINT)
      addch(' '|line_attr);

  /* attrset(A_NORMAL); */
  return count;
}



/* In lynx mode. searches for the next hyperlink. The argument determines how
   far to jump; negative arguments search backwards.

   In less or mutt mode, just make sure that there exists a line in that
   direction.

   Returns TRUE or FALSE depending on whether the link/line was found.
*/

int next_link(int dir) {
  /* If in less or mutt mode, make sure there's a line there. */
  /* TODO: should this include refresh mode, or would that even make sense? */
  int row;
  switch(mode)
    {
    case LESS_MODE:
    case MUTT_MODE:
	row = c_link.row + dir;
	if ( (row > nlines) || (row<1) ) {
	  printf("\a");
	  sprintf(StatusMsg,"%s","no more");
	  return (FALSE);
	}
	n_link.row = row;
	return (TRUE);
    }
    

  /* Lynx mode. Search for the hyperlink in the row */

  int in_row = FALSE;
  int t = c_link.cur + dir;

  if ( (t>=1) && (t<=c_link.count) ) {
    /* Hit. */
    /* TODO: optimization? */
    in_row = TRUE;
  }

  if (in_row) {
    /* The next link is the current link. */
    n_link.row = c_link.row;
    n_link.cur = c_link.cur + dir;
    n_link.count = c_link.count;
    return (TRUE);
  }

  /* Search in different rows. */

  int count = 0;
  int i;
  /* For each row... */
  for (i = c_link.row + dir; (i<=nlines) && (i>=1); i+=dir) {
    /* Does this row even have links in it? */
    count = scan_line(i, DO_COUNT);
    if (count > 0) {
      /* Yes, it does; we have a hit. */
      n_link.count = count;
      n_link.row = i;
      if (dir>0)
	n_link.cur = 1;
      else
	n_link.cur = count;
      return(TRUE);
    }
  }
  /*
    n_link.count=0;
    n_link.cur=0;
  */

  /* Link not found. */
  return(FALSE);
}

/* Take an array of Tab mappings and a key-value,
   and find the value associated with the key
*/
char *elem(struct Tab table[], int p) {
  int i;
  for (i=0; table[i].v != NULL; i++)
    if ( p == table[i].k)
      return table[i].v;

  /* No such */
  return NULL;
}

/* TODO: document */
int do_scroll(int key)
{
  /*int d,shift;*/

  int y=n_link.row-1;

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

  int rem=y % slines;
  int np=(y-rem)/slines ;

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

/* TODO: document */
void print_bottom() {
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

/* Send a key binding to the control program, if one is present */
void do_callback(char c) {
  /*int i=0;*/
  int rc;
  char *name=NULL;

  /* If there's no control program, then give a message and return */
  if (Control[0] == '\0') {
    msg("key unbound -- q for quit");
    return;
  }

  msg("");
  refresh();

  /* Construct the event string */
  switch(mode)
    {
    case LESS_MODE:
      sprintf(buffer,"%c %d %s",c,0,ifile==NULL?"(stdin)":ifile);
      break;
      
    case REFR_MODE:
    case MUTT_MODE:
      sprintf(buffer,"%c %d %s",c,c_link.row,lines[c_link.row]);
      break;
      
    case LYNX_MODE:
      scan_line(c_link.row,DO_COUNT);
      sprintf(buffer,"%c %d %s",c,c_link.row,current);
      break;
  }

  /* set instance name */

  if ( Name[0]!='\0') name=Name;

  /* fork and exec the control program */
  child = fork();

  if ( child ) {
    /* I'm the parent. wait for the child and restart the display */
    waitpid(child,&rc,0);
    endwin();
    init_ncurses();
    redraw();
  }
  else {
    /* I'm the child. exec my control program. */
    endwin();
    execl (Control,Control,buffer,pid_arg,name,NULL);
  }
}


/* Initialize the screen */
void init_ncurses() {
  /* ncurses init */

  initscr();      /* initialize  */
  keypad(stdscr, TRUE);
  nonl();         /* no NL->CR/NL on output */
  noecho();       /* don't echo input */
  meta(stdscr,TRUE);

  /* allow cbreak in event mode only */
  if (refr) nocbreak();
  else cbreak();

  /* apps init */

  if (has_colors() && (!mono) )  {
    /* init color styles */
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
  else {
    LINK=A_UNDERLINE;
    SELECT=A_REVERSE;
    BAR=A_REVERSE;
    MSG=A_BOLD;
    STATUS=A_BOLD;
  }
}

/* Main loop for the refresh mode. */
void loop_refresh() {
  int c;
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

/* Main loop for event-driven modes (lynx, less, and mutt modes) */
void loop_event() {
  int c;
  for(;;) {
    c=getch();

    switch(c) {
    case 'q':
      do_callback(c);
      finish(0);
      break;

    case KEY_NPAGE:
      n_link.row += slines;
      if(!do_scroll(c))
	n_link.row -= slines;
      redraw();
      break;

    case KEY_PPAGE:
      n_link.row -= slines;
      if (!do_scroll(c))
	n_link.row += slines;
      redraw();
      break;

    case KEY_DOWN:
      if (next_link(+1))
	do_scroll(c);
      redraw(); 
      break;

    case KEY_UP:
      if (next_link(-1))
	do_scroll(c);
      redraw(); 
      break;

    default:
      do_callback(c);
    }
  }
  finish(0);
}
