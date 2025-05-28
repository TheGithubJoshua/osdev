#include <stdint.h>
#include <stdint.h>
    
#define NVME_DOORBELL_BASE 0x1000
#define QUEUE_SIZE 64
#define NVME_OPC_CREATE_IO_CQ 0x05
#define QUEUE_FLAG_PHYS_CONTIG  (1 << 0)
#define QUEUE_FLAG_INTERRUPTS    (1 << 1)

typedef struct nvme_dword_0 {
    uint8_t opcode;   
    uint8_t fused : 2;   
    uint8_t psdt : 2;      
    uint16_t cid;
} nvme_dword_0_t;

typedef struct nvme_sq_entry {
    nvme_dword_0_t dword_0;
    uint32_t nsid;
    uint64_t reserved;
    uint64_t metadata_ptr;
    uint64_t prp1;
    uint64_t prp2;
    uint32_t cdw[6];
} nvme_sq_entry_t;

typedef struct nvme_cq_entry {
    uint32_t result;
    uint32_t reserved;     
    uint16_t sq_head;     
    uint16_t sq_id;      
    uint16_t cid;         
    uint16_t status;      
} nvme_cq_entry_t;

typedef struct nvme_queue {
    nvme_sq_entry_t *sq_entries;
    nvme_cq_entry_t *cq_entries;
    uint16_t id;
    uint16_t sq_head;
    uint16_t sq_tail;
    uint16_t cq_head;
    uint16_t cq_tail;
    uint16_t size;
    uint16_t qid;

    uint64_t address;
} nvme_queue_t;

struct nvme_admin_command {
    // DW0
    uint8_t  opcode;
    uint16_t cid;
    uint8_t  flags;
    uint16_t command_id;
    // DW1
    uint32_t nsid;
    // DW2-3
    uint64_t rsvd1;
    // DW4-5
    uint64_t metadata;
    // DW6-7
    uint64_t prp1;
    // DW8-9
    uint64_t prp2;
    // DW10-15
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed));

uint64_t get_nvme_base_address(uint16_t bus, uint16_t device, uint16_t function);
void init_nvme();
uint32_t nvme_read_reg(uint32_t offset);
uint64_t get_nvme_base_address(uint16_t bus, uint16_t device, uint16_t function);
