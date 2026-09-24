#include "pmm.h"
#include "vendor/limine/limine.h"
#include <stdint.h>
#include <stdbool.h>
#define PAGE_SIZE 4096ULL
#define MAX_PHYS  (64ULL*1024ULL*1024ULL*1024ULL)
#define MAX_PAGES (MAX_PHYS/PAGE_SIZE)
#define BITMAP_BYTES (MAX_PAGES/8ULL)
static uint8_t Bitmap[BITMAP_BYTES];
static uint64_t Total, Free;
static inline bool bit(uint64_t p){return (Bitmap[p>>3]>>(p&7))&1U;}
static inline void set(uint64_t p){Bitmap[p>>3]|=(uint8_t)(1U<<(p&7));}
static inline void clr(uint64_t p){Bitmap[p>>3]&=(uint8_t)~(1U<<(p&7));}
void PmmInitialize(void *Raw){
    struct limine_memmap_response *r=(struct limine_memmap_response*)Raw;
    for(uint64_t i=0;i<BITMAP_BYTES;i++) Bitmap[i]=0xFF;
    Total=Free=0;
    if(!r) return;
    for(uint64_t i=0;i<r->entry_count;i++){
        struct limine_memmap_entry *e=r->entries[i];
        if(e->type!=LIMINE_MEMMAP_USABLE) continue;
        uint64_t a=(e->base+PAGE_SIZE-1)&~(PAGE_SIZE-1);
        uint64_t end=(e->base+e->length)&~(PAGE_SIZE-1);
        if(end<=a || a>=MAX_PHYS) continue;
        if(end>MAX_PHYS) end=MAX_PHYS;
        for(uint64_t p=a;p<end;p+=PAGE_SIZE){uint64_t n=p/PAGE_SIZE; if(bit(n)){clr(n);Total++;Free++;}}
    }
    PmmReserveRange(0,0x100000);
}
void PmmReserveRange(uint64_t Base,uint64_t Length){
    uint64_t a=Base&~(PAGE_SIZE-1), end=Base+Length;
    if(end<a) return;
    end=(end+PAGE_SIZE-1)&~(PAGE_SIZE-1);
    for(uint64_t p=a;p<end && p<MAX_PHYS;p+=PAGE_SIZE){uint64_t n=p/PAGE_SIZE;if(!bit(n)){set(n);if(Free)Free--;}}
}
uint64_t PmmAllocPage(void){
    for(uint64_t n=256;n<MAX_PAGES;n++) if(!bit(n)){set(n);if(Free)Free--;uint64_t p=n*PAGE_SIZE;for(uint64_t i=0;i<512;i++)((volatile uint64_t*)(uintptr_t)p)[i]=0;return p;}
    return 0;
}
bool PmmFreePage(uint64_t Physical){
    if((Physical&(PAGE_SIZE-1))!=0 || Physical>=MAX_PHYS) return false;
    uint64_t n=Physical/PAGE_SIZE;if(bit(n)) return false;set(n);Free++;return true;
}
uint64_t PmmTotalPages(void){return Total;}
uint64_t PmmFreePages(void){return Free;}
