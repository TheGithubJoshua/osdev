#include <stdint.h>

extern uint64_t nvme_base_addr;

typedef struct pci_device_t {
	uint32_t vendor;
	uint32_t device;
	uint32_t func;
} pci_device_t;

uint16_t pci_read_word(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset);
void add_pci_device(pci_device_t *pdev);
void pci_probe();
void pci_init();
uint32_t pci_read_dword(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset);