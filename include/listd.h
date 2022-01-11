#ifndef LISTD_H
#define LISTD_H

struct element_t
{
    void*              data;
    struct element_t*  next;
    struct element_t*  prev;
};

struct listElements_t
{
    struct element_t*   first;
    struct element_t*   last;
    unsigned int        count;
};


//return 0 if success, -1 if error
int                 removeEl(struct listElements_t *list, struct element_t *el);
struct element_t*   addEl(struct listElements_t *list, struct element_t *prevEl, void *data);
void*               pullFirst(struct listElements_t *list);
void*               pullLast(struct listElements_t *list);
struct element_t*   pushFirst(struct listElements_t *list, void *data);
struct element_t*   pushLast(struct listElements_t *list, void *data);

#endif //LISTD_H