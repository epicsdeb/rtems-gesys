#define RSH_CMD			"cat "					/* Command for loading the image using RSH */

#define DFLT_FNAME		dflt_fname

#define UNSUP_PATH	(-1)

#define VALID_PATH(t) ((t) >= 0)

#define LOCAL_PATH	0

#ifdef TFTP_SUPPORT
#define TFTP_PATH	1
#else
#define TFTP_PATH	UNSUP_PATH
#endif

#ifdef RSH_SUPPORT
#define RSH_PATH	2
#else
#define RSH_PATH	UNSUP_PATH
#endif

#ifdef NFS_SUPPORT
#define NFS_PATH	3
#else
#define NFS_PATH	UNSUP_PATH
#endif

static char *dflt_fname  = "rtems.bin";
static char *path_prefix = 0;

static int
pathType(const char *path)
{
char *col1, *col2, *tild, *at, *slash;
struct in_addr dummy;
int help = 0, rc;

	if ( strlen(path) > 7 && !strncmp("/TFTP/",path,6)) {
		if ( (slash = strchr(path+6, '/')) ) {
			char ch = *slash;
			*slash = 0; /* assume path is not a constant string */
			rc = inet_pton(AF_INET,path+6,&dummy) || !strcmp(path+6, "BOOTP_HOST");
			*slash = ch;
			if ( rc )
				return TFTP_PATH;
		}
		help = 1;
	}

	col1  = strchr(path,':');
	tild  = strchr(path,'~');
	slash = strchr(path,'/');
	col2  = col1 ? strchr(col1+1,':') : 0;
	at    = strchr(path,'@');

	/* absolute, local path */
	if ( slash && slash == path )
		return LOCAL_PATH;

	/*
	 * tilde must be present and it must either follow the colon or must be the
	 * first character.
	 * Sanity: 
	 *   - the first slash must occur after the
	 *   - 'at' sign, if present, must occur after tilde.
	 */
	if ( tild && ( !slash || slash > tild ) ) {
		if (  tild == (col1 ? col1 + 1 : path)   &&
              ( !at || at > tild ) )
			return RSH_PATH;
		else {
			/* sanity check failed; print info but continue */
			help = 1;
		}
	}

	/* NFS:  two colons must be present; slash must follow the first colon
	 *       at, if present, must be less than first colon.
	 */
	if ( col1 && col2 && slash ) {
		if ( slash == col1+1 && ( !at || at < col1 ) )
			return NFS_PATH;
		else {
			/* sanity check failed; print info but continue */
			help = 1;
		}
	}

	if ( help ) {
		fprintf(stderr,"\nYou specified a possibly ill-formed remote path; trying local...\n");
		fprintf(stderr,"Valid pathspecs are:\n");
#ifdef NFS_SUPPORT
		fprintf(stderr,"   NFS: [<uid>.<gid>@][<host>]:<export_path>:<symfile_path>\n"); 
#endif
#ifdef TFTP_SUPPORT
		fprintf(stderr,"  TFTP: [/TFTP/<host_ip>]<symfile_path>\n"); 
#endif
#ifdef RSH_SUPPORT
		fprintf(stderr,"   RSH: [<host>:]~<user>/<symfile_path>\n"); 
#endif
	}
	return LOCAL_PATH;
}

static char *buildPath(int type, char *path, char *prefix)
{
	if ( ! VALID_PATH(type) )
		return 0;

	if ( type == pathType(path) )
		return strdup(path);

	if ( prefix && *prefix) {
		char *rval = malloc( strlen(prefix) + strlen(path) + 1 );
		sprintf(rval, "%s%s", prefix, path );
		if ( type == pathType( rval ) )
				return rval;
		free(rval);
	}
	return 0;
}

/* A couple of words about the isXXXPath() interface:
 *
 * isXXXPath( pServerName, pathspec, pErrfd, pThePath [, pTheMountPt] )
 *
 * pServerName: A malloced string specifying a the default server.
 * (IN/OUT)     This can be overridden by a host/server present in
 *              'pathspec'. In this case, *pServerName is reallocated()
 *              and updated with the actual server name.
 * 
 * pathspec:    A path specifier defining user, host and file names; see
 * (IN)         below.
 * 
 * pErrfd:      Pointer to an integer where a file descriptor for an
 * (IN/OUT)     error stream can be stored (only relevant for rcmd/rsh
 *              the other methods, i.e., NFS and TFTP set *pErrfd to -1).
 *
 *              Passing a NULL value for pErrfd is valid and instructs
 *              the routines to merely check parameters and build
 *              *pThePath and *pTheMountPt. No rcmd/nfsMount/ and/or
 *              file opening is attempted.
 *
 * pThePath:    If non-NULL, the canonicalized pathspec string is returned
 * (IN/OUT)     in *pThePath. I.e., the default server and filename etc.
 *              have been substituted. The caller is responsible for 
 *              free()ing this string;
 *
 * pTheMountPt: (NFS only) If the address of a non-empty string is passed,
 * (IN/OUT)     the string defines the mount point to be used (possibly
 *              created by 'nfsMount()'). If an empty string is passed,
 *              a default mount-point name is constructed according to the
 *              syntax:
 *                     [ <uid> '.' <gid> '@' ] <host> ':' <exported_path>
 *              NOTE: all '/' chars in <exported_path> are substitued by
 *                    '.'; trailing '/' are stripped. 
 *              It is the caller's responsibility to free() the string.
 *
 * RETURNS:     SUCCESS:
 *                 If pErrfd was NULL: 0 on success (valid path).
 *
 *                 If pErrfd was non-NULL: open file descriptor (integer >= 0)
 *                 The caller can read from the returned fd and must close it
 *                 eventually.
 *
 *              FAILURE: (value < 0)
 *                  < -10 --> invalid pathspec for this method.
 *                    -10 --> pathspec defined no host and no default
 *                            server passed (*pServerName empty).
 *                    -2  --> (NFS only) mount was successful but
 *                            file couldn't be opened. NFS is still mounted.
 *                  other --> NFS mount failed (NFS)
 *                            file couldn't be opened (others).
 *              
 *              NOTES: *pThePath and *pTheMountp are possibly still set
 *                     even if return value < 0.
 *
 *                     *pServerName can be modified even if return value < 0.
 *
 */

static char *
fnCheck(char *path)
{
char *rval = malloc( (path ? strlen(path) : 0 )+ (DFLT_FNAME ? strlen(DFLT_FNAME) : 0 ) + 1);
char *fn   = 0;

	if ( !path || !*path || '/' == path[strlen(path)-1] ) {
		if ( (fn=DFLT_FNAME) )
			fprintf(stderr,"No file; trying '%s'\n",fn);
	}
	sprintf(rval,"%s%s",path ? path : "", fn ? fn : "");
	return rval;
}

#define IDOT_STR_LEN	20	/* enough to hold an IP4 dotted address or "BOOTP_HOST" */

#if defined(RSH_SUPPORT) || defined(NFS_SUPPORT)
static int
srvCheck(char **srvname, char *path)
{
struct hostent	*h;
char			buf[IDOT_STR_LEN];	/* enough to hold a 'dot-notation' ip addr */

	if (   !path || !*path || 0==strcmp(path, "BOOTP_HOST") ) {
		if ( !*srvname ) {
			fprintf(stderr,"No server name in pathspec and default server not set :-(\n");
			return -1;
		}
	} else {
		/* Changed server name */
		/* canonicalize server name by looking it up */

		/* NOTE: gethostbyname is NOT REENTRANT */
		if ( ! (h = gethostbyname(path)) ||
		     ! inet_ntop( AF_INET,
			              (struct in_addr*)h->h_addr_list[0],
		                  buf,
                          sizeof(buf)-1 ) ) {
			fprintf(stderr,"Unable to lookup '%s'\n", path);
			return -1;
				}
		buf[sizeof(buf)-1]=0;
		free(*srvname);
		*srvname = strdup(buf);
	}
	return 0;
}
#endif

#if defined(NFS_SUPPORT)
#define MDESC_FLG_OWN_STRING (1<<0)

typedef struct MntDesc_ {
	char *mntpt;
	int	 flags;
	char *uidhost;
	char *rpath;
} MntDescRec, *MntDesc;

static MntDescRec dflt_mnt = { "/mnt", };

static int releaseMount(MntDesc m)
{
int  rc = 0;

	if ( !m )
		m = &dflt_mnt;

	/* uidhost serves as a flag to check if this descriptor is currently mounted */
    if ( m->uidhost && m->mntpt ) {
		if ( 0 == (rc = unmount( m->mntpt )) ) {
        	unlink( m->mntpt );
		}
    }
	if ( !rc ) {
		free( m->rpath );
		m->rpath = 0;
		free( m->uidhost );
		m->uidhost = 0;
		if ( m->flags & MDESC_FLG_OWN_STRING ) {
			free( m->mntpt );
			m->mntpt = 0;
			m->flags &= ~MDESC_FLG_OWN_STRING;
		}
	}
	return rc;
}

/* RETURNS -2 if mount is ok but file cannot be opened; leaves NFS mounted */
static int isNfsPath(char **srvname, char *opath, int *perrfd, char **thepathp, MntDesc md)
{

int  fd    = -1, l;
char *fn   = 0, *path = 0;
char *col1 = 0, *col2 = 0, *slas = 0, *at = 0, *srv=path, *ugid=0;
char *srvpart  = 0;
char *rpath = 0;
char *mnt   = 0;

int  allocMntstring;

	if ( !md )
		md = &dflt_mnt;

	allocMntstring = ! (mnt = md->mntpt);

	if ( perrfd )
		*perrfd = -1;

	if ( md->uidhost || md->rpath ) {
		fprintf(stderr,"Mount point '%s' is already in use\n", md->mntpt ? md->mntpt : "<CORRUPTED>");
		return -1;
	}


	if ( !(path = buildPath(NFS_PATH, opath, path_prefix)) ) {
		return -11;
	}

	col1=strchr(path,':');
	col2=strchr(col1+1,':');
	slas=strchr(path,'/');

	if ( (at = strchr(path,'@')) && at < col1 ) {
		srv = at + 1;
		ugid = path;
	} else {
		at = 0;
		srv = path;
	}

	if ( !nfsInited ) {
		if ( rpcUdpInit() ) {
			fprintf(stderr,"RPC-IO initialization failed - try RSH or TFTP\n");
			goto cleanup;
		}
		nfsInit(0,0);
		nfsInited = 1;
	}

	/* clear all separators */
	if ( at )
		*at = 0;
	*col1 = 0;
	*col2 = 0;

	rpath = strdup(col1+1);

	if ( srvCheck( srvname, srv ) ) {
		fd = -10;
		goto cleanup;
	}

	fn = fnCheck( col2 + 1);

	l = ( ugid ? strlen(ugid) + 1: 0 )     /* uid.gid@ */ +
		strlen(*srvname)                /* host     */ ;

	srvpart = malloc ( l + 1 );
	if ( ugid ) {
		strcpy(srvpart,ugid);
		strcat(srvpart,"@");
	} else {
		*srvpart = 0;
	}
	strcat(srvpart, *srvname);

	if ( allocMntstring ) {
		char *tmp;

		mnt = malloc( 1 /* '/' */ + l + 1 /* ':' */ + strlen(rpath) + 1 );

		sprintf(mnt,"/%s:%s", srvpart, rpath);

		/* remove trailing '/' in mnt */
		for ( tmp = mnt + strlen(mnt) - 1; '/' == *tmp && tmp >= mnt; )
				*tmp-- = 0;

		/* convert '/' in the server path into '.' */
		for ( tmp = mnt + 1 + l + 1; (tmp = strchr(tmp, '/')); ) {
			*tmp++ = '.';
		}
	} else {
		/* they provide a mountpoint */
	}

	path = realloc(path, strlen(mnt) + 1 /* '/' */ + strlen(fn) + 1);
	sprintf(path,"%s/%s",mnt,fn);

	if ( perrfd ) {
		struct stat probe;
		int         existed;

		/* race condition from here till possible unlink */
		existed = !stat(mnt, &probe);
		
		if ( nfsMount(srvpart, rpath , mnt) ) {
			if ( !existed )
				unlink(mnt);
			if ( allocMntstring ) {
				free(mnt);
				mnt = 0;
			}
			goto cleanup;
		} else {
			fd = open(path,O_RDONLY);
			if ( fd < 0 ) {
				perror("Opening file failed");
				/* leave mounted and continue */
				fd = -2;
			}
		}
	} else {
		/* Don't actually do anything but signal success */
		fd = 0;
	}

	/* are they interested in the path ? */
	if ( thepathp ) {
		*thepathp = path; 
		path = 0;
	}

	md->mntpt = mnt;
	md->uidhost = srvpart; 
	srvpart   = 0;
	md->rpath = rpath;
	rpath     = 0;
	if ( allocMntstring )
		md->flags |= MDESC_FLG_OWN_STRING;
	else
		md->flags &= ~MDESC_FLG_OWN_STRING;

cleanup:
	free(srvpart);
	free(fn);
	free(path);
	free(rpath);
	return fd;
}
#endif

#if defined(RSH_SUPPORT)
extern int rcmd();

#define RSH_PORT	514

#ifndef RSH_BUFSZ
#ifdef NVRAM_UCDIMM
#define RSH_BUFSZ   (1024*1024*8)
#else
#define RSH_BUFSZ	(1024*1024*20)
#endif
#endif

static int isRshPath(char **srvname, char *opath, int *perrfd, char **thepathp)
{
int	fd = -1;
char *username, *fn = 0, *tmp;
char *tild = 0, *col1 = 0;
char *cmd  = 0;
char *path = 0;

	if ( perrfd )
		*perrfd = -1;

	if ( !(path = buildPath(RSH_PATH, opath, path_prefix)) ) {
		return -11;
	}

	col1 = strchr(path,':');
	tild = strchr(path,'~');

	if ( !col1 && !*srvname ) {
		fprintf(stderr,"No default server; specify '<server>:~<user>/<path'\n");
		fd = -10;
		goto cleanup;
	}

	if (col1) {
		*col1 = 0;
	}
	*tild = 0;

	/* they gave us user name */
	username=tild+1;

	if ((tmp=strchr(username,'/'))) {
		*(tmp++)=0;
	}

	if ( srvCheck( srvname, path ) )
		goto cleanup;
	
	fprintf(stderr,"Loading as '%s' from '%s' using RSH\n",username, *srvname);

	fn = fnCheck(tmp);

	/* cat filename to command */
	cmd = malloc(strlen(RSH_CMD)+strlen(fn)+1);
	sprintf(cmd,"%s%s",RSH_CMD,fn);

	{
	char *willbechanged = *srvname;
	fd=rcmd(&willbechanged, RSH_PORT, username,username,cmd,perrfd);
	}

	if ( perrfd ) {
		if (fd<0) {
			fprintf(stderr,"rcmd"/* (%s)*/": got no remote stdout descriptor\n"
							/* ,strerror(errno)*/);
			if ( *perrfd >= 0 )
				close( *perrfd );
			*perrfd = -1;
		} else if ( *perrfd<0 ) {
			fprintf(stderr,"rcmd"/* (%s)*/": got no remote stderr descriptor\n"
							/* ,strerror(errno)*/);
			if ( fd>=0 )
				close( fd );
			fd = -1;
		}
	} else {
		/* don't actually do anything but signal success */
		fd = 0;
	}

	/* are they interested in the path ? */
	if ( thepathp ) {
		*thepathp = malloc(strlen(*srvname) + 2 + strlen(username) + 1 + strlen(fn) + 1);
		sprintf(*thepathp,"%s:~%s/%s",*srvname,username,fn);
	}

cleanup:
	free( fn   );
	free( path );
	free( cmd  );
	return fd;
}
#endif

#if defined(TFTP_SUPPORT)
static int isTftpPath(char **srvname, char *opath, int *perrfd, char **thepathp)
{
int     fd = -1;
char    *path    = 0, *fn = 0, *srvpart, *slash=0;
char	*ofn;


	if ( perrfd )
		*perrfd = -1;

	if ( !(srvpart = buildPath(TFTP_PATH, opath, path_prefix)) )
		return -11;

 	if (!tftpInited && rtems_bsdnet_initialize_tftp_filesystem()) {
		fprintf(stderr,"TFTP FS initialization failed - try NFS or RSH\n");
		goto cleanup;
	} else
		tftpInited=1;

	fprintf(stderr,"Using TFTP for transfer\n");

	/* 
	 * 'buildPath' has verified that the pathspec is of the
	 * form
	 *      /TFTP/2.3.4.5/<...>
	 * or
	 *      /TFTP/BOOTP_HOST/<...>
	 *
	 * copy header to srvpart and remember trailing path
	 * also, *srvname has to be reassigned.
	 */
	ofn   = srvpart + 6;
	/* may be necessary to rebuild the server name */
	slash = strchr(ofn,'/');
	*slash=0;	/* break into two pieces */
	/* reassign *srvname substituting 'BOOTP_HOST' */
	free(*srvname);
	*srvname = malloc(IDOT_STR_LEN);
	if ( strcmp(ofn,"BOOTP_HOST") ||
  	     ! inet_ntop( AF_INET, &rtems_bsdnet_bootp_server_address, *srvname, IDOT_STR_LEN ) ) {
		strcpy(*srvname, ofn);
	}
	/* ofn points to the path on the server */
	ofn = slash + 1;

	fn = fnCheck(ofn);
	path = malloc(strlen(srvpart) + 1 /* '/' */ + strlen(fn) + 1);
	sprintf(path,"%s/%s",srvpart,fn);

	if ( perrfd ) {
		if ((fd=open(path,TFTP_OPEN_FLAGS,0))<0) {
			fprintf(stderr,"unable to open %s\n",path);
		}
	} else {
		/* don't actually do anything but signal success */
		fd = 0;
	}

	/* are they interested in the path ? */
	if ( thepathp ) {
		*thepathp = path; 
		path = 0;
	}

cleanup:
	free(srvpart);
	free(fn);
	free(path);
	return fd;
}
#endif
