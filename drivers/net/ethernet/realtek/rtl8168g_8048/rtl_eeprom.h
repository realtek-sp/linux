/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (c) 2025 Realtek Semiconductor Corp. All rights reserved.
 *
 * This software is a confidential and proprietary property of Realtek
 * Semiconductor Corp. Disclosure, reproduction, redistribution, in
 * whole or in part, of this work and its derivatives without express
 * permission is prohibited.
 *
 * Realtek Semiconductor Corp. reserves the right to update, modify, or
 * discontinue this software at any time without notice. This software is
 * provided "as is" and any express or implied warranties, including, but
 * not limited to, the implied warranties of merchantability and fitness for
 * a particular purpose are disclaimed. In no event shall Realtek
 * Semiconductor Corp. be liable for any direct, indirect, incidental,
 * special, exemplary, or consequential damages (including, but not limited
 * to, procurement of substitute goods or services; loss of use, data, or
 * profits; or business interruption) however caused and on any theory of
 * liability, whether in contract, strict liability, or tort (including
 * negligence or otherwise) arising in any way out of the use of this software,
 * even if advised of the possibility of such damage.
 */

/************************************************************************************
 *	This product is covered by one or more of the following patents:
 *	US6,570,884, US6,115,776, and US6,327,625.
 ***********************************************************************************/

//EEPROM opcodes
#define RTL_EEPROM_READ_OPCODE	06
#define RTL_EEPROM_WRITE_OPCODE 05
#define RTL_EEPROM_ERASE_OPCODE 07
#define RTL_EEPROM_EWEN_OPCODE	19
#define RTL_EEPROM_EWDS_OPCODE	16

#define RTL_CLOCK_RATE 3

void rtl8168_eeprom_type(struct rtl8168_private *tp);
void rtl8168_eeprom_cleanup(struct rtl8168_private *tp);
u16 rtl8168_eeprom_read_sc(struct rtl8168_private *tp, u16 reg);
void rtl8168_eeprom_write_sc(struct rtl8168_private *tp, u16 reg, u16 data);
void rtl8168_shift_out_bits(struct rtl8168_private *tp, int data, int count);
u16 rtl8168_shift_in_bits(struct rtl8168_private *tp);
void rtl8168_raise_clock(struct rtl8168_private *tp, u8 *x);
void rtl8168_lower_clock(struct rtl8168_private *tp, u8 *x);
void rtl8168_stand_by(struct rtl8168_private *tp);
void rtl8168_set_eeprom_sel_low(struct rtl8168_private *tp);
