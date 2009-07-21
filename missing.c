/* Create weak alias for BSPs which don't have a RTC driver;
 * This just creates an empty driver table slot which
 * can be filled by a dynamically allocated driver
 * (see sapi/src/io.c sapi/src/ioregisterdriver.c)
 * why this works.
 */
__asm__(
"	.weak  rtc_initialize    \n"
"	.equ   rtc_initialize, 0 \n"
);
