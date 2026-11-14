/*
 * QEMU SRA32 CPU helpers
 *
 * Copyright (c) 2026 Astrido (asterd-og)
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include "cpu.h"
#include "exec/cputlb.h"
#include "exec/helper-proto.h"
#include "exec/log.h"
#include "exec/target_page.h"
#include "system/memory.h"
#include "system/address-spaces.h"
#include "accel/tcg/cpu-ldst.h"
#include "hw/core/irq.h"
#include "qemu/plugin.h"

void sra32_cpu_do_interrupt(CPUState *cs)
{
    CPUSRA32State *env = cpu_env(cs);
    uint32_t ret = env->pc;

    if (cpu_test_interrupt(cs, CPU_INTERRUPT_HARD)) {
        uint32_t intb = env->crs[CR_INTB] + 4;

        env->crs[CR_SVPC] = ret;
        env->crs[CR_SVFL] = env->flags;

        env->pc = cpu_ldl_le_data(env, intb);

        cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD);
        qemu_plugin_vcpu_interrupt_cb(cs, ret);
    } else {
        uint32_t excb = env->crs[CR_INTB];

        env->crs[CR_SVPC] = ret;
        env->crs[CR_SVFL] = env->flags;
        env->crs[CR_ESRC] = cs->exception_index;

        env->pc = cpu_ldl_le_data(env, excb);

        qemu_plugin_vcpu_exception_cb(cs, ret);
    }
}

void sra32_raise_excp(CPUSRA32State *env, int excp, uint32_t retpc)
{
    CPUState *cs = env_cpu(env);

    cs->exception_index = excp;
    cpu_loop_exit_restore(cs, retpc);
}

uint32_t helper_div(CPUSRA32State *env, uint32_t num, uint32_t den)
{
    uint32_t ret = 0;
    if (den == 0 || (num == INT_MIN && den == -1)) {
        sra32_raise_excp(env, EXCP_DIV_BY_ZERO, GETPC());
    } else {
        ret = (int32_t)num / (int32_t)den;
        return ret;
    }
    return ret;
}

uint32_t helper_divu(CPUSRA32State *env, uint32_t num, uint32_t den)
{
    uint32_t ret = 0;
    if (den == 0) {
        sra32_raise_excp(env, EXCP_DIV_BY_ZERO, GETPC());
    } else {
        ret = num / den;
        return ret;
    }
    return ret;
}

void helper_write_flg(CPUSRA32State *env, uint32_t val)
{
    uint32_t old_val = env->flags;
    env->flags = val;

    if ((old_val ^ val) & FLAG_PE) {
        tlb_flush(env_cpu(env));
    }
}

void helper_write_cr(CPUSRA32State *env, uint32_t cr, uint32_t val)
{
    env->crs[cr] = val;
    if (cr == CR_PTBL) {
        tlb_flush(env_cpu(env));
    }
}

int get_phys_nommu(CPUSRA32State *env, vaddr addr, hwaddr *phys)
{
    *phys = addr;
    return 0;
}

int get_phys_mmu(CPUSRA32State *env, int *prot, MMUAccessType access_type, vaddr addr, hwaddr *phys)
{
    *prot = 0;
    *phys = 0;
    CPUState *cs = env_cpu(env);

    addr &= TARGET_PAGE_MASK;

    uint32_t ptbl = env->crs[CR_PTBL];
    if (!ptbl) {
        *phys = 0;
        return -1;
    }

    uint32_t bidx = (addr >> 22) & 0x3FF;
    uint32_t pidx = (addr >> 12) & 0x3FF;

    MemTxResult result;
    uint32_t bottom = address_space_ldl_le(cs->as, ptbl + (bidx * 4), MEMTXATTRS_UNSPECIFIED, &result);
    if (result != MEMTX_OK) {
        return -1;
    }

    if (!(bottom & SRA32_PAGE_VALID)) {
        return -1;
    }

    bottom &= TARGET_PAGE_MASK;

    uint32_t page = address_space_ldl_le(cs->as, bottom + (pidx * 4), MEMTXATTRS_UNSPECIFIED, &result);
    if (result != MEMTX_OK) {
        return -1;
    }

    *prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
    *phys = page;

    if (!(page & SRA32_PAGE_VALID)) {
        qemu_log("Page 0x%lx isn't valid.\n", addr);
        return -1;
    }

    *prot |= PAGE_READ;

    if (page & SRA32_PAGE_WRITE) {
        *prot |= PAGE_WRITE;
    } else {
        if (access_type == MMU_DATA_STORE) {
            qemu_log("Page isn't writeable.\n");
            return -1;
        }
    }

    *prot |= PAGE_EXEC;

    // TODO: Handle user.

    return 0;
}
