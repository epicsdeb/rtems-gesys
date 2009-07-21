/* $Id: term.c,v 1.12 2007/08/25 00:19:22 till Exp $ */

/* (non thread safe) helper to determine the terminal size */

/* Author: Till Straumann <strauman@slac.stanford.edu>, 2003/3 */

/* 
 * Helper to query an ANSI conformant terminal for its dimensions
 * using ESC-sequences.
 *
 * This file can be compiled in two different ways:
 *
 * A) producing an ordinary subroutine
 *
 * int queryTerminalSize(int infoLevel, int *width, int *height); 
 *
 * to be used for obtaining the terminal size. The routine
 * tries TIOCGWINSZ and if that fails, it tries the ANSI
 * escape sequence.
 *
 * B) if compiled with -DWINS_LINE_DISC, a call
 * 
 * int ansiTiocGwinszInstall(int disc);
 *
 * is provided which installs a special line discipline into
 * linesw[disc] and switches the terminal to use it. The
 * line discipline then implements TIOCGWINSZ by doing an
 * ANSI escape sequence. This way, any software can simply
 * do TIOCGWINSZ.
 *
 * Installing to 'disc == 0' switches back the tty's default
 * discipline, i.e. removes this one.
 *
 * NOTE: on RTEMS, the only slots available to the user are
 *       6 and 7.
 *
 * int queryTerminalSize(int infoLevel, int *width, int *height); 
 *
 * is still provided for convenience - it does a simple
 * ioctl(TIOCGWINSZ).
 *
 */

/*
 * Copyright 2002,2003, Stanford University and
 * 		Till Straumann <strauman@@slac.stanford.edu>
 * 
 * Stanford Notice
 * ***************
 * 
 * Acknowledgement of sponsorship
 * * * * * * * * * * * * * * * * *
 * This software was produced by the Stanford Linear Accelerator Center,
 * Stanford University, under Contract DE-AC03-76SFO0515 with the Department
 * of Energy.
 * 
 * Government disclaimer of liability
 * - - - - - - - - - - - - - - - - -
 * Neither the United States nor the United States Department of Energy,
 * nor any of their employees, makes any warranty, express or implied,
 * or assumes any legal liability or responsibility for the accuracy,
 * completeness, or usefulness of any data, apparatus, product, or process
 * disclosed, or represents that its use would not infringe privately
 * owned rights.
 * 
 * Stanford disclaimer of liability
 * - - - - - - - - - - - - - - - - -
 * Stanford University makes no representations or warranties, express or
 * implied, nor assumes any liability for the use of this software.
 * 
 * This product is subject to the EPICS open license
 * - - - - - - - - - - - - - - - - - - - - - - - - - 
 * Consult the LICENSE file or http://www.aps.anl.gov/epics/license/open.php
 * for more information.
 * 
 * Maintenance of notice
 * - - - - - - - - - - -
 * In the interest of clarity regarding the origin and status of this
 * software, Stanford University requests that any recipient of it maintain
 * this notice affixed to any distribution by the recipient that contains a
 * copy or derivative of this software.
 */
#ifdef __rtems__
#include <rtems.h>
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <assert.h>

#ifdef WINS_LINE_DISC
#include <rtems/libio.h>
#endif

#define  ESC 27
#define  MAXSTR 20

#ifdef WINS_LINE_DISC

#include <rtems/termiostypes.h>

#include "minversion.h"

#if RTEMS_ISMINVERSION(4,7,99)
#define linesw rtems_termios_linesw
#endif

static int
tty_ioctl_gwinsz(struct rtems_termios_tty *tty, rtems_libio_ioctl_args_t *args);

/* just provide a TIOCGWINSZ ioctl which tries to
 * ask the terminal for its size using ANSI ESC sequences
 */
static struct linesw ttyDiscGwinszAnsi = {
	/* open  */ 0,
	/* close */ 0,
	/* read  */ 0,
	/* write */ 0,
	/* rxint */ 0,
	/* start */ 0,
	/* ioctl */ tty_ioctl_gwinsz,
	/* modem */ 0
};


#define PERROR(arg)					do {} while(0)
#define MESSAGE(level,where,fmt...)	do {} while(0)
#else
#define PERROR(arg)  \
	if (infoLevel>0) \
		perror(arg)

#define MESSAGE(level,where,fmt...) \
	if (infoLevel>level) \
		fprintf(where,fmt)
#endif

#ifdef WINS_LINE_DISC
static int
my_do_write(rtems_libio_t *iop, char *buf, int l)
{
rtems_libio_rw_args_t arg;
	arg.iop	        = iop;
	arg.offset      = 0; /* should be unused */
	arg.buffer      = buf;
	arg.count       = l;
	/* hmm - could be that iop->flags are read-only - but termios doesn't seem
	 * to look at the flags anyways...
	 */
	arg.flags       = iop->flags;
	arg.bytes_moved = 0;

	return RTEMS_SUCCESSFUL == rtems_termios_write(&arg) ? arg.bytes_moved : -1;
}

static int
my_do_read(rtems_libio_t *iop, char *buf, int l)
{
rtems_libio_rw_args_t arg;
	arg.iop	        = iop;
	arg.offset      = 0; /* should be unused */
	arg.buffer      = buf;
	arg.count       = l;
	/* hmm - could be that iop->flags are write-only - but termios doesn't seem
	 * to look at the flags anyways...
	 */
	arg.flags       = iop->flags;
	arg.bytes_moved = 0;

	return RTEMS_SUCCESSFUL == rtems_termios_read(&arg) ? arg.bytes_moved : -1;
}

static rtems_status_code
my_tcsetattr(rtems_libio_t *iop, struct termios *ptios)
{
rtems_libio_ioctl_args_t	args;
rtems_status_code			sc;
		args.iop		= iop;
		args.command = RTEMS_IO_TCDRAIN;
		args.buffer  = 0;
		if ( RTEMS_SUCCESSFUL != (sc=rtems_termios_ioctl(&args)) )
			return sc;
		args.command = RTEMS_IO_SET_ATTRIBUTES;
		args.buffer  = ptios;
		return rtems_termios_ioctl(&args);
}
#else
#define my_do_write(fd,buf,i) write(fd,buf,i)
#define my_do_read(fd,buf,i)  read(fd,buf,i)
#define my_tcsetattr(fd, ptios) tcsetattr(fd,TCSADRAIN,ptios)
#endif


static int
chat(
#ifdef WINS_LINE_DISC
	rtems_libio_t *fdi,
	rtems_libio_t *fdo,
#else
	int fdi, int fdo,
#endif
	char *str, char *reply,
	int infoLevel)
{
int i,l;

	for (i=0, l=strlen(str); l > 0; ) {
		int put = my_do_write(fdo, str+i, l);
		if (put <=0 ) {
			PERROR ("Unable to write ESC sequence");
			return -2;
		}
		i+=put;
		l-=put;
	}

	if (!reply)
		return 0;

	for (i=0; i < MAXSTR - 1 && 1==my_do_read(fdi,reply+i,1); i++) {
		if ('R' == reply[i]) {
			reply[++i] = 0;
			return 0;
		}
	}

	*reply = 0;
	return -1;
}


static int
ansiQuery(
	int infoLevel,
#ifndef WINS_LINE_DISC
	int							fdi,
	int							fdo,
#else
	rtems_libio_t				*fdi,	
	struct rtems_termios_tty	*tty,
#endif
	struct winsize				*pwin
		 )
{
struct termios				tios, tion;
char						cbuf[MAXSTR];
char						here[MAXSTR];
int							x,y;
int							rval = -1;
#ifdef WINS_LINE_DISC
rtems_libio_t				*fdo;
rtems_status_code			sc;
#else
int							sc;
#endif

		here[0] = 0;

#ifdef WINS_LINE_DISC
		tios = tty->termios;
		fdo  = fdi;
#else 
		if (infoLevel>1) {
			perror("TIOCGWINSZ failed");
			fprintf(stderr,"Trying ANSI VT100 Escapes instead\n");
		}

		if ( tcgetattr(fdi, &tios) ) {
			PERROR("Unable to get terminal attributes");
			return -1;
		}
#endif

		/* save old settings */
		tion = tios;

		/* switch to raw mode */
		tion.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP
		                 |INLCR|IGNCR|ICRNL|IXON);
		tion.c_oflag &= ~OPOST;
		tion.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
		tion.c_cflag &= ~(CSIZE|PARENB);
		tion.c_cflag |= CS8;
		tion.c_cc[VMIN]  = 0;/* use timeout even when reading single chars */
		tion.c_cc[VTIME] = 30;/* .3 seconds */


		if ( (sc=my_tcsetattr(fdi, &tion)) ) {
			PERROR("Unable to make raw terminal");
			return sc;
		}

		/* do the query */
		sprintf(cbuf,"%c[6n",ESC);

		if ( chat(fdi,fdo,cbuf,here,infoLevel) || 'R'!=here[strlen(here)-1] ) {
			MESSAGE(0,stderr,"Invalid answer from terminal\n");
			here[0] = 0;
			goto restore;
		}
		here[strlen(here)-1] = 'H';

		/* try to move into the desert */
		sprintf(cbuf,"%c[998;998H",ESC);

		if (chat(fdi,fdo,cbuf,0,infoLevel))
			goto restore;

		/* see where we effectively are */
		sprintf(cbuf,"%c[6n",ESC);

		if (0==chat(fdi,fdo,cbuf,cbuf,infoLevel)) {
			if (2!=sscanf(cbuf+1,"[%d;%dR",&y,&x)) {
				MESSAGE(0,stderr,"Unable to parse anser: '%s'\n",cbuf+1);
				goto restore;
			}
			if ( x < 2 ) {
				MESSAGE(0,stderr,"ansiQuery: %i columns??? - I bail\n",x);
				goto restore;
			}
 			if ( y < 2 ) {
				MESSAGE(0,stderr,"ansiQuery: %i rows??? - I bail\n",y);
				goto restore;
			}

			rval = 0;
			if (pwin) {
				pwin->ws_col = x;
				pwin->ws_row = y;
			}
		}


restore:
		/* restore the cursor */
		if (here[0])
			chat(fdi,fdo,here,0,infoLevel);

		if ( (sc=my_tcsetattr(fdi, &tios)) ) {
			PERROR("Oops, unable to restore terminal attributes");
			return sc;
		}

		return rval;
}


/* infoLevel: 0 suppress all error and success messages
 *            1 success messages and only error messages for fallback method (ANSI messages)
 *            2 all messages
 */

int
queryTerminalSize(int infoLevel, int *pwidth, int *pheight)
{
int					fdi;
#ifndef WINS_LINE_DISC
int					fdo;
#endif
struct winsize		win;
int					rval;

	if (	(fdi=fileno(stdin))  < 0
#ifndef WINS_LINE_DISC
		||
			(fdo=fileno(stdout)) < 0
#endif
	   ) {
		if (infoLevel)
			perror("Unable to determine file descriptor");
		return -1;
	}

	/* if we compiled with WINS_LINE_DISC, ansiQuery is done
	 * transparently
	 */
	if ( 0 == (rval=ioctl(fdi, TIOCGWINSZ, &win) )
#ifndef WINS_LINE_DISC
		 ||
		 0 == (rval=ansiQuery(infoLevel, fdi, fdo, &win))
#endif
	   ) {

		if (infoLevel)
			printf("Terminal size: %dx%d (columns x lines)\n", win.ws_col, win.ws_row);

		if (pwidth)  *pwidth  = win.ws_col;
		if (pheight) *pheight = win.ws_row;
	}

	return rval;
}

#ifdef WINS_LINE_DISC

static int
tty_ioctl_gwinsz(struct rtems_termios_tty *tty, rtems_libio_ioctl_args_t *args)
{
	if ( TIOCGWINSZ != args->command )
		return RTEMS_INVALID_NUMBER;

	return ansiQuery(0, args->iop, tty, (struct winsize *)args->buffer);
}

int
ansiTiocGwinszInstall(int disc)
{
int fd;


	if ( (fd=fileno(stdin)) < 0 ) {
		perror("Unable to determine file descriptor for 'stdin'");
		return -1;
	}

	if (!isatty(fd)) {
		fprintf(stderr,"stdin is not a terminal\n");
		return -1;
	}

	if ( disc <=0 ) {
		/* uninstall */
		disc =0;
		if ( ioctl(fd, TIOCSETD, &disc) ) {
			perror("Unable to install discipline #0");
			return -1;
		}
		/* leave the linesw entry - other ttys may still use it */
	} else {
		char *scan;
		int odisc;

		/* see if it works already */
		if ( !queryTerminalSize(0,0,0) )
			return 0;

		if ( ioctl(fd, TIOCGETD, &odisc) ) {
			perror("Unable to get current line discipline (no tty?)");
			return -1;
		}

		if (disc <= PPPDISC || disc >=MAXLDISC) {
			fprintf(stderr,"Invalid parameter\n");
			return -1;
		}
		if (odisc == disc) {
			fprintf(stderr,"Line %i currently in use\n",disc);
			return -1;
		}
		for (scan = (char*)(linesw+disc); scan < (char*)(linesw+disc+1); scan++) {
			if (*scan) {
				fprintf(stderr,"linesw slot %i seems to be in use already, sorry\n",disc);
				return -1;
			}
		}
		/* everything seems to be OK so far */
		linesw[disc] = ttyDiscGwinszAnsi;

		if (ioctl(fd, TIOCSETD, &disc)) {
			perror("Unable to install");

			memset(linesw+disc, 0, sizeof(linesw[disc]));

			return -1;
		}

		/* verify that the hack does the job */
		if (queryTerminalSize(0,0,0)) {
			if (ioctl(fd, TIOCSETD, &odisc)) {
				perror("Unable to restore original");
			} else {
				fprintf(stderr,"Ansi ESC sequence doesn't seem to work, sorry\n");
			}
			/* wipe out */
			memset(linesw+disc, 0, sizeof(linesw[disc]));
			return -1;
		}
	}
	return 0;
}
#endif

#ifndef __rtems__
int
main(int argc, char **argv)
{
	queryTerminalSize(4,0,0);
}
#endif
