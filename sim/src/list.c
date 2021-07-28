#include "list.h"
#include <assert.h>

void *
list_add(struct List ***tail, size_t size)
{
  struct List *list = malloc(size);
  assert(list);
  list->next = NULL;
  **tail = list;
  *tail = &list->next;
  return list;
}

void
list_destroy(struct List *seq)
{
  while(seq) {
    struct List *next = seq->next;
    free(seq);
    seq = next;
  }
}

void
list_sort(struct List **head, ListCmp cmp)
{
  int swapped;
  if (!*head) return;
  do {
    swapped = 0;
    struct List **ap = head;
    struct List *a = *ap;
    struct List *b;
    while((b = a->next)) {
      if (cmp(a, b) > 0) {
	// Swap a and b
	a->next = b->next;
	b->next = a;
	*ap = b;
	ap = &b->next;
	swapped = 1;
      } else {
	ap = &a->next;
      }
      a = *ap;
    }
  } while(swapped);
}
