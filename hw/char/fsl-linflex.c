/*
 * fsl-linflex.c, Freescale LINFlex UART model
 *
 * Copyright (c) 2024 Wind River Systems, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "qemu/osdep.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "hw/char/fsl-linflex.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-properties-system.h"

static void fsl_linflex_update_irq(FslLinflexState *s)
{
    // if interrupts on data reception complete enabled
    if (s->regs[LINFLEX_LINIER] & LINIER_DRIE) {
        if (s->regs[LINFLEX_UARTSR] & UARTSR_DRFRFE) {
            // raise interrupt
            qemu_set_irq(s->irq, 1);
            return;
        }
    }
    // if interrupts on data transmission complete enabled
    if (s->regs[LINFLEX_LINIER] & LINIER_DTIE) {
        if (s->regs[LINFLEX_UARTSR] & UARTSR_DTFTFF) {
            // raise interrupt
            qemu_set_irq(s->irq, 1);
            return;
        }
    }
    qemu_set_irq(s->irq, 0);
}

static void fsl_linflex_write(void *opaque, hwaddr offset,
                              uint64_t value, unsigned size)
{
    FslLinflexState *s = opaque;
    unsigned char ch;
    uint32_t mask = 0x0;
    LinflexRegs reg = offset >> 2; // registers are 4 bytes wide

    value = (uint32_t)value;

    switch (reg)
    {
        case LINFLEX_LINCR1:
            s->regs[reg] = value & 0x0001DF27;
            if (s->regs[reg] & LINCR1_INIT) {
                s->regs[LINFLEX_LINSR] |= LINSR_LINS_INIT;
            }
            break;
        case LINFLEX_LINIER:
            s->regs[reg] = value & 0x0000FFFF;
            break;
        case LINFLEX_LINSR:
        case LINFLEX_LINESR:
            qemu_log_mask(LOG_UNIMP, "%s: Register LINESR not implemented\n", __func__);
            break;
        case LINFLEX_UARTCR:
            if (s->regs[LINFLEX_LINCR1] & LINCR1_INIT) {
                // uart mode can only be toggled in initialization mode
                mask |= UARTCR_UART;
                s->regs[reg] |= value & UARTCR_UART;
                // if uart mode is enabled and we are in init mode we can set the other fields
                if (s->regs[reg] & UARTCR_UART) {
                    s->regs[reg] |= value;
                }
            } else if (s->regs[reg] & UARTCR_UART) {
                // certain fields are writable outside of initialization mode if uart mode is enabled
                s->regs[reg] |= value & 0x0070FC30;
            }
            break;
        case LINFLEX_UARTSR:
            value = value & 0xFFFF; // upper 16 bits are reserved
            // Detect if software cleared DRFRFE/RMB. If so, accept new char input
            mask = UARTSR_DRFRFE | UARTSR_RMB;
            if ((s->regs[reg] & mask) && !(value & mask)) {
                // accept new character input now that software has cleared DRFRFE/RMB
                qemu_chr_fe_accept_input(&s->chr);
            }
            // store new value of reg
            s->regs[reg] &= ~value;
            break;
        case LINFLEX_LINTCSR:
            qemu_log_mask(LOG_UNIMP, "%s: Register LINTCSR not implemented\n", __func__);
            break;
        case LINFLEX_LINOCR:
            qemu_log_mask(LOG_UNIMP, "%s: Register LINOCR not implemented\n", __func__);
            break;
        case LINFLEX_LINTOCR:
            qemu_log_mask(LOG_UNIMP, "%s: Register LINTOCR not implemented\n", __func__);
            break;
        case LINFLEX_LINFBRR:
            qemu_log_mask(LOG_UNIMP, "%s: Register LINFBRR not implemented\n", __func__);
            break;
        case LINFLEX_LINIBRR:
            qemu_log_mask(LOG_UNIMP, "%s: Register LINIBRR not implemented\n", __func__);
            break;
        case LINFLEX_LINCFR:
            qemu_log_mask(LOG_UNIMP, "%s: Register LINCFR not implemented\n", __func__);
            break;
        case LINFLEX_LINCR2:
            qemu_log_mask(LOG_UNIMP, "%s: Register LINCR2 not implemented\n", __func__);
            break;
        case LINFLEX_BIDR:
            qemu_log_mask(LOG_UNIMP, "%s: Register BIDR not implemented\n", __func__);
            break;
        case LINFLEX_BDRL:
            // write to uart mode buffer
            s->regs[reg] = (uint32_t)value;
            // todo: _write_all is blocking, use _write
            ch = value;
            qemu_chr_fe_write_all(&s->chr, &ch, 1);
            /* Update UART status reg to indicate data transmission completed.
             * We are always ready for next transmission since the execution
             * of this callback is synchronous with the memory write.
             */
            s->regs[LINFLEX_UARTSR] |= UARTSR_DTFTFF;
            break;
        case LINFLEX_BDRM:
            qemu_log_mask(LOG_UNIMP, "%s: Register BDRM not implemented\n", __func__);
            break;
        case LINFLEX_GCR:
            qemu_log_mask(LOG_UNIMP, "%s: Register GCR not implemented\n", __func__);
            break;
        case LINFLEX_UARTPTO:
            qemu_log_mask(LOG_UNIMP, "%s: Register UARTPTO not implemented\n", __func__);
            break;
        case LINFLEX_UARTCTO:
            qemu_log_mask(LOG_UNIMP, "%s: Register UARTCTO not implemented\n", __func__);
            break;
        case LINFLEX_DMATXE:
            qemu_log_mask(LOG_UNIMP, "%s: Register DMATXE not implemented\n", __func__);
            break;
        case LINFLEX_DMARXE:
            qemu_log_mask(LOG_UNIMP, "%s: Register DMARXE not implemented\n", __func__);
            break;
        default:
            qemu_log_mask(LOG_GUEST_ERROR, "%s: write to invalid register\n",
                          __func__);
            break;
    }
    // update irq on all writes
    fsl_linflex_update_irq(s);
}

static uint64_t fsl_linflex_read(void *opaque, hwaddr offset,
                                 unsigned size)
{
    FslLinflexState *s = opaque;
    uint32_t r;
    LinflexRegs reg = offset >> 2; // registers are 4 bytes wide

    switch (reg)
    {
        case LINFLEX_LINCR1:
            r = s->regs[reg] |= 0x00000080; // bit 7 is always high
            break;
        case LINFLEX_LINIER:
        case LINFLEX_LINSR:
        case LINFLEX_LINESR:
        case LINFLEX_UARTCR:
        case LINFLEX_UARTSR:
        case LINFLEX_LINTCSR:
        case LINFLEX_LINOCR:
        case LINFLEX_LINTOCR:
        case LINFLEX_LINFBRR:
        case LINFLEX_LINIBRR:
        case LINFLEX_LINCFR:
        case LINFLEX_LINCR2:
        case LINFLEX_BIDR:
        case LINFLEX_BDRL:
            qemu_log_mask(LOG_UNIMP, "%s: Unimplemented register read at offset 0x%"HWADDR_PRIx"\n",
                         __func__, offset);
            r = s->regs[reg];
            break;
        case LINFLEX_BDRM:
            // read buffer
            r = s->regs[reg] & 0xFF;
            //fsl_linflex_update_irq(s);
            // accept next char input only when drfrfe/rmb is cleared
            break;
        case LINFLEX_GCR:
        case LINFLEX_UARTPTO:
        case LINFLEX_UARTCTO:
        case LINFLEX_DMATXE:
        case LINFLEX_DMARXE:
            qemu_log_mask(LOG_UNIMP, "%s: Unimplemented register read at offset 0x%"HWADDR_PRIx"\n",
                         __func__, offset);
            r = s->regs[reg];
            break;
        default:
            qemu_log_mask(LOG_GUEST_ERROR, "%s: Invalid register read at offset 0x%"HWADDR_PRIx"\n",
                         __func__, offset);
            r = 0;
            break;
    }
    return r;
}

static void fsl_linflex_rx(void *opaque, const uint8_t *buf, int size)
{
    FslLinflexState *s = opaque;

    // Store character in read buffer
    s->regs[LINFLEX_BDRM] = *buf;

    // update status to indicate buffer full
    s->regs[LINFLEX_UARTSR] |= UARTSR_DRFRFE;
    s->regs[LINFLEX_UARTSR] |= UARTSR_RMB;

    // update irq
    fsl_linflex_update_irq(s);
}

static int fsl_linflex_can_rx(void *opaque)
{
    FslLinflexState *s = opaque;

    // check status register, if can receive then return 1 else 0
    if ((s->regs[LINFLEX_UARTSR] & UARTSR_DRFRFE) ||
        (s->regs[LINFLEX_UARTSR] & UARTSR_RMB) ) {
        return 0;
    }
    return 1;
}

static void fsl_linflex_realize(DeviceState *dev, Error **errp)
{
    FslLinflexState *s = FSL_LINFLEX(dev);

    qemu_chr_fe_set_handlers(&s->chr, fsl_linflex_can_rx,
                             fsl_linflex_rx, NULL,
                             NULL, s, NULL, true);
}

static void fsl_linflex_reset_init(Object *obj, ResetType type)
{
    FslLinflexState *s = FSL_LINFLEX(obj);

    memset(s->regs, 0, sizeof(s->regs));

    s->regs[LINFLEX_LINCR1] =   0x00000082;
    s->regs[LINFLEX_LINIER] =   0;
    s->regs[LINFLEX_LINSR] =    0x00000040;
    s->regs[LINFLEX_LINESR] =   0;
    s->regs[LINFLEX_UARTCR] =   0;
    s->regs[LINFLEX_UARTSR] =   0;
    s->regs[LINFLEX_LINTCSR] =  0x00000200;
    s->regs[LINFLEX_LINOCR] =   0x0000FFFF;
    s->regs[LINFLEX_LINTOCR] =  0x00000E2C;
    s->regs[LINFLEX_LINFBRR] =  0;
    s->regs[LINFLEX_LINIBRR] =  0;
    s->regs[LINFLEX_LINCFR] =   0;
    s->regs[LINFLEX_LINCR2] =   0x00006000;
    s->regs[LINFLEX_BIDR] =     0;
    s->regs[LINFLEX_BDRL] =     0;
    s->regs[LINFLEX_BDRM] =     0;
    s->regs[LINFLEX_GCR] =      0;
    s->regs[LINFLEX_UARTPTO] =  0x00000FFF;
    s->regs[LINFLEX_UARTCTO] =  0;
    s->regs[LINFLEX_DMATXE] =   0;
    s->regs[LINFLEX_DMARXE] =   0;
}

static void fsl_linflex_reset_hold(Object *obj)
{
    FslLinflexState *s = FSL_LINFLEX(obj);

    // hack for direct kernel boot since there
    // is no firmware to initialize the device
    s->regs[LINFLEX_UARTSR] |= UARTSR_DTFTFF;
}

static void fsl_linflex_reset_exit(Object *obj)
{
    FslLinflexState *s = FSL_LINFLEX(obj);

    fsl_linflex_update_irq(s);

    qemu_chr_fe_accept_input(&s->chr);
}

static const MemoryRegionOps fsl_linflex_ops = {
    .read = fsl_linflex_read,
    .write = fsl_linflex_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 1,
        .max_access_size = 4,
    }
};

static void fsl_linflex_init(Object *obj)
{
    FslLinflexState *s = FSL_LINFLEX(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    // device tree says reg space has length 0x3000 but the
    // mmio space isn't actually that big
    memory_region_init_io(&s->iomem, obj, &fsl_linflex_ops,
                          s, "uart", 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);

    sysbus_init_irq(sbd, &s->irq);

}

static int fsl_linflex_post_load(void *opaque, int  version_id)
{
    FslLinflexState *s = opaque;

    /* At exit from reset, LINIER is not set.
     * After restoring the register state from snapshot,
     * raise interrupts if the device is configured to do so.
     */
    fsl_linflex_update_irq(s);

    return 0;
}

static const VMStateDescription vmstate_fsl_linflex = {
    .name = "fsl_linflex",
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = fsl_linflex_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32_ARRAY(regs, FslLinflexState, LINFLEX_REGS_MAX),
        VMSTATE_END_OF_LIST()
    },
};

static Property fsl_linflex_properties[] = {
    DEFINE_PROP_CHR("chardev", FslLinflexState, chr),
    DEFINE_PROP_END_OF_LIST(),
};

static void fsl_linflex_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);
    ResettableClass *rc = RESETTABLE_CLASS(oc);

    dc->realize = fsl_linflex_realize;
    dc->vmsd = &vmstate_fsl_linflex;
    rc->phases.enter = fsl_linflex_reset_init;
    rc->phases.hold = fsl_linflex_reset_hold;
    rc->phases.exit = fsl_linflex_reset_exit;
    device_class_set_props(dc, fsl_linflex_properties);
}

static const TypeInfo fsl_linflex_info = {
    .name       = TYPE_FSL_LINFLEX,
    .parent     = TYPE_SYS_BUS_DEVICE,
    .instance_size  = sizeof(FslLinflexState),
    .instance_init  = fsl_linflex_init,
    .class_init     = fsl_linflex_class_init,
};

static void fsl_linflex_register_types(void)
{
    type_register_static(&fsl_linflex_info);
}

type_init(fsl_linflex_register_types)
