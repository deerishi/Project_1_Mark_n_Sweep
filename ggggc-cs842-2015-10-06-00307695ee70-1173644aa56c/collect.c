/*
 * The collector
 *
 * Copyright (c) 2014, 2015 Gregor Richards
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "ggggc/gc.h"
#include "ggggc-internals.h"

#ifdef __cplusplus
extern "C" {
#endif


/* run a collection */


//void ggggc_collect()
//{
    /* FILLME */
//}

struct ObjectsForMark
{
	struct ObjectsForMark *next;
	void **pointersFromObjects;
	ggc_size_t numPointers;
}

static struct ObjectsForMark listForMarking;

#define MarkListBufferSize 1024
#define Mark(obj) do { \
    struct GGGGC_Header *hobj = (obj); \
    hobj->descriptor__ptr = (struct GGGGC_Descriptor *) \
        ((ggc_size_t) hobj->descriptor__ptr | 2); \
} while (0)

#define UnMark(obj) do { \
    struct GGGGC_Header *hobj = (obj); \
    hobj->descriptor__ptr = (struct GGGGC_Descriptor *) \
        ((ggc_size_t) hobj->descriptor__ptr & (ggc_size_t)~2); \
} while (0)

#define MarkListInit() do {\
\
	if(listForMarking.pointersFromObjects==NULL)\
	{\
		listForMarking.pointersFromObjects=(void **)malloc(MarkListBufferSize * sizeof(void *));\
		if(listForMarking.pointersFromObjects==NULL){\
			 perror("malloc"); \
            abort(); \
        } \
    } \
    markList = &listForMarking; \
    markList->numPointers = 0; \
}while (0)
	
//here we pass the pointers to the roots
#define AddToMarkList(ptr) do {\
\
	markList->pointersFromObjects[markList->numPointers++]=ptr;\
}while (0)

#define MarklistPop(type,ptr) do{\
	ptr=(type)markList->pointersFromObjects[--markList->numPointers];\
	}while(0)
		
#define IsMarked(obj) ((ggc_size_t) obj->descriptor__ptr & (ggc_size_t) 2)


#define AddObjectPointers(obj,descriptor) do{\
	ggc_size_t size=descriptor->size;\
	if(!(descriptor->pointer[0] & 1))\
	{\
		break;\
	}\
	ggc_size_t pointerMap=descriptor->pointer[0];\
	void **objCopy=(void **)obj;\
	for(ggc_size_t i=0;i<size;i++)\
	{\
		if(pointerMap & 1)\
		{\
			AddToMarkList(&objCopy[i]);\
		}\
		pointerMap>>1;\
	}\
}
				
void ggggc_collect()
{
	struct GGGGC_PointerStack *pointerCur;
	int i,j;
	struct ObjectsForMark *markList;
	MarkListInit();
	//Now all the roots are put on the stack
	for(pointerCur=ggggc_pointerStack;pointerCur!=NULL;pointerCur=pointerCur->next)
	{
		for(i=0;i<pointerCur->size;i++)
		{
			AddToMarkList(pointerCur->pointer[i]);
		}
	}
	//Now the Mark Phase begins
	struct GGGGC_Header *obj;
	while(markList->numPointers)
	{
		void **ptr;
		MarkListPop(void **,ptr);
		obj=(GGGGC_Header *)*ptr; //This has been done so that ob1 now points to the object itself
		
		if(obj==NULL) continue;
		//UnMark(obj); //	Incase it was left marked by mistake
		
		if(!IsMarked(obj))
		{
			//That is the object is not marked, then fist copy the reference to the descriptor so as to 			save the further pointers
			struct GGGC_Descriptor *correctDescriptorAddress=obj->descriptor_ptr;
			
			GGGGC_POOL_OF(obj)->survivors+=correctDescriptorAddress->size; //increment the number of survivros this collection 
			
			Mark(obj);
			//add the pointers to the list for marking;
			 AddObjectPointers(obj,correctDescriptorAddress);
		}
	}
	// now call the sweep code
	GGGGC_Pool *pool=ggggc_poolList,*sweep;
	struct GGGGC_Header *obj;
	struct FreeObjects *fobj1,*LastPointer;
	freeList=NULL;
	while(pool)
	{
		sweep=pool->start;
		while(sweep!=pool->end  && sweep!=pool->free && sweep!=NULL)
		{
			obj=(struct GGGGC_Header *)sweep;
			if(IsMarked(obj))
			{
				Unmark(obj);
			}
			else
			{
				fobj1=(struct FreeObjects *)sweep;
				fobj1->next=NULL;
				if(freeList==NULL)
				{
					freeList=fobj1;
					LastPointer=fobj1;
				}
				else
				{
					LastPointer->next=fobj1; //appending to the freeList
				}
			}
			sweep+=sweep+->descriptor__ptr->size ;
		}
		pool=pool->next;
	}
}
		
					
				
			
		
			














/* explicitly yield to the collector */
//int ggggc_yield()
//{
    /* FILLME */
//    ggggc_collect();
//    return 0;
//}

int ggggc_yield()
{
    /* FILLME */
  	/*We'll will check how much space is available in curent pool, if its filled more than 3/4 , we'll call the Garbage Collector and 
  	  write the code if the current pool is NULL, then do garbage collection
  	*/
	ggc_size_t freeSpace,totalSpace;
	
	struct GGGGC_Pool  *pool=ggggc_curPool;
    if (pool==NULL) 
    {
    	ggggc_collect();
    	 return 0;
    }

    /* first figure out how much space was used */
    freeSpace = 0;
    freeSpace += pool->end - pool->start;
	
	totalSpace=pool->end-pool->start[0];
	totalSpace=GGGGC_WORDS_PER_POOL;
	ggc_size_t space=0, survivors=0, numPools=0;
    /* now decide if it's too much */
    if (freeSpace < ((3*totalSpace)/4))
    {
    	ggggc_collect();
    	

		pool=ggggc_poolList;

		/* first figure out how much space was used */
		
		while (pool!=NULL) 
		{
		    space += pool->end - pool->start;
		    survivors += pool->survivors;
		    pool->survivors = 0;
		    numPools++;
		    if(pool->next==NULL) break;
		    pool = pool->next;
		}
		
    	for(ggc_size_t i=0;i<2*numPools;i++)
    	{
    		pool->next=newPool(1);
    		pool->next->next=NULL;
    		pool->next->survivors = 0;
    	}	
    		
   	}
	
    return 0;
}









#ifdef __cplusplus
}
#endif
