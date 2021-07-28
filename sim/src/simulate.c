#include <simulate.h>
#include <stddef.h>
#include <dali_101.h>
#include <stdio.h>
#include <msg.h>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)


int
frames_equal(struct dali_msg *a, struct dali_msg *b)
{
  if (((a->flags ^ b->flags) & DALI_FLAGS_LENGTH) != 0) return 0;
  switch(a->flags & DALI_FLAGS_LENGTH) {
  case DALI_FLAGS_LENGTH_24:
    if (a->frame[2] != b->frame[2]) return 0;
  case DALI_FLAGS_LENGTH_16:
    if (a->frame[1] != b->frame[1]) return 0;
  case DALI_FLAGS_LENGTH_8:
    if (a->frame[0] != b->frame[0]) return 0;
  default:
    break;
  }
  return 1;
}
    
struct SimItem *
simulate(const struct SimItem *seq, int64_t max_ts)
{
  struct SimItem *out = NULL;
  struct SimItem **outp = &out;
  int out_level = DALI_HIGH;
  int in_level = DALI_HIGH;
  int bus_level = DALI_HIGH;
  int64_t ts = seq->ts;
  int64_t prev_ts = ts;
  int64_t bus_change = ts;
  struct DaliContext dali_ctxt;
  dali_init(&dali_ctxt);
  dali_ctxt.no_random = 1;
  struct dali_msg result;

  while(ts <= max_ts) {
    while (seq && ts >= seq->ts) {
      ts = seq->ts;
      switch(seq->type) {
      case ExternalLow:
	sim_seq_add_simple(&outp, ExternalLow, ts);
	in_level = 0;
	break;
      case ExternalHigh:
	sim_seq_add_simple(&outp, ExternalHigh, ts);
	in_level = 1;
	break;
      case Send:
	{
	  struct SimItemMsg *msg_item = (struct SimItemMsg*)seq;
	  dali_ctxt.send_msg = msg_item->msg;
	  dali_ctxt.send_left = 1; // Try once
	  sim_seq_add_msg(&outp, Send, ts, &dali_ctxt.send_msg);
	}
	break;
      default:
	break;
	}
      seq = seq->next;
    }
    
    if (bus_change >= ts) {
      bus_level = out_level;
    }
    dali_set_input_level(&dali_ctxt, in_level & bus_level);
    sim_seq_add_simple(&outp, dali_ctxt.in_level ? BusHigh : BusLow, ts);
    uint32_t delta = ts - prev_ts;
    //printf("Delta: %d, level: %d\n", delta, dali_ctxt.in_level);
    int res = dali_handler(&dali_ctxt, delta);
    if (res == DALI_FRAME_RECEIVED) {
      printf("%ld Received: ", ts);
      print_dali_msg(stdout, &dali_ctxt.receive_msg);
      printf("\n");
      sim_seq_add_msg(&outp, ResultRecv, ts, &dali_ctxt.receive_msg);
    } else if (res == DALI_FRAME_SENT) {
      printf("%ld Sent: ", ts);
      print_dali_msg(stdout, &dali_ctxt.send_msg);
      printf("\n");
      sim_seq_add_msg(&outp, ResultSend, ts, &dali_ctxt.send_msg);
    }
    //printf("Timeout: %d\n", dali_ctxt.timeout);
    prev_ts = ts;
    if (out_level != dali_get_output_level(&dali_ctxt)) {
      sim_seq_add_simple(&outp, dali_get_output_level(&dali_ctxt) ? OutHigh : OutLow, ts);
      out_level = dali_ctxt.out_level;
      ts = prev_ts + 13;
      bus_change = ts;
    } else {
      if (dali_ctxt.timeout > 0) {
	ts = prev_ts + dali_ctxt.timeout;
      } else {
	if (!seq) break;
	ts = INT64_MAX;
      }
    }
  }
  return out;
}
