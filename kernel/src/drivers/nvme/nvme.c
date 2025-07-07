#include <stdint.h>
#include "nvme.h"
#include "../../iodebug.h"
#include "../../memory.h"
#include "../../util/fb.h"
#include "../../mm/pmm.h"
#include "../../timer.h"
#include "../../thread/thread.h"
#include "../pci/pci.h"

uint64_t nvme_cap_strd;
uint32_t bar0;
uint32_t bar1;

uint64_t get_nvme_base_address(uint16_t bus, uint16_t device, uint16_t function) {
    bar0 = pci_read_dword(bus, device, function, 0x10); // BAR0
     bar1 = pci_read_dword(bus, device, function, 0x14); // BAR1 (high 32 bits)

    // Mask out lower 4 bits from bar0 (flags), keep upper 28 bits
    uint64_t base_addr = ((uint64_t)bar1 << 32) | (bar0 & 0xFFFFFFF0);
    //serial_puthex(base_addr);
    return base_addr;
}

uint32_t nvme_read_reg(uint32_t offset) {
	volatile uint32_t *nvme_reg = (volatile uint32_t *)(nvme_base_addr + offset);
	return *nvme_reg;
}

void nvme_write_reg(uint32_t offset, uint32_t value) {
	volatile uint32_t *nvme_reg = (volatile uint32_t *)(nvme_base_addr + offset);
	*nvme_reg = value;
}

void create_admin_sq(nvme_queue_t *sq) {
	sq->address = (uint64_t)palloc(1, true);
	if (sq->address == 0) {
		serial_puts("admin sq creation failed!");
	}
	sq->size = 63;
	nvme_write_reg(0x28, (uint32_t)(uintptr_t)sq->sq_entries);  // Lower 32 bits
	nvme_write_reg(0x2C, (uint32_t)(((uintptr_t)sq->sq_entries) >> 32));  // Upper 32 bits
}

void create_admin_cq(nvme_queue_t *cq) {
	cq->address = (uint64_t)palloc(1, true);
	if (cq->address == 0) {
		serial_puts("admin cq creation failed!");
	}
	cq->size = 63;
	nvme_write_reg(0x30, (uint32_t)(uintptr_t)cq->cq_entries);  // Lower 32 bits
	nvme_write_reg(0x34, (uint32_t)(((uintptr_t)cq->cq_entries) >> 32));  // Upper 32 bits
}

void nvme_send_command(uint8_t opcode, uint32_t nsid, void *data, uint64_t lba, uint16_t num_blocks, nvme_cq_entry_t *completion, nvme_queue_t *queue) {
	uint16_t submission_queue_tail = queue->sq_tail;
	uint16_t completion_queue_head = queue->cq_head;

	uint64_t sq_entry_addr = queue->address + (submission_queue_tail * sizeof(nvme_cq_entry_t));
	uint64_t cq_entry_addr = queue->address + (completion_queue_head * sizeof(nvme_cq_entry_t));
	nvme_sq_entry_t command_entry;
	command_entry.dword_0.opcode = opcode;
	command_entry.nsid = nsid;
	command_entry.prp1 = (uintptr_t)data;
	command_entry.prp2 = 0;
	command_entry.cdw[0] = (uint32_t)lba;
	command_entry.cdw[1] = (uint32_t)((uint64_t)lba >> 32);
	command_entry.cdw[2] = (uint16_t)(num_blocks - 1);
	memcpy((void *)sq_entry_addr, &command_entry, sizeof(nvme_cq_entry_t));
	submission_queue_tail++;
	nvme_write_reg(0x1000 + 2 * (4 << nvme_cap_strd), submission_queue_tail);
	if (submission_queue_tail == QUEUE_SIZE)
		submission_queue_tail = 0;
	// You should wait for completion here
	quickmap(cq_entry_addr, cq_entry_addr);
	completion = (nvme_cq_entry_t *)cq_entry_addr;
	completion_queue_head++;
	nvme_write_reg(0x1000 + 3 * (4 << nvme_cap_strd), completion_queue_head);
	if (completion_queue_head == QUEUE_SIZE)
		completion_queue_head = 0;
	//return completion->status != 0;
}

void create_io_cq(nvme_queue_t *cq) {
    cq->address = (uint64_t)palloc(1, true);
    if (cq->address == 0) {
        serial_puts("I/O CQ creation failed!");
        return;
    }
    cq->size = QUEUE_SIZE;
    nvme_write_reg(0x30, (uint32_t)cq->address);
    nvme_write_reg(0x34, (uint32_t)(cq->address >> 32));
}

void create_io_sq(nvme_queue_t *sq, nvme_queue_t *cq) {
    sq->address = (uint64_t)palloc(1, true);
    if (sq->address == 0) {
        serial_puts("I/O SQ creation failed!");
        return;
    }
    sq->size = QUEUE_SIZE;
    sq->id = 1;
    sq->sq_head = sq->sq_tail = 0;
    sq->cq_entries[0].cid = 1;
    sq->cq_entries[0].status = 0;
    //sq->cq_entries[0].phase = 0;
    nvme_write_reg(0x28, (uint32_t)sq->address);
    nvme_write_reg(0x2C, (uint32_t)(sq->address >> 32));
    nvme_write_reg(0x30, sq->id);
    nvme_write_reg(0x34, (uint32_t)(sq->id >> 32));
    nvme_write_reg(0x38, (uint32_t)(sq->size));
    nvme_write_reg(0x3C, (uint32_t)(sq->size >> 32));
    nvme_write_reg(0x40, (uint32_t)(sq->cq_entries));
   // nvme_write_reg(0x44, (uint32_t)(sq->cq_entries >> 32));
}

void submit_identify_cmd(nvme_queue_t *sq, nvme_queue_t *cq) {
	uint64_t cap = nvme_read_reg(0x00);  // Read entire CAP register
	uint8_t dstrd = (cap >> 32) & 0xF;  // Extract DSTRD bits (32-35)
    nvme_sq_entry_t cmd = {0};
    cmd.dword_0.opcode = 0x06;
    cmd.nsid = 0;
    cmd.prp1 = (uintptr_t)&cq->cq_entries[0];
    cmd.cdw[10] = 1;
    memcpy((void *)(sq->address + sq->sq_tail * sizeof(nvme_sq_entry_t)), &cmd, sizeof(nvme_sq_entry_t));
    sq->sq_tail = (sq->sq_tail + 1) % 64;
    nvme_write_reg(0x1000 + (2 * sq->id) * (4 << dstrd), sq->sq_tail);
}

void check_io_queues(nvme_queue_t *sq, nvme_queue_t *cq) {
    // Step 1: Check Controller Readiness
    uint32_t csts = nvme_read_reg(nvme_base_addr + 0x1C);
    if (!(csts & (1 << 0))) {
        serial_puts("Controller is not ready\n");
        return;
    }

    // Step 2: Submit Identify Controller Command
    submit_identify_cmd(sq, cq);
    while (!(cq->cq_entries[cq->cq_head].cid == 1 && cq->cq_entries[cq->cq_head].status == 0)) {
        asm volatile("pause");
    }
    serial_puts("Identify command completed successfully\n");

    while (!(cq->cq_entries[cq->cq_head].cid == 2 && cq->cq_entries[cq->cq_head].status == 0)) {
        asm volatile("pause");
    }
    serial_puts("I/O Completion Queue created successfully\n");

    // Step 4: Submit Dummy I/O Command
    uint8_t data[4096] = {0};  // Dummy data
    nvme_send_command(0x01, 0, data, 0, 1, NULL, sq);  // Read 1 block at LBA 0
    while (!(cq->cq_entries[cq->cq_head].cid == 3 && cq->cq_entries[cq->cq_head].status == 0)) {
        asm volatile("pause");
    }
    serial_puts("Dummy I/O command completed successfully\n");
}

void init_nvme() {
	quickmap(0x000000C000000000, 0x000000C000000000);
	quickmap(0x100C, 0x100C);
	quickmap(0x000000C0FFFFFFFF, 0x000000C0FFFFFFFF);
	quickmap(0x000000C100000000, 0x000000C100000000);
	quickmap(0x000000000000000C, 0x000000000000000C);
	quickmap(0x000000C000001004, 0x000000C000001004);

	nvme_cap_strd = (nvme_base_addr >> 12) & 0xF;
	uint32_t nvme_version_reg = nvme_read_reg(0x08);
	//uint16_t major = (nvme_version_reg >> 16) & 0xFFFF;
	//uint8_t minor  = (nvme_version_reg >> 8) & 0xFF;
	//uint8_t patch  = nvme_version_reg & 0xFF;
    char majorstr[5], minorstr[5], patchstr[5];
    //uint16_to_hex(major, majorstr);
    //uint16_to_hex(minor, minorstr);
    //uint16_to_hex(patch, patchstr);
    uint32_t max_queue_entries = nvme_read_reg(0) & 0xFFFF;
	/*flanterm_write(flanterm_get_ctx(), "[NVME] Version: ", 20);
	flanterm_write(flanterm_get_ctx(), majorstr, 20);
	flanterm_write(flanterm_get_ctx(), " ", 20);
	flanterm_write(flanterm_get_ctx(), minorstr, 20);
	flanterm_write(flanterm_get_ctx(), " ", 20);
	flanterm_write(flanterm_get_ctx(), patchstr, 20);
	flanterm_write(flanterm_get_ctx(), "\n", 20);
	*/
	serial_puts("[NVME] Version: ");
	serial_puthex(nvme_version_reg);
	serial_puts("\n");

	    // 7. Enable controller
    uint32_t cc =(nvme_read_reg(nvme_base_addr + 0x14));
    cc |= (1 << 0);  // Enable bit
    nvme_write_reg(0x14, cc);

    // 8. Wait for controller ready
    while (!(nvme_read_reg(nvme_base_addr + 0x1C) & (1 << 0))) 
        asm("pause");

	// allocate and create admin queues.
	nvme_queue_t *sq = palloc((sizeof(nvme_queue_t) + PAGE_SIZE - 1) / PAGE_SIZE, true);
	nvme_queue_t *cq = palloc((sizeof(nvme_queue_t) + PAGE_SIZE - 1) / PAGE_SIZE, true);
	create_admin_sq(sq);
	create_admin_cq(cq);

	#define IO_QUEUE_SIZE 64  // Actual entries = 64, size field = 63

	// Allocate contiguous memory for completion queue (16 bytes per entry)
	uint64_t *cq_mem = palloc((IO_QUEUE_SIZE * 16 + PAGE_SIZE - 1) / PAGE_SIZE, true);
    uint16_t cq_id = 1;  // IO CQ ID (0 is reserved for admin queues)
    uint16_t irq_vector = 35;  // MSI-X vector index

	struct nvme_admin_command cmd = {
	  .opcode = 0x05,  // Create IO CQ opcode
	  .cid = 1,
	  .prp1 = (uintptr_t)cq_mem,  // Physical address of CQ
	  .cdw10 = (IO_QUEUE_SIZE - 1) << 16 | cq_id,  // [31:16]=size, [15:0]=cq_id
	  .cdw11 = (1 << 1) | (1 << 0),  // Enable interrupts (bit 1), contiguous (bit 0)
	  .cdw12 = (1 << 1) | (1 << 0) | (irq_vector << 16)    // MSI-X vector in upper 16 bits
	};

uint64_t cap = nvme_read_reg(0x00);  // Read entire CAP register
uint8_t dstrd = (cap >> 32) & 0xF;  // Extract DSTRD bits (32-35)
uint32_t doorbell_stride = 4 << dstrd;  // Stride in bytes
volatile uint32_t *sq_db = (uint32_t*)(bar0 + 0x1000 + (2 * 1) * doorbell_stride);

	memcpy((void*)(sq->address + sq->sq_tail * 64), &cmd, sizeof(cmd));
	sq->sq_tail = (sq->sq_tail + 1) % 64;
	nvme_write_reg(*sq_db, sq->sq_tail);  // Ring admin doorbell

	// Wait for completion (poll ACQ)
nvme_cq_entry_t *cqe = &cq->cq_entries[cq->cq_head];

// Print completion status
serial_puts("IO CQ Creation Status: ");
serial_puthex(cqe->status);
serial_puts("\n");

#define NVME_ADMIN_CQ_DOORBELL (nvme_base_addr + 0x1000 + (1 * doorbell_stride))

// Advance completion queue head and ring doorbell
cq->cq_head = (cq->cq_head + 1) % IO_QUEUE_SIZE;
nvme_write_reg(NVME_ADMIN_CQ_DOORBELL, cq->cq_head);


	uint32_t csts = nvme_read_reg(nvme_base_addr + 0x1C);
    serial_puts("CSTS: ");
    serial_puthex(doorbell_stride);
    serial_puts("\n");
    serial_puts("doorbell_stride: ");
    serial_puthex(csts);
    serial_puts("\n");
    create_io_cq(cq);
    create_io_sq(sq, cq);
    check_io_queues(sq, cq);
	task_exit();
}