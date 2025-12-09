#include "xhci_common.h"
#include "xhci.h"
#include "../../iodebug.h"
#include "../../memory.h"
#include "../../mm/pmm.h"
#include "../../thread/thread.h"
#include <stddef.h>

volatile uint32_t *xhci_operational_regs = NULL;  // Mapped MMIO base
command_ring_t command_ring;
xhci_controller_t xhci;

void **scratchpad_array = NULL;
void **scratchpad_buffers = NULL;
uint32_t scratchpad_count = 0;

static inline void mmio_write32(volatile uint32_t *addr, uint32_t val) {
    *addr = val;
}

static inline uint8_t mmio_read8(volatile uint8_t *addr) {
    return *addr;
}

static inline uint32_t mmio_read32(volatile uint32_t *addr) {
    return *addr;
}

void xhci_reset_controller() {
    const uint32_t HCRST = (1 << 1);
    const uint32_t CNR = (1 << 11);

    xhci.op_regs->usbcmd = HCRST;  // Set reset bit

    // Wait for HCRST to clear (reset complete)
    int timeout = 1000000;
    while ((xhci.op_regs->usbcmd & HCRST) && --timeout) {}

    if (timeout == 0) {
        debug_usb("xHCI reset timed out waiting for HCRST clear\n");
        return;
    }

    // Wait for CNR to clear (controller ready)
    timeout = 1000000;
    while ((xhci.op_regs->usbsts & CNR) && --timeout) {}

    if (timeout == 0) {
        debug_usb("xHCI reset timed out waiting for CNR clear\n");
        return;
    }

    debug_usb("xHCI reset complete\n");
}

static inline uint64_t read64(const volatile void *addr) {
    const volatile uint32_t *p = (const volatile uint32_t *)addr;
    return ((uint64_t)p[1] << 32) | p[0];
}

static inline void write64(volatile void *addr, uint64_t val) {
    volatile uint32_t *p = (volatile uint32_t *)addr;
    p[0] = (uint32_t)val;
    p[1] = (uint32_t)(val >> 32);
}

void xhci_run_stop(xhci_controller_t *xhci, bool run) {
    if (run) {
        xhci->op_regs->usbcmd |= XHCI_USBCMD_RUN_STOP; // Set Run bit
    } else {
        xhci->op_regs->usbcmd &= ~XHCI_USBCMD_RUN_STOP; // Clear Run bit
    }

    // wait for CNR to clear after Run/Stop change
    int timeout = 1000000;
    while ((xhci->op_regs->usbsts & (1 << 11)) && --timeout) {}

    if (timeout == 0) {
        debug_usb("xHCI Run/Stop timed out waiting for CNR clear\n");
    }
}

// initilize the DCBAA and scratchpads
void dcbaa_init(uint32_t max_slots) {
    uint32_t hcsparams2 = xhci.cap_regs->hcs_params2; // HCSPARAMS2 offset

    // Extract MaxScratchpadBufs
    uint32_t max_sp_lo = (hcsparams2 >> 27) & 0x1F;
    uint32_t max_sp_hi = (hcsparams2 >> 21) & 0x1F;
    scratchpad_count = (max_sp_hi << 5) | max_sp_lo;

    // Allocate DCBAA (max_slots + 1 entries, each 8 bytes, page-aligned)
    xhci.dcbaa = (void*)palloc(((max_slots + 1) * sizeof(uint64_t) + 0xFFF) / 0x1000, true);
    memset(xhci.dcbaa, 0, (max_slots + 1) * sizeof(uint64_t));

    if (scratchpad_count > 0) {
        // Allocate scratchpad array (array of physical addresses)
        scratchpad_array = (void*)palloc((scratchpad_count * sizeof(void *) + 0xFFF) / 0x1000, true);
        memset(scratchpad_array, 0, scratchpad_count * sizeof(void *));

        // Allocate each scratchpad buffer
        scratchpad_buffers = (void*)palloc(((scratchpad_count) * sizeof(void *) + 0xFFF) / 0x1000, true);
        for (uint32_t i = 0; i < scratchpad_count; i++) {
            scratchpad_buffers[i] = (void*)palloc(1, true); // 1 page each
            memset(scratchpad_buffers[i], 0, 0x1000);
            scratchpad_array[i] = scratchpad_buffers[i];
        }

        // DCBAA[0] = physical address of scratchpad array
        xhci.dcbaa[0] = virt_to_phys(scratchpad_array);
    }

    // Write DCBAAP register
    xhci.op_regs->dcbaap = virt_to_phys(xhci.dcbaa);
}

void xhci_init_command_ring(xhci_controller_t* xhci) {
    // allocate command ring
    xhci->cmd_ring = (command_ring_t *)palloc((sizeof(command_ring_t) + PAGE_SIZE - 1) / PAGE_SIZE, true);

    // zero the ring and init bookkeeping
    memset(xhci->cmd_ring, 0, sizeof(*xhci->cmd_ring));
    xhci->cmd_ring->enqueue_index = 0;
    xhci->cmd_ring->cycle_state = 1;               // start with cycle = 1 per spec

    // Ensure first TRB is 16-byte aligned (CRCR requires lower 4 bits zero)
    //assert(((uintptr_t)&xhci->cmd_ring->trbs[0] & 0xFULL) == 0);

    // Get device-visible physical address of first TRB
    uint64_t trb_phys = virt_to_phys(&xhci->cmd_ring->trbs[0]);

    // Build CRCR value:
    // - bits 63:4 = address (lower 4 address bits must be zero)
    // - bit 3 = Cycle State (initially 1)
    const uint64_t ADDR_MASK = ~((uint64_t)0xFULL);
    uint64_t crcr_val = (trb_phys & ADDR_MASK) | ((uint64_t)(xhci->cmd_ring->cycle_state & 1) << 3);

    // Optional memory barrier to make sure TRBs are visible before we hand them to the controller
    __sync_synchronize();

    // Write CRCR (64-bit MMIO)
    xhci->op_regs->crcr = crcr_val;

    // Small read-back for sanity (some hardware may modify CRCR fields)
    uint64_t crcr_rb = xhci->op_regs->crcr;
}

void xhci_init(uint64_t mmio_base) {

    xhci.cap_regs = (volatile xhci_regs_t *)mmio_base;
    xhci_operational_regs = (volatile uint32_t *)(mmio_base + xhci.cap_regs->caplength);
    xhci.op_regs = (volatile xhci_operational_regs_t *)(mmio_base + xhci.cap_regs->caplength);

    uint64_t doorbell_phys = mmio_base + xhci.cap_regs->db_off;
    volatile uint32_t *doorbell_virt = (void*)palloc(PAGE_SIZE, true);
    quickmap((uint64_t)doorbell_virt, doorbell_phys);
    xhci.doorbell_regs = (xhci_doorbell_regs_t *)doorbell_virt;


    serial_puts("xHCI Capability Registers:\n");
    serial_puts("  CAPLENGTH: ");  serial_puthex(xhci.cap_regs->caplength);   serial_puts("\n");
    serial_puts("  HCSPARAMS1: "); serial_puthex(xhci.cap_regs->hcs_params1); serial_puts("\n");
    serial_puts("    Max Slots: "); serial_puthex(xhci.cap_regs->hcs_params1 & 0xFF); serial_puts("\n");
    serial_puts("    Max Ports: "); serial_puthex((xhci.cap_regs->hcs_params1 >> 24) & 0xFF); serial_puts("\n");
    serial_puts("  HCCPARAMS1: "); serial_puthex(xhci.cap_regs->hcc_params1); serial_puts("\n");
    serial_puts("\n");

    debug_usb("Resetting xHCI...\n");
    xhci_reset_controller();

    dcbaa_init(5);
    xhci_init_command_ring(&xhci);

    xhci.op_regs->config = (xhci.op_regs->config & ~0xFF) | 0x05; // Set MaxSlotsEn to 5
    debug_usb("xHCI max device slots default to 5\n");

    // interrupts
    mmio_write32(&xhci_operational_regs[XHCI_USB_INT_EN / 4], 0xFFFFFFFF);

    // start controller
    xhci_run_stop(&xhci, true);
}