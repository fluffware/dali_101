#include "msg.h"

void
print_dali_msg(FILE *f, struct dali_msg *msg)
{
  switch(msg->flags & DALI_FLAGS_LENGTH) {
  case DALI_FLAGS_LENGTH_24:
    fprintf(f, "%02x%02x%02x", msg->frame[0], msg->frame[1],  msg->frame[2]);
    break;
  case DALI_FLAGS_LENGTH_16:
    fprintf(f, "%02x%02x", msg->frame[0], msg->frame[1]);
    break;
  case DALI_FLAGS_LENGTH_8:
    fprintf(f, "%02x", msg->frame[0]);
    break;
  case DALI_FLAGS_DRIVER:
    fprintf(f, "Driver command");
    break;
  }
  fprintf(f," [ ");
  if (msg->flags & DALI_FLAGS_SEND_TWICE) {
    fprintf(f, "twice ");
  }
  if (msg->flags & DALI_FLAGS_EXPECT_ANSWER) {
    fprintf(f, "answer ");
  }
  if (msg->flags & DALI_FLAGS_RETRY) {
    fprintf(f, "retry ");
  }
  
  if (msg->flags & DALI_FLAGS_NO_COLLISIONS) {
    fprintf(f, "no-collision ");
  }

  fprintf(f,"] pri: %d", msg->flags & DALI_FLAGS_PRIORITY);
  fprintf(f," seq: %d", msg->seq);
  fprintf(f," res: %d", msg->result);
}
