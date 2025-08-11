#include "xhci_common.h"
#include "xhci.h"
#include "../../iodebug.h"
#include "../../thread/thread.h"
#include <stddef.h>

volatile uint32_t *xhci_operational_regs = NULL;  // Mapped MMIO base
command_ring_t command_ring;
xhci_regs_t regs;
xhci_operational_regs_t *operational_regs;

static inline void mmio_write32(volatile uint32_t *addr, uint32_t val) {
    *addr = val;
}

static inline uint8_t mmio_read8(volatile uint8_t *addr) {
    return *addr;
}

static inline uint32_t mmio_read32(volatile uint32_t *addr) {
    return *addr;
}

void xhci_reset_controller() { // probably not correct
    // Set Host Controller Reset bit
    operational_regs->usbcmd |= (1 << 1);

    // Wait for reset bit to clear (reset completion)
    while (operational_regs->usbcmd & (1 << 1)) {}

    // Wait for Controller Not Ready (CNR) bit to clear
    while (operational_regs->usbsts & (1 << 11)) {}

    debug_usb("xHCI reset complete\n");
}

// initilize the DCBAA
void dcbaa_init() {
    uint32_t max_scratchpad_lo = (regs.hcs_params2 >> 27) & 0x1F;  // bits 31:27
    uint32_t spr = (regs.hcs_params2 >> 26) & 0x1;                  // bit 26
    uint32_t max_scratchpad_hi = (regs.hcs_params2 >> 21) & 0x1F;  // bits 25:21
    uint32_t max_scratchpad_buffers = (max_scratchpad_hi << 5) | max_scratchpad_lo;

    serial_puts("max_scratchpad_buffers: ");
    serial_puthex(max_scratchpad_buffers);
}

void xhci_init(uint64_t mmio_base) {

    regs = *(xhci_regs_t *)mmio_base;
    xhci_operational_regs = (volatile uint32_t *)(mmio_base + regs.caplength);
    operational_regs = (xhci_operational_regs_t *)xhci_operational_regs;

    serial_puts("xHCI Capability Registers:\n");
    serial_puts("  CAPLENGTH: ");  serial_puthex(regs.caplength);   serial_puts("\n");
    serial_puts("  HCSPARAMS1: "); serial_puthex(regs.hcs_params1); serial_puts("\n");
    serial_puts("    Max Slots: "); serial_puthex(regs.hcs_params1 & 0xFF); serial_puts("\n");
    serial_puts("    Max Ports: "); serial_puthex((regs.hcs_params1 >> 24) & 0xFF); serial_puts("\n");
    serial_puts("  HCCPARAMS1: "); serial_puthex(regs.hcc_params1); serial_puts("\n");
    serial_puts("\n");
    
    debug_usb("Resetting xHCI...\n");
    xhci_reset_controller();

    operational_regs->config = (operational_regs->config & ~0xFF) | 0x05; // Set MaxSlotsEn to 5
    debug_usb("xHCI max device slots default to 5\n");

    // interrupts
    mmio_write32(&xhci_operational_regs[XHCI_USB_INT_EN / 4], 0xFFFFFFFF);
    dcbaa_init();
}