#include "my_vm.h"

/**  Overall Library Structure - as interpreted from project description and subsequent explanations
 * 
 * Page Directory - holds virtual addresses of pages (10 bits: size is 2^10 = 1024)
 * |-> Page Table - holds addresses to physical tables (10 bits: size is 2^10 = 1024)
 *     |-> Pages - can be assigned physical values 
 *         |-> up to 4096 bytes available per page for physical memory allocated in physical memory array
 *              address stored here in page dir/table structure
 *              allocations under 4096 bytes are rounded up to a single page worth in this implementation
 */


// global definitions used for this project
    // probably would be in better form to have these as #DEFINEs
    // and subsequent globals probably in the header file as well
int num_physical_pages = MEMSIZE/PGSIZE;
int num_virtual_pages = MAX_MEMSIZE/PGSIZE;
static char * physical_pages_states;
static char * virtual_pages_states;
static char * physical_memory;

static pde_t * page_dir[1024][1024];
//static pte_t * page_table;

static int next_page = -1;

int bitExtracted(int number, int k, int p) 
{ 
    return (((1 << k) - 1) & (number >> (p - 1))); 
} 

void bin(unsigned n) 
{ 
    if (n > 1) 
        bin(n/2); 
  
    printf("%d", n % 2); 
} 

/*
Function responsible for allocating and setting your physical memory 
*/
void SetPhysicalMem() {

    //Allocate physical memory using mmap or malloc; this is the total size of
    //your memory you are simulating
    physical_memory = (char*)malloc(MEMSIZE * sizeof(char));

    //HINT: Also calculate the number of physical and virtual pages and allocate
    //virtual and physical bitmaps and initialize them
    physical_pages_states = (char*)malloc(num_physical_pages * sizeof(char));
    virtual_pages_states = (char*)malloc(num_virtual_pages * sizeof(char));

    // initialize all values in page state arrays to 0
    int i = 0;
    for (i = 0; i < num_physical_pages; i++) {
        physical_pages_states[i] = 0;
    }
    for (i = 0 ; i < num_virtual_pages; i++) {
        virtual_pages_states[i] = 0;
    }

    // Page directory holds page table entries
        // page table entries hold addresses of physical_memory allocations
    //page_dir = (pde_t*)malloc(1024 * sizeof(pde_t));
    //page_table = (pde_t*)malloc(1024*1024 * sizeof(pte_t));
    

    //DEBUGGING
    //printf("Avaliable Physical Pages: %d\n", num_physical_pages);
    //printf("Available Virtual Pages: %d\n", num_virtual_pages);
}



/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t * Translate(pde_t *pgdir, void *va) {
    //HINT: Get the Page directory index (1st level) Then get the
    //2nd-level-page table index using the virtual address.  Using the page
    //directory index and page table index get the physical address

    pte_t* retVal = NULL;

    uintptr_t addr = (uintptr_t)va;
    int y = bitExtracted(addr, 10, 13);
    int x = bitExtracted(addr, 10, 23);
    retVal = page_dir[x][y];

    return retVal;

    //If translation not successfull
    return NULL; 
}


/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int PageMap(pde_t *pgdir, void *va, void *pa){
    /*HINT: Similar to Translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */
    
    uintptr_t x = (uintptr_t)pgdir;
    uintptr_t y = (uintptr_t)va;
    if (page_dir[x][y] == NULL || page_dir[x][y] == 0) {
        page_dir[x][y] = pa;
    }
    else {
        printf("Mapping exists.\n");
        return 0;
    }

    return -1;
}


/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {
 
    //Use virtual address bitmap to find the next free page

    int pageFree = 0;
    next_page = -1;

    int i = 0;
    for (i = 0; i < num_virtual_pages; i++) {
        if (virtual_pages_states[i] == 0) {
            pageFree = 1;
            next_page = i;
            return NULL;
        }
    }

    if (pageFree == 0) {
        printf("Error: No available pages.\n");
        // find out what appropriate behavior is here
        exit(1);
    }
}


/* Function responsible for allocating pages
and used by the benchmark
*/
void *m_alloc(unsigned int num_bytes) {

    //HINT: If the physical memory is not yet initialized, then allocate and initialize.
    if (physical_memory == NULL) {
        SetPhysicalMem();
        //printf("MEMORY SET\n");
    }

   /* HINT: If the page directory is not initialized, then initialize the
   page directory. Next, using get_next_avail(), check if there are free pages. If
   free pages are available, set the bitmaps and map a new page. Note, you will 
   have to mark which physical pages are used. */
    
    // find the next available page to allocate
    get_next_avail(num_virtual_pages);
    // find number of pages needed to allocate this new memory block
    int num_pages_needed = num_bytes / PGSIZE + 1;
    //printf("num pages needed: %d\n", num_pages_needed);
    if (next_page == -1) {
        printf("Error finding next page.\n");
        exit(1);
    }
    // set page(s) to be in use
    int i,j = 0;
    //printf("next page val: %d\n", next_page);
    for (i = next_page,j=0; j < num_pages_needed; i++, j++) {
        virtual_pages_states[i] = 1;
    }
    /*
    for(i = 0; i < 5; i++) {
        printf("%d\n", virtual_pages_states[i]);
    }
    */

    // set the addresses in the page directory and table (could/should be done through PageMap and Translate)
    int addr = (unsigned int)physical_memory + 4096*next_page;
    //uintptr_t addr = (uintptr_t)phys_addr;
    //printf("addr desired: %x\n", addr);
    //printf("address in binary: ");
    //bin(addr);
    //printf("\n");
    int y = bitExtracted(addr, 10, 13);
    int x = bitExtracted(addr, 10, 23);
    //printf("mapping to: %d, %d\n", x, y);
    if (page_dir[x][y] == NULL || page_dir[x][y] == 0) {
        page_dir[x][y] = (unsigned long int *)addr;
    }
    else {
        printf("ERROR: Mapping exists. (potentially a prior mapping)\n");
    }

    return (void*)addr;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void a_free(void *va, int size) {

    //Free the page table entries starting from this virtual address (va)
    // Also mark the pages free in the bitmap
    //Only free if the memory from "va" to va+size is valid

    pte_t * phys_addr = Translate(NULL, va);
    //printf("address to be freed: %x\n", phys_addr);
    int page_to_free = ((int)phys_addr - (unsigned int)physical_memory) / 4096;
    int num_pages_to_free = size / PGSIZE + 1;
    
    //printf("page to free: %d\n", page_to_free);
    int i, j = 0;
    for(i = page_to_free, j = 0; j < num_pages_to_free; i++,j++) {
        virtual_pages_states[page_to_free] = 0;
    }

    int addr = (unsigned long int)phys_addr;
    int y = bitExtracted(addr, 10, 13);
    int x = bitExtracted(addr, 10, 23);
    page_dir[x][y] = 0;

}


/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
*/
void PutVal(void *va, void *val, int size) {

    /* HINT: Using the virtual address and Translate(), find the physical page. Copy
       the contents of "val" to a physical page. NOTE: The "size" value can be larger
       than one page. Therefore, you may have to find multiple pages using Translate()
       function.*/

    pte_t * phys_addr = Translate(NULL, va);
    if (size < 4096) {
        memcpy(phys_addr, val, size);
    }

}


/*Given a virtual address, this function copies the contents of the page to val*/
void GetVal(void *va, void *val, int size) {

    /* HINT: put the values pointed to by "va" inside the physical memory at given
    "val" address. Assume you can access "val" directly by derefencing them.
    If you are implementing TLB,  always check first the presence of translation
    in TLB before proceeding forward */

    pte_t * phys_addr = Translate(NULL, va);
    if (size < 4096) {
        memcpy(val, phys_addr, size);
    }
}



/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void MatMult(void *mat1, void *mat2, int size, void *answer) {

    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
    matrix accessed. Similar to the code in test.c, you will use GetVal() to
    load each element and perform multiplication. Take a look at test.c! In addition to 
    getting the values from two matrices, you will perform multiplication and 
    store the result to the "answer array"*/

    int i,j,k = 0;
    int temp, num1, num2, x, y, z = 0;

    int * m1 = (int*)malloc(size*size*sizeof(int));
    int * m2 = (int*)malloc(size*size*sizeof(int));
    int * ans = (int*)malloc(size*size*sizeof(int));
    
    // set values in temp int matricies we create for multiplication
    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            num1 = (unsigned int)mat1 + ((i * size * sizeof(int))) + (j * sizeof(int));
            num2 = (unsigned int)mat2 + ((i * size * sizeof(int))) + (j * sizeof(int));
            GetVal((void *)num1, &x, sizeof(int));
            GetVal( (void *)num2, &y, sizeof(int));
            m1[size*i+j] = x;
            m2[size*i+j] = y;
        }
    }

    //perform matrix multiplication on the values collected
    num1 = 0;
    num2 = 0;
    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            for (k = 0; k < size; k++) {
                num1 = m1[i*size+k];
                num2 = m2[k*size+j];
                ans[i*size+j] += num1*num2;
            }
        }
    }

    //store mat mult values back into the answer matrix address space
    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            num1 = (unsigned int)answer + ((i * size * sizeof(int))) + (j * sizeof(int));
            PutVal((void*)num1, (void*)&ans[i*size+j], sizeof(int));
        }
    }

    return;
}
