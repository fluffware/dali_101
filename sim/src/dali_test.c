#include <dali_101.h>
#include <stdio.h>
#include <assert.h>

static void
dump_msg(struct dali_msg *msg)
{
  printf("%d: %04x [", msg->seq, msg->flags);
  switch(msg->flags & DALI_FLAGS_LENGTH) {
  case DALI_FLAGS_LENGTH_24:
    printf("%02x %02x %02x",msg->frame[0],msg->frame[1],msg->frame[2]);
    break;
  case DALI_FLAGS_LENGTH_16:
    printf("%02x %02x",msg->frame[0],msg->frame[1]);
    break;
  case DALI_FLAGS_LENGTH_8:
    printf("%02x",msg->frame[0]);
    break;
  }
  printf("] %d\n", msg->result);
}

#define STOP_CONDITION USEC_TO_TICKS(2400)
#define HALF_BIT USEC_TO_TICKS(417)
#define FULL_BIT USEC_TO_TICKS(833)

static unsigned int
build_frame(uint32_t *seq, unsigned int seq_len,
	    uint32_t frame, unsigned int len)
{
  unsigned int left = seq_len;
  assert(left >= 5);
  *seq++ = STOP_CONDITION;
  *seq++ = HALF_BIT;
  int high = 1;
  left -= 2;
  frame = (frame << (31 - len)) | 0x80000000;
  while(len > 0) {
    if ((frame ^ (frame << 1)) & 0x80000000) {
      assert(left >= 1);
      left--;
      *seq++ = FULL_BIT;
      high = !high;
    } else {
      assert(left >= 2);
      left-=2;
      *seq++ = HALF_BIT;
      *seq++ = HALF_BIT;
    }
    len--;
    frame <<= 1;
  }
  if (!high) {
    assert(left >= 1);
    left--;
    *seq++ = HALF_BIT;
  }
  assert(left >= 1);
  *seq = 0;
  return seq_len - left;
}

#define SPLIT_TICKS 100
static void
run_sequence(struct DaliContext *ctxt, const uint32_t *seq) {
  uint32_t seq_next = *seq++;
  uint32_t delta = seq_next;
  do {
  
    if (delta == 0) {
      printf("End\n");
      break;
    }
#if 0
    if (delta > SPLIT_TICKS) {
      delta -= SPLIT_TICKS;
    }
#endif
    if (seq_next > 0) {
      if (delta < seq_next) {
	seq_next -= delta;
      } else {
	assert(delta == seq_next);
	ctxt->in_level ^= 1;
	seq_next = *seq++;
      }
    }

    printf("Delta: %d, level: %d\n", delta, ctxt->in_level);
    int res = dali_handler(ctxt, delta);
    printf("Timeout: %d\n", ctxt->timeout);
    if (res == DALI_FRAME_RECEIVED) {
      dump_msg(&ctxt->receive_msg);
    }
    
    delta = ctxt->timeout;
    if (seq_next > 0 && (delta > seq_next || delta == 0)) {
      delta = seq_next;
    }
  }  while(1);
}

#define CHECK_LENGTH(ctxt, len) \
  (((ctxt).receive_msg.flags & DALI_FLAGS_LENGTH) == (len))

#define CHECK_RESULT(ctxt, res) \
  ((ctxt).receive_msg.result == (res))

int
main()
{
  struct DaliContext dali_ctxt;
  dali_init(&dali_ctxt);
  

  const uint32_t bit8_seq[] = {STOP_CONDITION, HALF_BIT, HALF_BIT, HALF_BIT,HALF_BIT,HALF_BIT,FULL_BIT,FULL_BIT,HALF_BIT,HALF_BIT,FULL_BIT, FULL_BIT,HALF_BIT, HALF_BIT,0};
  uint32_t frame_seq[32*2+3];
  uint32_t short_high_seq[] = {STOP_CONDITION,HALF_BIT, USEC_TO_TICKS(100), FULL_BIT,0};
  uint32_t short_low_seq[] = {STOP_CONDITION,HALF_BIT, FULL_BIT, USEC_TO_TICKS(100),0};
  uint32_t long_low_seq[] = {STOP_CONDITION,HALF_BIT, HALF_BIT, FULL_BIT,0};
  uint32_t bus_drop_seq[] = {STOP_CONDITION,USEC_TO_TICKS(190000), HALF_BIT, HALF_BIT,0};

  
  build_frame(frame_seq, sizeof(frame_seq) / sizeof(frame_seq[0]), 0xf3f05d, 24);
  run_sequence(&dali_ctxt, frame_seq);
  assert(CHECK_LENGTH(dali_ctxt,DALI_FLAGS_LENGTH_24));
  assert(dali_ctxt.receive_msg.frame[0] == 0xf3
	 && dali_ctxt.receive_msg.frame[1] == 0xf0
	 && dali_ctxt.receive_msg.frame[2] == 0x5d);
  
  build_frame(frame_seq, sizeof(frame_seq) / sizeof(frame_seq[0]), 0x0350, 16);
  run_sequence(&dali_ctxt, frame_seq);
  assert(CHECK_LENGTH(dali_ctxt,DALI_FLAGS_LENGTH_16));
  assert(dali_ctxt.receive_msg.frame[0] == 0x03
	 && dali_ctxt.receive_msg.frame[1] == 0x50);
  
  run_sequence(&dali_ctxt, bit8_seq);
  assert(CHECK_LENGTH(dali_ctxt,DALI_FLAGS_LENGTH_8));
  assert(dali_ctxt.receive_msg.frame[0] == 0xdb);


  build_frame(frame_seq, sizeof(frame_seq) / sizeof(frame_seq[0]), 0xf350, 15);
  run_sequence(&dali_ctxt, frame_seq);
  assert(CHECK_RESULT(dali_ctxt,DALI_ERR_FRAMING));
  
  build_frame(frame_seq, sizeof(frame_seq) / sizeof(frame_seq[0]), 0xf350, 25);
  run_sequence(&dali_ctxt, frame_seq);
  assert(CHECK_RESULT(dali_ctxt,DALI_ERR_FRAMING));
  
  run_sequence(&dali_ctxt, short_low_seq);
  assert(CHECK_RESULT(dali_ctxt,DALI_ERR_FRAMING));
  
  run_sequence(&dali_ctxt, short_high_seq);
  assert(CHECK_RESULT(dali_ctxt,DALI_ERR_FRAMING));
  
  run_sequence(&dali_ctxt, long_low_seq);
  assert(CHECK_RESULT(dali_ctxt,DALI_ERR_FRAMING));

  run_sequence(&dali_ctxt, bus_drop_seq);
  assert(CHECK_RESULT(dali_ctxt,DALI_INFO_BUS_HIGH));


  dali_ctxt.send_msg.flags = DALI_FLAGS_LENGTH_8;
  dali_ctxt.send_msg.frame[0] = 0xa5;
  dali_handler(&dali_ctxt, USEC_TO_TICKS(20000));
  dali_ctxt.in_level = dali_ctxt.out_level;
  while(dali_ctxt.timeout != 0) {
    printf("Delta: %d %d\n", dali_ctxt.timeout, dali_ctxt.in_level);
    dali_handler(&dali_ctxt, dali_ctxt.timeout);
    dali_ctxt.in_level = dali_ctxt.out_level;
  }
  
}
