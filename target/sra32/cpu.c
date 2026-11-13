/*
 * QEMU SRA32 CPU
 *
 * Copyright (c) 2026 Astrido (asterd-og)
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/qemu-print.h"
#include "qemu/log.h"
#include "cpu.h"
#include "exec/translation-block.h"
#include "exec/target_page.h"
#include "exec/cputlb.h"
#include "exec/page-protection.h"
#include "accel/tcg/cpu-ops.h"
#include "hw/core/sysemu-cpu-ops.h"
#include "hw/core/irq.h"
#include "tcg/tcg.h"

static void sra32_cpu_set_pc(CPUState *cs, vaddr value)
{
    SRA32_CPU(cs)->env.pc = value;
}

static vaddr sra32_cpu_get_pc(CPUState *cs)
{
    return SRA32_CPU(cs)->env.pc;
}

static TCGTBCPUState sra32_get_tb_cpu_state(CPUState *cs)
{
    return (TCGTBCPUState){
        .pc = SRA32_CPU(cs)->env.pc,
        .flags = 0
    };
}

static void sra32_cpu_synchronize_from_tb(CPUState *cs, const TranslationBlock *tb)
{
    SRA32_CPU(cs)->env.pc = tb->pc;
}

static void sra32_restore_state_to_opc(CPUState *cs, const TranslationBlock *tb, const uint64_t *data)
{
    SRA32_CPU(cs)->env.pc = data[0];
}

static int sra32_cpu_mmu_index(CPUState *cs, bool ifetch)
{
    return 0;
}

static bool sra32_cpu_has_work(CPUState *cs)
{
    if (cpu_env(cs)->flags & FLAG_IE && cpu_test_interrupt(cs, CPU_INTERRUPT_HARD))
        return true;
    return false;
}

static bool sra32_cpu_exec_interrupt(CPUState *cs, int interrupt_request)
{
    CPUSRA32State *env = cpu_env(cs);
    if ((env->flags & FLAG_IE) == 0)
        return false;

    sra32_cpu_do_interrupt(cs);
    return true;
}

static bool sra32_cpu_tlb_fill(CPUState *cs, vaddr addr, int size, MMUAccessType access_type, int mmu_idx, bool probe, uintptr_t retaddr)
{
    CPUSRA32State *env = &SRA32_CPU(cs)->env;
    hwaddr paddr;

    if (env->flags & FLAG_PE) {
        // Paging enabled.
        int res = get_phys_mmu(env, addr, &paddr);
        if (unlikely(res < 0)) {
            if (probe) {
                return false;
            }

            // Raise page fault.
            sra32_raise_excp(env, EXCP_PAGE_FAULT, retaddr);
            cpu_loop_exit_restore(cs, retaddr);

            return false;
        }
    } else {
        get_phys_nommu(env, addr, &paddr);
    }

    tlb_set_page(cs, addr & TARGET_PAGE_MASK, paddr & TARGET_PAGE_MASK,
                 PAGE_READ | PAGE_WRITE | PAGE_EXEC, mmu_idx,
                 TARGET_PAGE_SIZE);
    return true;
}

static hwaddr sra32_cpu_get_phys_page_debug(CPUState *cs, vaddr addr)
{
    return addr;
}

static void sra32_cpu_set_int(void *opaque, int irq, int level)
{
    SRA32CPU *cpu = opaque;
    CPUState *cs = CPU(cpu);

    if (level) {
        cpu_interrupt(cs, CPU_INTERRUPT_HARD);
    } else {
        cpu_reset_interrupt(cs, CPU_INTERRUPT_HARD);
    }
}

void sra32_cpu_dump_state(CPUState *cs, FILE *f, int flags)
{
    static const char *cr_names[CR_COUNT] = {
        "INTB", "SVPC", "SVFL", "ESRC",
        "????", "????", "????", "PTBL",
        "????", "????", "????", "????",
        "????", "????", "????", "????"
    };

    CPUSRA32State *env = &(SRA32_CPU(cs)->env);
    int i;

    qemu_fprintf(f, "PC=%08x ", env->pc);
    qemu_fprintf(f, "Flags=%08x\n", env->flags);
    qemu_fprintf(f, "X0=%08x ", env->regs[0]);
    qemu_fprintf(f, "RP=%08x ", env->regs[1]);
    qemu_fprintf(f, "SP=%08x ", env->regs[2]);
    qemu_fprintf(f, "FP=%08x\n", env->regs[3]);
    for (i = 4; i < REG_COUNT; i++)
        qemu_fprintf(f, "R%02d=%08x%c", i-4, env->regs[i], (i % 4 == 3) ? '\n' : ' ');
    for (i = 0; i < CR_COUNT; i++)
        qemu_fprintf(f, "%s=%08x%c", cr_names[i], env->crs[i], (i % 4 == 3) ? '\n' : ' ');
}

static void sra32_cpu_reset_hold(Object *obj, ResetType type)
{
    CPUState *cs = CPU(obj);
    SRA32CPUClass *scc = SRA32_CPU_GET_CLASS(obj);

    if (scc->parent_phases.hold)
        scc->parent_phases.hold(obj, type);

    memset(cpu_env(cs), 0, sizeof(CPUSRA32State));
    cs->exception_index = -1;
}

static void sra32_cpu_realizefn(DeviceState *dev, Error **errp)
{
    CPUState *cs = CPU(dev);
    SRA32CPUClass *scc = SRA32_CPU_GET_CLASS(cs);
    Error *local_err = NULL;

    cpu_exec_realizefn(cs, &local_err);
    if (local_err != NULL) {
        error_propagate(errp, local_err);
        return;
    }

    qemu_init_vcpu(cs);
    cpu_reset(cs);

    scc->parent_realize(dev, errp);
}

static ObjectClass *sra32_cpu_class_by_name(const char *cpu_model)
{
    ObjectClass *oc;
    char *typename;

    typename = g_strdup_printf(SRA32_CPU_TYPE_NAME("%s"), cpu_model);
    oc = object_class_by_name(typename);
    g_free(typename);

    return oc;
}

static void sra32_cpu_init(Object *obj)
{
    SRA32CPU *cpu = SRA32_CPU(obj);

    cpu->irq = qemu_allocate_irq(sra32_cpu_set_int, cpu, 0);
}

static const struct SysemuCPUOps sra32_sysemu_ops = {
    .has_work = sra32_cpu_has_work,
    .get_phys_page_debug = sra32_cpu_get_phys_page_debug
};

static const TCGCPUOps sra32_tcg_ops = {
    .guest_default_memory_order = 0,
    .initialize = sra32_translate_init,
    .translate_code = sra32_translate_code,
    .get_tb_cpu_state = sra32_get_tb_cpu_state,
    .synchronize_from_tb = sra32_cpu_synchronize_from_tb,
    .restore_state_to_opc = sra32_restore_state_to_opc,
    .mmu_index = sra32_cpu_mmu_index,
    .tlb_fill = sra32_cpu_tlb_fill,
    .pointer_wrap = cpu_pointer_wrap_uint32,

    .cpu_exec_interrupt = sra32_cpu_exec_interrupt,
    .cpu_exec_halt = sra32_cpu_has_work,
    .cpu_exec_reset = cpu_reset,
    .do_interrupt = sra32_cpu_do_interrupt
};

static void sra32_cpu_class_init(ObjectClass *oc, const void *data)
{
    SRA32CPUClass *scc = SRA32_CPU_CLASS(oc);
    CPUClass *cc = CPU_CLASS(scc);
    DeviceClass *dc = DEVICE_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);

    device_class_set_parent_realize(dc, sra32_cpu_realizefn, &scc->parent_realize);
    resettable_class_set_parent_phases(rc, NULL, sra32_cpu_reset_hold, NULL, &scc->parent_phases);

    cc->class_by_name = sra32_cpu_class_by_name;
    cc->dump_state = sra32_cpu_dump_state;
    cc->set_pc = sra32_cpu_set_pc;
    cc->get_pc = sra32_cpu_get_pc;
    cc->sysemu_ops = &sra32_sysemu_ops;
    cc->tcg_ops = &sra32_tcg_ops;
}

static const TypeInfo sra32_cpus_type_infos[] = {
    {
        .name = TYPE_SRA32_CPU,
        .parent = TYPE_CPU,
        .instance_size = sizeof(SRA32CPU),
        .instance_align = __alignof(SRA32CPU),
        .instance_init = sra32_cpu_init,
        .abstract = true,
        .class_size = sizeof(SRA32CPUClass),
        .class_init = sra32_cpu_class_init,
    },
    {
        .name = SRA32_CPU_TYPE_NAME("generic"),
        .parent = TYPE_SRA32_CPU,
    },
};

DEFINE_TYPES(sra32_cpus_type_infos)
