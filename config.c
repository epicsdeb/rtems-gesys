/*
 * RTEMS configuration for EPICS
 *  rtems_config.c,v 1.1 2001/08/09 17:54:04 norume Exp
 *      Author: W. Eric Norum
 *              eric@cls.usask.ca
 *              (306) 966-6055
 */

#include <rtems.h>

#include "verscheck.h"

/*
 ***********************************************************************
 *                         RTEMS CONFIGURATION                         *
 ***********************************************************************
 */
/* #define STACK_CHECKER_ON                1 */

#define CONFIGURE_RTEMS_INIT_TASKS_TABLE

#ifdef MEMORY_SCARCE
#define CONFIGURE_EXECUTIVE_RAM_SIZE        MEMORY_SCARCE
#elif defined MEMORY_HUGE
#define CONFIGURE_EXECUTIVE_RAM_SIZE        (15*1024*1024)
#else
#define CONFIGURE_EXECUTIVE_RAM_SIZE        (5*1024*1024)
#endif
#define CONFIGURE_MAXIMUM_TASKS             rtems_resource_unlimited(30)
#define CONFIGURE_MAXIMUM_SEMAPHORES        rtems_resource_unlimited(500)
#define CONFIGURE_MAXIMUM_TIMERS            rtems_resource_unlimited(20)
#define CONFIGURE_MAXIMUM_MESSAGE_QUEUES    rtems_resource_unlimited(5)
#define CONFIGURE_MAXIMUM_PERIODS		    rtems_resource_unlimited(8)
#define CONFIGURE_MAXIMUM_DRIVERS			15
#define CONFIGURE_MAXIMUM_USER_EXTENSIONS	2

#ifdef USE_POSIX
#define CONFIGURE_MAXIMUM_POSIX_THREADS			20
#define CONFIGURE_MAXIMUM_POSIX_MUTEXES			20
#define CONFIGURE_MAXIMUM_POSIX_CONDITION_VARIABLES	20
#define CONFIGURE_MAXIMUM_POSIX_KEYS			20
#define CONFIGURE_MAXIMUM_POSIX_TIMERS			20
#define CONFIGURE_MAXIMUM_POSIX_QUEUED_SIGNALS	20
#define CONFIGURE_MAXIMUM_POSIX_MESSAGE_QUEUES	20
#define CONFIGURE_MAXIMUM_POSIX_SEMAPHORES		20
#endif


#define CONFIGURE_LIBIO_MAXIMUM_FILE_DESCRIPTORS 100
#define CONFIGURE_USE_IMFS_AS_BASE_FILESYSTEM

#define CONFIGURE_MICROSECONDS_PER_TICK 20000

#define CONFIGURE_INIT_TASK_PRIORITY    80

#define CONFIGURE_INIT_TASK_INITIAL_MODES (RTEMS_PREEMPT | \
                    RTEMS_NO_TIMESLICE | \
                    RTEMS_NO_ASR | \
                    RTEMS_INTERRUPT_LEVEL(0))
#define CONFIGURE_INIT_TASK_ATTRIBUTES (RTEMS_FLOATING_POINT | RTEMS_LOCAL)
#ifdef MEMORY_SCARCE
#define CONFIGURE_INIT_TASK_STACK_SIZE  (50*1024)
#else
#define CONFIGURE_INIT_TASK_STACK_SIZE  (100*1024)
#endif
rtems_task Init (rtems_task_argument argument);

#define CONFIGURE_HAS_OWN_DEVICE_DRIVER_TABLE

#if RTEMS_VERSION_ATLEAST(4,6,99)
#include <rtems/console.h>
#include <rtems/clockdrv.h>
#ifdef USE_RTC_DRIVER
#include <rtems/rtc.h>
#endif
#else
#include <console.h>
#include <clockdrv.h>
#ifdef USE_RTC_DRIVER
#include <rtc.h>
#endif
#endif

rtems_driver_address_table Device_drivers[]={
    CONSOLE_DRIVER_TABLE_ENTRY,
    CLOCK_DRIVER_TABLE_ENTRY,
#ifdef USE_RTC_DRIVER
	RTC_DRIVER_TABLE_ENTRY,
#endif
    {0}
};


#define CONFIGURE_INIT
#if RTEMS_VERSION_ATLEAST(4,6,99)
#include <rtems/confdefs.h>
#else
#include <confdefs.h>
#endif
