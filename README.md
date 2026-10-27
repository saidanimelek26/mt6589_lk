# MT6589 LK bootloader — standalone tree (ASUS project)

Little Kernel (LK) bootloader for the MediaTek MT6589 platform, assembled as a
self-contained tree so it can be built without an Android source checkout.

The code is the 2013-era MTK LK framework (the generation that matches the
MT6589 BSP), the MT6589 platform layer taken from the MT6589 Android 4.4
release (`mediatek/platform/mt6589/lk`), and the ASUS project customisation
(`mediatek/custom/asus`, `mediatek/custom/common`) merged into
`mediatek/custom/out/asus` the same way the MTK build system does it.

## Layout

```
bootable/bootloader/lk/      LK framework (2013) + project/asus.mk + local.mk
mediatek/build/              MTK build system (mkimage, rules, tools)
mediatek/platform/
    rules.mk                 platform selection
    mt6589/lk/               MT6589 platform sources (137 files)
mediatek/config/
    asus/ProjectConfig.mk    project configuration
    common/                  shared config fragments
    mt6589/                  platform config fragments
mediatek/custom/
    asus/                    original ASUS customisation (reference)
    common/                  shared customisation (reference)
    out/asus/                merged tree actually used by the build
```

`out` and `custom` symlinks inside the LK directory point at the merged tree so
that `make/mtk-config.mk` and `../../../mediatek/build/Makefile` resolve without
copying files around.

## Build

```sh
./build_lk.sh
```

or directly:

```sh
make asus TOOLCHAIN_PREFIX=$HOME/toolchains/arm-linux-androideabi-4.9/bin/arm-linux-androideabi-
```

The toolchain is the Android 4.9 `arm-linux-androideabi-` GCC.

Outputs (under `build/build-asus/`):

| File                    | Contents                                          |
|-------------------------|---------------------------------------------------|
| `lk`                    | ELF image (for inspection with objdump)           |
| `lk.bin`                | flashable image: 512-byte MTK header + payload    |
| `lk-no-mtk-header.bin`  | raw payload, no header                            |

## Project configuration (asus)

Key settings from `mediatek/config/asus/ProjectConfig.mk`:

```
MTK_PLATFORM            = MT6589
BOOT_LOGO               = wxga
CUSTOM_LK_LCM           = auo_b079xat02_dsi_vdo
LCM_WIDTH/HEIGHT        = 768 / 1024
MTK_EMMC_SUPPORT        = yes
MTK_KERNEL_POWER_OFF_CHARGING = yes
```

Platform link parameters (`mediatek/platform/mt6589/lk/rules.mk`):

```
MEMBASE      = 0x81E00000
MEMSIZE      = 0x00100000
SCRATCH_ADDR = 0x85000000
MACH_TYPE    = 6589
MMC_SLOT     = 1
```

## Changes made for the standalone build

The tree is not a verbatim copy of a released tree; the following fixes were
required to make it build outside the Android build system:

1. `crt0.S`: `.rodata:` replaced with `.section .rodata` (newer GNU as).
2. `target/mediatek/rules.mk`: added `-Iout/lk/include` so generated headers are
   found.
3. DCT headers exposed at `mediatek/custom/out/asus/kernel/dct/` (the build
   expects them next to the merged custom tree).
4. `makefile`: `BOOTLOADER_OUT ?= build`; `APPSBOOTHEADER` removed from `all`.
5. `project/asus.mk`: `MODULES += app/mt_boot out/lk`.
6. `download_commands.c`: NAND image-type macros for the eMMC build.
7. `cust_display.h`: logo index block (`ANIM_*`, `V2_*`) restored.
8. `partition.c`: EBR2/FAT entries removed — the ASUS eMMC
   `partition_define.h` has no such partitions.
9. Security libraries: `libsec.a`, `libauth.a`, `libsplat.a`, `libdevinfo.a`
   are prebuilt binaries that are not shipped with the kernel BSP, so the link
   is made conditional and `platform/mediatek/mt6589/lk/sec_stub.c` provides
   the four entry points. `sec_usbdl_enabled()` returns 0 (no signed download)
   and `get_devinfo_with_index()` returns 0 (no eFuse data).
10. `lk_globals.c`: defines `lcm_suspend_flag`, a flag normally owned by the
    kernel display driver, so the LCM driver links unmodified.
11. `mediatek/custom/out/asus/lk/rules.mk`: compiles the selected LCM driver
    (`lcm/*/*.c`) — the MTK build normally copies it into `lk/lcm/`.
12. `mediatek/build/tools/mkimage`: the shipped tool is a 32-bit i386 binary
    that cannot run under WSL1. It was replaced by a Python 3 implementation of
    the same 512-byte header (magic `0x58881688`, size, 32-byte name, `0xFF`
    fill). The original is kept as `mkimage.i386.bin`.

## Status

* The image **builds**: `lk` (ELF), `lk.bin` (with MTK header) and
  `lk-no-mtk-header.bin` are produced.
* The image has **not** been booted on hardware. Building is not proof of a
  working bootloader.
* The security/devinfo stubs change runtime behaviour: no secure USB download,
  empty `ATAG_DEVINFO_DATA` for the kernel, no triple-buffering decision based
  on eFuse data.
* The partition table follows the ASUS eMMC layout and does not contain the
  generic `EBR2`/`FAT` entries.
