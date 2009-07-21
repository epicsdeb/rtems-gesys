/* flashInit.c,v 1.5 2003/08/16 00:33:53 till Exp */

/* Initialize a generic system with a symbol table in FLASH
 *
 * This file is of limited usefulness on boards other than
 * the Synergy Microsystem's SVGM series SBC and it is NOT
 * used by the generic system application.
 *
 * It is here for maintenance and historic reasons.
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
#include <bsp.h>
#include <bsp/irq.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <rtems/rtems_bsdnet.h>
#include <rtems/libio.h>
#include <rtems/tftp.h>

#include <rtems/userenv.h>

#include <cexp.h>

#ifdef HAVE_LIBBSPEXT
#include <bspExt.h>
#endif

#define HAVE_BSP_EXCEPTION_EXTENSION
#ifdef HAVE_BSP_EXCEPTION_EXTENSION
#include <bsp/bspException.h>

static void
cexpExcHandler(BSP_ExceptionExtension ext)
{
		cexp_kill(0);
}

static BSP_ExceptionExtensionRec excExt={
		0, /* no lowlevel handler */
		0, /* be verbose (not quiet) */
		cexpExcHandler
};

static void
cexpExcHandlerInstall(void (*handler)(int))
{
		/* we use cexp_kill() directly; no need
		 * for the handler arg
		 */
        BSP_exceptionHandlerInstall(&excExt);
}
#else
#define cexpExcHandlerInstall 0
#endif

#include <bsp/vmeUniverse.h>

#define BOOTPF rtems_bsdnet_bootp_boot_file_name
#define SYSSCRIPT	"st.sys"

extern cexpFlashSymLoad();

rtems_task Init(
  rtems_task_argument ignored
)
{
char	*user_script=0;
int		argc=4;
int		result=0;
char	*argv[5]={
	"Cexp",	/* program name */
	SYSSCRIPT,
	0
};
char	*slash;


  rtems_libio_set_private_env();

#ifdef HAVE_LIBBSPEXT
  bspExtInit();
#endif

  cexpInit(cexpExcHandlerInstall);

  cexpFlashSymLoad(2);

  if (!(result=cexp_main(argc,argv)) || CEXP_MAIN_NO_SCRIPT==result) {
		/* try a user script */
		if ((user_script=getenv("INIT"))) {
			printf("Trying user script '%s':\n",user_script);
			argv[1]=user_script=strdup(user_script);
			if ((slash = strrchr(user_script,'/'))) {
				/* chdir to where the user script resides */
				int ch=*(++slash);
				*(slash)=0;
				printf("Change Dir to '%s'\n",user_script);
				chdir(user_script);
				*(slash)=ch;
				argv[1]=slash;
			}
			argc=2;
		} else {
			argc=1;
		}
		do {
			result=cexp_main(argc,argv);
			argc=1;
		} while (!result || CEXP_MAIN_NO_SCRIPT==result);
  }
  switch (result) {
		case CEXP_MAIN_INVAL_ARG: fprintf(stderr,"CEXP_MAIN_INVAL_ARG\n"); break;
		case CEXP_MAIN_NO_SYMS  : fprintf(stderr,"CEXP_MAIN_NO_SYMS\n");   break;
		case CEXP_MAIN_NO_SCRIPT: fprintf(stderr,"CEXP_MAIN_NO_SCRIPT\n"); break;
		case CEXP_MAIN_KILLED   : fprintf(stderr,"CEXP_MAIN_KILLED\n");    break;
		case CEXP_MAIN_NO_MEM   : fprintf(stderr,"CEXP_MAIN_NO_MEM\n");    break;

	default:
		if (result)
			fprintf(stderr,"unknown error\n");
  }
  fprintf(stderr,"Unable to execute CEXP - suspending initialization...\n");
  rtems_task_suspend(RTEMS_SELF);
  exit( 1 );
}
