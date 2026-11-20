/*
 * QEMU Local Interrupt Controller for SRA32
 *
 * Copyright (c) 2026 Astrido (asterd-og)
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_INTC_SRA32_LIC_H
#define HW_INTC_SRA32_LIC_H

#include "hw/core/sysbus.h"
#include "cpu.h"
#include "qom/object.h"
#include "hw/intc/sra32_sic.h"

struct SRA32LICState {
    /*< private >*/
    SysBusDevice parent_obj;
    /*< public >*/
    MemoryRegion mem;
    uint32_t reason[CPU_MAX];

    qemu_irq irqs[CPU_MAX];
};

#define TYPE_SRA32_LIC "sra32-lic"
OBJECT_DECLARE_SIMPLE_TYPE(SRA32LICState, SRA32_LIC)


#endif
