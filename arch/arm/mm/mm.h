#ifdef CONFIG_MMU

/* the upper-most page table pointer */
extern pmd_t *top_pmd;

/*
 * 0xffff8000 to 0xffffffff is reserved for any ARM architecture
 * specific hacks for copying pages efficiently, while 0xffff4000
 * is reserved for VIPT aliasing flushing by generic code.
 *
 * Note that we don't allow VIPT aliasing caches with SMP.
 */
#define COPYPAGE_MINICACHE	0xffff8000
#define COPYPAGE_V6_FROM	0xffff8000
#define COPYPAGE_V6_TO		0xffffc000
/* PFN alias flushing, for VIPT caches */
#define FLUSH_ALIAS_START	0xffff4000

static inline void set_top_pte(unsigned long va, pte_t pte)
{
	pte_t *ptep = pte_offset_kernel(top_pmd, va);
	set_pte_ext(ptep, pte, 0);
	local_flush_tlb_kernel_page(va);
}

static inline pte_t get_top_pte(unsigned long va)
{
	pte_t *ptep = pte_offset_kernel(top_pmd, va);
	return *ptep;
}

static inline pmd_t *pmd_off_k(unsigned long virt)
{
	return pmd_offset(pud_offset(pgd_offset_k(virt), virt), virt);
}

struct mem_type;

const struct mem_type *get_mem_type(unsigned int type);

extern void __flush_dcache_page(struct address_space *mapping, struct page *page);

#endif

struct map_desc;

void __init bootmem_init(void);
void arm_mm_memblock_reserve(void);
void __init create_mapping(struct map_desc *md);
