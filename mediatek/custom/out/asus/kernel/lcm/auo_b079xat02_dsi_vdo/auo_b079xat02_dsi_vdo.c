/*
 * ============================================================================
 *  lcm_auo_b079xat02.c  —  AUO B079XAT02 DSI video-mode LCM driver (LK)
 * ============================================================================
 *  Reconstructed from the LK bootloader binary of the Supra M845G tablet
 *  (MediaTek MT6589/MT8389, firmware build 130823).
 *
 *  Provenance (all addresses are runtime VAs, image base 0x81E00000):
 *    - lk.bin IMG header: magic 0x58881688 "LK", payload 0x3B6F4 bytes @+0x200
 *    - LCM_DRIVER  auo_b079xat02_dsi_vdo_lcm_drv  @ 0x81E387AC
 *        .name           = 0x81E35898  "auo_b079xat02_dsi_vdo"
 *        .set_util_funcs = 0x81E1DA15  (thumb)
 *        .get_params     = 0x81E1D995
 *        .init           = 0x81E1D8C5   (empty function in LK build: "bx lr")
 *        .suspend        = 0x81E1DB59
 *        .resume         = 0x81E1DAA1
 *        (fields after .resume are NULL — no compare_id registered)
 *    - Auto-detect list @ 0x81E387E8 contains exactly this one driver.
 *
 *  Binary-verified LCM_UTIL_FUNCS layout (set_util_funcs memcpy's 0x4C bytes
 *  = 19 pointers into BSS @ 0x81E4538C; source table @ 0x81E3823C):
 *        +0x00  ?                                    -> 0x81E0C368
 *        +0x04  mt_set_gpio_out wrapper              -> 0x81E13944
 *        +0x08  udelay                               -> 0x81E0C1FC -> 0x81E09EA8
 *        +0x0C  mdelay   (used with 5..200 ms)       -> 0x81E0C1F8 -> 0x81E09E90
 *        +0x10  ?                                    -> 0x81E0C2F4
 *        +0x14  ?                                    -> 0x81E0C280
 *        +0x18  ?                                    -> 0x81E0C200
 *        +0x1C  dsi_set_cmdq_V2                      -> 0x81E11188
 *        +0x20  dsi read (lcm_reg)                   -> 0x81E113D4
 *        +0x24  dsi_set_cmdq(cmdline,count,force)    -> 0x81E1107C
 *        +0x28  dsi_write_cmds(cmd)                  -> 0x81E10ED4
 *        +0x2C  dsi_write_regs(addr,data,n)          -> 0x81E10EE4
 *        +0x30  dsi_read_reg      (stub, returns 0)  -> 0x81E10F18
 *        +0x34  dsi_read_reg_v2   (stub, returns 0)  -> 0x81E10F14
 *        +0x38  dsi_dcs_read_lcm_reg_v2              -> 0x81E11704
 *        +0x3C  NULL
 *        +0x40  mt_set_gpio_mode wrapper             -> 0x81E13950
 *        +0x44  mt_set_gpio_dir  wrapper             -> 0x81E1392C
 *        +0x48  mt_set_gpio_?    wrapper             -> 0x81E13938
 *
 *  DSI_clk_HS_mode() is linked directly (not through the util struct):
 *      FUN_81E10C68 — sets/clears bit0 of DSI reg 0x1400D104 (after a
 *      readiness check in FUN_81E10C54) followed by a DSB barrier.
 *
 *  Differences vs. the generic auo_b079xat02 template found on other devices:
 *    1. data_format = RGB888 and PS = packed 24-bit RGB888 (template: RGB666)
 *    2. lcd_power_en() also drives a second power GPIO133 (besides GPIO142)
 *    3. lcd_power_en(1) ends with MDELAY(20)
 *    4. debug prints inside lcd_power_en: "==lk===lcd_power_on/off===\n"
 *    5. a global flag (BSS 0x81E45388) is cleared on resume entry and set
 *       on suspend exit
 *  Timing values (VSA/VBP/VFP/HSA/HBP/HFP, bllp=0x115, pll 0/1/19,
 *  cont_clock=1) are identical to the generic template.
 * ============================================================================
 */

#ifndef BUILD_LK
#include <linux/string.h>
#endif

#ifdef BUILD_LK
#include <platform/mt_gpio.h>
#include <platform/mt_pmic.h>
#else
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>
#endif
#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (768)
#define FRAME_HEIGHT (1024)

#define GPIO_LCD_PWR_EN      GPIO142        /* 0x8E */
#define GPIO_LCD_PWR2_EN     GPIO133        /* 0x85 — second power pin, driven
                                               together with GPIO142 in this
                                               build (exact net name unknown) */
#define GPIO_LCD_RST_EN      GPIO178        /* 0xB2 */
#define GPIO_LCD_BL_EN       GPIO179        /* 0xB3 */

#define LCM_DSI_CMD_MODE     0

/* PMIC (MT6329) — raw register addresses extracted from the binary.
 * Mask/shift pairs match the generic template's VGP6 defines exactly:
 *   0x45A = DIGLDO_CON33 : VGP6_VOSEL, mask 0x7,  shift 5
 *   0x434 = DIGLDO_CON12 : VGP6_EN,    mask 0x1,  shift 15               */
#define DIGLDO_CON33         (0x45A)
#define DIGLDO_CON12         (0x434)

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)     (lcm_util.set_reset_pin((v)))

#define UDELAY(n)            (lcm_util.udelay(n))
#define MDELAY(n)            (lcm_util.mdelay(n))

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)                   \
                            lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)                      \
                            lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)

/* Global state flag touched by suspend/resume.
 * Storage: BSS 0x81E45388 (accessed through GOT entry 0x81E3B6B4).
 * Cleared to 0 at lcm_resume() entry, set to 1 at lcm_suspend() exit. */
extern unsigned int lcm_suspend_flag;   /* name invented — symbol not present */

// ---------------------------------------------------------------------------
//  Local Functions
// ---------------------------------------------------------------------------

/* FUN_81E1D8C8 — GPIO142 + GPIO133 + VGP6 (3.3V) power rail control */
static void lcd_power_en(unsigned char enabled)
{
    if (enabled)
    {
        mt_set_gpio_mode(GPIO_LCD_PWR_EN,  GPIO_MODE_00);
        mt_set_gpio_dir (GPIO_LCD_PWR_EN,  GPIO_DIR_OUT);
        mt_set_gpio_out (GPIO_LCD_PWR_EN,  GPIO_OUT_ONE);

        mt_set_gpio_mode(GPIO_LCD_PWR2_EN, GPIO_MODE_00);
        mt_set_gpio_dir (GPIO_LCD_PWR2_EN, GPIO_DIR_OUT);
        mt_set_gpio_out (GPIO_LCD_PWR2_EN, GPIO_OUT_ONE);

      #ifdef BUILD_LK
        /* VGP6 3.3V — order in binary: VOSEL first, then EN */
        pmic_config_interface(DIGLDO_CON33, 0x07, 0x7, 5); /* PMIC_RG_VGP6_VOSEL */
        pmic_config_interface(DIGLDO_CON12, 0x01, 0x1, 15);/* PMIC_RG_VGP6_EN    */
      #else
        hwPowerOn(MT65XX_POWER_LDO_VGP6, VOL_3300, "LCM");
      #endif

        printf("==lk===lcd_power_on===\n");
        MDELAY(20);          /* present in this build, absent in generic template */
    }
    else
    {
        mt_set_gpio_mode(GPIO_LCD_PWR_EN,  GPIO_MODE_00);
        mt_set_gpio_dir (GPIO_LCD_PWR_EN,  GPIO_DIR_OUT);
        mt_set_gpio_out (GPIO_LCD_PWR_EN,  GPIO_OUT_ZERO);

        mt_set_gpio_mode(GPIO_LCD_PWR2_EN, GPIO_MODE_00);
        mt_set_gpio_dir (GPIO_LCD_PWR2_EN, GPIO_DIR_OUT);
        mt_set_gpio_out (GPIO_LCD_PWR2_EN, GPIO_OUT_ZERO);

      #ifdef BUILD_LK
        /* order in binary: EN off first, then VOSEL */
        pmic_config_interface(DIGLDO_CON12, 0x0, 0x1, 15); /* PMIC_RG_VGP6_EN    */
        pmic_config_interface(DIGLDO_CON33, 0x0, 0x7, 5);  /* PMIC_RG_VGP6_VOSEL */
      #else
        hwPowerDown(MT65XX_POWER_LDO_VGP6, "LCM");
      #endif

        printf("==lk===lcd_power_off===\n");
    }
}

/* FUN_81E1DA28 — reset pin (GPIO178) */
static void lcd_reset(unsigned char enabled)
{
    if (enabled)
    {
        mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
        mt_set_gpio_dir (GPIO_LCD_RST_EN, GPIO_DIR_OUT);
        mt_set_gpio_out (GPIO_LCD_RST_EN, GPIO_OUT_ONE);
    }
    else
    {
        mt_set_gpio_mode(GPIO_LCD_RST_EN, GPIO_MODE_00);
        mt_set_gpio_dir (GPIO_LCD_RST_EN, GPIO_DIR_OUT);
        mt_set_gpio_out (GPIO_LCD_RST_EN, GPIO_OUT_ZERO);
    }
}

/* FUN_81E1DA64 — backlight enable pin (GPIO179) */
static void lcd_bl_en(unsigned char enabled)
{
    if (enabled)
    {
        mt_set_gpio_mode(GPIO_LCD_BL_EN, GPIO_MODE_00);
        mt_set_gpio_dir (GPIO_LCD_BL_EN, GPIO_DIR_OUT);
        mt_set_gpio_out (GPIO_LCD_BL_EN, GPIO_OUT_ONE);
    }
    else
    {
        mt_set_gpio_mode(GPIO_LCD_BL_EN, GPIO_MODE_00);
        mt_set_gpio_dir (GPIO_LCD_BL_EN, GPIO_DIR_OUT);
        mt_set_gpio_out (GPIO_LCD_BL_EN, GPIO_OUT_ZERO);
    }
}

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------

/* FUN_81E1DA14 — binary: memcpy(&lcm_util, util, 0x4C) -> sizeof==19 pointers */
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

/* FUN_81E1D994 — every store below is byte-exact from the binary.
 * sizeof(LCM_PARAMS) in this build = 0x1C4 (memset length).
 * Comments show the absolute offset of each field inside LCM_PARAMS. */
static void lcm_get_params(LCM_PARAMS *params)
{
    memset(params, 0, sizeof(LCM_PARAMS));          /* 0x1C4 bytes */

    params->type   = LCM_TYPE_DSI;                  /* +0x00 = 2   */
    params->width  = FRAME_WIDTH;                   /* +0x08 = 768 */
    params->height = FRAME_HEIGHT;                  /* +0x0C = 1024 */

    params->dsi.mode   = BURST_VDO_MODE;            /* +0xF4 = 3   */

    /* DSI */
    /* Command mode setting */
    params->dsi.LANE_NUM = LCM_FOUR_LANE;           /* +0x104 = 4  */

    /* The following defined the format for data coming from LCD engine. */
    params->dsi.data_format.format = LCM_DSI_FORMAT_RGB888;  /* +0x114 = 2 */

    /* Video mode setting */
    /* +0x11C = 2. Confirmed consumed as the PS argument by the DSI driver
     * (FUN_81E10E64): value 2 maps to HW PS_SEL=3 = packed 24-bit, with
     * word-count 768*3=2304 — consistent with RGB888. In this LK's
     * lcm_drv.h the enum is assumed to be
     *   { PACKED_PS_16BIT_RGB565=0, PACKED_PS_18BIT_RGB666=1,
     *     PACKED_PS_24BIT_RGB888=2, LOOSE_PS_24BIT_RGB666=3 }
     * (note: newer kernel headers swap the last two entries). */
    params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

    params->dsi.vertical_sync_active        = 50;   /* +0x128 */
    params->dsi.vertical_backporch          = 30;   /* +0x12C */
    params->dsi.vertical_frontporch         = 36;   /* +0x130 */
    params->dsi.vertical_active_line        = FRAME_HEIGHT;  /* +0x134 = 1024 */

    params->dsi.horizontal_sync_active      = 64;   /* +0x138 */
    params->dsi.horizontal_backporch        = 56;   /* +0x13C */
    params->dsi.horizontal_frontporch       = 60;   /* +0x140 */
    params->dsi.horizontal_active_pixel     = FRAME_WIDTH;   /* +0x144 = 768 */

    params->dsi.horizontal_bllp             = 0x115;/* +0x148 */

    /* Bit rate calculation */
    params->dsi.pll_div1 = 0;                       /* +0x180 */
    params->dsi.pll_div2 = 1;                       /* +0x184 */
    params->dsi.fbk_div  = 19;                      /* +0x188 */

    params->dsi.cont_clock = 1;                     /* +0x1A4 */
}

extern void DSI_clk_HS_mode(unsigned char enter);

/* Inlined into lcm_resume() in the binary (print at 0x81E1DAF6). */
static void init_lcm_registers(void)
{
    unsigned int data_array[16];

  #ifdef BUILD_LK
    printf("%s, LK \n", __func__);
  #else
    printk("%s, kernel", __func__);
  #endif

    data_array[0] = 0x00010500;  /* DCS 0x01 : software reset        */
    dsi_set_cmdq(data_array, 1, 1);
    MDELAY(10);

    DSI_clk_HS_mode(1);          /* FUN_81E10C68: DSI+0x104 bit0 = 1 */
    MDELAY(80);

    data_array[0] = 0x00290500;  /* DCS 0x29 : display on            */
    dsi_set_cmdq(data_array, 1, 1);
}

/* FUN_81E1D8C4 — LK build does nothing ("bx lr"); kernel side would run
 * the full power-on sequence below. */
static void lcm_init(void)
{
  #ifdef BUILD_LK
    return; /* LK do nothing */
  #else
    lcd_bl_en(0);
    lcd_reset(0);
    lcd_power_en(0);
    lcd_power_en(1);
    MDELAY(50);              /* Must > 5ms */
    lcd_reset(1);
    MDELAY(200);             /* Must > 50ms */
    init_lcm_registers();
    lcd_bl_en(1);
  #endif
}

/* FUN_81E1DB58 — source line 301 (from the "%d" of its debug print). */
static void lcm_suspend(void)
{
    unsigned int data_array[16];

    printf("LK :%s, %d \n", "lcm_suspend", 301);

    data_array[0] = 0x00280500;  /* DCS 0x28 : display off */
    dsi_set_cmdq(data_array, 1, 1);
    MDELAY(5);

    lcd_bl_en(0);
    lcd_reset(0);
    lcd_power_en(0);

    DSI_clk_HS_mode(0);          /* FUN_81E10C68: DSI+0x104 bit0 = 0 */
    MDELAY(10);

    lcm_suspend_flag = 1;        /* BSS 0x81E45388 */
}

/* FUN_81E1DAA0 — source line 322 (from the "%d" of its debug print).
 * The binary inlines init_lcm_registers() here. */
static void lcm_resume(void)
{
    printf("LK :%s, %d \n", "lcm_resume", 322);

    lcm_suspend_flag = 0;        /* BSS 0x81E45388 */

    lcd_bl_en(0);
    lcd_reset(0);
    lcd_power_en(0);
    lcd_power_en(1);
    MDELAY(50);                  /* Must > 5ms  (plus the MDELAY(20) inside
                                    lcd_power_en(1) of this build) */
    lcd_reset(1);
    MDELAY(200);                 /* Must > 50ms */
    init_lcm_registers();
    lcd_bl_en(1);
}

// ---------------------------------------------------------------------------
//  Driver Interface (struct image @ 0x81E387AC, pointers are thumb +1)
// ---------------------------------------------------------------------------

LCM_DRIVER auo_b079xat02_dsi_vdo_lcm_drv =
{
    .name          = "auo_b079xat02_dsi_vdo",
    .set_util_funcs= lcm_set_util_funcs,   /* 0x81E1DA15 */
    .get_params    = lcm_get_params,       /* 0x81E1D995 */
    .init          = lcm_init,             /* 0x81E1D8C5 */
    .suspend       = lcm_suspend,          /* 0x81E1DB59 */
    .resume        = lcm_resume,           /* 0x81E1DAA1 */
    /* .compare_id  — NULL in this binary */
};
