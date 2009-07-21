/* $Id: flash.c,v 1.2 2003/04/18 01:40:20 till Exp $ */

/* Copy data from the SVGM board flash area to a scratch file
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
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int
cexpModuleLoad(char*,char*);

void
cexpFlashSymLoad(unsigned char bank)
{
unsigned char	*flashctrl=(unsigned char*)0xffeffe50;
unsigned char	*flash    =(unsigned char*)0xfff80000;
int				fd;
struct	stat	stbuf;
int				len,i,written;
char 			tmpfname[30]={
				        '/','t','m','p','/','m','o','d','X','X','X','X','X','X',
					        0};

	/* switch to desired bank */
	*flashctrl=(1<<7)|bank;

	if (stat("/tmp",&stbuf)) {
		mode_t old = umask(0);
		mkdir("/tmp",0777);
		umask(old);
	}
	if ( (fd=mkstemp(tmpfname)) < 0) {
		perror("creating scratch file");
		goto cleanup;
	}

	len=*(unsigned long*)flash;
	flash+=sizeof(unsigned long);

	printf("Copying %i bytes\n",len);
	for (i=written=0; i < len; i+=written ) {
		written=write(fd,flash+i,len-i);
		if (written<=0)
			break;
	}
	close(fd);
	if (written<0) {
		perror("writing to tempfile\n");
	} else {
		printf("OK, ready to load\n");
		cexpModuleLoad(tmpfname,0);
	}

cleanup:
	unlink(tmpfname);
}
