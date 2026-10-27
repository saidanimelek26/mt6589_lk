/*
 * MT6589 LK port: stubs for the proprietary security/devinfo libraries.
 *
 * The Android release ships libsec.a, libauth.a, libsplat.a and libdevinfo.a
 * as prebuilt binaries. They are not part of the kernel BSP, so a bootloader
 * built from source cannot link them. These stubs keep the image linkable and
 * make the security entry points behave like an unlocked engineering build:
 *
 *   sec_func_init()        - nothing to initialise
 *   sec_usbdl_enabled()    - 0: no signed USB download
 *   sec_usbdl_verify_da()  - 0: accept (only called when the above is 1)
 *   get_devinfo_with_index - 0: no eFuse data
 *
 * The zero devinfo values disable triple buffering (safe default) but also
 * mean the kernel receives an empty ATAG_DEVINFO_DATA block.
 */
#include <platform/mt_typedefs.h>

int sec_func_init(int dev_type)
{
    (void)dev_type;
    return 0;
}

int sec_usbdl_enabled(void)
{
    return 0;
}

int sec_usbdl_verify_da(u8 *da_addr, u32 da_len, u8 *sig_addr, u32 sig_len)
{
    (void)da_addr;
    (void)da_len;
    (void)sig_addr;
    (void)sig_len;
    return 0;
}

u32 get_devinfo_with_index(u32 index)
{
    (void)index;
    return 0;
}
