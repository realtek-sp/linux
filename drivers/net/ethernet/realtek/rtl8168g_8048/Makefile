################################################################################
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (c) 2025 Realtek Semiconductor Corp. All rights reserved.
#
# This software is a confidential and proprietary property of Realtek
# Semiconductor Corp. Disclosure, reproduction, redistribution, in
# whole or in part, of this work and its derivatives without express
# permission is prohibited.
#
# Realtek Semiconductor Corp. reserves the right to update, modify, or
# discontinue this software at any time without notice. This software is
# provided "as is" and any express or implied warranties, including, but
# not limited to, the implied warranties of merchantability and fitness for
# a particular purpose are disclaimed. In no event shall Realtek
# Semiconductor Corp. be liable for any direct, indirect, incidental,
# special, exemplary, or consequential damages (including, but not limited
# to, procurement of substitute goods or services; loss of use, data, or
# profits; or business interruption) however caused and on any theory of
# liability, whether in contract, strict liability, or tort (including
# negligence or otherwise) arising in any way out of the use of this software,
# even if advised of the possibility of such damage.
################################################################################

################################################################################
#  This product is covered by one or more of the following patents:
#  US6,570,884, US6,115,776, and US6,327,625.
################################################################################

CONFIG_SOC_LAN = n
ENABLE_FIBER_SUPPORT = n
ENABLE_REALWOW_SUPPORT = n
ENABLE_DASH_SUPPORT = n
ENABLE_DASH_PRINTER_SUPPORT = n
CONFIG_DOWN_SPEED_100 = n
CONFIG_ASPM = y
ENABLE_S5WOL = y
ENABLE_S5_KEEP_CURR_MAC = n
ENABLE_EEE = y
ENABLE_S0_MAGIC_PACKET = n

ifneq ($(KERNELRELEASE),)
	obj-y := r8168.o
	r8168-objs := r8168_n.o r8168_asf.o rtl_eeprom.o rtltool.o
	ifeq ($(CONFIG_SOC_LAN), y)
		EXTRA_CFLAGS += -DCONFIG_SOC_LAN
	endif
	ifeq ($(ENABLE_FIBER_SUPPORT), y)
		r8168-objs += r8168_fiber.o
		EXTRA_CFLAGS += -DENABLE_FIBER_SUPPORT
	endif
	ifeq ($(ENABLE_REALWOW_SUPPORT), y)
		r8168-objs += r8168_realwow.o
		EXTRA_CFLAGS += -DENABLE_REALWOW_SUPPORT
	endif
	ifeq ($(ENABLE_DASH_SUPPORT), y)
		r8168-objs += r8168_dash.o
		EXTRA_CFLAGS += -DENABLE_DASH_SUPPORT
	endif
	ifeq ($(ENABLE_DASH_PRINTER_SUPPORT), y)
		r8168-objs += r8168_dash.o
		EXTRA_CFLAGS += -DENABLE_DASH_SUPPORT -DENABLE_DASH_PRINTER_SUPPORT
	endif
	EXTRA_CFLAGS += -DCONFIG_R8168_NAPI
	EXTRA_CFLAGS += -DCONFIG_R8168_VLAN
	ifeq ($(CONFIG_DOWN_SPEED_100), y)
		EXTRA_CFLAGS += -DCONFIG_DOWN_SPEED_100
	endif
	ifeq ($(CONFIG_ASPM), y)
		EXTRA_CFLAGS += -DCONFIG_ASPM
	endif
	ifeq ($(ENABLE_S5WOL), y)
		EXTRA_CFLAGS += -DENABLE_S5WOL
	endif
	ifeq ($(ENABLE_S5_KEEP_CURR_MAC), y)
		EXTRA_CFLAGS += -DENABLE_S5_KEEP_CURR_MAC
	endif
	ifeq ($(ENABLE_EEE), y)
		EXTRA_CFLAGS += -DENABLE_EEE
	endif
	ifeq ($(ENABLE_S0_MAGIC_PACKET), y)
		EXTRA_CFLAGS += -DENABLE_S0_MAGIC_PACKET
	endif
else
	BASEDIR := /lib/modules/$(shell uname -r)
	KERNELDIR ?= $(BASEDIR)/build
	PWD :=$(shell pwd)
	DRIVERDIR := $(shell find $(BASEDIR)/kernel/drivers/net/ethernet -name realtek -type d)
	ifeq ($(DRIVERDIR),)
		DRIVERDIR := $(shell find $(BASEDIR)/kernel/drivers/net -name realtek -type d)
	endif
	ifeq ($(DRIVERDIR),)
		DRIVERDIR := $(BASEDIR)/kernel/drivers/net
	endif
	RTKDIR := $(subst $(BASEDIR)/,,$(DRIVERDIR))

	KERNEL_GCC_VERSION := $(shell cat /proc/version | sed -n 's/.*gcc version \([[:digit:]]\.[[:digit:]]\.[[:digit:]]\).*/\1/p')
	CCVERSION = $(shell $(CC) -dumpversion)

	KVER = $(shell uname -r)
	KMAJ = $(shell echo $(KVER) | \
	sed -e 's/^\([0-9][0-9]*\)\.[0-9][0-9]*\.[0-9][0-9]*.*/\1/')
	KMIN = $(shell echo $(KVER) | \
	sed -e 's/^[0-9][0-9]*\.\([0-9][0-9]*\)\.[0-9][0-9]*.*/\1/')
	KREV = $(shell echo $(KVER) | \
	sed -e 's/^[0-9][0-9]*\.[0-9][0-9]*\.\([0-9][0-9]*\).*/\1/')

	kver_ge = $(shell \
	echo test | awk '{if($(KMAJ) < $(1)) {print 0} else { \
	if($(KMAJ) > $(1)) {print 1} else { \
	if($(KMIN) < $(2)) {print 0} else { \
	if($(KMIN) > $(2)) {print 1} else { \
	if($(KREV) < $(3)) {print 0} else { print 1 } \
	}}}}}' \
	)

.PHONY: all
all: print_vars clean modules install

print_vars:
	@echo
	@echo "CC: " $(CC)
	@echo "CCVERSION: " $(CCVERSION)
	@echo "KERNEL_GCC_VERSION: " $(KERNEL_GCC_VERSION)
	@echo "KVER: " $(KVER)
	@echo "KMAJ: " $(KMAJ)
	@echo "KMIN: " $(KMIN)
	@echo "KREV: " $(KREV)
	@echo "BASEDIR: " $(BASEDIR)
	@echo "DRIVERDIR: " $(DRIVERDIR)
	@echo "PWD: " $(PWD)
	@echo "RTKDIR: " $(RTKDIR)
	@echo

.PHONY:modules
modules:
#ifeq ($(call kver_ge,5,0,0),1)
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
#else
#	$(MAKE) -C $(KERNELDIR) SUBDIRS=$(PWD) modules
#endif

.PHONY:clean
clean:
#ifeq ($(call kver_ge,5,0,0),1)
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean
#else
#	$(MAKE) -C $(KERNELDIR) SUBDIRS=$(PWD) clean
#endif

.PHONY:install
install:
#ifeq ($(call kver_ge,5,0,0),1)
	$(MAKE) -C $(KERNELDIR) M=$(PWD) INSTALL_MOD_DIR=$(RTKDIR) modules_install
#else
#	$(MAKE) -C $(KERNELDIR) SUBDIRS=$(PWD) INSTALL_MOD_DIR=$(RTKDIR) modules_install
#endif

endif
