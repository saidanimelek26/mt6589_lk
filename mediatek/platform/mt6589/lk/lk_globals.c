/*
 * MT6589 LK port: globals normally provided by the kernel display driver.
 *
 * The AUO LCM driver keeps a suspend state flag that the kernel mtkfb driver
 * owns. LK only needs it for its (unused) suspend/resume hooks, so the flag is
 * defined here to keep the driver source unmodified.
 */
unsigned int lcm_suspend_flag = 0;
