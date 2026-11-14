/*
 * QEMU Apollo Machine Emulator
 *
 * Copyright (c) 2026 Astrido (asterd-og)
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "qemu/osdep.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "cpu.h"
#include "hw/sra32/apollo.h"
#include "hw/core/boards.h"
#include "hw/core/loader.h"
#include "hw/char/serial-mm.h"
#include "hw/core/qdev-properties.h"
#include "hw/pci-host/gpex.h"
#include "hw/ide/ahci.h"
#include "hw/ide/ahci-pci.h"
#include "hw/ide/ide-bus.h"
#include "system/address-spaces.h"
#include "system/reset.h"
#include "system/system.h"
#include "qom/object.h"

static void sra32_cpu_reset_cb(void *opaque)
{
    cpu_reset(CPU(opaque));
}

static void apollo_init_sic(ApolloMachineState *ams)
{
    SysBusDevice *sic;

    object_initialize_child(
        OBJECT(ams), "sic",
        &ams->sic, TYPE_SRA32_SIC
    );

    sic = SYS_BUS_DEVICE(&ams->sic);
    sysbus_realize(sic, &error_abort);

    for (int i = 0; i < ams->num_cpus; i++) {
        sysbus_connect_irq(sic, i, SRA32_CPU(ams->cpus[i])->irq);
    }

    sysbus_mmio_map(sic, 0, APOLLO_SIC_BASE);
}

static void apollo_init_gpex_pcie(ApolloMachineState *ams)
{
    DeviceState *dev;
    MemoryRegion *ecam_reg, *mmio_reg;
    MemoryRegion *ecam_alias, *mmio_alias;

    dev = qdev_new(TYPE_GPEX_HOST);

    object_property_set_uint(OBJECT(dev), PCI_HOST_ECAM_BASE,
                            APOLLO_PCI_ECAM_BASE, NULL);
    object_property_set_int(OBJECT(dev), PCI_HOST_ECAM_SIZE,
                            APOLLO_PCI_ECAM_SIZE, NULL);
    object_property_set_uint(OBJECT(dev), PCI_HOST_BELOW_4G_MMIO_BASE,
                            APOLLO_PCI_MMIO_BASE, NULL);
    object_property_set_int(OBJECT(dev), PCI_HOST_BELOW_4G_MMIO_SIZE,
                            APOLLO_PCI_MMIO_SIZE, NULL);

    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    ecam_alias = g_new0(MemoryRegion, 1);
    ecam_reg = sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0);
    memory_region_init_alias(ecam_alias, OBJECT(dev), "pcie-ecam",
                             ecam_reg, 0, APOLLO_PCI_ECAM_SIZE);
    memory_region_add_subregion(get_system_memory(), APOLLO_PCI_ECAM_BASE, ecam_alias);

    mmio_alias = g_new0(MemoryRegion, 1);
    mmio_reg = sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 1);
    memory_region_init_alias(mmio_alias, OBJECT(dev), "pcie-mmio",
                             mmio_reg, APOLLO_PCI_MMIO_BASE, APOLLO_PCI_MMIO_SIZE);
    memory_region_add_subregion(get_system_memory(), APOLLO_PCI_MMIO_BASE, mmio_alias);

    ams->pci_bus = PCI_HOST_BRIDGE(dev)->bus;
}

static void apollo_init_ahci(ApolloMachineState *ams)
{
    PCIDevice *pdev;

    pdev = pci_create_simple(ams->pci_bus, -1, TYPE_ICH9_AHCI);

    // Connect disks to AHCI.
    AHCIPCIState *ich9 = ICH9_AHCI(pdev);
    DriveInfo *drives[6] = { 0 };
    ide_drive_get(drives, ich9->ahci.ports);
    ahci_ide_create_devs(&ich9->ahci, drives);

    ams->ahci = pdev;
}

static void apollo_init_pl110(ApolloMachineState *ams)
{
    DeviceState *dev = qdev_new("pl110");

    object_property_set_link(
        OBJECT(dev), "framebuffer-memory",
        OBJECT(get_system_memory()), &error_fatal
    );

    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    sysbus_mmio_map(SYS_BUS_DEVICE(dev), 0, APOLLO_PL110_BASE);
}

static void apollo_machine_init(MachineState *machine)
{
    ApolloMachineState *ams = APOLLO_MACHINE(machine);
    SRA32CPU *cpu = SRA32_CPU(cpu_create(machine->cpu_type));
    ams->cpus[0] = cpu;
    ams->num_cpus = 1;

    MemoryRegion *sysmem = get_system_memory();

    qemu_register_reset(sra32_cpu_reset_cb, cpu);

    memory_region_init_ram(&ams->rom, NULL, "sra32.rom", APOLLO_ROM_SIZE, &error_fatal);
    memory_region_add_subregion(sysmem, APOLLO_ROM_BASE, &ams->rom);

    memory_region_init_ram(&ams->ram, NULL, "sra32.ram", machine->ram_size, &error_fatal);
    memory_region_add_subregion(sysmem, APOLLO_RAM_BASE, &ams->ram);

    if (machine->kernel_filename) {
        if (load_image_targphys(machine->kernel_filename, 0, machine->ram_size, NULL) < 0) {
            error_report("Couldn't load '%s'", machine->kernel_filename);
            exit(1);
        }
    }

    apollo_init_sic(ams);

    apollo_init_gpex_pcie(ams);
    apollo_init_ahci(ams);
    apollo_init_pl110(ams);

    qemu_irq uart_irq = qdev_get_gpio_in(DEVICE(&ams->sic), 1);
    serial_mm_init(sysmem, APOLLO_UART_BASE, 2, uart_irq, 115200, serial_hd(0), DEVICE_LITTLE_ENDIAN);
}

static void apollo_class_init(ObjectClass *oc, const void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "SRA32 Apollo Machine";
    mc->init = apollo_machine_init;
    mc->is_default = true;
    mc->default_cpu_type = SRA32_CPU_TYPE_NAME("generic");
}

static const TypeInfo apollo_machine_typeinfo = {
    .name       = TYPE_APOLLO_MACHINE,
    .parent     = TYPE_MACHINE,
    .class_init = apollo_class_init,
    .instance_size = sizeof(ApolloMachineState),
};

static void apollo_machine_init_register_types(void)
{
    type_register_static(&apollo_machine_typeinfo);
}

type_init(apollo_machine_init_register_types)