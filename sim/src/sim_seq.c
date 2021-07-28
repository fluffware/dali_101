#include <sim_seq.h>
#include <stdlib.h>
#include <stdio.h>
#include <msg.h>
#include <list.h>

void
sim_seq_destroy(struct SimItem *seq)
{
  list_destroy((struct List*)seq);
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

static int
cmp(struct List *a, struct List *b)
{
  int64_t a_ts = ((struct SimItem*)a)->ts;
  int64_t b_ts = ((struct SimItem*)b)->ts;
  if (a_ts > b_ts) return 1;
  else if (a_ts < b_ts) return -1;
  else return 0;
}

void
sim_seq_sort(struct SimItem **head)
{
  list_sort((struct List**)head, cmp);
}

struct SimItem *
sim_seq_add_simple(struct SimItem ***tail, enum SimItemType type, int64_t ts)
{
  struct SimItem *item = list_add((struct List ***)tail,sizeof(struct SimItem));
  item->type = type;
  item->ts = ts;
  return item;
}

struct SimItem *
sim_seq_add_msg(struct SimItem ***tail, enum SimItemType type, int64_t ts,
		const struct dali_msg *msg)
{
  struct SimItemMsg *item = 
    list_add((struct List ***)tail,sizeof(struct SimItemMsg));
  item->item.type = type;
  item->item.ts = ts;
  item->msg =*msg;
  return &item->item;
}
