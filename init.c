/*  Init
 *
 *  Initialization task for a generic RTEMS/CEXP system
 *
 *  Author: Till Straumann <strauman@slac.stanford.edu>
 *
 *  $Id: init.c,v 1.43 2008/03/22 20:36:13 guest Exp $
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


/*
 *  The initialization task performs the following steps:
 *
 *   - initialize networking
 *   - mount the TFTP filesystem on '/TFTP/'
 *   - synchronize with NTP server
 *   - initialize CEXP
 *   - retrieve the boot file name using
 *
 *     rtems_bsdnet_bootp_boot_file_name
 *
 *     i.e. the file name obtained by RTEMS/BOOTP
 *     from a BOOTP/DHCP server.
 *
 *     if the file name is relative, "/TFTP/BOOTP_HOST/"
 *     is prepended.
 *
 *   - substitute the boot file's extension by ".sym"
 *     (e.g. sss/yyy/z.exe becomes /TFTP/BOOTP_HOST/sss/yyy/z.sym)
 *
 *   - retrieve 'name=value' pairs from the command line
 *     string (BOOTP/DHCP option 129) and stick them
 *     into the environment.
 *
 *   - chdir into the directory where the boot (and symbol)
 *     files reside.
 *
 *   - invoke 'cexpsh("-s <symfile> st.sys")', i.e. start
 *     CEXP and try to load the symbol table and subsequently
 *     execute a 'st.sys' file AKA 'system script'.
 *
 *     If loading the symbol file fails, the user is
 *     prompted for a path and loading the symbol table
 *     reattempted (repeated until success).
 *
 *   - try to 'getenv("INIT")', thus retrieve the name
 *     of a 'user script' from the command line.
 *
 *     (e.g. dhcpd.conf provides:
 *          option cmdline code 129 = text;
 *          ...
 *     		option cmdline "INIT=/TFTP/BOOTP_HOST/epics/iocxxx/st.cmd"
 *     )
 *
 *     If getenv() succeeds, 'chdir' to the directory
 *     where the user script resides and execute the
 *     user (CEXP) script.
 *
 *   - eventually, CEXP enters interactive mode at the console.
 *
 */
// I386 #include <bsp/irq.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <rtems.h>

#include "verscheck.h"

#include <rtems/rtems_bsdnet.h>
#include <rtems/libio.h>
#include <rtems/tftp.h>
#if RTEMS_VERSION_ATLEAST(4,6,99)
#include <rtems/imfs.h>
#else
#include <imfs.h>
#endif

#include "verscheck.h"


#if RTEMS_VERSION_LATER_THAN(4,6,10)
/* in a new place */
#include <rtems/bsdnet/servers.h>
#endif
#include <bsp.h>


#include <sys/select.h>

#include <rtems/userenv.h>

#include <cexp.h>

#ifdef USE_TECLA
#include <libtecla.h>
#else
#define del_GetLine(gl)					free(gl)
#define	new_GetLine(len,lines)			malloc(len)
#define gl_configure_getline(gl,a,b,c)	do {} while(0)
typedef char GetLine;
static	char *my_getline(char *rval, char *prompt, int len);
#endif

#ifdef HAVE_PCIBIOS
#include <pcibios.h>
#endif

#ifdef HAVE_LIBNETBOOT
#include <libnetboot.h>
#endif

#define LINE_LENGTH 200

#define SYMEXT      ".sym"

#include "builddate.c"

#define TFTP_OPEN_FLAGS (O_RDONLY)

#define ISONTMP(str) ( ! strncmp((str),"/tmp/",5) )
#define ISONTFTP(str) ( ! strncmp((str),"/TFTP/",6))

#ifdef NFS_SUPPORT
static int nfsInited     = 1; /* initialization done by application itself */
#endif
#ifdef TFTP_SUPPORT
static int tftpInited    = 1; /* initialization done by application itself */
#endif


#ifdef NFS_SUPPORT
#include <librtemsNfs.h>
#endif
/* helper code borrowed from netboot */
#include "pathcheck.c"

#ifdef HAVE_LIBBSPEXT
#include <bsp/bspExt.h>
#endif

#if defined(HAVE_BSP_EXCEPTION_EXTENSION)
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

#define BOOTPFN  rtems_bsdnet_bootp_boot_file_name
#define BOOTPSA  rtems_bsdnet_bootp_server_address
#define SYSSCRIPT	"st.sys"

#ifdef USE_TECLA
int
ansiTiocGwinszInstall(int slot);
#endif

#ifdef RSH_SUPPORT
static int rshCopy(char **pDfltSrv, char *pathspec, char **pFnam);
#endif

void
cmdlinePairExtract(char *buf, int (*putpair)(char *str), int removeFound);

int
getchar_timeout(int fd, int timeout);

static void
dummy_clock_init();

static void freeps(char **ps)
{
	free(*ps); *ps = 0;
}

static char *theSrv = 0;

#define DFLT_SRV_LEN 100

static void getDfltSrv(char **pdfltSrv)
{

	*pdfltSrv = realloc(*pdfltSrv, theSrv ? strlen(theSrv)+1 : DFLT_SRV_LEN);

	if ( theSrv ) {
		strcpy(*pdfltSrv, theSrv);
	} else {
  		if ( !inet_ntop( AF_INET, &BOOTPSA, *pdfltSrv, DFLT_SRV_LEN ) ) {
			freeps(pdfltSrv);
  		}
	}
}


static void mkTmpDir()
{
struct stat stbuf;
    if (stat("/tmp",&stbuf)) {
        mode_t old=umask(0);
        mkdir("/tmp",0777);
        umask(old);
    }
}

int
gesys_network_start()
{
char *buf;

#ifdef MULTI_NETDRIVER
  printf("Going to probe for Ethernet chips when initializing networking:\n");
  printf("(supported are 3c509 (ISA), 3c90x (PCI) and eepro100 (PCI) variants).\n");
  printf("NOTES:\n");
  printf("  - Initializing a 3c90x may take a LONG time (~1min); PLUS: it NEEDS media\n");
  printf("    autonegotiation!\n");
  printf("  - A BOOTP/DHCP server must supply my IF configuration\n");
  printf("    (ip address, mask, [gateway, dns, ntp])\n");
#endif

#ifdef BSP_NETWORK_SETUP
  {
  extern int BSP_NETWORK_SETUP(struct rtems_bsdnet_config *, struct rtems_bsdnet_ifconfig *);
  BSP_NETWORK_SETUP(&rtems_bsdnet_config, 0);
  }
#endif

  rtems_bsdnet_initialize_network();

  /* remote logging only works after a call to openlog()... */
  openlog(0, LOG_PID | LOG_CONS, 0); /* use RTEMS defaults */

#ifdef TFTP_SUPPORT
  if (rtems_bsdnet_initialize_tftp_filesystem())
	perror("TFTP FS initialization failed");
#endif

#ifdef NFS_SUPPORT
  if ( rpcUdpInit() || nfsInit(0,0) )
	/* nothing else to do */;
#endif

  if ( rtems_bsdnet_ntpserver_count > 0 ) {
  	printf("Trying to synchronize NTP...");
  	fflush(stdout);
  	if (rtems_bsdnet_synchronize_ntp(0,0)<0)
		printf("FAILED\n");
  	else
		printf("OK\n");
		rtems_time_of_day time;
		rtems_clock_get(RTEMS_CLOCK_GET_TOD, &time);
		printf("yr:%u m:%u d:%u hr:%u min:%u sec:%u\n",\
				time.year,time.month,time.day,\
				time.hour,time.minute,time.second);
		fflush(stdout);
  }

  /* stuff command line 'name=value' pairs into the environment */
  if ( (buf = strdup(rtems_bsdnet_bootp_cmdline)) ) {
	cmdlinePairExtract(buf, putenv, 1);
	free(buf);
  }

  return 0;
}

extern void *cexpSystemSymbols;

#define BUILTIN_SYMTAB (0!=cexpSystemSymbols)

rtems_task Init(
  rtems_task_argument ignored
)
{
GetLine	*gl       = 0;
char	*symf     = 0, *sysscr=0, *user_script=0, *bufp;
int	argc      = 0;
int	result    = 0;
int	no_net    = 0;
char	*dfltSrv  = 0;
char	*pathspec = 0;
#ifdef NFS_SUPPORT
MntDescRec	bootmnt = { "/boot", 0, 0 };
MntDescRec      homemnt = { "/home", 0, 0 };
#endif
char	*argv[7]={
	"Cexp",	/* program name */
	0,
	0,
	0,
	0
};

  rtems_libio_set_private_env();

#ifdef HAVE_PCIBIOS
#if RTEMS_VERSION_ATLEAST(4,6,99)
  pci_initialize();
#else
  pcib_init();
#endif
#endif

#ifdef STACK_CHECKER_ON
  {
	extern void Stack_check_Initialize();
	Stack_check_Initialize();
  }
#endif

#ifdef HAVE_LIBBSPEXT
  bspExtInit();
#endif

  /* make /tmp directory */
  mkTmpDir();


  printf("Welcome to RTEMS GeSys\n");
  printf("This system $Name: GeSys_2_4 $ was built on %s\n",system_build_date);
  printf("$Id: init.c,v 1.43 2008/03/22 20:36:13 guest Exp $\n");

#ifdef EARLY_CMDLINE_GET
  {
	char *cmdlinetmp;
	EARLY_CMDLINE_GET(&cmdlinetmp);

#ifdef HAVE_LIBNETBOOT
  /* Let libnetboot process the command line string; all
   * special name=value pairs recognized by libnetboot will
   * be removed...
   */
   nvramFixupBsdnetConfig(1, cmdlinetmp);
#endif

	cmdlinePairExtract(cmdlinetmp, putenv, 1);
  }
#endif

#if defined(USE_TECLA)
  /*
   * Install our special line discipline which implements
   * TIOCGWINSZ
   */
  printf("Installing TIOCGWINSZ line discipline: %s.\n",
		 ansiTiocGwinszInstall(7) ? "failed" : "ok");
#endif

  /*
   * Make sure the time-of-day clock is at least initialized.
   * The dummy routine just sets the date to 1/1/2000
   */
  dummy_clock_init();

  cexpInit(cexpExcHandlerInstall);

  printf("To skip initialization, press a key now...");
  fflush(stdout);
  if ( getchar_timeout(fileno(stdin),10) > 0 ) {
	printf("OK; skipping to shell");
	no_net = 1;
	argc   = 1;
  }
  printf("\n");

#ifndef CDROM_IMAGE
#ifndef SKIP_NETINI
#define SKIP_NETINI	getenv("SKIP_NETINI")
#endif
  /* check if we have a real ifconfig (first is loopback) */
  if ( !no_net && (! (SKIP_NETINI) || !BUILTIN_SYMTAB) && rtems_bsdnet_config.ifconfig )
  {
    gesys_network_start();
  }
  else
  {
	fprintf(stderr,"Skipping network initialization - you can do it manually\n");
	fprintf(stderr,"by invoking 'gesys_network_start()' (needs BOOTP/DHCP server)\n");
	argc   = 1;
	no_net = 1;
  }
#endif

#ifndef CDROM_IMAGE
  if ( BOOTPFN ) {
	char *slash,*dot;
	pathspec = malloc(strlen(BOOTPFN) + (BUILTIN_SYMTAB ? strlen(SYSSCRIPT) : strlen(SYMEXT)) + 1);
	strcpy(pathspec, BOOTPFN);
	slash    = strrchr(pathspec,'/');

	if ( BUILTIN_SYMTAB ) {
 		if ( slash )
			strcpy(slash+1,SYSSCRIPT);
		else
			strcpy(pathspec,SYSSCRIPT);
	} else {
		dot      = strrchr(pathspec,'.');
		if (slash>dot)
			dot=0;
		/* substitute suffix */
		if (dot) {
			strcpy(dot,SYMEXT);
		} else {
			strcat(pathspec,SYMEXT);
		}
	}
  }
  {
  char *tarvar;
  void *addr;
  int   len;
  	if ( (tarvar=getenv("TARFS")) && 2 == sscanf(tarvar,"%p:%i",&addr,&len) && len > 0 ) {
		mkdir("/tar",0777);
		rtems_tarfs_load("/tar",addr,len);
  	}
  }
#else
  {
	extern void *gesys_tarfs_image_start;
	extern unsigned long gesys_tarfs_image_size;
	printf("Loading TARFS... %s\n",
		rtems_tarfs_load("/tmp", gesys_tarfs_image_start, gesys_tarfs_image_size) ? "FAILED" : "OK");
	pathspec=strdup(BUILTIN_SYMTAB ? "/tmp/"SYSSCRIPT : "/tmp/rtems.sym");
  }
#endif

  dflt_fname = "rtems.sym";
#ifdef TFTP_SUPPORT
  path_prefix = strdup("/TFTP/BOOTP_HOST/");
#elif defined(NFS_SUPPORT)
  path_prefix = strdup(":/remote/rtems:");
#elif defined(RSH_SUPPORT)
  path_prefix = strdup("~rtems/");
#endif
  getDfltSrv(&dfltSrv);

#ifdef PSIM
  {
  extern int loadTarImg(int verbose, int lun);
  if ( !pathspec ) {
	loadTarImg(1, 0);
	pathspec = strdup("/bin/"SYSSCRIPT);
  } else {
	fprintf(stderr,"PSIM: root tarfs not loaded (pathspec was '%s')\n",pathspec);
  }
  }
#endif


  if ( no_net && ( !pathspec || LOCAL_PATH != pathType(pathspec) ) )
	goto shell_entry;

  /* omit prompting for the symbol file */
  if ( pathspec )
  	goto firstTimeEntry;

  /* no pathspec but a builtin symtab ->
   * skip reading symtab / system script
   */
  if ( BUILTIN_SYMTAB )
	goto bare_entry;


  do {
	chdir("/");
#ifdef NFS_SUPPORT
	if ( releaseMount( &bootmnt ) ) {
		fprintf(stderr,"Unable to unmount /boot NFS - don't know what to do, sorry\n");
		break;
	}
#endif
	freeps(&symf);
	freeps(&user_script);

	if (!gl) {
		assert( gl = new_GetLine(LINE_LENGTH, 10*LINE_LENGTH) );
		/* silence warnings about missing .teclarc */
		gl_configure_getline(gl,0,0,0);
	}

	do {
		printf("Symbol file can be loaded by:\n");
#ifdef NFS_SUPPORT
		printf("   NFS: [<uid>.<gid>@][<host>]:<export_path>:<symfile_path>\n");
#endif
#ifdef TFTP_SUPPORT
		printf("  TFTP: [/TFTP/<host_ip>]<symfile_path>\n");
#endif
#ifdef RSH_SUPPORT
		printf("   RSH: [<host>:]~<user>/<symfile_path>\n");
#endif
#ifdef USE_TECLA
		bufp = gl_get_line(gl, "Enter Symbol File Name: ",
			               pathspec,
                           ( pathspec && *pathspec ) ? strlen(pathspec) : 0 );
#else
		bufp = my_getline(gl, "Enter Symbol File Name: ", LINE_LENGTH);
#endif
	} while (!bufp || !*bufp);
	pathspec = realloc(pathspec, strlen(bufp) + 1);
	strcpy(pathspec, bufp);
	bufp = pathspec + strlen(bufp) - 1;
	while ( bufp >= pathspec && ('\n' == *bufp || '\r' == *bufp) )
		*bufp-- = 0;

firstTimeEntry:


  {
	int fd = -1, ed = -1;
	char *slash;

	getDfltSrv( &dfltSrv );

#ifdef TFTP_SUPPORT
	chdir("/TFTP/BOOTP_HOST/");
#endif

	switch ( pathType(pathspec) ) {
		case LOCAL_PATH:
			fd = open(pathspec,O_RDONLY);
			if ( fd >= 0 )
				symf = strdup(pathspec);
		break;


#ifdef TFTP_SUPPORT
		case TFTP_PATH:
			fd = isTftpPath( &dfltSrv, pathspec, &ed, &symf );
		break;
#endif

#ifdef NFS_SUPPORT
		case NFS_PATH:
    		fd = isNfsPath( &dfltSrv, pathspec, &ed, &symf, &bootmnt );
		break;
#endif

#ifdef RSH_SUPPORT
		case RSH_PATH:
    		fd = rshCopy( &dfltSrv, pathspec, &symf );
		break;
#endif

		default:
			fprintf(stderr,"Unrecognized pathspec; maybe remote FS support is not compiled in ?\n");
		break;
	}

	if ( 0==result && dfltSrv ) {
		/* allow the default server to be overridden by the pathspec
		 * during the first pass (original pathspec from boot)
		 */
		theSrv = strdup(dfltSrv);
	}


	if ( (fd < 0) && !BUILTIN_SYMTAB ) {
		fprintf(stderr,"Unable to open symbol file (%s)\n",
			-11 == fd ? "not a valid pathspec" : strerror(errno));
continue;
	}


	if ( fd >= 0 )
		close(fd);
	if ( ed >= 0 )
		close(ed);

	freeps( &sysscr );
	sysscr = strdup(SYSSCRIPT);

#if defined(RSH_SUPPORT) && !defined(CDROM_IMAGE)
	if ( !ISONTMP(symf) ) {
#endif
		if ( (slash = strrchr(symf,'/')) ) {
			int ch=*(slash+1);
			*(slash+1)=0;
			printf("Change Dir to '%s'",symf);
			if ( chdir(symf) )
				printf(" FAILED: %s",strerror(errno));
			fputc('\n',stdout);
			*(slash+1)=ch;
		}
#if defined(RSH_SUPPORT) && !defined(CDROM_IMAGE)
	} else {
		char *scrspec = malloc( strlen(pathspec) + strlen(SYSSCRIPT) + 1);

		strcpy(scrspec, pathspec);
		if ( (slash = strrchr(scrspec, '/')) )
			strcpy( slash+1, SYSSCRIPT );
		else
			strcpy( scrspec, SYSSCRIPT );

		getDfltSrv( &dfltSrv );

		freeps( &sysscr );
		if ( (fd = rshCopy( &dfltSrv, scrspec, &sysscr )) >= 0 ) {
			close( fd );
		} else {
			freeps( &sysscr );
		}
		freeps(&scrspec);
	}
#endif

bare_entry:
	printf("Trying symfile '%s', system script '%s'\n",
		BUILTIN_SYMTAB ? "BUILTIN" : symf,
		sysscr ? sysscr :"(NONE)");

	argc = 1;
#ifdef DEFAULT_CPU_ARCH_FOR_CEXP
	if ( DEFAULT_CPU_ARCH_FOR_CEXP && *DEFAULT_CPU_ARCH_FOR_CEXP ) {
		argv[argc++] = "-a";
		argv[argc++] = DEFAULT_CPU_ARCH_FOR_CEXP;
	}
#endif
	if ( !BUILTIN_SYMTAB ) {
		argv[argc++] = "-s";
		argv[argc++] = symf;
	}
	if ( sysscr ) {
		argv[argc++] = sysscr;
	}


shell_entry:

	result = argc > 1 ? cexp_main(argc, argv) : 0;

	if ( ISONTMP( symf ) )
		unlink( symf );
	if ( sysscr && ISONTMP( sysscr ) )
		unlink( sysscr );

	freeps(&symf);
	freeps(&sysscr);


	if (!result || CEXP_MAIN_NO_SCRIPT==result) {
		int  rc;

		if (gl) {
			del_GetLine(gl);
			gl = 0;
		}

		freeps(&pathspec);

		/* try a user script */
		if ((user_script=getenv("INIT"))) {

			printf("Trying user script '%s':\n",user_script);

			pathspec = strdup(user_script); user_script = 0;

			getDfltSrv(&dfltSrv);

			switch ( pathType( pathspec ) ) {
#ifdef NFS_SUPPORT
				case NFS_PATH:
					if ( 0 == (rc = isNfsPath( &dfltSrv, pathspec, 0, &user_script, &homemnt ) ) ) {
						/* valid NFS path; try to mount; */
						if ( !bootmnt.uidhost || strcmp( homemnt.uidhost, bootmnt.uidhost ) ||
						     !bootmnt.rpath   || strcmp( homemnt.rpath  , bootmnt.rpath   ) )
							rc = nfsMount(homemnt.uidhost, homemnt.rpath, homemnt.mntpt);
					}
				break;
#endif
				case RSH_PATH:
					fprintf(stderr,"RSH download of user scripts is not supported\n");
					rc = -1;
				break;

#ifdef TFTP_SUPPORT
				case TFTP_PATH:
					rc = isTftpPath( &dfltSrv, pathspec, 0, &user_script );
				break;
#endif

				case LOCAL_PATH:
					/* they might refer to a path on the local FS (already mounted NFS) */
					user_script = pathspec; pathspec = 0;
					rc = 0;
				break;

				default:
					fprintf(stderr,"Invalid path specifier; support for remote FS not compiled in?\n");
					rc = -1;
				break;
			}

			freeps(&pathspec);

			argc = 2;

			if ( rc ) {
				fprintf(stderr,"Unable to determine filesystem for user script\n");
				freeps(&user_script);
				argc = 1;
			} else {
				if ((slash = strrchr(user_script,'/'))) {
					/* chdir to where the user script resides */
					int ch=*(++slash);
					*(slash)=0;
					printf("Change Dir to '%s'\n",user_script);
					chdir(user_script);
					*(slash)=ch;
					argv[1]=slash;
				} else {
					argv[1]=user_script;
				}
			}
			argc=2;
		} else {
			argc=1;
		}
		do {
			result=cexp_main(argc,argv);
			argc=1;
  			freeps(&user_script);
		} while (!result || CEXP_MAIN_NO_SCRIPT==result);
		chdir("/");
#ifdef NFS_SUPPORT
		releaseMount( &homemnt );
#endif
	}
  }

  switch (result) {
		case CEXP_MAIN_NO_SYMS  :
				fprintf(stderr,"CEXP_MAIN_NO_SYMS\n");
				result = 0;
		break;
		case CEXP_MAIN_INVAL_ARG: fprintf(stderr,"CEXP_MAIN_INVAL_ARG\n"); break;
		case CEXP_MAIN_NO_SCRIPT: fprintf(stderr,"CEXP_MAIN_NO_SCRIPT\n"); break;
		case CEXP_MAIN_KILLED   : fprintf(stderr,"CEXP_MAIN_KILLED\n");    break;
		case CEXP_MAIN_NO_MEM   : fprintf(stderr,"CEXP_MAIN_NO_MEM\n");    break;

	default:
		if (result)
			fprintf(stderr,"unknown error\n");
  }

  }  while ( !result ); /* retry asking for a sym-filename */
  if (gl) {
	del_GetLine(gl);
	gl = 0;
  }
  fprintf(stderr,"Unable to execute CEXP - suspending initialization...\n");
  rtems_task_suspend(RTEMS_SELF);
  exit( 1 );
}

#ifndef USE_TECLA
static char *my_getline(char *rval, char *prompt, int len)
{
int		ch;
char	*cp;

    if (prompt)
        fputs(prompt,stdout);

    for (cp=rval; cp<rval+len-1 && (ch=getchar())>=0;) {
            switch (ch) {
                case '\n': goto done;
                case '\b':
                    if (cp>rval) {
                        cp--;
                        fputs("\b ",stdout);
                    }
                    break;
                default:
                    *cp++=ch;
                    break;
            }
    }
done:
	*cp=0;
    return rval;
}
#endif

#ifdef RSH_SUPPORT
static int cpfd(int *pi, int o)
{
int  got,put,n;
char buf[BUFSIZ];
char *b = buf;

	if ( (got = read( *pi, buf, sizeof(buf) )) < 0 ) {
		fprintf(stderr,"rshCopy() -- cpfd unable to read");
		return -1;
	}

	if ( 0 == got ) {
		close(*pi);
		*pi = -1;
		return 0;
	}

	b = buf;

	for ( b = buf, n = got; n > 0; n-=put, b+=put ) {
		if ( (put = write(o, b, n)) <= 0 ) {
			fprintf(stderr,"rshCopy() -- cpfd unable to write");
			return -1;
		}
	}
	return got;
}

static int rshCopy(char **pDfltSrv, char *pathspec, char **pFnam)
{
int		fd = -1, ed = -1, tmpfd = -1, maxfd, got;
fd_set	r,w,e;
struct timeval timeout;

int rval = -1;

	fd = isRshPath( pDfltSrv, pathspec, &ed, 0 );
	if ( fd < 0 ) {
		rval = fd;
		goto cleanup;
	}

	assert( !*pFnam );

	*pFnam = strdup("/tmp/rshcpyXXXXXX");

   	if ( (tmpfd=mkstemp(*pFnam)) < 0 ) {
		perror("rshCopy() -- creating scratch file");
		goto cleanup;
	}

	while ( fd >= 0 || ed >= 0 ) {

		FD_ZERO( &r ); FD_ZERO( &w ); FD_ZERO( &e );
		timeout.tv_sec  = 5;
		timeout.tv_usec = 0;

		maxfd = 0;

		if ( fd >= 0 ) {
			FD_SET( fd, &r );
			if ( fd > maxfd )
				maxfd = fd;
		}
		if ( ed >= 0 ) {
			FD_SET( ed, &r );
			if ( ed > maxfd )
				maxfd = ed;
		}
		maxfd++;

		got = select(maxfd, &r, &w, &e, &timeout);

		if ( got <= 0 ) {
			if ( got )
				perror("rshCopy() network select() error");
			else
				fprintf(stderr,"rshCopy() network select() timeout\n");
			goto cleanup;
		}
		if ( ed >= 0 && FD_ISSET( ed, &r ) ) {
			if ( cpfd( &ed, 2 ) < 0 ) {
				perror(" error file descriptor");
				goto cleanup;
			}
		}
		if ( fd >= 0 && FD_ISSET( fd, &r ) ) {
			if ( cpfd( &fd, tmpfd ) < 0 ) {
				perror(" temp file descriptor");
				goto cleanup;
			}
		}
	}

	rval = tmpfd; tmpfd = -1;

cleanup:

	if ( ed >= 0 )
		close(ed);

	if ( fd >= 0 )
		close(fd);

	if ( tmpfd >= 0 ) {
		close(tmpfd);
		unlink( *pFnam );
	}

	if ( rval < 0 )
		freeps(pFnam);

	return rval;
}
#endif


/* timeout is in 10ths of a second;
 * RETURNS: -1 on error
 *           0 on timeout
 *          char > 0 otherwise
 */
int
getchar_timeout(int fd, int timeout)
{
struct termios ot,nt;
unsigned char	val;
int		st;

	/* establish timeout using termios */
	if (tcgetattr(fd,&ot)) {
		perror("TCGETATTR");
		return -1;
	} else {
		nt=ot;
		nt.c_lflag &= ~ICANON;
		nt.c_cc[VMIN]=0;
		/* 1s tics */
		nt.c_cc[VTIME]=timeout;
		if (tcsetattr(fd,TCSANOW,&nt)) {
			perror("TCSETATTR");
			return -1;
		} else {
			st = read(fd, &val, 1);
			if ( st > 0 )
				st = val;
		}
	}
	if (tcsetattr(fd,TCSANOW,&ot)) {
		perror("TCSETATTR (restoring old settings)");
		return -1;
	}
	return st;
}

/* EPICS doesn't start up if the TOD is not initialized.
 * Normally, this should be set from NTP but we use this
 * as a fallback if NTP fails.
 *
 * FIXME: should read the RTC instead!.
 */
static void
dummy_clock_init()
{
rtems_time_of_day rt;
	if ( RTEMS_SUCCESSFUL != rtems_clock_get( RTEMS_CLOCK_GET_TOD, &rt) ) {
		rt.year   = 2000;
		rt.month  = 1;
		rt.day    = 1;
		rt.hour   = 0;
		rt.minute = 0;
		rt.second = 0;
		rt.ticks  = 0;
		rtems_clock_set(&rt);
	}
}
