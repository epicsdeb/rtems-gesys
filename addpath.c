/* $Id: addpath.c,v 1.4 2008/03/21 19:26:06 guest Exp $ */

#include <reent.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

extern const char *rtems_bsdnet_domain_name;

extern void __env_lock(struct _reent *);
extern void __env_unlock(struct _reent *);

/* append/prepend to environment var */
int
addenv(char *var, char *val, int prepend)
{
char *buf;
char *a,*b;
int  len, rval;

	if ( !var || !val ) {
		return -1;
	}

	__env_lock(_REENT);

	if ( ! (a=getenv(var)) ) {
		rval = setenv(var, val, 0);
		__env_unlock(_REENT);
		return rval;
	}
	len  = strlen(val) + strlen(a) + 1 /* terminating 0 */;

	if ( !(buf = malloc(len)) ) {
		__env_unlock(_REENT);
		return -1;
	}

	if ( prepend ) {
		b = a;
		a = val;
	} else {
		b = val;
	}
	strcpy(buf,a);
	strcat(buf,b);
	rval = setenv(var, buf, 1);
	__env_unlock(_REENT);

	free(buf);

	return rval;	
}

/* Add to PATH */
int
addpath(char *val, int prepend)
{
	return addenv("PATH",val,prepend);
}

/* Add CWD with suffix to PATH */
int
addpathcwd(char *suffix, int prepend)
{
int     l = suffix ? (strlen(suffix) + 1) : 0;
char *buf = 0; 
int  rval = -1;
	
	l += MAXPATHLEN;
	if ( ! (buf = malloc(l)) )
		goto cleanup;

	if ( ! getcwd(buf, MAXPATHLEN) )
		goto cleanup;

	if ( suffix )
		strcat(buf, suffix);

	rval = addpath(buf, prepend);

cleanup:
	free(buf);
	return rval;
}

/* Allocate a new string substituting all '%<tagchar>' occurrences
 * in 'p' by substitutions listed in s[].
 * If no matching substitution is found, %<tagchar> is copied to
 * the destination string verbatim.
 * '%%' is expanded to a single '%' character.
 * 's' is a list of substitution strings with the first
 * character being the 'tag char' followed by the substitution
 * string/expansion.
 * It is the user's responsibility to free the returned string.
 *
 *  'p': template (input) with '%<tagchar>' occurrences
 *  's': array of 'ns' "<tagchar><subst>" strings
 * 'ns': number of substitutions
 *
 * RETURNS: malloc()ed string with expanded substitutions on
 *          success, NULL on error (NULL p, no memory).
 *      
 *
 * Example: 
 *    char *s[] = { "wworld" };
 *
 *    newp = stringSubstitute("hello %w %x %%",s,1)
 *
 *    generates the string newp -> "hello world %x %"
 */

char *
stringSubstitute(const char *p, const char * const *s, int ns)
{
	register const char *pp;
	register char       *dd;
	char *rval;
	int l,i,ch;

	if ( !p )
		return 0;

	for ( l = 0, rval = dd = 0; !rval; ) {
		if ( l ) {
			if ( ! (dd = rval = malloc(l+1)) )
				return 0;
		}
		for ( pp=p; *pp; pp++ ) {
			if ( '%' != *pp ) {
				if ( dd )
					*dd++ = *pp;
				else
					l++;
			} else {
				const char *ptmp = pp;

				if ( (ch = *(pp+1)) ) {

					if ( '%' == ch ) {
						pp++;
					} else {

						for ( i = 0; i<ns; i++ ) {
							if ( *s[i] == ch ) {
								/* don't count initial substitution char
								 * subtract 1 more char; will be added
								 * again a few lines below...
								 */
								int ll = strlen(s[i]) - 1 - 1;
								if ( dd ) {
									strcpy(dd, s[i]+1);	
									dd  += ll;
									ptmp = dd;
								} else {
									l   += ll;
								}
								pp++;
								break;
							}
						}
						/* no substitution found; treat as ordinary '%'
						 * (in case a substitution was found, the extra
						 * char added here had been subtracted above)
						 */
					}
				}
				/* if % was the last char then this will just copy it */
				if ( dd ) {
					*dd++ = *ptmp;
				} else {
					l++;
				}
			}
		}
	}
	return rval;
}

/* A vararg wrapper */

char *
stringSubstituteVa(const char *p, ...)
{
va_list    ap;
int        ns;
const char **sp = 0;
const char *cp;
char       *rval;

	/* count number of substitutions */
	va_start(ap,p);
	for (ns = 0; va_arg(ap, const char *); ns++)
		;
	va_end(ap);
	if ( ns && ! (sp = malloc(sizeof(*sp) * ns)) )
		return 0;
	va_start(ap,p);
	for (ns = 0; (cp = va_arg(ap, const char *)); ns++)
		sp[ns] = cp;
	va_end(ap);

	rval = stringSubstitute(p, sp, ns);

	free(sp);

	return rval;
}


/* Allocate a string and copy the template 'tmpl'
 * making the following substitutions:
 *
 *        %H -> hostname ('gethostname')
 *        %D -> domainname ('getdomainname')
 *        %I -> IP address (dot notation) [NOT SUPPORTED YET -- it's not trivial to find our IP address].
 *
 * RETURNS: newly allocated string (user must free())
 *          or NULL (no memory or gethostname etc. failure).
 */

#define MAX_SUBST 4

char *
pathSubstitute(const char *tmpl)
{
int  i;
char *s[MAX_SUBST];
int     ns = 0;
char *rval = 0;
char * const *sp = s;

	if ( strstr(tmpl,"%H") ) {
		if ( ! (s[ns] = malloc(100)) )
			goto bail;
		s[ns][0]='H';	/* tag char */
		if ( gethostname(s[ns]+1,99) ) {
			/* gethostname failure */
			goto bail;
		}
		ns++;
	}
	if ( strstr(tmpl,"%D") ) {
		if ( ! (s[ns] = malloc(100)) )
			goto bail;
		s[ns][0]='D';	/* tag char */
		if ( rtems_bsdnet_domain_name ) {
			strcpy(s[ns]+1,rtems_bsdnet_domain_name);
		} else {
			s[ns][1]=0;
		}
		ns++;
	}

	rval = stringSubstitute(tmpl, (const char * const *)sp, ns);

bail:
	for ( i = 0; i<ns; i++ ) {
		free(s[i]);
	}
	return rval;
}

/* chdir to a path containing %H / %D substitutions */
int
chdirTo(const char *tmpl)
{
char *p = pathSubstitute(tmpl);
int  rval = -1;
	if ( p )
		rval = chdir(p);
	free(p);
	return rval;
}


#ifdef DEBUG_MAIN
static void
usage(char *nm)
{
	fprintf(stderr,"Usage: %s [-s <subst>] [-s <subst>] <path>\n",nm);
	fprintf(stderr,"       <subst> : 'subst_char' 'subst_string', e.g., Hhhh\n");
}

int main(int argc, char **argv)
{
const char *s[20];
int   ns = 0;
char *p;
int   ch;

	while ( (ch = getopt(argc, argv, "s:")) > 0 ) {
		switch ( ch ) {
			case 's':
				if ( ns < sizeof(s)/sizeof(s[0]) ) {
					s[ns]=optarg;
					ns++;
				} else {
					fprintf(stderr,"Too many substitutions; skipping\n");
				}
			break;
			default:
				fprintf(stderr,"Unknown option '%c'\n",ch);
				usage(argv[0]);
			break;
		}
	}

	if ( optind >= argc ) {
		fprintf(stderr,"Path argument missing\n");
		usage(argv[0]);
		return(1);
	}
	if ( !(p = stringSubstitute(argv[optind],s,ns)) ) {
		fprintf(stderr,"stringSubstitute failed\n");
		return(1);
	}
	printf("%s\n",p);
	free(p);
}
#endif

