#include "pci.h"
#include "../../iodebug.h"
#include "../../util/fb.h"
#include "../../mm/pmm.h"
#include "../../memory.h"
#include "../../drivers/ahci/ahci.h"
#include "../../thread/thread.h"
#include "../nvme/nvme.h"
#include <stdbool.h>

pci_device_t **pci_devices = 0;
uint32_t devs = 0;
uint64_t nvme_base_addr = 0;

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

uint32_t pci_read_dword(uint16_t bus, uint16_t slot, uint16_t func, uint16_t offset) {
    uint64_t address;
    uint64_t lbus = (uint64_t)bus;
    uint64_t lslot = (uint64_t)slot;
    uint64_t lfunc = (uint64_t)func;

    // Construct the config address
    address = (uint64_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

    // Write address to CONFIG_ADDRESS port
    outd(0xCF8, (uint32_t)address);

    // Read full 32-bit value from CONFIG_DATA port
    uint32_t value = ind(0xCFC);

    return value;
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

uint8_t getProgIf(uint16_t bus, uint16_t device, uint16_t function) {
    uint16_t r0 = pci_read_word(bus, device, function, 0x08);
    return r0 & 0xFF; // Lower byte contains ProgIF
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        ++len;
    }
    return len;
}


void add_pci_device(pci_device_t *pdev) {
	pci_devices[devs] = pdev;
	devs ++;
	return;
}

void pci_probe() {
    for (uint32_t bus = 0; bus < 256; bus++) {
        for (uint32_t slot = 0; slot < 32; slot++) {
            for (uint32_t function = 0; function < 8; function++) {
                uint16_t vendor = getVendorID(bus, slot, function);
                if (vendor == 0xffff) continue;

                uint16_t device = getDeviceID(bus, slot, function);
                uint8_t class_id = getClassId(bus, slot, function);
                uint8_t subclass_id = getSubClassId(bus, slot, function);
                uint8_t prog_if = getProgIf(bus, slot, function);

                // Convert vendor and device ID to hex strings
                char venstr[5], devstr[5];
                uint16_to_hex(vendor, venstr);
                uint16_to_hex(device, devstr);

                // Determine device type label
                const char *device_type = "Unknown";
                if (class_id == 0x01 && subclass_id == 0x08 && prog_if == 0x02) {
                    device_type = "NVMe Controller";
                    nvme_base_addr = get_nvme_base_address(bus, slot, function);
                } else if (class_id == 0x01 && subclass_id == 0x06) {
                    device_type = "SATA Controller";
                    // i should check if the SATA controller is in IDE emulation mode or AHCI mode.
                    // but i wont.
                    map_nvme_mmio(0x0000000081086028,0x0000000081086028);
                    map_nvme_mmio(0x00000000FC011028,0x00000000FC011028);
                    uint64_t base = 0x0000000000400000; // BAR address
                    uint64_t size = 0x00010000;         // 64KB (for example)

                    for (uint64_t offset = 0; offset < size; offset += 0x1000) {
                        map_nvme_mmio(base + offset, base + offset);
                    }
                      
                    // fix this mess
                    uint32_t bar_low = pci_read_dword(bus, slot, function, 0x24);
                    uint32_t bar_high = pci_read_dword(bus, slot, function, 0x28);;
                    //uint64_t bar_addr = ((uint64_t)bar_high << 32) | (bar_low & ~0xFULL);
                    //map_size(bar_addr, bar_addr, 999999);
                    map_nvme_mmio(bar_high,bar_high);
                    map_nvme_mmio(bar_low,bar_low);
                    
                    init_ahci(((uint64_t)bar_high << 32) | (bar_low & ~0xF)); // get bar5 and pass to init_ahci
                } else if (class_id == 0x03 && subclass_id == 0x00) {
                    device_type = "VGA Compatible Controller";
                } else if (class_id == 0x02 && subclass_id == 0x00) {
                    device_type = "Ethernet Controller";
                } else if (class_id == 0x06 && subclass_id == 0x00) {
                    device_type = "Host Bridge";
                } else if (class_id == 0x0C && subclass_id == 0x03) {
                    device_type = "USB Controller";
                } else if (class_id == 0x04 && subclass_id == 0x01) {
                    device_type = "Audio Device";
                }

                // Print PCI device info
                flanterm_write(flanterm_get_ctx(), "\033[32m[KERNEL][PCI] Found PCI device: ", 38);
                flanterm_write(flanterm_get_ctx(), venstr, 4);
                flanterm_write(flanterm_get_ctx(), ":", 2);
                flanterm_write(flanterm_get_ctx(), devstr, 4);
                flanterm_write(flanterm_get_ctx(), " [", 2);
                flanterm_write(flanterm_get_ctx(), device_type, strlen(device_type));
                flanterm_write(flanterm_get_ctx(), "]\n\033[0m", 6);

                // Allocate and store device
                pci_device_t *pdev = (pci_device_t *)palloc((sizeof(pci_device_t) + PAGE_SIZE - 1) / PAGE_SIZE, true);
                pdev->vendor = vendor;
                pdev->device = device;
                pdev->func = function;
                add_pci_device(pdev);
            }
        }
    }
}

void pci_init() {
	pci_devices = (pci_device_t **)palloc((32 * sizeof(pci_device_t) + PAGE_SIZE - 1) / PAGE_SIZE, true);
	pci_probe();
        flanterm_write(flanterm_get_ctx(),"\033[32m[KERNEL][PCI] PCI driver initialized\033[0m", 45);
        flanterm_write(flanterm_get_ctx(), "\n", 2);
	// exit thread here.
	task_exit();
}
