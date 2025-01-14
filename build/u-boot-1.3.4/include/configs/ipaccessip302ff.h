/*****************************************************************************
 * BSP Version: 3.2.4, RevisionID: ac30f57, Date: 20100223 17:42:05
 *****************************************************************************/

/*!
* \file picochippc7302.h
* \brief Configuration file for U-Boot on the PC7302 platform.
*
* Copyright (c) 2006-2009 picoChip Designs Ltd
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* All enquiries to support@picochip.com
*/

#ifndef __CONFIG_H
#define __CONFIG_H

#include <asm/arch/pc302.h>
#include <asm/arch/sizes.h>
#include <asm/arch/uart.h>

/*-----------------------------------------------------------------------------
 * Platform Identification Stuff
 */
#define PICOCHIP "picochip"

/* Which hardware platform I am destined for */
#define PICOCHIP_PLATFORM "pc7302"

/* Specific version of this build */
#ifndef PICOCHIP_PLATFORM_VERSION
#define PICOCHIP_PLATFORM_VERSION "3.2.4"
#endif /* PICOCHIP_PLATFORM_VERSION */

#if 0
#define CONFIG_IDENT_STRING " "PICOCHIP"-"PICOCHIP_PLATFORM_VERSION \
                            "-"PICOCHIP_PLATFORM
#endif

/*-----------------------------------------------------------------------------
 * High Level Configuration Options
 */
/* Running on picoChip PC302 Silicon */
#define CONFIG_PICOCHIP_PC302

/* Running on a picoChip PC7302 platform */
#define CONFIG_PICOCHIP_PC7302

/* Define this if running code in PC302 rtl simulation land */
#undef CONFIG_PC302_SIMULATION

/* Define to disable the verify after U-Boot has relocated from ROM to RAM */
#undef CONFIG_SKIP_VERIFY_RELOCATE_UBOOT

/* Bootable Flash memory has to live here (/ebi_decode0) */
#define PC302_BOOTABLE_FLASH_BASE   (PC302_FLASH_BASE)

/* Base address of the onchip SRAM */
#define PC302_ONCHIP_SRAM_BASE      (PC302_SRAM_BASE)
#define PC302_ONCHIP_SRAM_SIZE      (PC302_SRAM_SIZE)

/* ARM Sub-system peripherals are clocked at 200MHz */
#define PC302_AHB_CLOCK_FREQ        (200000000)

/* Don't use Interrupts */
#undef CONFIG_USE_IRQ

/* Onchip timer runs at this frequency */
#define CFG_HZ			    (PC302_AHB_CLOCK_FREQ)
#define CONFIG_SYS_HZ               (CFG_HZ)

/* Display board info */
#define CONFIG_DISPLAY_BOARDINFO    (1)

/* Are we are going to be running from RAM ? */
#ifdef CONFIG_RUN_FROM_RAM
#define CONFIG_SKIP_LOWLEVEL_INIT
#define CONFIG_SKIP_RELOCATE_UBOOT
#endif /* CONFIG_RUN_FROM_RAM */

/*-----------------------------------------------------------------------
 * Stack Sizes
 *
 * The stack sizes are set up in start.S using the settings below
 */
#if defined(CONFIG_PC302_SIMULATION)
/* We are running in RTL simulation land, reduce the stack sizes
   so we do not have any memory model problems */
#define CONFIG_STACKSIZE	(SZ_1K)             /* regular stack */
#ifdef CONFIG_USE_IRQ
#define CONFIG_STACKSIZE_IRQ	(SZ_128)            /* IRQ stack */
#define CONFIG_STACKSIZE_FIQ	(SZ_128)    	    /* FIQ stack */
#endif /* CONFIG_USE_IRQ */
#else
/* We are running on a real hardware platform, therefore set the stack
   sizes to their 'correct' values */
#define CONFIG_STACKSIZE	(SZ_256K) 	    /* regular stack */
#ifdef CONFIG_USE_IRQ
#define CONFIG_STACKSIZE_IRQ	(SZ_4K)             /* IRQ stack */
#define CONFIG_STACKSIZE_FIQ	(SZ_4K)    	    /* FIQ stack */
#endif /* CONFIG_USE_IRQ */

#endif  /* CONFIG_PC302_SIMULATION */

/*-----------------------------------------------------------------------------
 * Size of malloc() pool
 */
#if defined(CONFIG_PC302_SIMULATION)
/* We are running in RTL simulation land */

/* Memory size for use by malloc () */
#define CFG_MALLOC_LEN		(SZ_4K)
#else
/* We are running on a real hardware platform */

/* Memory size for use by malloc () */
#define CFG_MALLOC_LEN		(SZ_256K)
#endif  /* CONFIG_PC302_SIMULATION */

/* Size in bytes reserved for initial data */
#define CFG_GBL_DATA_SIZE	(SZ_256)

/*-----------------------------------------------------------------------------
 * Linux Kernel Stuff
 */
/* Allow passing of command line args (bootargs) to the linux kernel*/
#define CONFIG_CMDLINE_TAG          1
#define CONFIG_SETUP_MEMORY_TAGS    1

/*-----------------------------------------------------------------------------
 * DDR2 RAM Memory Map
 */
/* We have 1 linear addressable RAM bank */
#define CONFIG_NR_DRAM_BANKS    (1)
#define PHYS_SDRAM_1		(PC302_DDRBANK_BASE)
#define PHYS_SDRAM_1_SIZE	(SZ_128M)

/*-----------------------------------------------------------------------------
 * Flash Memory Stuff
 */
#ifndef CONFIG_RUN_FROM_RAM

/* Define Flash memory sector size */
#define FLASH_SECTOR_SIZE	(SZ_128K)

#define CFG_FLASH_CFI
#define CFG_FLASH_CFI_DRIVER
#define CFG_FLASH_EMPTY_INFO

/* Provide a much improved performance when writing to the Flash */
#define CFG_FLASH_USE_BUFFER_WRITE

#define CFG_FLASH_BASE		(PC302_BOOTABLE_FLASH_BASE)

/* Maximum number of memory banks (devices) */
#define CFG_MAX_FLASH_BANKS	(1)

/* Maximum number of sectors per flash device */
#define CFG_MAX_FLASH_SECT	(1024)

/* Timeouts for Flash Erasing and writing */
#define CFG_FLASH_ERASE_TOUT	(2*CFG_HZ)
#define CFG_FLASH_WRITE_TOUT	(2*CFG_HZ)

/* U-Boot occupies 3 Flash sectors */
#define CFG_MONITOR_LEN		(3*FLASH_SECTOR_SIZE)

/* U-Boot does not live in the bottom of the Flash memory any more */
#define CFG_MONITOR_BASE        (CFG_FLASH_BASE+0x03E00000)

#else

/* No flash memory in the system */
#define CFG_NO_FLASH

#endif /* CONFIG_RUN_FROM_RAM */

/*-----------------------------------------------------------------------------
 * NAND Flash Memory Stuff
 */
#define CONFIG_SYS_NAND_BASE        PC302_EBI_CS2_BASE
#define CONFIG_SYS_NAND_MAX_CHIPS   1
#define CONFIG_SYS_MAX_NAND_DEVICE  1

#define NAND_FLASH_SECTOR_SIZE      (SZ_128K)

/* Pin definitions for NAND Flash control signals are
 * defined in file board/picochip/pc7302/mt29f2g08aadwp.c
 */

/* Include support / commands for NAND Flash
 *
 * Note: Please read the comments in file
 *       board/picochip/pc7302/mt29f2g08aadwp.c about gpio pins used
 *       and PC302 booting modes before defining CONFIG_CMD_NAND
 */
// #define CONFIG_CMD_NAND

/*-----------------------------------------------------------------------------
 * SPI Flash Memory Stuff
 */
#define CFG_DW_SPI

/* Include generic support for SPI Flash memory devices */
#define CONFIG_SPI_FLASH

/* Include support for SPI Flash memory devices from Spansion */
#define CONFIG_SPI_FLASH_SPANSION

/* Include support for SPI Flash memory devices from AMIC */
#undef CONFIG_SPI_FLASH_AMIC

/* Include support for SPI Flash memory devices from ST Micro/Numonyx */
#define CONFIG_SPI_FLASH_STMICRO

/* Include support for SPI Flash memory devices from EON */
#define CONFIG_SPI_FLASH_EON

/* flash layout for 64Mb+ boards - note that some of the partition definitions
   specify addresses as we do not want MTD partition numbering to be sequential.
   definitions without addresses follow directly after the previous parititon.
 */
#define MTDPARTS_64M  "384K@0x03e00000(uBoot),256K@0x40000(env),2M(kernel1),2M(kernel2),3584K(config)," \
                      "27M(FS1),27M(FS2),256K@0(fsboot),128K@0x03e60000(oem_divert2),256K(oem_data1)," \
                      "256K(oem_data2),256K(oem_lib1),256K(oem_lib2),256K(resv),256K(ipa_calib)"

#define MTDPARTS_DEFAULT MTDPARTS_64M

/*-----------------------------------------------------------------------------
 * U-Boot Environment Stuff
 */
#if defined(CONFIG_PC302_SIMULATION)

/* We are running in RTL simulation land */

/* No writable environment available in RTL sim land */
#define CFG_ENV_IS_NOWHERE      (1)
#define CFG_ENV_ADDR            (PC302_BOOTABLE_FLASH_BASE + FLASH_SECTOR_SIZE)

/* One flash sector for environment info */
#define CFG_ENV_SECT_SIZE       (FLASH_SECTOR_SIZE)

/* But 2KBytes is sufficient */
#define CFG_ENV_SIZE		(SZ_2K)

/* Turn off wite protection for vendor parameters */
#define CONFIG_ENV_OVERWRITE

#else

/* We are running on a real hardware platform */
#ifndef CONFIG_RUN_FROM_RAM

/* Environment variables stored in Flash memory */
#define CFG_ENV_IS_IN_FLASH     (1)
#define CFG_ENV_ADDR            (PC302_BOOTABLE_FLASH_BASE+(2*FLASH_SECTOR_SIZE))

/* One flash sector for environment info */
#define CFG_ENV_SECT_SIZE       (FLASH_SECTOR_SIZE)

/* But 64 KBytes is sufficient */
/* #define CFG_ENV_SIZE		(SZ_64K) */
/* Better to leave env occupying a whole sector as it reduces number of writes */

/* Turn off wite protection for vendor parameters */
#define CONFIG_ENV_OVERWRITE

/* Defining CFG_ENV_ADDR_REDUND enables redundant environment */
#define CFG_ENV_ADDR_REDUND     (CFG_ENV_ADDR + FLASH_SECTOR_SIZE)

#define CONFIG_FALLBACK_TO_NONREDUND_ENV        /* Enable for upgrades from old env format */

#elif defined(CONFIG_CMD_NAND)

/* We are runing from ram with NAND support */

/* NAND Flash memory map
 *
 *  Block 0 U-Boot image
 *  Block 1 Redundant U-Boot image
 *  Block 2 Spare
 *  Block 3 Spare
 *  Block 4 U-Boot Environment
 *  Block 5 Redundant U-Boot environment
 *  Block 6 Spare
 *  Block 7 Spare
 *  Block 8 Linux kernel
 *
 */
#define CFG_ENV_IS_IN_NAND      (1)
#define CFG_ENV_OFFSET          (NAND_FLASH_SECTOR_SIZE * 4)
#define CFG_ENV_SIZE            (NAND_FLASH_SECTOR_SIZE)
#define CFG_ENV_OFFSET_REDUND   (NAND_FLASH_SECTOR_SIZE * 5)

/* Turn off wite protection for vendor parameters */
#define CONFIG_ENV_OVERWRITE

#else

/* We are running from ram, therefore no environment */
#define CFG_ENV_IS_NOWHERE

/* Need to define this for the build to be successful */
#define CFG_ENV_SIZE		(SZ_64K)

#endif /* CONFIG_RUN_FROM_RAM */

#endif /* CONFIG_PC302_SIMULATION */

/*-----------------------------------------------------------------------------
 * Timer Stuff
 */
#define CFG_TIMERBASE           (PC302_TIMER_BASE)

/*-----------------------------------------------------------------------------
 * Ethernet Stuff
 */
#define CFG_DW_EMAC
#define CONFIG_PHY_ADDR         (1)
#define CONFIG_NET_MULTI

/*-----------------------------------------------------------------------------
 * Serial Port Stuff
 */
#define CFG_DW_APB_UART

/* Baud rate generators clock with a 3.6864 MHz clock */
#define CONFIG_DW_APB_UART_CLOCK    (3686400)

/* Console on Uart #0 */
#define CONFIG_CONS_INDEX	    (1)
#define CFG_BAUDRATE_TABLE	    { 9600, 19200, 38400, 57600, 115200, 230400 }

/*-----------------------------------------------------------------------------
 * Watchdog Stuff
 */
/* Use the GPIO reset method as supported on this hardware platform
   Note: Undef this macro to use the 'fallback' watcdog reset method */
#undef CONFIG_USE_GPIO_RESET_METHOD

/*-----------------------------------------------------------------------------
 * U-Boot Memory Test (mtest command) Stuff
 */
/* Default start address for memory test */
#define CFG_MEMTEST_START	(PC302_ONCHIP_SRAM_BASE)

/* Default end address for memory test */
#define CFG_MEMTEST_END		(CFG_MEMTEST_START + PC302_ONCHIP_SRAM_SIZE - 1)

/* Define this to use the super duper memory test */
#define CFG_ALT_MEMTEST

/* Use Uart #1 scratch pad reg */
#define CFG_MEMTEST_SCRATCH     (PC302_UART1_BASE + UART_SCRATCH_REG_OFFSET)

/*-----------------------------------------------------------------------------
 * U-Boot Supported Commands
 */
#include "config_cmd_default.h"

#define CONFIG_CMD_PING

/* Include commands for SPI bus */
#undef CONFIG_CMD_SPI

/* Include commands for SPI Flash memory */
#define CONFIG_CMD_SF

/* Include commands for BSP specific command */
#define CONFIG_CMD_BSP

#undef CONFIG_CMD_AUTOSCRIPT
#undef CONFIG_CMD_BOOTD
#undef CONFIG_CMD_CONSOLE
#undef CONFIG_CMD_ECHO
#undef CONFIG_CMD_FPGA
#undef CONFIG_CMD_IMLS
#undef CONFIG_CMD_ITEST
#undef CONFIG_CMD_LOADB
#undef CONFIG_CMD_LOADS
#undef CONFIG_CMD_MISC
#undef CONFIG_CMD_NFS
#undef CONFIG_CMD_SETGETDCR
#undef CONFIG_CMD_XIMG

#ifdef CFG_NO_FLASH
#undef CONFIG_CMD_FLASH
#endif /* CFG_NO_FLASH */

#ifdef CFG_ENV_IS_NOWHERE
#undef CONFIG_CMD_ENV
#endif /* CFG_ENV_IS_NOWHERE */


#if defined(CONFIG_PC302_SIMULATION)
/* We are running in RTL simulation land */

/* Do not use the HUSH parser */
#undef  CFG_HUSH_PARSER
#else
/* We are running on a real hardware platform */

/* Use the HUSH parser */
#define CFG_HUSH_PARSER
#endif /* CONFIG_PC302_SIMULATION */

#ifdef  CFG_HUSH_PARSER
/* This defines the secondary prompt string */
#define CFG_PROMPT_HUSH_PS2 "> "
#endif /* CFG_HUSH_PARSER */

/* Enable command line editing and history */
#define CONFIG_CMDLINE_EDITING

/* Enable gpio test commands */
#define CONFIG_CMD_PC302_GPIO

/*-----------------------------------------------------------------------------
 * Miscellaneous Configurable Options...
 */
/* Use 'long' help messages */
#define CFG_LONGHELP

/* Monitor Command Prompt */
#define CFG_PROMPT	"=> "

/* Console I/O Buffer Size*/
#define CFG_CBSIZE	(SZ_1K)

/* Print buffer size */
#define CFG_PBSIZE	(CFG_CBSIZE+sizeof(CFG_PROMPT)+16)

/* Maximum number of command args */
#define CFG_MAXARGS	(16)
#define CONFIG_SYS_MAXARGS (CFG_MAXARGS)

#define CONFIG_SYS_HELP_CMD_WIDTH (79)

/* Boot Argument Buffer Size */
#define CFG_BARGSIZE	(CFG_CBSIZE)

/* everything, incl board info, in Hz */
#undef	CFG_CLKS_IN_HZ

/* Default load address for bootm and friends */
#define CFG_LOAD_ADDR	        (0x00200000)
#define CONFIG_SYS_LOAD_ADDR    (CFG_LOAD_ADDR)
/*-----------------------------------------------------------------------
 * Environment Configuration
 */

/* Note: The MAC Address defined by 'CONFIG_ETHADDR' is based on
 * picoChip's OUI,see http://standards.ieee.org/regauth/oui/index.shtml
 * for more information. It will need to be modified for each and every
 * individual hardware platform.
 */

#if defined(CFG_DW_EMAC)

/* picoChip OUI, will need noodling by users */
#define CONFIG_ETHADDR          00:02:95:FF:FD:FD

/* picoChip default for testing, will need noodling by users */
#define CONFIG_IPADDR           172.28.11.254

#define CONFIG_HOSTNAME	        ip302ff
#define CONFIG_ROOTPATH	        /exports/home/nm1/pc7302_test
#define CONFIG_BOOTFILE	        uImage-pc7302_test

#define CONFIG_SERVERIP         172.28.1.211
#define CONFIG_GATEWAYIP        172.28.0.254
#define CONFIG_NETMASK          255.255.0.0

#endif /* CFG_DW_EMAC */

/* Second flash sector... */
#define CFG_FLASH_KERNEL_BASE   0x40080000
#define CFG_FLASH_KERNEL_BASE2  0x40280000

/* This is the offset from the start of NAND Flash
 * to where the Linux kernel can be found.
 */
#define NAND_KERNEL_OFFSET      0x00100000

/* Default location for tftp and bootm */
#define CONFIG_LOADADDR         0x00200000

#if defined(CONFIG_PC302_SIMULATION)
/* We are running in RTL simulation land */

/* Time in seconds before autoboot, -1 disables auto-boot */
#define CONFIG_BOOTDELAY        -1
#else
/* We are running on a real hardware platform */

/* Time in seconds before autoboot, -1 disables auto-boot */
#define CONFIG_BOOTDELAY        5
#endif  /* CONFIG_PC302_SIMULATION */

/* The boot command will set bootargs */
#undef  CONFIG_BOOTARGS

/* Default console baud rate */
#define CONFIG_BAUDRATE	        115200

#define CONFIG_BOOTCOUNT_LIMIT  4
#define CONFIG_BOOT_RETRY_TIME  -1
#undef CONFIG_BOOT_RETRY_TIME
#define CONFIG_RESET_TO_RETRY

/* Unless specified here we'll just rely on the kernel default */
#define OTHERBOOTARGS

#define CONFIG_EXTRA_ENV_SETTINGS                                               \
   "othbootargs=panic=1 " MK_STR (OTHERBOOTARGS) "\0"                           \
   "netdev=eth0\0"                                                              \
   "consoledev=/dev/console\0"                                                  \
   "bootlimit=" MK_STR(CONFIG_BOOTCOUNT_LIMIT) "\0"                             \
   "kernel_nand_offset=" MK_STR(NAND_KERNEL_OFFSET) "\0"                        \
   "mtdparts=physmap-flash.0:" MTDPARTS_DEFAULT "\0"                            \
   "nfs_args=setenv bootargs root=/dev/nfs rw nfsroot=$serverip:$rootpath"      \
   " ip=$ipaddr:$serverip:$gatewayip:$netmask:$hostname:$netdev:any"            \
   " console=$consoledev,$baudrate $othbootargs mtdparts=$mtdparts;\0"          \
   "fixed_nfs=run nfs_args; tftp; bootm\0"                                      \
   "nand_jffs2=run nand_jffs2_args; nboot $loadaddr 0 "                         \
   "$kernel_nand_offset; bootm $loadaddr\0"                                     \
   "nand_jffs2_args=setenv bootargs root=/dev/mtdblock6 rw rootfstype=jffs2 "   \
   "ip=$ipaddr:$serverip:$gatewayip:$netmask:$hostname:$netdev:any "            \
   "console=$consoledev,$baudrate $othbootargs;\0"                              \
   "flash_args=setenv bootargs"                                                 \
   " root=$rootdev ro rootfstype=cramfs,jffs2"                                  \
   " ip=$ipaddr:$serverip:$gatewayip:$netmask:$hostname:$netdev:any"            \
   " console=$consoledev,$baudrate $othbootargs mtdparts=$mtdparts;\0"          \
   "set_args_1=setenv kernel_addr " MK_STR(CFG_FLASH_KERNEL_BASE) ";"           \
   " setenv rootdev /dev/mtdblock5\0"                                           \
   "set_args_2=setenv kernel_addr " MK_STR(CFG_FLASH_KERNEL_BASE2) ";"          \
   " setenv rootdev /dev/mtdblock6\0"                                           \
   "check_bank=if test -z $bank; then setenv bank 1; fi\0"                      \
   "bootflash=run check_bank;"                                                  \
   " if test $bank -eq 1; then run set_args_1; else run set_args_2; fi;"        \
   " run flash_args; bootm $kernel_addr || run altbootcmd\0"                    \
   "altbootcmd=run check_bank;"                                                 \
   " if test $bank -eq 1; then run set_args_2; else run set_args_1; fi;"        \
   " run flash_args; bootm $kernel_addr || set_led red\0"

#define CONFIG_NFSBOOTCOMMAND                                           \
   "setenv bootargs root=/dev/nfs rw "                                  \
   "nfsroot=$serverip:$rootpath "                                       \
   "ip=$ipaddr:$serverip:$gatewayip:$netmask:$hostname:$netdev:any "    \
   "console=$consoledev,$baudrate $othbootargs;"                        \
   "bootm $kernel_flash_addr"

#define CONFIG_RAMBOOTCOMMAND                                           \
   "setenv bootargs root=/dev/ram rw "                                  \
   "console=$consoledev,$baudrate $othbootargs;"                        \
   "tftp $ramdiskaddr $ramdiskfile;"                                    \
   "tftp $loadaddr $bootfile;"                                          \
   "bootm $loadaddr $ramdiskaddr"

/* Define as "run bootflash" for on-board boot from parallel nor flash,
 * or "run nand_jffs2" for on-board boot from nand flash or
 * "run fixed_nfs" for standard NFS with fixed IP address.
 */
#if defined(CONFIG_CMD_NAND)

#define CONFIG_BOOTCOMMAND  "run nand_jffs2"

#else

#define CONFIG_BOOTCOMMAND  "run bootflash"


#endif

/* perform digital signature verification on the kernel image
 * before passing control to it.
 */
#define CONFIG_VERIFY_KERNEL	 1
/* address of stand alone application which performs the digital
 * signature verification.
 */
#define VALIDATE_APP_ADDR        (CFG_MONITOR_BASE + CFG_MONITOR_LEN - SZ_64K - SZ_128K)

#endif /* __CONFIG_H */

