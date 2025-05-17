struct rsdp_t *get_acpi_table(void);
struct xsdt_t *get_xsdt_table(void);
void findFACP (struct xsdt_t *xsdt);

typedef struct rsdp_t rsdp_t;
typedef struct rsdp_ext_t;