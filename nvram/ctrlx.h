/* $Id: ctrlx.h,v 1.4 2007/04/12 21:44:40 strauman Exp $ */
#ifndef CTRLX_TERMIOS_HACK
#define CTRLX_TERMIOS_HACK

/* an ugly hack to intercept CTRL chars on the console for
 * because libtecla doesn't have provision for callbacks
 * or user-defined key handlers :-(
 *
 * The idea is to use this as follows:
 *
 * a) at init: call installConsoleCtrlXHack()
 *    add tecla bindings for the control chars to 'newline'
 *
 * b) while () {
 * 		gl_get_line();
 *
 * 		if (getConsoleSpecialChar() >= 0) {
 * 			/ * a special char was pressed, process * /
 * 		}
 *    }
 */


/* magicChar causes a reboot;
 *
 * the special lowlevel handler listens for any
 * char listed added by addConsoleSpecialChar() and
 * remembers the first one encountered in a
 * static variable.
 *
 * RETURNS always 0
 *
 * NOTE: This routine hacks into the current line discipline
 *       by installing a l_rint method to inspect all incoming
 *       characters. Hence, the hack must be installed *after*
 *       changing the line discipline, if any.
 *
 *       The caller must provide a routine that causes a hard-reset.
 *       She may pass a NULL pointer to uninstall the hack.
 *
 * RETURNS: 0 on success, -errno on error.
 */
int
installConsoleCtrlXHack(int magicChar, void (*board_reset_fun)());

/* 
 * getConsoleCtrlChar()
 * 
 * RETURNS: the last recorded 'special' character and
 *          resets the buffer.
 *
 */
int
getConsoleSpecialChar(void);

/* add a special character to listen for
 *
 * RETURNS: 0 on success, -1 if the hack was not initialized
 *          or the max number or special chars is exceeded.
 */
int
addConsoleSpecialChar(int ch);

#endif
