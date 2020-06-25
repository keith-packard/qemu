/*
 * Virtual ARM Cortex M
 *
 * Copyright © 2020, Keith Packard <keithp@keithp.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "hw/arm/boot.h"
#include "hw/boards.h"
#include "exec/address-spaces.h"
#include "hw/arm/armv7m.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-clock.h"
#include "hw/misc/unimp.h"
#include "sysemu/reset.h"
#include "cpu.h"

#define NUM_IRQ_LINES 32
#define ROM_BASE    0x00000000
#define ROM_SIZE    0x20000000
#define RAM_BASE    0x20000000
#define RAM_SIZE    0x20000000

#define TYPE_VIRTM "virtm"
OBJECT_DECLARE_SIMPLE_TYPE(VirtMState, VIRTM)

struct VirtMState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    char *cpu_type;

    ARMv7MState armv7m;

    MemoryRegion rom;
    MemoryRegion ram;

    Clock *sysclk;
};

static void virtm_instance_init(Object *obj)
{
    VirtMState *s = VIRTM(obj);

    object_initialize_child(obj, "armv7m", &s->armv7m, TYPE_ARMV7M);

    s->sysclk = qdev_init_clock_in(DEVICE(s), "sysclk", NULL, NULL, 0);
}

static void virtm_realize(DeviceState *dev_soc, Error **errp)
{
    VirtMState *s = VIRTM(dev_soc);
    DeviceState *armv7m;

    MemoryRegion *system_memory = get_system_memory();

    /* Init ROM region */
    memory_region_init_rom(&s->rom, OBJECT(dev_soc), "virtm.rom",
                           ROM_SIZE, &error_fatal);
    memory_region_add_subregion(system_memory, ROM_BASE, &s->rom);

    /* Init RAM region */
    memory_region_init_ram(&s->ram, NULL, "virtm.ram", RAM_SIZE,
                           &error_fatal);
    memory_region_add_subregion(system_memory, RAM_BASE, &s->ram);

    /* Init ARMv7m */
    armv7m = DEVICE(&s->armv7m);
    qdev_prop_set_uint32(armv7m, "num-irq", 61);
    qdev_prop_set_string(armv7m, "cpu-type", s->cpu_type);
    qdev_connect_clock_in(armv7m, "cpuclk", s->sysclk);

    object_property_set_link(OBJECT(&s->armv7m), "memory",
                             OBJECT(get_system_memory()), &error_abort);

    if (!sysbus_realize(SYS_BUS_DEVICE(&s->armv7m), errp)) {
        return;
    }
}

static Property virtm_properties[] = {
    DEFINE_PROP_STRING("cpu-type", VirtMState, cpu_type),
    DEFINE_PROP_END_OF_LIST(),
};

static void virtm_class_init(ObjectClass *oc, void *data)
{
    DeviceClass  *dc = DEVICE_CLASS(oc);

    dc->realize = virtm_realize;
    device_class_set_props(dc, virtm_properties);
}

static const TypeInfo virtm_info = {
    .name = TYPE_VIRTM,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(VirtMState),
    .instance_init = virtm_instance_init,
    .class_init = virtm_class_init,
};

static void virtm_type_init(void)
{
    type_register_static(&virtm_info);
}

type_init(virtm_type_init);

/* Machine bits */

/* Main SYSCLK frequency in Hz (24MHz) */
#define SYSCLK_FRQ 24000000ULL

static void virtm_init(MachineState *machine)
{
    DeviceState *dev;
    Clock *sysclk;

    /* This clock doesn't need migration because it is fixed-frequency */
    sysclk = clock_new(OBJECT(machine), "SYSCLK");
    clock_set_hz(sysclk, SYSCLK_FRQ);

    dev = qdev_new(TYPE_VIRTM);
    qdev_prop_set_string(dev, "cpu-type", machine->cpu_type);
    qdev_connect_clock_in(dev, "sysclk", sysclk);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    armv7m_load_kernel(ARM_CPU(first_cpu),
                       machine->kernel_filename,
                       0, ROM_SIZE);
}


static void virtm_machine_init(MachineClass *mc)
{
    mc->desc = "VirtM";
    mc->init = virtm_init;
}

DEFINE_MACHINE("virtm", virtm_machine_init);
