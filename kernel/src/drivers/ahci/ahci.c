#include "ahci.h"
#include "../../thread/thread.h"
#include "../../iodebug.h"
#include "../../timer.h"
#include "../../util/fb.h"
#include "../../memory.h"

void init_ahci(uint32_t abar) {
	// get base address and map
	uint64_t base_addr = (abar & 0xFFFFFFF0);
	map_nvme_mmio(base_addr, base_addr);
	// map random address bc ig it isn't
	map_nvme_mmio(0x000000010000000B, 0x000000010000000B);
	map_nvme_mmio(0x0000000100001027, 0x0000000100001027);

	// read identity data
	FIS_REG_H2D fis;
	memset(&fis, 0, sizeof(FIS_REG_H2D));
	fis.fis_type = FIS_TYPE_REG_H2D;
	fis.command = ATA_CMD_IDENTIFY;	// 0xEC
	fis.device = 0;			// Master device
	fis.c = 1;

	HBA_MEM* hba = (HBA_MEM*)abar;

	for (int i = 0; i < 32; i++) {
	    if (hba->pi & (1 << i)) {
	        HBA_PORT* port = &hba->ports[i];

	        port->clb = AHCI_BASE + (i << 10);     // Command List: 1KB per port
	        port->clbu = 0;
	        memset((void*)port->clb, 0, 1024);

	        port->fb = AHCI_BASE + 0x8000 + (i << 8);  // FIS: 256 bytes per port
	        port->fbu = 0;
	        memset((void*)port->fb, 0, 256);
	    }
	}

	// we scan for active ports
	uint32_t pi = hba->pi;
	for (int i = 0; i < 32; i++) {
		HBA_PORT* port = &hba->ports[i];
	    if (pi & (1 << i)) {
	        // Check port->sig to determine if it’s a SATA device
	        uint32_t ssts = port->ssts;
	        uint8_t det = ssts & 0xF;
	        uint8_t ipm = (ssts >> 8) & 0xF;

	        if (det == 3 && ipm == 1 && port->sig != 0xFFFF0101) {
	            // sata device connected and active
	            serial_puts("connected, active SATA device found at ");
	            serial_puthex(port->sig);
	            flanterm_write(flanterm_get_ctx(), "\033[32m[ACPI] Device found!\033[0m", 30);
	            flanterm_write(flanterm_get_ctx(), "\n", 2);
	        }

	    }
	}

}