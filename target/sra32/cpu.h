/*
 * QEMU SRA32 Virtual CPU header
 *
 * Copyright (c) 2026 Astrido (asterd-og)
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QEMU_SRA32_CPU_H
#define QEMU_SRA32_CPU_H

#include "cpu-qom.h"
#include "exec/cpu-common.h"
#include "exec/cpu-defs.h"
#include "exec/cpu-interrupt.h"
#include "system/memory.h"

#define FLAG_Z (1 << 0)
#define FLAG_N (1 << 1)
#define FLAG_C (1 << 2)
#define FLAG_V (1 << 3)
#define FLAG_IE (1 << 4)
#define FLAG_PE (1 << 5)
#define FLAG_UM (1 << 6)

#define REG_COUNT 32
#define REG_RP 1
#define REG_SP 2
#define REG_FP 3

#define CR_COUNT 16
#define CR_INTB 0
#define CR_SVPC 1
#define CR_SVFL 2
#define CR_ESRC 3
#define CR_PTBL 8

#define MAX_IRQ 32

#define EXCP_DIV_BY_ZERO 0
#define EXCP_UNALIGNED_ACCESS 1
#define EXCP_PAGE_FAULT 2

typedef struct CPUArchState {
    uint32_t regs[REG_COUNT];
    uint32_t crs[CR_COUNT];
    uint32_t pc;
    uint32_t flags;
    uint32_t intr;
} CPUSRA32State;

struct ArchCPU {
    CPUState parent_obj;

    CPUSRA32State env;
    qemu_irq irq;
};

struct SRA32CPUClass {
    CPUClass parent_class;

    DeviceRealize parent_realize;
    ResettablePhases parent_phases;
};

#define CPU_RESOLVING_TYPE TYPE_SRA32_CPU

void sra32_cpu_dump_state(CPUState *cs, FILE *f, int flags);
void sra32_cpu_do_interrupt(CPUState *cs);
void sra32_raise_excp(CPUSRA32State *env, int excp, uint32_t retpc);
void sra32_translate_init(void);
void sra32_translate_code(CPUState *cs, TranslationBlock *tb, int *max_insn, vaddr pc, void *host_pc);
int get_phys_nommu(CPUSRA32State *env, vaddr addr, hwaddr *phys);
int get_phys_mmu(CPUSRA32State *env, vaddr addr, hwaddr *phys);

#endif
