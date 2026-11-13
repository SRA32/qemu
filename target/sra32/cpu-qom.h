/*
 * QEMU SRA32 CPU QOM header (target agnostic)
 *
 * Copyright (c) 2026 Astrido (asterd-og)
 *
 * This work is licensed under the terms of the GNU GPLv2 or later.
 * See the COPYING file in the top-level directory.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef QEMU_SRA32_CPU_QOM_H
#define QEMU_SRA32_CPU_QOM_H

#include "hw/core/cpu.h"

#define TYPE_SRA32_CPU "sra32-cpu"

OBJECT_DECLARE_CPU_TYPE(SRA32CPU, SRA32CPUClass, SRA32_CPU)

#define SRA32_CPU_TYPE_SUFFIX "-" TYPE_SRA32_CPU
#define SRA32_CPU_TYPE_NAME(model) model SRA32_CPU_TYPE_SUFFIX

#endif
