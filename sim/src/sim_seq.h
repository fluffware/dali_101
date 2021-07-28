#ifndef __SIM_SEQ_H__NFO6YAOPL0__
#define __SIM_SEQ_H__NFO6YAOPL0__
#include <stdint.h>
#include <dali_msg.h>

enum SimItemType
  {
    ExternalHigh,
    ExternalLow,
    OutHigh,
    OutLow,
    BusHigh,
    BusLow,
    Send,
    ResultSend,
    ResultRecv,
  };
  
struct SimItem
{
  struct SimItem *next;
  enum SimItemType type;
  int64_t ts;
};

struct SimItemMsg
{
  struct SimItem item;
  struct dali_msg msg;
};


void
sim_seq_destroy(struct SimItem *seq);

void
sim_seq_dump(struct SimItem *seq);

void
sim_seq_sort(struct SimItem **head);

struct SimItem *
sim_seq_add_simple(struct SimItem ***tail, enum SimItemType type, int64_t ts);

struct SimItem *
sim_seq_add_msg(struct SimItem ***tail, enum SimItemType type, int64_t ts,
		const struct dali_msg *msg);
  
#endif /* __SIM_SEQ_H__NFO6YAOPL0__ */
