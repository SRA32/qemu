/*
 * QEMU System Interrupt Controller for SRA32
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
#include "hw/intc/sra32_sic.h"

#define SIC_REG_DEST  0
#define SIC_REG_CLAIM 0x30
#define SIC_REG_EOI   0x40

static void sic_set_irq(void *opaque, int irq, int level)
{
    SRA32SICState *s = opaque;

    uint8_t dest = s->dest[irq];
    if (dest == 0xFF) {
        return; // Masked
    }

    uint8_t cpu = dest & CPU_MASK;

    if (level) {
        s->pend[cpu] |= 1 << irq;
        qemu_log("SIC: Irq #%d fired (To cpu %d, prio %d).\n", irq, cpu, dest & PRIO_MASK);
    } else {
        s->pend[cpu] &= ~(1 << irq);
    }

    // TODO: If the irq being raised has a greater priority, interrupt.
    if (!s->serv[cpu])
        qemu_set_irq(s->irqs[cpu], level);
}

static uint64_t sic_read(void *opaque, hwaddr addr, unsigned size)
{
    SRA32SICState *s = opaque;
    CPUState *cpu = current_cpu;
    switch (addr) {
        case SIC_REG_DEST...SIC_REG_DEST+IRQ_MAX-1:
            if (size > 1) {
                qemu_log("SIC: Invalid read size 0x%" HWADDR_PRIX "\n", addr);
                return UINT64_MAX;
            }
            return s->dest[addr];
        case SIC_REG_CLAIM: {
            uint32_t index = cpu->cpu_index;
            int      highest = -1;
            uint8_t  vec = 0;

            for (int i = 0; i < IRQ_MAX; i++) {
                bool pend = s->pend[index] & (1 << i);
                if (!pend)
                    continue;

                int prio = s->dest[i] & PRIO_MASK;
                if (prio > highest) {
                    highest = prio;
                    vec = i;
                }
            }

            s->pend[index] &= ~(1 << vec);
            s->serv[index] = 1;

            return vec;
        }
        default:
            qemu_log("SIC: Invalid read at unimplemented register 0x%" HWADDR_PRIX "\n", addr);
            return UINT64_MAX;
    }
    return UINT64_MAX;
}

static void sic_write(void *opaque, hwaddr addr, uint64_t val, unsigned size)
{
    SRA32SICState *s = opaque;
    switch (addr) {
        case SIC_REG_DEST...SIC_REG_DEST+IRQ_MAX:
            if (size > 1) {
                qemu_log("SIC: Invalid write size 0x%" HWADDR_PRIX "\n", addr);
                return;
            }
            s->dest[addr] = val & 0xFF;
            return;
        case SIC_REG_EOI: {
            uint32_t index = current_cpu->cpu_index;
            s->serv[index] = 0;
            if (s->pend[index] != 0) {
                qemu_set_irq(s->irqs[index], 1);
            }
            return;
        }
        default:
            qemu_log("SIC: Invalid write at unimplemented register 0x%" HWADDR_PRIX "\n", addr);
            return;
    }
    return;
}

static const MemoryRegionOps sic_ops = {
    .read = sic_read,
    .write = sic_write,
    .endianness = DEVICE_LITTLE_ENDIAN,
    .impl = {
        .max_access_size = 4
    }
};

static void sic_init(Object *obj)
{
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    SRA32SICState *s = SRA32_SIC(obj);

    memory_region_init_io(&s->mem, OBJECT(s), &sic_ops, s,
                          "sra32-sic", 0x100);
    sysbus_init_mmio(sbd, &s->mem);

    qdev_init_gpio_in(DEVICE(sbd), sic_set_irq, IRQ_MAX);

    for (int i = 0; i < CPU_MAX; i++) {
        sysbus_init_irq(sbd, &s->irqs[i]);
    }
}

static void sic_realize(DeviceState *dev, Error **errp)
{
    SRA32SICState *s = SRA32_SIC(dev);

    for (int i = 0; i < IRQ_MAX; i++) {
        s->dest[i] = 0xFF; // Masked
    }
}

static void sic_class_init(ObjectClass *klass, const void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = sic_realize;
}

static const TypeInfo sic_info = {
    .name          = TYPE_SRA32_SIC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(SRA32SICState),
    .instance_init = sic_init,
    .class_init    = sic_class_init,
};

static void sic_register_types(void)
{
    type_register_static(&sic_info);
}

type_init(sic_register_types)

