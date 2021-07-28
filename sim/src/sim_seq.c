#include <sim_seq.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <msg.h>

void
sim_seq_destroy(struct SimItem *seq)
{
  while(seq) {
    struct SimItem *next = seq->next;
    free(seq);
    seq = next;
  }
}

void
sim_seq_dump(struct SimItem *seq)
{
  while(seq) {
    printf("%ld: ", seq->ts);
    switch(seq->type) {
    case ExternalHigh:
      printf("External high");
      break;
    case ExternalLow:
      printf("External low");
      break;
    case OutHigh:
      printf("Out high");
      break;
    case OutLow:
      printf("Out low");
      break;
    case BusHigh:
      printf("Bus high");
      break;
    case BusLow:
      printf("Bus low");
      break;
    case Send:
      {
	struct SimItemMsg *msg_item = (struct SimItemMsg*)seq;
	printf("Send: ");
	print_dali_msg(stdout, &msg_item->msg);
      }
      break;
    case ResultSend:
      printf("Result Send");
      break;
    case ResultRecv:
      printf("Result Receive");
      break;
    }
    printf("\n");
    seq = seq->next;
  }
}

void
sim_seq_sort(struct SimItem **head)
{
  int swapped;
  if (!*head) return;
  do {
    swapped = 0;
    struct SimItem **ap = head;
    struct SimItem *a = *ap;
    struct SimItem *b;
    while((b = a->next)) {
      if (a->ts > b->ts) {
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

struct SimItem *
sim_seq_add_simple(struct SimItem ***tail, enum SimItemType type, int64_t ts)
{
  struct SimItem *item = malloc(sizeof(struct SimItem));
  assert(item);
  item->type = type;
  item->ts = ts;
  item->next = NULL;
  **tail = item;
  *tail = &item->next;
  return item;
}

struct SimItem *
sim_seq_add_msg(struct SimItem ***tail, enum SimItemType type, int64_t ts,
		const struct dali_msg *msg)
{
  struct SimItemMsg *item = malloc(sizeof(struct SimItemMsg));
  assert(item);
  item->item.type = type;
  item->item.ts = ts;
  item->msg =*msg;
  item->item.next = NULL;
  **tail = &item->item;
  *tail = &item->item.next;
  return &item->item;
}
