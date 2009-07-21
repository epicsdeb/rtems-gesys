#ifndef RTEMS_MINIMAL_VERSION_H
#define RTEMS_MINIMAL_VERSION_H
/* $Id: minversion.h,v 1.1 2007/07/28 19:32:14 strauman Exp $ */

/* Macro to detect RTEMS version */
#include <rtems.h>
#include <rtems/system.h>

#define RTEMS_ISMINVERSION(ma,mi,re) \
	(    __RTEMS_MAJOR__  > (ma)	\
	 || (__RTEMS_MAJOR__ == (ma) && __RTEMS_MINOR__  > (mi))	\
	 || (__RTEMS_MAJOR__ == (ma) && __RTEMS_MINOR__ == (mi) && __RTEMS_REVISION__ >= (re)) \
    )

#endif
