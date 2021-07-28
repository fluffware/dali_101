#include <stdlib.h>

struct List
{
  struct List *next;
};

/* >0 if a > b
   0 if a == b
   <0 if a < b
*/
typedef int (*ListCmp)(struct List *a, struct List *b);

void *
list_add(struct List ***tail, size_t size);

void
list_destroy(struct List *seq);

void
list_sort(struct List **head, ListCmp cmp);
