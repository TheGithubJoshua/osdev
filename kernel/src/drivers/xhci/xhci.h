#include "xhci_common.h"
#include "../../util/fb.h"
#include <stddef.h>
#include <stdint.h>

#define XHCI_USBSTS      0x04  // USB Status register offset
#define XHCI_USB_CMD     0x00  // USB Command register offset
#define XHCI_USB_INT_EN  0x08  // Interrupt Enable register offset
#define XHCI_PAGE_SIZE   4096
#define XHCI_CAPLENGTH_OFFSET   0x00
#define XHCI_HCSPARAMS1_OFFSET  0x04
#define XHCI_HCCPARAMS1_OFFSET  0x08

#define DEBUG_USB

#ifdef DEBUG_USB
    #define debug_usb(text) do { \
        const char _dbg_prefix[] = "[DEBUG][USB] "; \
        flanterm_write(flanterm_get_ctx(), _dbg_prefix, sizeof(_dbg_prefix) - 1); \
        flanterm_write(flanterm_get_ctx(), text, sizeof(text) - 1); \
    } while (0)
#else
    #define debug_usb(text) do {} while (0)
#endif

typedef struct {
    uint64_t parameter;
    uint32_t status;
    uint32_t control;
} __attribute__((packed)) xhci_trb_t;

typedef struct {
    xhci_trb_t trbs[XHCI_COMMAND_RING_TRB_COUNT];
    size_t enqueue_index;
    uint8_t cycle_state;  // 0 or 1, toggled each ring wrap
} command_ring_t;

typedef volatile struct xhci_regs {
  uint8_t caplength;
  uint8_t reserved;
  uint16_t hci_version;
  uint32_t hcs_params1;
  uint32_t hcs_params2;
  uint32_t hcs_params3;
  uint32_t hcc_params1;
  uint32_t db_off;  /* Doorbell offset */
  uint32_t rts_off; /* Runtime register space offset */
  uint32_t hcc_params2;
} __attribute__((packed)) xhci_regs_t;

typedef struct xhci_port_register_set_t {
    volatile uint32_t portsc; ///< Port Status and Control
    volatile uint32_t portpmsc; ///< Port Power Management Status and Control
    volatile uint32_t portli; ///< Port Link Info
    volatile uint32_t porthlpmc; ///< Port Hardware LPM Control
} __attribute__((packed)) xhci_port_register_set_t;

typedef struct xhci_operational_regs_t {
    volatile uint32_t                     usbcmd;
    volatile uint32_t                     usbsts;
    volatile uint32_t                     pagesize;
    volatile uint32_t                     reserved1[2];
    volatile uint32_t                     dnctrl;
    volatile uint64_t                     crcr;
    volatile uint32_t                     reserved2[4];
    volatile uint64_t                     dcbaap;
    volatile uint32_t                     config;
    volatile uint32_t                     reserved3[0xf1];
    volatile xhci_port_register_set_t portsc[0];
} __attribute__((packed)) xhci_operational_regs_t;

void xhci_reset_controller();
void xhci_init(uint64_t mmio_base);
void dcbaa_init();