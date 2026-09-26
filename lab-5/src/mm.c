#include "irq.h"
#include "mm.h"
#include "uart.h"
#include "print.h"
#include "cpio.h"
#include "dtb.h"
#include "startup.h"
#include "mailbox.h"

#include <stddef.h>
page_t *bookkeep;
unsigned long page_frame_count;
uintptr_t memory_base;
free_area_t free_area[MAX_ORDER + 1];

obj_allocator_t obj_alloc_pool[MAX_OBJ_ALLOCTOR_NUM];

/* Set after free_area[] contains valid list heads. */
static int free_area_ready;

/*
 * Add an end-exclusive PFN range to the buddy allocator.  Split the range
 * into the largest aligned blocks possible so no block crosses a range
 * boundary (in particular, a reserved-memory boundary).
 */
static void add_free_pfn_range(unsigned long start_pfn,
                               unsigned long end_pfn)
{
    unsigned long pfn;

    for (pfn = start_pfn; pfn < end_pfn; pfn++) {
        bookkeep[pfn].order = -1;
        bookkeep[pfn].used = Free;
        INIT_LIST_HEAD(&bookkeep[pfn].list);
    }

    pfn = start_pfn;
    while (pfn < end_pfn) {
        int order = MAX_ORDER;

        while (order > 0) {
            unsigned long block_pages = 1UL << order;

            if ((pfn & (block_pages - 1)) == 0 &&
                block_pages <= end_pfn - pfn)
                break;
            order--;
        }

        push_block_to_free_area(&bookkeep[pfn], &free_area[order], order);
        pfn += 1UL << order;
    }
}

void page_init() 
{
    free_area_ready = 0;
    for (unsigned long i = 0;i < PAGE_FRMAME_NUM;i++) {
        bookkeep[i].pfn = i;
        bookkeep[i].used = Free;
        bookkeep[i].phy_addr = LOW_MEMORY + i*PAGE_SIZE;
        bookkeep[i].obj_used = 0;
        bookkeep[i].obj_alloc = NULL;
        bookkeep[i].free = NULL;
        bookkeep[i].order = -1; 
        INIT_LIST_HEAD(&bookkeep[i].list);
    }
}

void free_area_init()
{
    for (int i = 0;i < MAX_ORDER + 1;i++) {
        INIT_LIST_HEAD(&free_area[i].freelist);
        free_area[i].nr_free = 0;
    }

    free_area_ready = 1;

    /* Preserve pages reserved between page_init() and free_area_init(). */
    unsigned long pfn = 0;
    while (pfn < PAGE_FRMAME_NUM) {
        unsigned long range_start;

        while (pfn < PAGE_FRMAME_NUM && bookkeep[pfn].used != Free)
            pfn++;
        range_start = pfn;
        while (pfn < PAGE_FRMAME_NUM && bookkeep[pfn].used == Free)
            pfn++;

        if (range_start < pfn)
            add_free_pfn_range(range_start, pfn);
    }
}

void push_block_to_free_area(page_t *pushed_block, free_area_t *fa, int order) 
{
    list_add_tail(&pushed_block->list, &fa->freelist);
    pushed_block->order = order;
    pushed_block->used = Free;
    fa->nr_free += 1;
}

void pop_block_from_free_area(page_t *poped_block, free_area_t *fa) {
    list_del(&poped_block->list);
    poped_block->used = Taken;
    fa->nr_free--;
}

static struct page * buddy_block_alloc_locked(int order)
{
    #ifdef __DEBUG
    // printf("\n[buddy_block_alloc]Before allocate buddy memory:");
    // dump_buddy();
    printf("[buddy_block_alloc] Requested Order: %d, Size: %d\n\n", order, 1 << order);
    #endif //__DEBUG

    if ( (order<0) | (order>MAX_ORDER) ) {
        printf("[buddy_block_alloc] %d is invalid order!\n", order);
        return 0;
    }

   

    for (int i = order;i <= MAX_ORDER;i++) {
        if (list_empty(&free_area[i].freelist)) continue;
        
        // Found target block, get and remove one block from target freelist
        struct page *target_block = (struct page *) free_area[i].freelist.next;
        pop_block_from_free_area(target_block, &free_area[i]);
        target_block->order = order;

        // Cut off bottom half of the block and put it back to corresponding freelist
        // until the size equals the requested order. 
        for (int current_order = i;current_order > order;current_order--) {
            int downward_order = current_order - 1; 

            int buddy_pfn = FIND_BUDDY_PFN(target_block->pfn, downward_order);
            struct page *bottom_half_block = &bookkeep[buddy_pfn];
            push_block_to_free_area(bottom_half_block, &free_area[downward_order], downward_order);

            #ifdef __DEBUG   
            printf("Push back -> Redundant block(bottom half)  { pfn(%d), order(%d) }\n", 
                    bottom_half_block->pfn, bottom_half_block->order);
            #endif //__DEBUG
        }

        #ifdef __DEBUG   
        printf("\n[buddy_block_alloc]After allocate buddy memory:");
        dump_buddy();
        
        printf("[buddy_block_alloc] Result - Allocated block{ pfn(%d), order(%d), phy_addr_16(0x%x) }\n",
                target_block->pfn, target_block->order, target_block->phy_addr);
        printf("[buddy_block_alloc] **Done**\n\n");
        #endif //__DEBUG

        return target_block;
        
    }

    
    printf("[buddy_block_alloc] No free memory space!\n");
    return 0;
}

static void buddy_block_free_locked(struct page *block)
{
    if (!block || block->used != Taken || block->order < 0)
        return;

    #ifdef __DEBUG
     printf("\n[buddy_block_free] **Start free block{ pfn(%d), order(%d) }**\n", 
           block->pfn, block->order);
    // printf("\n[buddy_block_free]Before free memory:");
    // dump_buddy();
    #endif //__DEBUG

    // A runtime-sized pool may end before the computed buddy PFN.
    while (block->order < MAX_ORDER) {
        unsigned long buddy_pfn = FIND_BUDDY_PFN(block->pfn, block->order);
        if (buddy_pfn >= page_frame_count)
            break;
        page_t *buddy = &bookkeep[buddy_pfn];
        if (buddy->used != Free || buddy->order != block->order)
            break;

        int order = block->order;
        unsigned long left = FIND_LBUDDY_PFN(block->pfn, order);
        pop_block_from_free_area(buddy, &free_area[order]);
        block->order = -1;
        buddy->order = -1;
        block = &bookkeep[left];
        block->order = order + 1;
    }
    // Push merged block to freelist
    push_block_to_free_area(block, &free_area[block->order], block->order);

    #ifdef __DEBUG
    printf("\n[buddy_block_free]After free memory:");
    dump_buddy();
    printf("[buddy_block_free] **Done**\n\n");
    #endif //__DEBUG

    
}


#ifdef __DEBUG
void dump_buddy()
{
    printf("\n---------Buddy Debug---------\n");
    printf("***Freelist(free_area) Debug***");
    for (int i = 0;i < MAX_ORDER+1 ;i++) {
        printf("\nOrder-%d\n", i);

        struct list_head *pos;
        list_for_each(pos, (struct list_head *) &free_area[i].freelist) {
            printf(" -> {pfn(%d)} {phy_addr_16(0x%x)}", ((struct page *)pos)->pfn, ((struct page *)pos)->phy_addr);
        }
        
            
    }
    printf("\n---------End Buddy Debug---------\n\n");

}
#endif

void __init_obj_alloc(obj_allocator_t *obj_allocator_p, int objsize)
{
    INIT_LIST_HEAD(&obj_allocator_p->full);   
    INIT_LIST_HEAD(&obj_allocator_p->partial); 
    INIT_LIST_HEAD(&obj_allocator_p->empty);   
    obj_allocator_p->curr_page = NULL;
    
    obj_allocator_p->objsize = objsize;
    obj_allocator_p->obj_per_page = PAGE_SIZE / objsize;
    obj_allocator_p->obj_used = 0;
    obj_allocator_p->page_used = 0;
    
}

void __init_obj_page(page_t *page_p) 
{
    page_p->obj_used = 0;
    page_p->obj_alloc = NULL;
    page_p->free = NULL;
}

static int register_obj_allocator_locked(int objsize)
{
    if (objsize < MIN_ALLOCATAED_OBJ_SIZE) {
        objsize = MIN_ALLOCATAED_OBJ_SIZE;
        printf("[register_obj_allocator] Min object size is 8, automatically set it to 8 \n");
    }

    if (objsize > MAX_ALLOCATAED_OBJ_SIZE) {
        objsize = MAX_ALLOCATAED_OBJ_SIZE;
        printf("[register_obj_allocator] Max object size is 2048, automatically set it to 2048 \n");
    }

    for (int token = 0;token < MAX_OBJ_ALLOCTOR_NUM;token++) {
        if (obj_alloc_pool[token].objsize != 0) 
            continue;

        __init_obj_alloc(&obj_alloc_pool[token], objsize);

        //#ifdef __DEBUG
        printf("[register_obj_allocator] Successfully Register object allocator! {objsize(%d), token(%d)}\n"
                ,objsize, token);
        //#endif //__DEBUG 

        return token;
    }

    printf("[register_obj_allocator] Allocator pool has been fully registered.\n");
    return -1;
}

static void * obj_allocate_locked(int token)
{
    if (token < 0 || token >= MAX_OBJ_ALLOCTOR_NUM) {
        printf("[obj allocator] Invalid token\n");
        return 0;
    }    
    
    obj_allocator_t *obj_allocator_p = &obj_alloc_pool[token];
    void *allocated_addr = NULL; // address of allocated object 

    #ifdef __DEBUG
    printf("[obj_allocate] Requested token: %d, size: %d\n",token, obj_allocator_p->objsize);
    // printf("[obj_allocate] Before allocation:");
    // dump_obj_alloc(obj_allocator_p);
    #endif //__DEBUG
    
    if (obj_allocator_p->curr_page == NULL) {
        page_t *page_p;
        if (!list_empty(&obj_allocator_p->partial)) { 
            // Use partial allocated page
            page_p = (page_t *) obj_allocator_p->partial.next;
            list_del(&page_p->list);
        } else if (!list_empty(&obj_allocator_p->empty)) {
            // Use empty(no alloacted object) page
            page_p = (page_t *) obj_allocator_p->empty.next;
            list_del(&page_p->list);
        } else {
            // Demand a new page from buddy memory allocator
            page_p = buddy_block_alloc(0);
            if (!page_p)
                return NULL;
            __init_obj_page(page_p);
            page_p->obj_alloc = obj_allocator_p;

            obj_allocator_p->page_used += 1;
        }
        obj_allocator_p->curr_page = page_p;
    }

    /* TODO: Explain how obj_freelist work*/
    struct list_head *obj_freelist = obj_allocator_p->curr_page->free;
    if (obj_freelist != NULL) {
        // Allocate memory by free list in current page
        allocated_addr = obj_freelist;
        obj_allocator_p->curr_page->free = obj_freelist->next;
    }
    else {
        // Allocate memory to requested object 
        allocated_addr = (void *) obj_allocator_p->curr_page->phy_addr + 
                         obj_allocator_p->curr_page->obj_used * obj_allocator_p->objsize;
    }

    obj_allocator_p->obj_used += 1;
    obj_allocator_p->curr_page->obj_used += 1;

    // Check if page full
    if (obj_allocator_p->obj_per_page == obj_allocator_p->curr_page->obj_used) {
        list_add_tail(&obj_allocator_p->curr_page->list, &obj_allocator_p->full);
        obj_allocator_p->curr_page = NULL;
    }

    
    #ifdef __DEBUG
    printf("[obj_allocate] Allocated address: {phy_addr_16(%x)}\n", allocated_addr);
    printf("[obj_allocate] After allocation:");
    dump_obj_alloc(obj_allocator_p);
    printf("[obj_allocate] **Done**\n\n");
    #endif //__DEBUG

    
    return allocated_addr;
    

}

static void obj_free_locked(void *obj_addr)
{
    // Find out corressponding page frame number and object allocator it belongs to.
    int obj_pfn = PHY_ADDR_TO_PFN(obj_addr);
    page_t *page_p = &bookkeep[obj_pfn];
    obj_allocator_t *obj_allocator_p = page_p->obj_alloc;
    
    #ifdef __DEBUG
    printf("\n[obj_free] Free object procedure!\n");
    printf("[obj_free] Page info: 0x%x {pfn=(%d), obj_used(%d))\n", obj_addr, obj_pfn, page_p->obj_used);
    printf("[obj_free] object free list point to {0x%x}\n", page_p->free);
    // printf("[obj_free] Before free:");
    // dump_obj_alloc(obj_allocator_p);
    #endif // __DEBUG

    // Make page's object freelist point to address of new first free object.
    // And the contect of released object should record the orginal address 
    // of first free object that object freelist previously point to.
    // As the result, if we want to access second free object, just 
    // using free->next(first 8 bytes) to get expected address. 
    // So we can link and access all free object by this strategy without extra
    // moemory space.
    struct list_head *temp = page_p->free;
    page_p->free = (struct list_head *) obj_addr;
    page_p->free->next = temp;

    obj_allocator_p->obj_used -= 1;
    page_p->obj_used -= 1;

    // From full to partial 
    if (obj_allocator_p->obj_per_page-1 == page_p->obj_used) {
        list_del(&page_p->list); // pop out from full list
        list_add_tail(&page_p->list, &obj_allocator_p->partial); // add to partial list 
    }

    // From partial to empty
    // and make sure this page not currently used by object allocator
    if (page_p->obj_used == 0 && obj_allocator_p->curr_page != page_p) {
        list_del(&page_p->list); // pop out from partial list

        // Return empty page to free page pool(free_area) if memory becomes tight
        // otherwise, add it to empty list 
        if (obj_allocator_p->page_used >= 10) { // TODO: Return empty page only if memory becomes tight
            #ifdef __DEBUG
            printf("[obj_free] Free empty page bacause memory is tight");
            #endif // __DEBUG

            obj_allocator_p->page_used -= 1;
            page_p->free = NULL;
            page_p->obj_alloc = NULL;
            page_p->obj_used = 0;
            buddy_block_free(page_p);
        } else {
            // Add to empty list
            list_add_tail(&page_p->list, &obj_allocator_p->empty);
        }
    }

    #ifdef __DEBUG
    printf("[obj_free] After free:");
    dump_obj_alloc(obj_allocator_p);
    printf("[obj_free] **Done**\n\n");
    #endif // __DEBUG
}

#ifdef __DEBUG
void dump_obj_alloc(obj_allocator_t *obj_allocator_p)
{
    printf("\n---------Object Allocator Debug---------\n");
    printf("objsize = %d\n", obj_allocator_p->objsize);
    printf("obj_per_page = %d\n", obj_allocator_p->obj_per_page);
    printf("obj_used = %d\n", obj_allocator_p->obj_used);
    printf("page_used = %d\n", obj_allocator_p->page_used);

    
    printf("\nobject_allocator->curr_page current page info:\n");
    if (obj_allocator_p->curr_page != NULL) {
        printf("obj_allocator_p->curr_page = {0x%x}\n", obj_allocator_p->curr_page->phy_addr);
        printf("obj free list point to {0x%x}\n", obj_allocator_p->curr_page->free);
        printf("obj_used = %d\n", obj_allocator_p->curr_page->obj_used);
        printf("pfn = %d\n", obj_allocator_p->curr_page->pfn);
        
    }
    else {
        printf("object_allocator->curr_page is NULL currently\n");
    }
    printf("\n");

    struct list_head *pos;
    printf("object_allocator->full list:\n");
    list_for_each(pos, (struct list_head *) &obj_allocator_p->full) {
        printf("--> {pfn(%d)}", ((struct page*) pos)->pfn);
    }
    printf("\n");

    printf("object_allocator->partial list:\n");
    list_for_each(pos, (struct list_head *) &obj_allocator_p->partial) {
        printf("--> {pfn(%d)}", ((struct page*) pos)->pfn);
    }
    printf("\n");

    printf("object_allocator->empty list:\n");
    list_for_each(pos, (struct list_head *) &obj_allocator_p->empty) {
        printf("--> {pfn(%d)}", ((struct page*) pos)->pfn);
    }

    printf("\n---------End Object Allocator Debug---------\n\n");

}
#endif

void __init_kmalloc()
{
    for (int i = MIN_KMALLOC_ORDER;i <= MAX_KMALLOC_ODER;i++) {
        register_obj_allocator(1 << i);
    }
}

static void * kmalloc_locked(int size)
{
    #ifdef __DEBUG
    printf("[kmalloc] Requested Size: %d\n", size);
    #endif //__DEBUG

    void *allocated_addr;

    // Object allocator
    for (int i = MIN_KMALLOC_ORDER;i <= MAX_KMALLOC_ODER;i++) {
        if (size <= (1<<i)) {
            allocated_addr = obj_allocate(i - MIN_KMALLOC_ORDER);

            #ifdef __DEBUG
            printf("[kmlloc] Allocated address: 0x%x\n", allocated_addr);
            printf("[kmlloc] **Done**\n\n");
            #endif //__DEBUG

            return allocated_addr;
        }
    }
    // Buddy Memory allocator
    for (int i = 0;i <= MAX_ORDER;i++) {
        if (size <= 1<<(i + PAGE_SHIFT)) {
            page_t *block = buddy_block_alloc(i);
            allocated_addr = block ? (void *)block->phy_addr : NULL;

            #ifdef __DEBUG
            printf("[kmlloc] Allocated address: 0x%x\n", allocated_addr);
            printf("[kmlloc] **Done**\n\n");
            #endif //__DEBUG

            return allocated_addr;
        }
    }
    
    printf("[kmalloc] %d Bytes too large!\n", size);
    return NULL;
}

static void kfree_locked(void *addr)
{
    if (!addr)
        return;
    #ifdef __DEBUG
    printf("[kfree] Free Memory Address: 0x%x\n", addr);
    #endif //__DEBUG

    int pfn = PHY_ADDR_TO_PFN(addr);
    page_t *page_p = &bookkeep[pfn];

    if (page_p->obj_alloc != NULL) {
        // Belongs to Object Allocator
        obj_free(addr);
    } else {
        // Belongs to Buddy Memory Allocator
        buddy_block_free(page_p);
    }

    #ifdef __DEBUG
    printf("[kfree] **Done**\n\n");
    #endif //__DEBUG
}

static void startup_panic(void)
{
    printf("[mm] Startup memory initialization failed\n");
    for (;;)
        ;
}

static void reserve_boot_range(uintptr_t start, uintptr_t end)
{
    if (start < end && startup_reserve(start, end - start) < 0)
        startup_panic();
}

void mm_init()
{
    extern char _start[], _end[];
    extern void *_dtb_ptr;
    static int initialized;
    uintptr_t base, end;
    size_t size;

    /* The shell's ma command must not reset live allocations. */
    if (initialized)
        return;
    if (mbox_get_arm_memory(&base, &size) < 0 || size > UINTPTR_MAX - base ||
        base > UINTPTR_MAX - (PAGE_SIZE - 1))
        startup_panic();
    end = (base + size) & ~(uintptr_t)(PAGE_SIZE - 1);
    memory_base = (base + PAGE_SIZE - 1) & ~(uintptr_t)(PAGE_SIZE - 1);
    if (end <= memory_base)
        startup_panic();
    page_frame_count = (end - memory_base) / PAGE_SIZE;

    startup_init(memory_base, end);
    reserve_boot_range(0, PAGE_SIZE); // firmware spin table / low memory
    reserve_boot_range((uintptr_t)_start, (uintptr_t)_end); // includes stack/BSS
    reserve_boot_range((uintptr_t)cpio_addr, (uintptr_t)cpio_end);

    if (_dtb_ptr) {
        uintptr_t dtb = (uintptr_t)_dtb_ptr;
        struct fdt_header *header = _dtb_ptr;
        uint32_t total = fdt_u32_le2be(&header->totalsize);
        uint32_t offset = fdt_u32_le2be(&header->off_mem_rsvmap);
        if (fdt_u32_le2be(&header->magic) != 0xd00dfeed ||
            total < sizeof(*header) || total > UINTPTR_MAX - dtb)
            startup_panic();
        reserve_boot_range(dtb, dtb + total);

        /* FDT reserve-map entries are pairs of big-endian 64-bit values. */
        for (;;) {
            if (offset > total || total - offset < 16)
                startup_panic();
            const unsigned char *entry = (const unsigned char *)dtb + offset;
            uint64_t address = ((uint64_t)fdt_u32_le2be(entry) << 32) |
                               fdt_u32_le2be(entry + 4);
            uint64_t length = ((uint64_t)fdt_u32_le2be(entry + 8) << 32) |
                              fdt_u32_le2be(entry + 12);
            if (!address && !length)
                break;
            if (length && startup_reserve(address, length) < 0)
                startup_panic();
            offset += 16;
        }
    }

    if (page_frame_count > SIZE_MAX / sizeof(*bookkeep))
        startup_panic();
    bookkeep = startup_alloc(page_frame_count * sizeof(*bookkeep), PAGE_SIZE);
    if (!bookkeep)
        startup_panic();
    page_init();
    for (unsigned int i = 0; i < startup_block_count; i++)
        memory_reserve(startup_blocks[i].start,
                       startup_blocks[i].start + startup_blocks[i].size);
    free_area_init();
    startup_finish();
    __init_kmalloc();
    initialized = 1;
    printf("[mm] Startup allocation complete; buddy allocator ready\n");
}

static void memory_reserve_locked(uintptr_t start, uintptr_t end)
{
    const uintptr_t pool_start = (uintptr_t)LOW_MEMORY;
    const uintptr_t pool_end = pool_start +
                               (uintptr_t)PAGE_FRMAME_NUM * PAGE_SIZE;
    const uintptr_t page_mask = (uintptr_t)PAGE_SIZE - 1;
    unsigned long first_pfn;
    unsigned long end_pfn;

    /* The interval is empty or does not intersect the managed memory. */
    if (start >= end || end <= pool_start || start >= pool_end)
        return;

    if (start < pool_start)
        start = pool_start;
    if (end > pool_end)
        end = pool_end;

    start &= ~page_mask;
    end = (end + page_mask) & ~page_mask;
    if (end > pool_end)
        end = pool_end;

    first_pfn = (unsigned long)((start - pool_start) >> PAGE_SHIFT);
    end_pfn = (unsigned long)((end - pool_start) >> PAGE_SHIFT);

    if (free_area_ready) {
        /*
         * A reservation may lie in the middle of a large free block.  Remove
         * every overlapping block and return only its non-reserved left and
         * right pieces to the appropriate order lists.
         */
        for (int order = 0; order <= MAX_ORDER; order++) {
            struct list_head *head = &free_area[order].freelist;
            struct list_head *pos = head->next;

            while (pos != head) {
                struct list_head *next = pos->next;
                page_t *block = (page_t *)pos;
                unsigned long block_start = (unsigned long)block->pfn;
                unsigned long block_end = block_start + (1UL << order);

                if (block_start < end_pfn && block_end > first_pfn) {
                    unsigned long left_end = first_pfn < block_end ?
                                             first_pfn : block_end;
                    unsigned long right_start = end_pfn > block_start ?
                                                end_pfn : block_start;

                    pop_block_from_free_area(block, &free_area[order]);

                    /* Clear stale child-block metadata before rebuilding. */
                    for (unsigned long pfn = block_start;
                         pfn < block_end; pfn++) {
                        bookkeep[pfn].order = -1;
                        bookkeep[pfn].used = Taken;
                        INIT_LIST_HEAD(&bookkeep[pfn].list);
                    }

                    if (block_start < left_end)
                        add_free_pfn_range(block_start, left_end);
                    if (right_start < block_end)
                        add_free_pfn_range(right_start, block_end);
                }

                pos = next;
            }
        }
    }

    /* Reserved pages must never satisfy a future buddy merge. */
    for (unsigned long pfn = first_pfn; pfn < end_pfn; pfn++) {
        bookkeep[pfn].order = -1;
        bookkeep[pfn].used = Taken;
        INIT_LIST_HEAD(&bookkeep[pfn].list);
    }
}

/* IRQ masking is nestable: no thread can observe a partial allocator update. */

struct page * buddy_block_alloc(int order)
{
    uint64_t flags = irq_save();
    struct page * result = buddy_block_alloc_locked(order);
    irq_restore(flags);
    return result;
}

void buddy_block_free(struct page *block)
{
    uint64_t flags = irq_save();
    buddy_block_free_locked(block);
    irq_restore(flags);
}

int register_obj_allocator(int objsize)
{
    uint64_t flags = irq_save();
    int result = register_obj_allocator_locked(objsize);
    irq_restore(flags);
    return result;
}

void * obj_allocate(int token)
{
    uint64_t flags = irq_save();
    void * result = obj_allocate_locked(token);
    irq_restore(flags);
    return result;
}

void obj_free(void *obj_addr)
{
    uint64_t flags = irq_save();
    obj_free_locked(obj_addr);
    irq_restore(flags);
}

void * kmalloc(int size)
{
    uint64_t flags = irq_save();
    void * result = kmalloc_locked(size);
    irq_restore(flags);
    return result;
}

void kfree(void *addr)
{
    uint64_t flags = irq_save();
    kfree_locked(addr);
    irq_restore(flags);
}

void memory_reserve(uintptr_t start, uintptr_t end)
{
    uint64_t flags = irq_save();
    memory_reserve_locked(start, end);
    irq_restore(flags);
}
