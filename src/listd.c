#include <stdlib.h> 
#include "listd.h"

// Remove element (return 0 if success, -1 if error)
int removeEl(struct listElements_t *list, struct element_t *el)
{
    int result = 0;

    if ((!list) || (!el))
    {
        return -1;
    }

    if (el->prev && el->next) 
    {
        el->prev->next = el->next;
        el->next->prev = el->prev;
    }
    else
    { 
        result = -1;

        if (el == list->first)
        {
            list->first = el->next;

            if (list->first)
            {
                list->first->prev = NULL;
            }

            result = 0;  
        }

        if (el == list->last)
        {
            list->last = el->prev;
            if (list->last) 
            {
                list->last->next = NULL; 
            }

            result = 0;  
        }
    }

    free(el);

    list->count--;

    return result;
}

// Add element after prevEl
struct element_t* addEl(struct listElements_t *list, struct element_t *prevEl, void *data)
{
    struct element_t *newEl = (struct element_t*)malloc(sizeof(struct element_t));
    
    if (!newEl)
    {
        return NULL;
    }

    newEl->data = data;
    newEl->next = NULL;
    newEl->prev = NULL;
    list->count++;
    
    if (prevEl)
    {
        newEl->next = prevEl->next; 
        newEl->prev = prevEl;
        if (prevEl->next)
        {
            prevEl->next->prev = newEl;
        }
        prevEl->next = newEl;
    }

    if ((!prevEl) && (list->first))
    {
        list->first->prev = newEl;
        newEl->next = list->first;
        list->first = newEl;
    }

    if (!list->first)
    {
        list->first = newEl;
        list->last = newEl;
    }

    if ((list->last) && (list->last == prevEl))
    {
        list->last = newEl;        
    }
    
    return newEl;
}

// Pull first (remove first + return it)
void* pullFirst(struct listElements_t *list)
{
    if ((!list) || (!list->first)) { return NULL; }

    void *el = list->first->data;

    removeEl(list, list->first);
    return el;
}

// Pull last (remove last + return it)
void* pullLast(struct listElements_t *list)
{
    if ((!list) || (!list->last)) { return NULL; }

    void *el = list->last->data;
    removeEl(list, list->last);
    return el;
}

// Push first (add first + return struct)
struct element_t* pushFirst(struct listElements_t *list, void *data)
{
    return addEl(list, list->first, data);
}

// Push last (add last + return struct)
struct element_t* pushLast(struct listElements_t *list, void *data)
{
    return addEl(list, list->last, data);
}