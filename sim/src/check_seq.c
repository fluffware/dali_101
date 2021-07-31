#include <check_seq.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <msg.h>
#include <list.h>

void
check_seq_destroy(struct CheckItem *seq)
{
  list_destroy((struct List*)seq);
}

static int
cmp(struct List *a, struct List *b)
{
  int64_t a_ts = ((struct CheckItem*)a)->min_ts;
  int64_t b_ts = ((struct CheckItem*)b)->min_ts;
  if (a_ts > b_ts) return 1;
  else if (a_ts < b_ts) return -1;
  else return 0;
}

void
check_seq_sort(struct CheckItem **head)
{
  list_sort((struct List**)head, cmp);
}

struct CheckItem *
check_seq_add_simple(struct CheckItem ***tail, enum SimItemType type, int64_t min_ts, int64_t max_ts)
{
  struct CheckItem *item = list_add((struct List ***)tail, 
				    sizeof(struct CheckItem));
  item->type = type;
  item->min_ts = min_ts;
  item->max_ts = max_ts;
  return item;
}

struct CheckItem *
check_seq_add_msg(struct SimItem ***tail, enum SimItemType type, 
		  int64_t min_ts, int64_t max_ts,
		  const struct dali_msg *msg,
		  uint16_t flags_mask, uint16_t check_mask)
{
  struct CheckItemMsg *item = list_add((struct List ***)tail, 
				    sizeof(struct CheckItemMsg));
  item->item.type = type;
  item->item.min_ts = min_ts;
  item->item.max_ts = max_ts;
  item->msg =*msg;
  item->flags_mask = flags_mask;
  item->check_mask = check_mask;
  return &item->item;
}
