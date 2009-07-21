/*
 * RTEMS network configuration for EPICS
 *  rtems_netconfig.c,v 1.1 2001/08/09 17:54:05 norume Exp
 *      Author: W. Eric Norum
 *              eric.norum@usask.ca
 *              (306) 966-5394
 *
 * This file can be copied to an application source dirctory
 * and modified to override the values shown below.
 *
 * The following CPP symbols may be passed from the Makefile:
 *
 *   symbol                   default       description
 *
 *   NETWORK_TASK_PRIORITY    150           can be read by app from public var 'gesysNetworkTaskPriority'
 *   FIXED_IP_ADDR            <undefined>   hardcoded IP address (e.g., "192.168.0.10"); disables BOOTP;
 *                                          must also define FIXED_NETMASK
 *   FIXED_NETMASK            <undefined>   IP netmask string (e.g. "255.255.255.0")
 *   LO_IF_ONLY               <undefined>   If defined, do NOT configure any ethernet driver but only the
 *                                          loopback interface.
 *   MULTI_NETDRIVER          <undefined>   ugly hack; if defined try to probe a variety of PCI and ISA drivers
 *                                          (i386 ONLY) use is discouraged!
 *   NIC_NAME                 <undefined>   Ethernet driver name (e.g. "pcn1"); must also define NIC_ATTACH
 *   NIC_ATTACH               <undefined>   Ethernet driver attach function (e.g., rtems_fxp_attach).
 *                                          If these are undefined then
 *                                            a) MULTI_NETDRIVER is used (if defined)
 *                                            b) RTEMS_BSP_NETWORK_DRIVER_NAME/RTEMS_BSP_NETWORK_DRIVER_ATTACH
 *                                               are tried
 *   MEMORY_CUSTOM            <undefined>   Allocate the defined amount of memory for mbufs and mbuf clusters,
 *                                          respectively. Define to a comma ',' separated pair of two numerical
 *                                          values, e.g: 100*1024,200*1024
 *   MEMORY_SCARCE            <undefined>   Allocate few memory for mbufs (hint for how much memory the board has)
 *   MEMORY_HUGE              <undefined>   Allocate a lot of memory for mbufs (hint for how much memory the board has)
 *                                          If none of MEMORY_CUSTOM/MEMORY_SCARCE/MEMORY_HUGE are defined then a
 *                                          medium amount of memory is allocated for mbufs.
 */
#include <stdio.h>
#include <bsp.h>
#include <rtems/rtems_bsdnet.h>

#include "verscheck.h"

#ifndef NETWORK_TASK_PRIORITY
#define NETWORK_TASK_PRIORITY   150	/* within EPICS' range */
#endif

/* make publicily available for startup scripts... */
const int gesysNetworkTaskPriority = NETWORK_TASK_PRIORITY;

#ifdef  FIXED_IP_ADDR
#define RTEMS_DO_BOOTP 0
#else
#define RTEMS_DO_BOOTP rtems_bsdnet_do_bootp
#define FIXED_IP_ADDR  0
#undef  FIXED_NETMASK
#define FIXED_NETMASK  0
#endif

#ifdef LO_IF_ONLY
#undef NIC_NAME
#elif !defined(NIC_NAME)

#ifdef MULTI_NETDRIVER

#if RTEMS_VERSION_ATLEAST(4,6,99)
#define pcib_init pci_initialize
#endif

extern int rtems_3c509_driver_attach (struct rtems_bsdnet_ifconfig *, int);
extern int rtems_fxp_attach (struct rtems_bsdnet_ifconfig *, int);
extern int rtems_elnk_driver_attach (struct rtems_bsdnet_ifconfig *, int);
extern int rtems_dec21140_driver_attach (struct rtems_bsdnet_ifconfig *, int);

/* these don't probe and will be used even if there's no device :-( */
extern int rtems_ne_driver_attach (struct rtems_bsdnet_ifconfig *, int);
extern int rtems_wd_driver_attach (struct rtems_bsdnet_ifconfig *, int);

static struct rtems_bsdnet_ifconfig isa_netdriver_config[] = {
	{
		"ep0", rtems_3c509_driver_attach, isa_netdriver_config + 1,
	},
	{
		"ne1", rtems_ne_driver_attach, 0, irno: 9 /* qemu cannot configure irq-no :-(; has it hardwired to 9 */
	},
};

static struct rtems_bsdnet_ifconfig pci_netdriver_config[]={
	{
	"fxp1", rtems_fxp_attach, pci_netdriver_config+1,
	},
	{
	"dc1", rtems_dec21140_driver_attach, pci_netdriver_config+2,
	},
	{
	"elnk1", rtems_elnk_driver_attach, isa_netdriver_config,
	},
};

static int pci_check(struct rtems_bsdnet_ifconfig *ocfg, int attaching)
{
struct rtems_bsdnet_ifconfig *cfg;
int if_index_pre;
extern int if_index;
	if ( attaching ) {
		cfg = pcib_init() ? isa_netdriver_config : pci_netdriver_config;
	}
	while ( cfg ) {
		printf("Probing '%s'", cfg->name); fflush(stdout);
		/* unfortunately, the return value is unreliable - some drivers report
		 * success even if they fail.
		 * Check if they chained an interface (ifnet) structure instead
		 */
		if_index_pre = if_index;
		cfg->attach(cfg, attaching);
		if ( if_index > if_index_pre) {
			/* assume success */
			printf(" .. seemed to work\n");
			ocfg->name   = cfg->name;
			ocfg->attach = cfg->attach;
			return 0;
		}
		printf(" .. failed\n");
		cfg = cfg->next;
	}
	return -1;
}

#define NIC_NAME   "dummy"
#define NIC_ATTACH pci_check

#else

/*
 * The following conditionals select the network interface card.
 *
 * By default the network interface specified by the board-support
 * package is used.
 * To use a different NIC for a particular application, copy this file to the
 * application directory and add the appropriate -Dxxxx to the compiler flag.
 * To specify a different NIC on a site-wide basis, add the appropriate
 * flags to the site configuration file for the target.  For example, to
 * specify a 3COM 3C509 for all RTEMS-pc386 targets at your site, add
 *      TARGET_CFLAGS += -DEPICS_RTEMS_NIC_3C509
 * to configure/os/CONFIG_SITE.Common.RTEMS-pc386.
 */
#if defined(EPICS_RTEMS_NIC_3C509)            /* 3COM 3C509 */
  extern int rtems_3c509_driver_attach (struct rtems_bsdnet_ifconfig *, int);
# define NIC_NAME   "ep0"
# define NIC_ATTACH rtems_3c509_driver_attach

#elif defined(RTEMS_BSP_NETWORK_DRIVER_NAME)  /* Use NIC provided by BSP */
# define NIC_NAME   RTEMS_BSP_NETWORK_DRIVER_NAME
# define NIC_ATTACH RTEMS_BSP_NETWORK_DRIVER_ATTACH
#endif

#endif /* ifdef MULTI_NETDRIVER */

#endif /* ifdef LO_IF_ONLY */

#ifdef NIC_NAME

/* provide declaration, just in case */
int
NIC_ATTACH(struct rtems_bsdnet_ifconfig *ifconfig, int attaching);

static struct rtems_bsdnet_ifconfig netdriver_config[1] = {{
    NIC_NAME,	/* name */
    NIC_ATTACH,	/* attach function */
    0,			/* link to next interface */
	FIXED_IP_ADDR,
	FIXED_NETMASK
}};
#else
#ifndef LO_IF_ONLY
#warning "NO KNOWN NETWORK DRIVER FOR THIS BSP -- YOU MAY HAVE TO EDIT rtems_netconfig.c"
#endif
#endif

extern void rtems_bsdnet_loopattach();
static struct rtems_bsdnet_ifconfig loopback_config = {
    "lo0",                          /* name */
    (int (*)(struct rtems_bsdnet_ifconfig *, int))rtems_bsdnet_loopattach, /* attach function */
#ifdef NIC_NAME
    netdriver_config,               /* link to next interface */
#else
    0,                              /* link to next interface */
#endif
    "127.0.0.1",                    /* IP address */
    "255.0.0.0",                    /* IP net mask */
};

struct rtems_bsdnet_config rtems_bsdnet_config = {
    &loopback_config,         /* Network interface */
#ifdef NIC_NAME
    RTEMS_DO_BOOTP,           /* Use BOOTP to get network configuration */
#else
    0,                        /* Use BOOTP to get network configuration */
#endif
    NETWORK_TASK_PRIORITY,    /* Network task priority */
#if   defined(MEMORY_CUSTOM)
	MEMORY_CUSTOM,
#elif defined(MEMORY_SCARCE)
    100*1024,                 /* MBUF space */
    200*1024,                 /* MBUF cluster space */
#elif defined(MEMORY_HUGE)
    2*1024*1024,              /* MBUF space */
    5*1024*1024,              /* MBUF cluster space */
#else
    180*1024,                 /* MBUF space */
    350*1024,                 /* MBUF cluster space */
#endif
};
