#include <stdio.h>
#include <string.h>

#include <rtems.h>
#include <rtems/rtems_bsdnet.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct GevHeader_ {
	const char	*name;
	const char	*value;
	unsigned perm,valid;
	struct GevHeader_ *next;
} GevHeaderRec, *GevHeader;

#define PERM_B(p)	((p)&0xf)
#define PERM_S(p)	(((p)>>4)&0xf)
#define PERM_U(p)	(((p)>>8)&0xf)

#define GEV_NULL	((void*)0xffffffff)
#define H_NULL(h)	(GEV_NULL==(h) || GEV_NULL==(h)->name)

#define FORALLDO(h)	for ( h=(GevHeader)0xf0020000; !H_NULL(h); h=h->next )
void
prall()
{
GevHeader h;
	FORALLDO(h) {
		if ( !h->valid )
			printf("  [");
		printf("'%s=%s' (perm (sub): 0x%08x, valid: 0x%08x)", h->name, h->value, h->perm, h->valid);
		if ( !h->valid )
			printf("]");
		printf("\n");
	}
}

const char *
getbenv(const char *name)
{
GevHeader h;
	FORALLDO(h) {
		if ( !strcmp(name,h->name) && h->valid )
			return h->value;
	}
	return 0;
}

#ifndef HAVE_LIBNETBOOT
/* Use bsdnet fixup routine to retrieve flash variables and setup
 * the bsdnet_config.
 */
/* provide routine to setup the bsdnet_config from the flash variables */
int
bev_network_setup(struct rtems_bsdnet_config *cfg, struct rtems_bsdnet_ifconfig *ifcfg)
{
char *addr = (char*)getbenv("IPADDR0");
char *mask = (char*)getbenv("NETMASK");
char *hnam = (char*)getbenv("HOSTNAME");
char *domn = (char*)getbenv("DNS_DOMAIN");
char *gway = (char*)getbenv("GATEWAY");
char *logh = (char*)getbenv("LOGHOST");
char *dnss = (char*)getbenv("DNS_SERVER");
char *ntps = (char*)getbenv("NTP_SERVER");
char *cmdl = (char*)getbenv("INIT");
char *bootp = (char*)getbenv("DO_BOOTP");

	rtems_bsdnet_bootp_boot_file_name = (char*)getbenv("SYS_SCRIPT");

	if ( !ifcfg ) {
		fprintf(stderr,"No ifcfg argument; using 1st interface after loopback...\n");
		if ( !cfg->ifconfig || !(ifcfg = cfg->ifconfig->next) ) {
			fprintf(stderr,"No interface configuration found\n");
			return -1;
		}
	}
	printf("Configuring '%s' from boot environment parameters...\n", ifcfg->name);

	if ( bootp && 'Y'==toupper(*bootp) ) {
		/* they enforce boot; bail */
		return 0;
	}

	if ( !addr ) {
		fprintf(stderr,"No IP address found -- insist on using BOOTP...\n");
		return -1;
	}

	if ( !mask ) {
		in_addr_t a = inet_addr(addr);

		if ( IN_CLASSA(a) )
			mask = "255.0.0.0";
		else if ( IN_CLASSB(a) )
			mask = "255.255.0.0";
		else if ( IN_CLASSC(a) )
			mask = "255.255.255.0";
		else /* class d */
			mask = "240.0.0.0";
	}
	
	printf("%s %s/%s\n", ifcfg->name, addr, mask);
	ifcfg->ip_address = addr;
	ifcfg->ip_netmask = mask;

	if ( gway ) {
		printf("gateway: %s\n", gway);
		cfg->gateway = gway;
	}

	if ( hnam ) {
		printf("hostname: %s\n", hnam);
		cfg->hostname = hnam;
	}
	if ( domn ) {
		printf("domainname: %s\n", domn);
		cfg->domainname = domn;
	}

	if ( logh ) {
		printf("loghost: %s\n", logh);
		cfg->log_host = logh;
	}

	if ( dnss ) {
		printf("DNS server: %s\n",dnss);
		cfg->name_server[0] = dnss;
	}

	if ( ntps ) {
		printf("NTP server: %s\n",ntps);
		cfg->ntp_server[0] = ntps;
	}

	cfg->bootp = 0;

	if ( cmdl )
		setenv("INIT",cmdl,1);

	return 0;
}
#endif
