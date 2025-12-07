#include "ahci.h"
#include "../../thread/thread.h"
#include "../../iodebug.h"
#include "../../timer.h"
#include "../../mm/pmm.h"
#include "../fat/fat.h"
#include "../../fs/fs.h"
#include "../../util/fb.h"
#include "../../ff16/source/ff.h"
#include "../../memory.h"
#include <stdint.h>

HBA_MEM* hba;
HBA_PORT* port;

// Check device type
static int check_type(HBA_PORT *port) {
	uint32_t ssts = port->ssts;

	uint8_t ipm = (ssts >> 8) & 0x0F;
	uint8_t det = ssts & 0x0F;

	if (det != HBA_PORT_DET_PRESENT)	// Check drive status
		return AHCI_DEV_NULL;
	if (ipm != HBA_PORT_IPM_ACTIVE)
		return AHCI_DEV_NULL;

	switch (port->sig)
	{
	case SATA_SIG_ATAPI:
		return AHCI_DEV_SATAPI;
	case SATA_SIG_SEMB:
		return AHCI_DEV_SEMB;
	case SATA_SIG_PM:
		return AHCI_DEV_PM;
	default:
		return AHCI_DEV_SATA;
	}
}

void trace_ahci(char buf[], int param1) {
	serial_puts(buf);
	serial_puthex(param1);
	serial_puts("\n");
}

void probe_port(HBA_MEM *abar) {
	// Search disk in implemented ports
	uint32_t pi = abar->pi;
	int i = 0;
	while (i<32)
	{
		if (pi & 1)
		{
			int dt = check_type(&abar->ports[i]);
			if (dt == AHCI_DEV_SATA)
			{
				trace_ahci("SATA drive found at port ", i);
			}
			else if (dt == AHCI_DEV_SATAPI)
			{
				trace_ahci("SATAPI drive found at port ", i);
			}
			else if (dt == AHCI_DEV_SEMB)
			{
				trace_ahci("SEMB drive found at port ", i);
			}
			else if (dt == AHCI_DEV_PM)
			{
				trace_ahci("PM drive found at port ", i);
			}
			else
			{
				trace_ahci("No drive found at port ", i);
			}
		}

		pi >>= 1;
		i ++;
	}
}

// Start command engine
void start_cmd(HBA_PORT *port) {
	// Wait until CR (bit15) is cleared
	while (port->cmd & HBA_PxCMD_CR)
		;

	// Set FRE (bit4) and ST (bit0)
	port->cmd |= HBA_PxCMD_FRE;
	port->cmd |= HBA_PxCMD_ST; 
}

// Stop command engine
void stop_cmd(HBA_PORT *port)
{
	// Clear ST (bit0)
	port->cmd &= ~HBA_PxCMD_ST;

	// Clear FRE (bit4)
	port->cmd &= ~HBA_PxCMD_FRE;

	// Wait until FR (bit14), CR (bit15) are cleared
	while(1)
	{
		if (port->cmd & HBA_PxCMD_FR)
			continue;
		if (port->cmd & HBA_PxCMD_CR)
			continue;
		break;
	}

}

void port_rebase(HBA_PORT *port, int portno) {
	stop_cmd(port);	// Stop command engine

	// Command list offset: 1K*portno
	// Command list entry size = 32
	// Command list entry maxim count = 32
	// Command list maxim size = 32*32 = 1K per port
	port->clb = AHCI_BASE + (portno<<10);
	port->clbu = 0;
	memset((void*)(port->clb), 0, 1024);

	// FIS offset: 32K+256*portno
	// FIS entry size = 256 bytes per port
	port->fb = AHCI_BASE + (32<<10) + (portno<<8);
	port->fbu = 0;
	memset((void*)(port->fb), 0, 256);

	// Command table offset: 40K + 8K*portno
	// Command table size = 256*32 = 8K per port
	HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)(port->clb);
	for (int i=0; i<32; i++)
	{
		cmdheader[i].prdtl = 8;	// 8 prdt entries per command table
					// 256 bytes per command table, 64+16+48+16*8
		// Command table offset: 40K + 8K*portno + cmdheader_index*256
		cmdheader[i].ctba = AHCI_BASE + (40<<10) + (portno<<13) + (i<<8);
		cmdheader[i].ctbau = 0;
		memset((void*)cmdheader[i].ctba, 0, 256);
	}

	start_cmd(port);	// Start command engine
}

int find_cmdslot(HBA_PORT *port) {
	// If not set in SACT and CI, the slot is free
	uint32_t slots = (port->sact | port->ci);
	for (int i=0; i<32; i++)
	{
		if ((slots&1) == 0)
			return i;
		slots >>= 1;
	}
	serial_puts("Cannot find free command list entry\n");
	return -1;
}

bool ahci_write(HBA_PORT *port, uint32_t start1, uint32_t starth, uint32_t count, uint16_t *buf) {
	port->is = (uint32_t) -1;
	int spin = 0;
	int slot = find_cmdslot(port);
	if (slot == -1)
		return false;

	HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)PHYS_TO_VIRT(port->clb);
	cmdheader += slot;
	cmdheader->cfl = sizeof(FIS_REG_H2D)/sizeof(uint32_t);
	cmdheader->p = 1;
	cmdheader->w = 1; // write to device
	cmdheader->prdtl = (uint16_t)((count-1)>>4) + 1;

	HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL*)PHYS_TO_VIRT(cmdheader->ctba);
	memset(cmdtbl, 0, sizeof(HBA_CMD_TBL) + 
		(cmdheader->prdtl-1)*sizeof(HBA_PRDT_ENTRY));

	// 8k bytes per PRDT
	for (int i = 0; i < cmdheader->prdtl - 1; i++) {
		uintptr_t buf_virt = (uintptr_t)buf;
		uintptr_t buf_phys = buf_virt - get_phys_offset();
        
        // we setup command table
		cmdtbl->prdt_entry[i].dba = (uint32_t)(buf_phys & 0xFFFFFFFF);
		cmdtbl->prdt_entry[i].dbau = (uint32_t)((buf_phys >> 32) & 0xFFFFFFFF);
		cmdtbl->prdt_entry[i].dbc = 8*1024 - 1; // 8k bytes 
		cmdtbl->prdt_entry[i].i = 1;

		buf += 4*1024; // advance buffer pointer by 4k words (8KB)
		count -= 16;
	}
	// last entry
	uintptr_t buf_virt = (uintptr_t)buf;
	uintptr_t buf_phys = buf_virt - get_phys_offset();

	cmdtbl->prdt_entry[cmdheader->prdtl - 1].dba = (uint32_t)(buf_phys & 0xFFFFFFFF);
	cmdtbl->prdt_entry[cmdheader->prdtl - 1].dbau = (uint32_t)((buf_phys >> 32) & 0xFFFFFFFF);
	cmdtbl->prdt_entry[cmdheader->prdtl - 1].dbc = (count << 9) - 1;  // remaining bytes
	cmdtbl->prdt_entry[cmdheader->prdtl - 1].i = 1;

	FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&cmdtbl->cfis);

	cmdfis->fis_type = FIS_TYPE_REG_H2D;
	cmdfis->c = 1; // command
	cmdfis->command = ATA_WRITE_DMA_EXT;

	cmdfis->lba0 = (uint8_t)start1;
	cmdfis->lba1 = (uint8_t)(start1>>8);
	cmdfis->lba2 = (uint8_t)(start1>>16);
	cmdfis->device = 1<<6; // LBA mode

	cmdfis->lba3 = (uint8_t)(start1>>24);
	cmdfis->lba4 = (uint8_t)starth;
	cmdfis->lba5 = (uint8_t)(starth>>8);

	cmdfis->countl = count & 0xFF;
	cmdfis->counth = (count >> 8) & 0xFF;

	// The below loop waits until the port is no longer busy before issuing a new command
	while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000)
	{
		spin++;
	}
	if (spin == 1000000)
	{
		serial_puts("Port is hung\n");
		return false;
	}

	/*serial_puts("Port tfd before command: 0x");
	serial_puthex(port->tfd);
	serial_puts("\n");
	serial_puts("Port ssts: 0x");
	serial_puthex(port->ssts);
	serial_puts("\n");
	serial_puts("Port is: 0x");
	serial_puthex(port->is);
	serial_puts("\n");

	serial_puts("Port ci before command: 0x");
	serial_puthex(port->ci);
	serial_puts("\n");*/

	for (int i = 0; i < 10; i++) {
	    serial_puts("Port ssts: 0x");
	    serial_puthex(port->ssts);
	    serial_puts("\n");
	    //timer_wait(100);
	}

	port->ci = 1<<slot;	

	// Poll for completion
	spin = 0;
	while (port->ci & (1 << slot)) {
	    if (port->is & HBA_PxIS_TFES) { // Task file error
	        serial_puts("Write disk error\n");
	        return false;
	    }
	    if (++spin > 10000000) { // timeout (adjust as needed)
	        serial_puts("AHCI write timeout\n");
	        return false;
	    }
	}
	//serial_puts("past loop!");

	// Check again
	if (port->is & HBA_PxIS_TFES)
	{
		serial_puts("Write disk error\n");
		return false;
	}
return true;
}

bool ahci_read(HBA_PORT *port, uint32_t startl, uint32_t starth, uint32_t count, uint16_t *buf) {
	port->is = (uint32_t) -1;		// Clear pending interrupt bits
	int spin = 0; // Spin lock timeout counter
	int slot = find_cmdslot(port);
	if (slot == -1)
		return false;

	HBA_CMD_HEADER *cmdheader = (HBA_CMD_HEADER*)PHYS_TO_VIRT(port->clb);
	cmdheader += slot;
	cmdheader->cfl = sizeof(FIS_REG_H2D)/sizeof(uint32_t);	// Command FIS size
	cmdheader->p = 1;
	cmdheader->w = 0;		// Read from device
	cmdheader->prdtl = (uint16_t)((count-1)>>4) + 1;	// PRDT entries count

    map_page(read_cr3(), cmdheader->ctba, cmdheader->ctba, PAGE_PRESENT | PAGE_WRITABLE | PAGE_USER | PAGE_NO_EXECUTE);
	HBA_CMD_TBL *cmdtbl = (HBA_CMD_TBL*)PHYS_TO_VIRT((cmdheader->ctba));
	memset(cmdtbl, 0, sizeof(HBA_CMD_TBL)+(cmdheader->prdtl-1)*sizeof(HBA_PRDT_ENTRY));

	// 8K bytes (16 sectors) per PRDT
	for (int i = 0; i < cmdheader->prdtl - 1; i++) {
	    uintptr_t buf_virt = (uintptr_t)buf;
	    uintptr_t buf_phys = buf_virt - get_phys_offset();

	    cmdtbl->prdt_entry[i].dba = (uint32_t)(buf_phys & 0xFFFFFFFF);
	    cmdtbl->prdt_entry[i].dbau = (uint32_t)((buf_phys >> 32) & 0xFFFFFFFF);

	    cmdtbl->prdt_entry[i].dbc = 8*1024 - 1;  // 8K bytes
	    cmdtbl->prdt_entry[i].i = 1;

	    buf += 4*1024;  // advance buffer pointer by 4K words (8KB)
	    count -= 16;    // 16 sectors
	}

	// last entry
	uintptr_t buf_virt = (uintptr_t)buf;
	uintptr_t buf_phys = buf_virt - get_phys_offset();

	cmdtbl->prdt_entry[cmdheader->prdtl - 1].dba = (uint32_t)(buf_phys & 0xFFFFFFFF);
	cmdtbl->prdt_entry[cmdheader->prdtl - 1].dbau = (uint32_t)((buf_phys >> 32) & 0xFFFFFFFF);
	cmdtbl->prdt_entry[cmdheader->prdtl - 1].dbc = (count << 9) - 1;  // remaining bytes
	cmdtbl->prdt_entry[cmdheader->prdtl - 1].i = 1;


	// Setup command
	FIS_REG_H2D *cmdfis = (FIS_REG_H2D*)(&cmdtbl->cfis);

	cmdfis->fis_type = FIS_TYPE_REG_H2D;
	cmdfis->c = 1;	// Command
	cmdfis->command = ATA_CMD_READ_DMA_EX;

	cmdfis->lba0 = (uint8_t)startl;
	cmdfis->lba1 = (uint8_t)(startl>>8);
	cmdfis->lba2 = (uint8_t)(startl>>16);
	cmdfis->device = 1<<6;	// LBA mode

	cmdfis->lba3 = (uint8_t)(startl>>24);
	cmdfis->lba4 = (uint8_t)starth;
	cmdfis->lba5 = (uint8_t)(starth>>8);

	cmdfis->countl = count & 0xFF;
	cmdfis->counth = (count >> 8) & 0xFF;

	// The below loop waits until the port is no longer busy before issuing a new command
	while ((port->tfd & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && spin < 1000000)
	{
		spin++;
	}
	if (spin == 1000000)
	{
		serial_puts("Port is hung\n");
		return false;
	}

	/*serial_puts("Port tfd before command: 0x");
	serial_puthex(port->tfd);
	serial_puts("\n");
	serial_puts("Port ssts: 0x");
	serial_puthex(port->ssts);
	serial_puts("\n");
	serial_puts("Port is: 0x");
	serial_puthex(port->is);
	serial_puts("\n");

	serial_puts("Port ci before command: 0x");
	serial_puthex(port->ci);
	serial_puts("\n");
*/
	for (int i = 0; i < 10; i++) {
	    /*serial_puts("Port ssts: 0x");
	    serial_puthex(port->ssts);
	    serial_puts("\n");*/
	    //timer_wait(100);
	}

	port->ci = 1<<slot;	// Issue command

	// Poll for completion
	spin = 0;
	while (port->ci & (1 << slot)) {
	    if (port->is & HBA_PxIS_TFES) { // Task file error
	        serial_puts("Read disk error\n");
	        return false;
	    }
	    if (++spin > 10000000) { // timeout (adjust as needed)
	        serial_puts("AHCI read timeout\n");
	        return false;
	    }
	}
	serial_puts("past loop!");

	// Check again
	if (port->is & HBA_PxIS_TFES)
	{
		serial_puts("Read disk error\n");
		return false;
	}

	return true;
}

bool ahci_readblock(uint64_t lba, unsigned char *buffer, unsigned int num) {
    if (!port) {
        // No SATA device found.
        return false;
    }
    uint16_t *buf = (uint16_t *)buffer;

    uint32_t startl = (uint32_t)(lba & 0xFFFFFFFF);
    uint32_t starth = (uint32_t)(lba >> 32);
    /*serial_puts("lba: ");
    serial_puthex(lba);*/

    bool status = ahci_read(port, startl, starth, num, buf);
    if (!status) return false;
    uint8_t *byte_buf = (uint8_t *)buffer;
    /*for (int i = 0; i < 16; i++) {
        serial_puthex(byte_buf[i]);
        serial_puts(" ");
    }*/
    return true;
}

bool ahci_writeblock(uint64_t lba, const unsigned char *buffer, unsigned int num) {
    if (!port) {
        // No SATA device found.
        return false;
    }
    uint16_t *buf = (uint16_t *)buffer;

    uint32_t startl = (uint32_t)(lba & 0xFFFFFFFF);
    uint32_t starth = (uint32_t)(lba >> 32);
    /*serial_puts("lba: ");
    serial_puthex(lba);*/

    bool status = ahci_write(port, startl, starth, num, buf);
    if (!status) return false;
    uint8_t *byte_buf = (uint8_t *)buffer;
    /*for (int i = 0; i < 16; i++) {
        serial_puthex(byte_buf[i]);
        serial_puts(" ");
    }*/
    return true;
}

unsigned char ahci_status() {
	// stub for now
	return 1;
}

void init_ahci(uint32_t abar) {
	// get base address and map
	uint64_t base_addr = (abar & 0xFFFFFFF0);
	quickmap(base_addr, base_addr);
	// map random address bc ig it isn't
	quickmap(0x000000010000000B, 0x000000010000000B);
	quickmap(0x0000000100001027, 0x0000000100001027);

	uint64_t base = 0x0000000000400000; // BAR address
	uint64_t size = 0x00090000;         // 64KB (for example)
	for (uint64_t offset = 0; offset < size; offset += 0x1000) {
	    quickmap(base + offset, base + offset);
	}
	// map allat bc idk.

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
	    	if (hba->pi & (1 << i) && check_type(&hba->ports[i]) == AHCI_DEV_SATA) {
	    		   flanterm_write(flanterm_get_ctx(), "\033[32m[AHCI] SATA device found!\033[0m", 30);
	    		   flanterm_write(flanterm_get_ctx(), "\n", 2);
	    	       port = &hba->ports[i];
	    	   }

	        port_rebase(port, i);
	    }
	}

	// we scan for active ports
	probe_port(hba);

	//hba->ghc |= (1 << 1);   // GHC.IE (interrupt enable)
	//hba->ghc |= (1 << 31);  // GHC.AE (AHCI enable)
  
	/*uint16_t wbuffer[256] = {0x6969};
	bool write = ahci_write(port, 0, 0, 1, wbuffer);
	if (write) {
		serial_puts("write success! \n");
		serial_puts("wbuffer: ");
		//serial_puthex(wbuffer);
		serial_puts("\n");
	} else {
		serial_puts("write failed :(");
	}

	uint16_t buffer[256]; // Buffer to hold 512 bytes (1 sector)
	uint8_t *byte_buf = (uint8_t *)buffer;
	bool success = ahci_read(port, 0, 0, 1, buffer);
    serial_puts("Buffer ptr: ");
    serial_puthex((uintptr_t)buffer);
    serial_puts("\n");
	if (success) {
	    // Output the first few bytes
	    for (int i = 0; i < 16; i++) {
	        serial_puthex(byte_buf[i]);5
	        serial_puts(" ");
	    }
	    serial_puts("\n");
	} else {
	    serial_puts("Read failed.\n");
	}
	*/
	unsigned int cluster;
	if (fat_getpartition()) {
    // 11 bytes 8.3 name, last 3 for extension.
    static const char fn[11] = {
        'T','E','S','T',' ',' ',' ',' ','T','X','T'
    };
    static const char fn2[11] = {
    	'S','U','B','D','I','R','~','1','T','X','T'
	};
	char *fd = fat_read("subdir/subdir2/abc.txt", 0);
	serial_puts("filedata.: ");
	for (size_t i = 0; i < 20; i++) {
	    serial_putc(fd[i]); // print each byte in hex
	    //serial_puts(" ");
	}
	serial_puts("\n");
	
}

char *path = "test.txt";
int fd = open("test.txt", ACCESS_READ, 0);  // flags, mode
if (fd < 0) {
    serial_puts("open failed :(\n");
    return;
}

char buf[70];
int bytes_read = read(fd, buf, 69);
if (bytes_read > 0) { 
    buf[bytes_read] = '\0';  // Null terminate for serial_puts
    serial_puts("read success!\n");
    serial_puts("buf: ");
    serial_puts(buf);
} else if (bytes_read == 0) {
    serial_puts("EOF reached\n");
} else {
    serial_puts("read failed :(\n");
}
close(fd);

stat_t file_stat_t;
stat("test.txt", &file_stat_t);
serial_puts("size (from fstat): ");
serial_puthex(file_stat_t.st_size);

//unsigned char buf[512];
//ahci_readblock(0, buf, 1);
//serial_puts(buf);
unsigned char d[512];
serial_puts("testing readblock...\n");
ahci_readblock(0, d, 1);
}