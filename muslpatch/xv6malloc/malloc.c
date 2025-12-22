#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <errno.h>
#include <sys/mman.h>
#include "libc.h"
#include "atomic.h"
#include "pthread_impl.h"
#include "fork_impl.h"
#include "glue.h"

// Memory allocator by Kernighan and Ritchie,
// The C programming Language, 2nd ed.  Section 8.7.

typedef long Align;

union header {
    struct {
        union header *ptr;
        unsigned int size;
    } s;
    Align x;
};

typedef union header Header;

static Header base;
static Header *freep;

void
free(void *ap)
{
    Header *bp, *p;

    bp = (Header*)ap - 1;
    for(p = freep; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
        if(p >= p->s.ptr && (bp > p || bp < p->s.ptr))
            break;
    if(bp + bp->s.size == p->s.ptr){
        bp->s.size += p->s.ptr->s.size;
        bp->s.ptr = p->s.ptr->s.ptr;
    } else
        bp->s.ptr = p->s.ptr;
    if(p + p->s.size == bp){
        p->s.size += bp->s.size;
        p->s.ptr = bp->s.ptr;
    } else
        p->s.ptr = bp;
    freep = p;
}

static Header*
morecore(unsigned int nu)
{
    char *p;
    Header *hp;

    if(nu < 4096)
        nu = 4096;
    p = (char*)__syscall(59, nu * sizeof(Header)); // sbrk
    if(p == (char*)-1)
        return 0;
    hp = (Header*)p;
    hp->s.size = nu;
    free((void*)(hp + 1));
    return freep;
}

void*
malloc(unsigned int nbytes)
{
    Header *p, *prevp;
    unsigned int nunits;

    nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;
    if((prevp = freep) == 0){
        base.s.ptr = freep = prevp = &base;
        base.s.size = 0;
    }
    for(p = prevp->s.ptr; ; prevp = p, p = p->s.ptr){
        if(p->s.size >= nunits){
            if(p->s.size == nunits)
                prevp->s.ptr = p->s.ptr;
            else {
                p->s.size -= nunits;
                p += p->s.size;
                p->s.size = nunits;
            }
            freep = prevp;
            return (void*)(p + 1);
        }
        if(p == freep)
            if((p = morecore(nunits)) == 0)
                return 0;
    }
}

void*
realloc(void *ap, unsigned int nbytes)
{
	Header *bp;
	unsigned int nunits, oldsize;
	void *newp;

	// If ptr is NULL, behave like malloc
	if(ap == 0)
		return malloc(nbytes);

	// If size is 0, behave like free
	if(nbytes == 0){
		free(ap);
		return 0;
	}

	nunits = (nbytes + sizeof(Header) - 1)/sizeof(Header) + 1;

	bp = (Header*)ap - 1;
	oldsize = bp->s.size;

	// If the new size fits in the current block, just return it
	if(oldsize >= nunits)
		return ap;

	newp = malloc(nbytes);
	if(newp == 0)
		return 0;

	// Copy old data to new block
	char *src = (char*)ap;
	char *dst = (char*)newp;
	unsigned int copysize = (oldsize - 1) * sizeof(Header);
	for(unsigned int i = 0; i < copysize; i++)
		dst[i] = src[i];

	free(ap);
	return newp;
}
