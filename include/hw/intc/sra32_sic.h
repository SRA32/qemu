/*
 * QEMU System Interrupt Controller for SRA32
 *
 * Copyright (c) 2026 Astrido (asterd-og)
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_INTC_SRA32_SIC_H
#define HW_INTC_SRA32_SIC_H

#include "hw/core/sysbus.h"
#include "cpu.h"
#include "qom/object.h"

#define IRQ_MAX 32
#define CPU_MAX 32

#define CPU_MASK 0x1F
#define PRIO_MASK 0x80

struct SRA32SICState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/
    MemoryRegion mem;
    uint8_t dest[IRQ_MAX];
    uint32_t pend[CPU_MAX];
    uint32_t serv[CPU_MAX];
    CPUSRA32State *envs[CPU_MAX];
    qemu_irq irqs[CPU_MAX];
};

#define TYPE_SRA32_SIC "sra32-sic"
OBJECT_DECLARE_SIMPLE_TYPE(SRA32SICState, SRA32_SIC)

#endif
