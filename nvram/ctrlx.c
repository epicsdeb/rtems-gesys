/* $Id: ctrlx.c,v 1.7 2007/08/25 00:17:48 till Exp $ */

#include <assert.h>

#include <rtems.h>
#include <termios.h>
#include <rtems/termiostypes.h>
#include <bsp.h>
#include <stdio.h>
#include <errno.h>

#include <ctrlx.h>

#include "minversion.h"

#define  CTRL_X 030

#if RTEMS_ISMINVERSION(4,7,99)
#define linesw rtems_termios_linesw
#endif


static int rebootChar = CTRL_X;

static int specialChars[10] = {0};
static int numSpecialChars  =  -1; /* can't add chars prior to installing hack */

static int lastSpecialChar = -1;

static void (*resetFun)()  = 0;

int
getConsoleSpecialChar(void)
{
int rval = lastSpecialChar;
	lastSpecialChar = -1;
	return rval;
}

/* hack into termios - THIS ROUTINE RUNS IN INTERRUPT CONTEXT */
static
int incharIntercept(int ch, struct rtems_termios_tty *tty)
{
/* Note that struct termios is not struct rtems_termios_tty */ 
int                      i;
char                     c;

	/* Do special processing only in canonical mode
	 * (e.g., do not do special processing during ansiQuery...
	 */
	if ( (tty->termios.c_lflag & ICANON) ) {

		/* did they just press Ctrl-X? */
		if (rebootChar == ch) {
			/* OK, we shouldn't call anything from IRQ context,
			 * but for reboot - who cares...
			 */
			if ( resetFun )
				resetFun();
		}

		if ( lastSpecialChar < 0 ) {
			for (i = 0; i<numSpecialChars; i++) {
				if (ch == specialChars[i]) {
					lastSpecialChar = ch;
					break;
				}
			}
		}
	}

	/* Unfortunately, there is no way for a line discipline
	 * method (and we are in 'l_rint' at this point) to instruct
	 * termios to continue 'normal' processing.
	 * Hence we resort to an ugly hack:
	 * We temporarily set l_rint to NULL and call
	 * rtems_termios_enqueue_raw_characters() again:
	 */
	linesw[tty->t_line].l_rint = NULL;
	c = ch;
	rtems_termios_enqueue_raw_characters(tty, &c, 1);
	linesw[tty->t_line].l_rint = incharIntercept;

	return 0;
}

int
addConsoleSpecialChar(int ch)
{
	if (numSpecialChars >=0 &&
		numSpecialChars < sizeof(specialChars)/sizeof(specialChars[0])) {
		specialChars[numSpecialChars++] = ch;
		return 0;
	} 
	return -1;
}

int
installConsoleCtrlXHack(int magicChar, void (*reset_fun)())
{
int				dsc;
unsigned long   flags;
int             rval = -EINVAL;

	if ( ioctl(0,TIOCGETD,&dsc) )
		return -errno;

	rtems_interrupt_disable(flags);

	if ( !reset_fun ) {
		if ( linesw[dsc].l_rint == incharIntercept ) {
			/* uninstall */
			resetFun           = 0;
			rebootChar         = CTRL_X;
			linesw[dsc].l_rint = 0;
			rval               = 0;	
		}
		rtems_interrupt_enable(flags);
		/* bad argument */
		return rval;
    }

	if (magicChar)
		rebootChar=magicChar;

	/* now install our 'Ctrl-X' hack, so they can abort anytime while
	 * network lookup and/or loading is going on...
	 */

	if ( linesw[dsc].l_rint ) {
		rtems_interrupt_enable(flags);
		fprintf(stderr,"Current line discipline already has l_rint set -- unable to install hack\n");
		return -ENOSPC;
	}

	/* install hack */
	linesw[dsc].l_rint = incharIntercept;

	numSpecialChars = 0;

	rtems_interrupt_enable(flags);
	return 0;
}
