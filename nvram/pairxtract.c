#include <string.h>
#include <stdio.h>

/*
 * Find next unquoted '='
 */
static char *feq(char *buf)
{
int q = 0;
	while ( ('=' != *buf || q % 2 ) ) {
		if ( ! *buf )
			return 0;
		else if ( '\'' == *buf )
			q++;
		buf++;
	}
	return buf;
}

/* find next name eliminating white space between variable name and '='.
 * Name cannot contain ' nor  space nor '='.
 * Skip over quoted stuff.
 */

static char *fnam(char *str)
{
char *eq, *e, *b;
	if ( !(eq=feq(str)) )
		return 0;
	/* skip white space */
	e = eq;
	do {
		if ( --e <= str )
			return 0;
	} while ( ' '==*e );

	/* e points to last char of name */
	for ( b = e; ' '!= *b && '\'' !=*b && b>=str; b-- ) 
		;
	if ( ++b > e )
		return 0;
	/* eliminate white space */
	if ( eq > e+1 )
		memmove(e+1, eq, strlen(eq)+1);
	return b;
}

/*
 * Scan 'buf' for name=value pairs.
 *
 * pair = name { ' ' } '=' value
 *
 * name:  string not containing '\'', ' ' or '='
 *
 * value: quoted_value | simple_value
 *
 * simple_value: string terminated by the first ' '
 *
 * quoted_value: '\'' { non_quote_char | '\'''\'' } '\''
 *
 * On each 'name=' tag found the callback is invoked with the
 * 'str' parameter pointing to the first character of the name.
 * The value is NULL terminated.
 *
 * If the callback returns 0 and the 'removeFound' argument is nonzero
 * then the 'name=value' pair is removed from 'buf'.
 *
 * SIDE EFFECTS:
 *    - space between 'name' and '=' is removed
 *    - values are unquoted and NULL terminated (undone after callback returns)
 *    - name=value pairs are removed from 'buf' if callback returns 0 and removeFound
 *      is nonzero.
 * If side-effects cannot be tolerated then the routine should be executed on a
 * 'strdup()'ed copy...
 */

void
cmdlinePairExtract(char *buf, int (*putpair)(char *str), int removeFound)
{
char *beg,*end;

	if ( !buf || !putpair )
		return;

	/* find 'name=' tags */

	/* this algorithm is copied from the svgm BSP (startup/bspstart.c) */
	for (beg=buf; beg && (beg=fnam(beg)); beg=end) {
		/* simple algorithm to find the end of quoted 'name=quoted'
		 * substring. As a side effect, quotes are removed from
		 * the value.
		 */
		if ( (end = strchr(beg,'=')) ) {
			char *dst;

			/* now unquote the value */
			if ('\'' == *++end) {
				/* end points to the 1st char after '=' which is a '\'' */

				dst = end++;

				/* end points to 1st char after '\'' */

				while ('\'' != *end || '\'' == *++end) {
					if ( 0 == (*dst++=*end++) ) {
						/* NO TERMINATING QUOTE FOUND
						 * (for a properly quoted string we
						 * should never get here)
						 */
						end = 0;
						dst--;
						break;
					}
				}
				*dst = 0;
			} else {
				/* first space terminates non-quoted strings */
				if ( (dst = end = strchr(end,' ')) )
					*(end++)=0;
			}
			if ( 0 == putpair(beg) && removeFound ) {
				if ( end ) {
					memmove(beg, end, strlen(end)+1);
					end = beg;
				} else {
					*beg = 0;
				}
			} else {
				if ( dst )
					*dst = ' ';
			}
		}
	}
	return;
}

#ifdef TESTING
int
ptenv(char *s)
{
printf("Putenv(%s)\n",s);
return 0;
}

int
main(int argc, char **argv)
{
char *buf, *eq, *p;
	if ( argc < 2 ) {
		fprintf(stderr,"Need string argument\n");
		exit(1);
	}
	buf = strdup(argv[1]);
	printf("%s\n",buf);
	for ( p = buf; (eq=feq(p)); ) {
		while ( p != eq ) {
			printf(".");
			p++;
		}
		printf("^"); p++;
	}
	printf("\n");

	for (p=buf; p=fnam(p); p++ ) {
		printf("Found %s\n",p);
		if ( ! (p = feq(p)) )
			break;
	}
	
	cmdlinePairExtract(buf,ptenv,1);

	printf("Line now '%s'\n", buf);	
}
#endif
