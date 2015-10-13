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
	struct ObjectsForMark *next,*prev;
	void **pointersFromObjects;
	ggc_size_t numPointers;
};

static struct ObjectsForMark listForMarking;

#define MarkListBufferSize 1024
#define Mark(obj) do { \
    struct GGGGC_Header *hobj = (obj); \
    hobj->descriptor__ptr = (struct GGGGC_Descriptor *) \
        ((ggc_size_t) hobj->descriptor__ptr | 2); \
} while (0)

#define NewObj() do { \
    if (!markList->next) { \
        struct ObjectsForMark *ob2 = (struct ObjectsForMark *) malloc(sizeof(struct ObjectsForMark)); \
        markList->next = ob2; \
        ob2->prev = markList; \
        ob2->next = NULL; \
        ob2->pointersFromObjects = (void **) malloc(MarkListBufferSize * sizeof(void *)); \
        if (ob2->pointersFromObjects == NULL) { \
            perror("malloc"); \
            abort(); \
        } \
    } \
    markList = markList->next; \
    markList->numPointers = 0; \
} while(0)

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
    markList->next=NULL; \
    markList->prev=NULL; \
}while (0)
	
//here we pass the pointers to the roots
#define AddToMarkList(ptr) do {\
\
	if (markList->numPointers >= MarkListBufferSize) NewObj(); 	\
	markList->pointersFromObjects[markList->numPointers++]=ptr;\
}while (0)

#define MarkListPop(ptr) do{\
	ptr=(void **)markList->pointersFromObjects[--markList->numPointers];\
	if(markList->numPointers==0 && markList->prev)	\
	{\
		markList=markList->prev;	\
	}\
}while(0)
		
#define IsMarked(obj) ((ggc_size_t) obj->descriptor__ptr & (ggc_size_t) 2)


#define AddObjectPointers(obj,descriptor) do{\
	ggc_size_t size=descriptor->size;\
	if(!(descriptor->pointers[0] & 1))\
	{\
		break;\
	}\
	ggc_size_t pointerMap=descriptor->pointers[0];\
	printf("pointermap is %zu\n",pointerMap);\
	void **objCopy=(void **)obj;\
	ggc_size_t i;\
	for( i=0;i<size;i++)\
	{\
		printf("1 pointermap in for is %zu\n",pointerMap);\
		if(pointerMap & 1)\
		{\
			printf("adding %u\n",&objCopy[i]);\
			AddToMarkList(&objCopy[i]);\
		}\
		pointerMap/=2;\
	}\
}while(0)
				
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
			printf("adding %zu root %zu\n",i,(pointerCur->pointers[i]));
			AddToMarkList(pointerCur->pointers[i]); //increment the number of survivros thMarkList(pointerCur->pointers[i]);
		}
	}
	//Now the Mark Phase begins
	struct GGGGC_Header *obj;
	while((markList->numPointers)>0)
	{
		void **ptr;
		MarkListPop(ptr);
		obj=(struct GGGGC_Header *) *ptr; //This has been done so that ob1 now points to the object itself
		
		if(obj==NULL ){
		 continue;
		 }
		//UnMark(obj); //	Incase it was left marked by mistake
		printf("collect obj is %u and numpointers is %zu\n",obj,markList->numPointers);
		if(!IsMarked(obj))
		{
			//That is the object is not marked, then fist copy the reference to the descriptor so as to save the further pointers
			printf("descriptor is %zu and its size is %zu\n",(ggc_size_t)obj->descriptor__ptr,GGGGC_POOL_OF(obj));
			ggc_size_t poolNos=(ggc_size_t)GGGGC_POOL_OF(obj);
			if((ggc_size_t)(GGGGC_POOL_OF(obj) + GGGGC_WORDS_PER_POOL ) < (ggc_size_t)obj->descriptor__ptr) 
			{	
				printf("continue\n");
				continue;
			}
			struct GGGGC_Descriptor *descriptor= obj->descriptor__ptr;
			printf("descriptor is %u\n",descriptor);
			struct GGGGC_Pool *POOL=GGGGC_POOL_OF(obj);
			POOL->survivors+=  descriptor->size; //increment the number of survivros this collection 
			
			Mark(obj);
			//add the pointers to the list for marking;
			printf("before adding the pointers obj is %u and descriptor is %u \n",obj,descriptor);
			 AddObjectPointers(obj,descriptor);
		}
	}
	// now call the sweep code
	struct GGGGC_Pool *pool=ggggc_poolList;
	struct FreeObjects *fobj1,*LastPointer;
	freeList=NULL;
	ggc_size_t *sweep; // we need this pois pointer to traverse the heap pools
	while(pool)
	{
		sweep=(ggc_size_t *)(pool+ pool->start[0]);
		while(sweep!=pool->end  && sweep!=pool->free && sweep!=NULL)
		{
			obj=(struct GGGGC_Header *)sweep;
			if(IsMarked(obj))
			{
				UnMark(obj);
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
			sweep+=obj->descriptor__ptr->size ;
			
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
  	ggggc_collect();return 0;
	ggc_size_t freeSpace,totalSpace;
	printf("in yield\n");
	struct GGGGC_Pool  *pool=ggggc_curPool;
    if (pool==NULL) 
    {
    	printf("collect\n");
    	ggggc_collect();
    	 return 0;
    }
	
    /* first figure out how much space was used in the current pool*/
    freeSpace = 0;
    freeSpace += (ggc_size_t) pool->end - (ggc_size_t) pool->start;
	totalSpace=(ggc_size_t) pool->end-(ggc_size_t) pool->start[0];
	totalSpace=GGGGC_WORDS_PER_POOL;
    /* now decide if it's too much */
    if (freeSpace < ((3*totalSpace)/4))
    {
    	ggggc_collect();
    }
	
    return 0;
}



#ifdef __cplusplus
}
#endif
