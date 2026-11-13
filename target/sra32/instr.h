/*
 * QEMU SRA32 Instructions header
 *
 * Copyright (c) 2026 Astrido (asterd-og)
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QEMU_SRA32_INSTR_H
#define QEMU_SRA32_INSTR_H

#define INSTR_HINT_MASK (0x3 << 30)

#define INSTR_OPC_MASK_R (0xF << 26)
#define INSTR_OPC_MASK_I (0x7 << 27)
#define INSTR_OPC_MASK_J (0x3 << 28)

#define INSTR_RD_MASK_R (0x1F << 21)
#define INSTR_RT_MASK_R (0x1F << 16)
#define INSTR_RS_MASK_R (0x1F << 11)

#define INSTR_RD_MASK_I (0x1F << 22)
#define INSTR_RT_MASK_I (0x1F << 17)

#define INSTR_RD_MASK_U (0x1F << 25)

#define INSTR_FUNCT_MASK_R (0x1F << 6)
#define INSTR_FUNCT_MASK_I (0x1F << 12)
#define INSTR_FUNCT_MASK_J (0xF << 24)

#define INSTR_GET_HINT(x) ((x & INSTR_HINT_MASK) >> 30)
#define INSTR_GET_OPC_R(x) ((x & INSTR_OPC_MASK_R) >> 26)
#define INSTR_GET_OPC_I(x) ((x & INSTR_OPC_MASK_I) >> 27)
#define INSTR_GET_OPC_J(x) ((x & INSTR_OPC_MASK_J) >> 28)

#define INSTR_GET_RD_R(x) ((x & INSTR_RD_MASK_R) >> 21)
#define INSTR_GET_RT_R(x) ((x & INSTR_RT_MASK_R) >> 16)
#define INSTR_GET_RS_R(x) ((x & INSTR_RS_MASK_R) >> 11)

#define INSTR_GET_RD_I(x) ((x & INSTR_RD_MASK_I) >> 22)
#define INSTR_GET_RT_I(x) ((x & INSTR_RT_MASK_I) >> 17)

#define INSTR_GET_RD_U(x) ((x & INSTR_RD_MASK_U) >> 25)

#define INSTR_GET_FUNCT_R(x) ((x & INSTR_FUNCT_MASK_R) >> 6)
#define INSTR_GET_FUNCT_I(x) ((x & INSTR_FUNCT_MASK_I) >> 12)
#define INSTR_GET_FUNCT_J(x) ((x & INSTR_FUNCT_MASK_J) >> 24)

#define INSTR_GET_IMM(x) (x & 0xFFF)
#define INSTR_GET_IMM_U(x) (x & 0xFFFFF)
#define INSTR_GET_IMM_J(x) (x & 0xFFFFFF)

#define HINT_R 0
#define HINT_I 1
#define HINT_U 2
#define HINT_J 3

#define FUNCT_ADD 0
#define FUNCT_ADDC 1
#define FUNCT_SUB 2
#define FUNCT_SUBC 3
#define FUNCT_MUL 4
#define FUNCT_UMUL 5
#define FUNCT_REM 6
#define FUNCT_UREM 7
#define FUNCT_DIV 8
#define FUNCT_UDIV 9
#define FUNCT_AND 10
#define FUNCT_OR 11
#define FUNCT_XOR 12
#define FUNCT_SHL 13
#define FUNCT_SHR 14
#define FUNCT_SAR 15
#define FUNCT_NOT 16

#define FUNCT_UCOND (1 << 3)
#define FUNCT_COND_MASK (0b111)
#define FUNCT_COND_EQ 1
#define FUNCT_COND_NE 2
#define FUNCT_COND_LT 3
#define FUNCT_COND_GT 4
#define FUNCT_COND_LTE 5
#define FUNCT_COND_GTE 6

#endif
