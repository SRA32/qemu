/*
 * QEMU Apollo Machine Interface
 *
 * Copyright (c) 2026 Astrido (asterd-og)
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef HW_SRA32_APOLLO_H
#define HW_SRA32_APOLLO_H

#include "hw/core/boards.h"
#include "hw/core/sysbus.h"
#include "hw/intc/sra32_sic.h"
#include "hw/pci/pci.h"

#define APOLLO_ROM_BASE 0
#define APOLLO_ROM_SIZE 0x100000
#define APOLLO_RAM_BASE 0x100000

#define APOLLO_PCI_ECAM_BASE 0xC0000000
#define APOLLO_PCI_ECAM_SIZE 0x10000000
#define APOLLO_PCI_MMIO_BASE 0xE0000000
#define APOLLO_PCI_MMIO_SIZE 0x20000000

#define APOLLO_SYSC_BASE 0xF0000000

#define APOLLO_GPIO_BASE 0xF0010000
#define APOLLO_UART_BASE 0xF0020000
#define APOLLO_PL110_BASE 0xF0040000

#define APOLLO_SIC_BASE 0xF0100000

typedef struct {
    /*< private >*/
    MachineState parent_obj;
    /*< public >*/
    MemoryRegion rom;
    MemoryRegion ram;
    PCIBus *pci_bus;

    SRA32SICState sic;
    PCIDevice *ahci;

    SRA32CPU *cpus[CPU_MAX];
    int num_cpus;
} ApolloMachineState;

typedef struct {
    /*< private >*/
    MachineClass parent_class;
    /*< public >*/
} ApolloMachineClass;

#define TYPE_APOLLO_MACHINE MACHINE_TYPE_NAME("apollo")

DECLARE_OBJ_CHECKERS(ApolloMachineState, ApolloMachineClass,
                     APOLLO_MACHINE, TYPE_APOLLO_MACHINE)

#endif
