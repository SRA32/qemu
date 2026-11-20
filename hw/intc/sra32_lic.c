/*
 * QEMU Local Interrupt Controller for SRA32
 *
 * Copyright (c) 2026 Astrido (asterd-og)
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "hw/core/irq.h"
#include "hw/core/qdev-properties.h"
#include "hw/intc/sra32_lic.h"

#define LIC_REG_IPI_SEND 0x00
#define LIC_REG_REASON 0x04
#define LIC_REG_CLEAR 0x08

#define REASON_TIMER 1
#define REASON_IPI 2

static void lic_update(SRA32LICState *lic, int cpu)
{
    if (lic->reason[cpu] > 0)
        qemu_irq_raise(lic->irqs[cpu]);
}

static void lic_set_irq(void *opaque, int irq, int level)
{
    // The only device that can raise LIC's irq line is local timer.
    SRA32LICState *lic = opaque;

    if (level) {
        lic->reason[irq] |= REASON_TIMER;
        lic_update(lic, irq);
    }
}

static uint64_t lic_read(void *opaque, hwaddr addr, unsigned size)
{
    SRA32LICState *lic = opaque;
    switch (addr) {
        case LIC_REG_REASON:
            return lic->reason[current_cpu->cpu_index];
        default:
            qemu_log("LIC: Invalid read at unimplemented register 0x%" HWADDR_PRIX "\n", addr);
            return UINT64_MAX;
    }
    return UINT64_MAX;
}

static void lic_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    SRA32LICState *lic = opaque;
    switch (addr) {
        case LIC_REG_IPI_SEND:
            for (int i = 0; i < CPU_MAX; i++) {
                if (val & (1 << i)) {
                    lic->reason[i] |= REASON_IPI;
                    lic_update(lic, i);
                }
            }
            break;
        case LIC_REG_CLEAR:
            lic->reason[current_cpu->cpu_index] &= ~val;
            lic_update(lic, current_cpu->cpu_index);
            break;
        default:
            qemu_log("LIC: Invalid write at unimplemented register 0x%" HWADDR_PRIX "\n", addr);
            return;
    }
    return;
}

static const MemoryRegionOps lic_ops = {
    .read = lic_read,
    .write = lic_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .max_access_size = 4
    }
};

static void lic_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    SRA32LICState *lic = SRA32_LIC(obj);

    memory_region_init_io(&lic->mem, OBJECT(lic), &lic_ops, lic,
                          "sra32-lic", 0x100);
    sysbus_init_mmio(sbd, &lic->mem);

    qdev_init_gpio_in(DEVICE(sbd), lic_set_irq, CPU_MAX);

    for (int i = 0; i < CPU_MAX; i++) {
        sysbus_init_irq(sbd, &lic->irqs[i]);
    }
}

static void lic_realize(DeviceState *dev, Error **errp)
{
}

static void lic_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = lic_realize;
}

static const TypeInfo lic_info = {
    .name          = TYPE_SRA32_LIC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SRA32LICState),
    .instance_init = lic_init,
    .class_init    = lic_class_init,
};

static void lic_register_types(void)
{
    type_register_static(&lic_info);
}

type_init(lic_register_types)
