#include "pci.h"
#include "../../iodebug.h"
#include "../../util/fb.h"
#include "../../mm/pmm.h"
#include "../../thread/thread.h"
#include <stdbool.h>

pci_device_t **pci_devices = 0;
uint32_t devs = 0;

uint16_t pci_read_word(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset) {
	uint64_t address;
    uint64_t lbus = (uint64_t)bus;
    uint64_t lslot = (uint64_t)slot;
    uint64_t lfunc = (uint64_t)func;
    uint16_t tmp = 0;
    address = (uint64_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
    outd (0xCF8, address);
    tmp = (uint16_t)((ind (0xCFC) >> ((offset & 2) * 8)) & 0xffff);
    return (tmp);
}

uint16_t getVendorID(uint16_t bus, uint16_t device, uint16_t function) {
        uint32_t r0 = pci_read_word(bus,device,function,0);
        return r0;
}

uint16_t getDeviceID(uint16_t bus, uint16_t device, uint16_t function) {
        uint32_t r0 = pci_read_word(bus,device,function,2);
        return r0;
}

uint16_t getClassId(uint16_t bus, uint16_t device, uint16_t function) {
        uint32_t r0 = pci_read_word(bus,device,function,0xA);
        return (r0 & ~0x00FF) >> 8;
}

uint16_t getSubClassId(uint16_t bus, uint16_t device, uint16_t function) {
        uint32_t r0 = pci_read_word(bus,device,function,0xA);
        return (r0 & ~0xFF00);
}

void add_pci_device(pci_device_t *pdev) {
	pci_devices[devs] = pdev;
	devs ++;
	return;
}

void pci_probe() {
	for(uint32_t bus = 0; bus < 256; bus++)
    {
        for(uint32_t slot = 0; slot < 32; slot++)
        {
            for(uint32_t function = 0; function < 8; function++)
            {
                    uint16_t vendor = getVendorID(bus, slot, function);
                    if(vendor == 0xffff) continue;
                    uint16_t device = getDeviceID(bus, slot, function);
                    char venstr[5], devstr[5];
                    uint16_to_hex(vendor, venstr);
                    uint16_to_hex(device, devstr);
                    flanterm_write(flanterm_get_ctx(),"\033[32m[KERNEL][PCI] Found PCI device: ", 38);
                    flanterm_write(flanterm_get_ctx(), venstr, 4);
                    flanterm_write(flanterm_get_ctx(), ":", 2);
                    flanterm_write(flanterm_get_ctx(), devstr, 4);
                    flanterm_write(flanterm_get_ctx(), "\n\033[0m", 6);
                    pci_device_t *pdev = (pci_device_t *)palloc(sizeof(pci_device_t), true);
                    pdev->vendor = vendor;
                    pdev->device = device;
                    pdev->func = function;
                    add_pci_device(pdev);
            }
        }
    }
}

void pci_init() {
	pci_devices = (pci_device_t **)palloc(32 * sizeof(pci_device_t), true);
	pci_probe();
    flanterm_write(flanterm_get_ctx(),"\033[32m[KERNEL][PCI] PCI driver initialized\033[0m", 45);
    flanterm_write(flanterm_get_ctx(), "\n", 2);
	// exit thread here.
	task_exit();
}
