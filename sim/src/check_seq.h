#ifndef __CHECK_SEQ_H__Y7DQXQZA1P__
#define __CHECK_SEQ_H__Y7DQXQZA1P__

#include <sim_seq.h>

struct CheckItem
{
  struct CheckItem *next;
  enum SimItemType type;
  int64_t min_ts;
  int64_t max_ts;
};

#define CHECK_MASK_SEQ 0x01
#define CHECK_MASK_RESULT 0x02
#define CHECK_MASK_FRAME 0x04

struct CheckItemMsg
{
  struct CheckItem item;
  struct dali_msg msg;
  uint16_t flags_mask;
  uint16_t check_mask;
};
void
check_seq_destroy(struct CheckItem *seq);

void
check_seq_dump(struct CheckItem *seq);

void
check_seq_sort(struct CheckItem **head);

struct CheckItem *
check_seq_add_simple(struct CheckItem ***tail, enum SimItemType type, int64_t min_ts, int64_t max_ts);

struct CheckItem *
check_seq_add_msg(struct SimItem ***tail, enum SimItemType type, 
		  int64_t min_ts, int64_t max_ts,
		  const struct dali_msg *msg,
		  uint16_t flags_mask, uint16_t check_mask);

#endif /* __CHECK_SEQ_H__Y7DQXQZA1P__ */
