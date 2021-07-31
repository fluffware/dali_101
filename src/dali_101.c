#include <dali_101.h>

//#define DEBUG
#ifdef DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

struct DaliTimingParams default_timing =
{
	.max_switch = USEC_TO_TICKS(30),
	.half_bit = USEC_TO_TICKS(417),
	.double_half_bit = USEC_TO_TICKS(833),
	.stop_condition = USEC_TO_TICKS(2400),
	.min_half_bit = USEC_TO_TICKS(330),
	.max_half_bit = USEC_TO_TICKS(503),
	.min_double_half_bit = USEC_TO_TICKS(663),
	.max_double_half_bit = USEC_TO_TICKS(1003),

	.destroy_short = USEC_TO_TICKS(380),
	.accept_half = USEC_TO_TICKS(440),
	.destroy_mid = USEC_TO_TICKS(780),
	.accept_double_half = USEC_TO_TICKS(890)
};
#define BREAK_TIME USEC_TO_TICKS(1300)
#define RECOVERY_TIME_MIN USEC_TO_TICKS(4000)
#define RECOVERY_TIME_MAX USEC_TO_TICKS(4600)

#define FORWARD_BACKWARD_DELAY USEC_TO_TICKS(5600)
#define SEND_TWICE_DELAY USEC_TO_TICKS(13500)
#define ANSWER_MAX_TIME  USEC_TO_TICKS(12900)

const static struct
{
	uint32_t min;
	uint32_t max;
} priority_timing[] =
  {
	{USEC_TO_TICKS(13500), USEC_TO_TICKS(14700)},
	{USEC_TO_TICKS(14900), USEC_TO_TICKS(16100)},
	{USEC_TO_TICKS(16300), USEC_TO_TICKS(17700)},
	{USEC_TO_TICKS(17900), USEC_TO_TICKS(19300)},
	{USEC_TO_TICKS(19500), USEC_TO_TICKS(21100)}
};

static uint32_t rstate = 3;

static uint32_t random(void)
{
	/* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
	uint32_t x = rstate;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	rstate = x;
	return x;
}

#define BUS_LOW_REPORT_INTERVAL USEC_TO_TICKS(100000)
#define MAX_TIMER MAX_DELTA

static uint32_t
get_settling_time(struct DaliContext *ctxt)
{
  unsigned int priority = ctxt->send_msg.flags & DALI_FLAGS_PRIORITY;
  if (ctxt->recovery_timing) {
    ctxt->recovery_timing = 0;
    if (ctxt->no_random) return RECOVERY_TIME_MIN;
    else {
      return RECOVERY_TIME_MIN + ((RECOVERY_TIME_MAX - RECOVERY_TIME_MIN)
				  /128 * (random()%(16*128))) / 16;
    }
  }
  if (ctxt->twice_pending) {
    return SEND_TWICE_DELAY;
  } else if (priority == 0) {
    // Backward frame
    return FORWARD_BACKWARD_DELAY;
  } else {
    if (priority > 5) {
      priority = 5;
    }
    uint32_t min = priority_timing[priority-1].min;
    uint32_t max = priority_timing[priority-1].max;
    uint32_t jitter = 0;
    if (!(ctxt->no_random)) {
      jitter = ((max - min)/128 * (random()%(16*128))) / 16;
    }
    return min + jitter;
  }
}

#define DALI_WAIT(t)				\
  do {						\
    ctxt->timeout = (t);			\
    PT_YIELD(&ctxt->pt);			\
  } while(0)

#define DALI_WAIT_TIMEOUT(t)					\
  do {								\
    ctxt->timeout = (t);					\
    while(ctxt->timeout > 0) {					\
      PT_YIELD(&ctxt->pt);					\
      if (ctxt->in_level == DALI_HIGH				\
	  && ctxt->out_level == DALI_LOW) goto output_failure;	\
      if (ctxt->timed_out) break;				\
      if (ctxt->timeout < ctxt->last_in_flank_timer) break;	\
      ctxt->timeout -= ctxt->last_in_flank_timer;		\
    }								\
  } while(0)


// Returns true if a collision is detected
inline int
check_collision(struct DaliContext *ctxt)
{
  if (ctxt->in_level == ctxt->out_level) return 0;
  if ((ctxt->send_msg.flags & DALI_FLAGS_NO_COLLISIONS) == 0) return 1;
  return ctxt->out_level == DALI_LOW;
}

static void
prepare_send(struct DaliContext *ctxt)
{
  /* Build a word with the frame left adjusted with a one before
     and after.
     1x...x10...0 */
  uint32_t frame = 0x40000000;
  switch(ctxt->send_msg.flags & DALI_FLAGS_LENGTH) {
  case DALI_FLAGS_LENGTH_24:
    frame = frame >> 8 | ctxt->send_msg.frame[2] << 23;
  case DALI_FLAGS_LENGTH_16:
    frame = frame >> 8 | ctxt->send_msg.frame[1] << 23;
  case DALI_FLAGS_LENGTH_8:
    frame = frame >> 8 | ctxt->send_msg.frame[0] << 23 | 0x80000000;
    break;
  default: // Send a short frame if the length is not handled
    frame =  0x80400000;
  }
  ctxt->frame = frame;
  ctxt->mask = 0x80000000;
  ctxt->half_bits = 0;
}
    
static PT_THREAD(main_loop(struct DaliContext *ctxt))
{
  PT_BEGIN(&ctxt->pt);
  /* Wait for the bus to become idle */
 wait_idle:
  while(1) {
    if (ctxt->last_in_level == DALI_HIGH
	&& ctxt->last_in_flank_timer >= ctxt->timing.stop_condition) {
      break;
    }
    if (ctxt->in_level == DALI_LOW) {
      DALI_WAIT(BUS_LOW_REPORT_INTERVAL);
      if (ctxt->timed_out) {
	ctxt->reply_ready = 1;
	ctxt->receive_msg.result = DALI_ERR_BUS_LOW;
	DALI_WAIT(0);
	ctxt->reply_ready = 1;
	ctxt->receive_msg.result = DALI_INFO_BUS_HIGH;
      }
    } else {
      if (ctxt->last_in_flank_timer < ctxt->timing.stop_condition) {
	DALI_WAIT(ctxt->timing.stop_condition - ctxt->last_in_flank_timer);
      } else {
	DALI_WAIT(ctxt->timing.stop_condition);
      }
    }
  }
  PRINTF("Idle\n");

  while (ctxt->in_level == DALI_HIGH) {
    if (ctxt->send_left > 0) {
      if (ctxt->send_msg.flags & DALI_FLAGS_NO_COLLISIONS) {
	goto send_no_collision;
      } else {
	goto send;
      }
    } else {
      // Wait for start bit or a new message to be sent
      ctxt->notify_send = 1;
      DALI_WAIT(0);
      ctxt->notify_send = 0;
    }
  }
  ctxt->receive_msg.seq = 0; // Not an answer
  goto receive;

  /* Send frame ignoring collisions */
 send_no_collision:
  {
    uint32_t settling_time = get_settling_time(ctxt);
    ctxt->send_left--;  
    if (ctxt->last_in_flank_timer < settling_time) {
      DALI_WAIT_TIMEOUT(settling_time - ctxt->last_in_flank_timer);
    }
    prepare_send(ctxt);
    ctxt->out_level = DALI_LOW;
    ctxt->last_out_flank_timer = 0;
    DALI_WAIT_TIMEOUT(ctxt->timing.max_switch);
    DALI_WAIT_TIMEOUT(ctxt->timing.half_bit - ctxt->last_out_flank_timer);
    // Middle of startbit
    while(1) {
      // Change output level
      ctxt->out_level ^= 1;
      ctxt->last_out_flank_timer = 0;
      // Wait for the output to switch
      DALI_WAIT(ctxt->timing.max_switch);
      ctxt->half_bits = (ctxt->half_bits + 1) | 1;
      
      uint32_t frame = ctxt->frame;
      if ((frame & ((ctxt->mask - 1)>>1)) == 0) break; 
      if ((frame ^ (frame<<1)) & ctxt->mask) {
	// Double half bit
	DALI_WAIT_TIMEOUT(ctxt->timing.double_half_bit -ctxt->last_out_flank_timer);
      } else {
	// Two half bits
	DALI_WAIT_TIMEOUT(ctxt->timing.half_bit - ctxt->last_out_flank_timer);
	ctxt->out_level ^= 1;
	ctxt->last_out_flank_timer = 0;
	DALI_WAIT_TIMEOUT(ctxt->timing.max_switch);
	ctxt->half_bits++;
	
	DALI_WAIT_TIMEOUT(ctxt->timing.half_bit - ctxt->last_out_flank_timer);
      }
      ctxt->mask >>= 1;
    }
    if (!(ctxt->frame & ctxt->mask)) {
      // Add another half bit if last bit is 0
      DALI_WAIT_TIMEOUT(ctxt->timing.half_bit - ctxt->last_out_flank_timer);
	
      ctxt->out_level ^= 1;
      ctxt->last_out_flank_timer = 0;
      DALI_WAIT_TIMEOUT(ctxt->timing.max_switch);
      ctxt->half_bits++;
	
    }
    goto send_finish;
  }
  /* Send frame with collision detection */
 send:
  {
    uint32_t settling_time = get_settling_time(ctxt);
    ctxt->send_left--;  
    if (ctxt->last_in_flank_timer < settling_time) {
      DALI_WAIT(settling_time - ctxt->last_in_flank_timer);
      if (ctxt->in_changed) {
	if (ctxt->send_left == 0) {
	  ctxt->send_msg.result = DALI_ERR_BUS_BUSY;
	  ctxt->send_done = 1;
	}
	goto receive; /* Got low bus during settling time */
      }
    }
    PRINTF("Send\n");
    prepare_send(ctxt);
    // Start of startbit
    ctxt->out_level = DALI_LOW;
    ctxt->last_out_flank_timer = 0;
    DALI_WAIT(ctxt->timing.max_switch);
    if (ctxt->in_level == DALI_HIGH) goto collision_no_change;
    DALI_WAIT(ctxt->timing.half_bit - ctxt->last_out_flank_timer);
    if (ctxt->in_level == DALI_HIGH) goto collision_change;
    
    // Middle of startbit
    while(1) {
      // Change output level
      ctxt->out_level ^= 1;
      ctxt->last_out_flank_timer = 0;
      // Wait for the output to switch
      DALI_WAIT(ctxt->timing.max_switch);
      if (ctxt->in_level != ctxt->out_level) goto collision_no_change;
      ctxt->half_bits = (ctxt->half_bits + 1) | 1;
      
      uint32_t frame = ctxt->frame;
      if ((frame & ((ctxt->mask - 1)>>1)) == 0) break; 
      if ((frame ^ (frame<<1)) & ctxt->mask) {
	// Double half bit
	DALI_WAIT(ctxt->timing.double_half_bit -ctxt->last_out_flank_timer);
	if (ctxt->in_level != ctxt->out_level) goto collision_change;
      } else {
	// Two half bits
	DALI_WAIT(ctxt->timing.half_bit - ctxt->last_out_flank_timer);
	if (ctxt->in_level != ctxt->out_level) goto collision_change;
	ctxt->out_level ^= 1;
	ctxt->last_out_flank_timer = 0;
	DALI_WAIT(ctxt->timing.max_switch);
	if (ctxt->in_level != ctxt->out_level) goto collision_no_change;
	ctxt->half_bits++;
	
	DALI_WAIT(ctxt->timing.half_bit - ctxt->last_out_flank_timer);
	if (ctxt->in_level != ctxt->out_level) goto collision_change;
      }
     
      
      ctxt->mask >>= 1;
    }
    
    if (!(ctxt->frame & ctxt->mask)) {
      // Add another half bit if last bit is 0
      DALI_WAIT(ctxt->timing.half_bit - ctxt->last_out_flank_timer);
      if (ctxt->in_level != ctxt->out_level) goto collision_change;
	
      ctxt->out_level ^= 1;
      ctxt->last_out_flank_timer = 0;
      DALI_WAIT(ctxt->timing.max_switch);
      if (ctxt->in_level != ctxt->out_level) goto collision_no_change;
      ctxt->half_bits++;
	
    }
  }
 send_finish:
  if (ctxt->send_msg.flags & DALI_FLAGS_EXPECT_ANSWER) {
    DALI_WAIT(ANSWER_MAX_TIME);
    if (ctxt->in_changed) {
      ctxt->twice_pending = 0;
      ctxt->send_done = 1;
      ctxt->send_left = 0; // Stop further transmission attempts
      ctxt->send_msg.result = 0;
      ctxt->receive_msg.seq = ctxt->send_msg.seq;
      goto receive;
    }
    ctxt->send_msg.result = DALI_NO_REPLY;
  } else {
    if (ctxt->send_msg.flags & DALI_FLAGS_SEND_TWICE && !ctxt->twice_pending) {
      ctxt->send_left = 1; // Only try sending second frame once
      ctxt->twice_pending = 1;
      goto wait_idle;
    }
  }
  
  ctxt->twice_pending = 0;
  ctxt->send_done = 1;
  ctxt->send_left = 0; // Stop further transmission attempts
  goto wait_idle;
  
  /* Receive frame */
 receive:
  DALI_WAIT(ctxt->timing.stop_condition);
  // First interval is always half bit
  uint32_t flank_timer = ctxt ->last_in_flank_timer;
  if (flank_timer < ctxt->timing.min_half_bit
      || flank_timer > ctxt->timing.max_half_bit) {
    ctxt->reply_ready = 1;
    ctxt->receive_msg.result = DALI_ERR_FRAMING;
    goto wait_idle;
  }
  PRINTF("Start bit\n");
  ctxt->half_bits = 1;
  ctxt->frame = 0;
  // Middle of start bit
  while(1) {
    DALI_WAIT(ctxt->timing.stop_condition);//  Mid -> (Full or Mid)
    if (ctxt->timed_out) {
      break;
    } else {
      if (ctxt->last_in_flank_timer >= ctxt->timing.min_half_bit
	  && ctxt->last_in_flank_timer <= ctxt->timing.max_half_bit) {
	// Accepted as second half of bit 
	ctxt->half_bits++;
      accept_as_mid_to_full:
	DALI_WAIT(ctxt->timing.stop_condition); // Full -> Mid
	if (ctxt->timed_out) {
	  break;
	}
	if (ctxt->last_in_flank_timer < ctxt->timing.min_half_bit
	    || ctxt->last_in_flank_timer > ctxt->timing.max_half_bit) {
	  ctxt->reply_ready = 1;
	  ctxt->receive_msg.result = DALI_ERR_FRAMING;
	  break;
	}
	ctxt->half_bits++;
	// Accepted as first half of bit
      } else {
	if (ctxt->last_in_flank_timer < ctxt->timing.min_double_half_bit
	    || ctxt->last_in_flank_timer > ctxt->timing.max_double_half_bit) {
	  ctxt->reply_ready = 1;
	  ctxt->receive_msg.result = DALI_ERR_FRAMING;
	  break;
	}
	ctxt->half_bits += 2;
      }
    accept_as_to_mid:
      // Mid bit
      ctxt->frame = ctxt->frame << 1 | ctxt->in_level;
      //PRINTF("Frame: %08x\n", ctxt->frame);
    }
  }
  if (ctxt->reply_ready) goto wait_idle; // Got an error
  if (ctxt->timed_out) {
    // Got stop condition
      
    if (ctxt->in_level == DALI_HIGH) {
      ctxt->receive_msg.flags = 0;
      ctxt->receive_msg.result = DALI_OK;
      uint32_t frame = ctxt->frame;
      switch((ctxt->half_bits-1)/2) {
      case 24:
	ctxt->receive_msg.frame[0] = frame>>16;
	ctxt->receive_msg.frame[1] = frame>>8;
	ctxt->receive_msg.frame[2] = frame;
	ctxt->receive_msg.flags = DALI_FLAGS_LENGTH_24;
	break;
      case 16:
	ctxt->receive_msg.frame[0] = frame>>8;
	ctxt->receive_msg.frame[1] = frame;
	ctxt->receive_msg.flags = DALI_FLAGS_LENGTH_16;
	break;
      case 8:
	ctxt->receive_msg.frame[0] = frame;
	ctxt->receive_msg.flags = DALI_FLAGS_LENGTH_8;
	break;
      default:
	// Invalid frame length
	ctxt->receive_msg.result = DALI_ERR_FRAMING;
      }
      ctxt->reply_ready = 1;
	
    } else {
      ctxt->reply_ready = 1;
      ctxt->receive_msg.result = DALI_ERR_FRAMING;
    }
  }
  goto wait_idle;

 collision_no_change:
  if (ctxt->out_level == DALI_LOW) goto output_failure;
  // Wait for the change 
  DALI_WAIT(ctxt->timing.stop_condition - ctxt->last_in_flank_timer);
 collision_change:
  if (ctxt->out_level == DALI_LOW) goto output_failure;

  if (ctxt->send_left == 0) {
    // Given up on transmission
    ctxt->send_msg.result = DALI_ERR_BUS_BUSY;
    ctxt->send_done = 1;
  }
  //collision:
  if (ctxt->half_bits & 1) {
    // Incorrect interval started on half bit boundary
    if (ctxt->last_in_flank_timer > ctxt->timing.accept_double_half) {
      goto collision_destroy;
    }
    if (ctxt->last_in_flank_timer > ctxt->timing.destroy_mid) {
      // Accept as double half bit
      ctxt->frame >>= 32 - (ctxt->half_bits + 1) / 2;
       ctxt->half_bits += 2;
      goto accept_as_to_mid;
    }
  }

  if (ctxt->last_in_flank_timer > ctxt->timing.accept_half) {
    goto collision_destroy;
  }
  if (ctxt->last_in_flank_timer < ctxt->timing.destroy_short) {
    goto collision_destroy;
  }

  // Accept as half bit
  ctxt->frame >>= 32 - (ctxt->half_bits + 1) / 2;
  ctxt->half_bits++;
  goto accept_as_mid_to_full;
  
 collision_destroy:
  ctxt->out_level = DALI_LOW;
  ctxt->last_out_flank_timer = 0;
  DALI_WAIT(ctxt->timing.max_switch);
  if (ctxt->in_level != ctxt->out_level) goto output_failure;
  DALI_WAIT(BREAK_TIME - ctxt->last_out_flank_timer);
  if (ctxt->in_level != ctxt->out_level) goto output_failure;
  ctxt->out_level = DALI_HIGH;
  DALI_WAIT(ctxt->timing.max_switch);
  if (ctxt->in_level == DALI_HIGH) {
    ctxt->recovery_timing = 1;
  }

  PRINTF("Collision detected\n");
  goto wait_idle;
  
 output_failure:
  // Low output and high bus means some kind of hardware failure
  ctxt->out_level = 1;
  // Yield so the bus is released
  DALI_WAIT(ctxt->timing.stop_condition);
  ctxt->send_left = 0;
  ctxt->send_msg.result = DALI_ERR_DRIVER;
  ctxt->send_done = 1;
  goto wait_idle;
  PT_END(&ctxt->pt);
}

void
dali_init(struct DaliContext *ctxt)
{
  PT_INIT(&ctxt->pt);
  ctxt->last_in_flank_timer = 0;
  ctxt->last_out_flank_timer = 0;
  ctxt->timeout = 0;
  ctxt->last_in_level = DALI_HIGH;
  ctxt->in_level = DALI_HIGH;
  ctxt->out_level = DALI_HIGH;
  ctxt->no_random = 0;
  ctxt->timing = default_timing;
  ctxt->reply_ready = 0;
  ctxt->send_done = 0;
  ctxt->receive_msg.seq = 0;
  ctxt->twice_pending = 0;
  ctxt->recovery_timing = 0;
  ctxt->send_left = 0;
  ctxt->send_retries = 2;
  ctxt->notify_send = 0;
}

uint32_t
dali_handler(struct DaliContext *ctxt, uint32_t delta)
{
  if (ctxt->last_in_flank_timer < MAX_TIMER) {
    ctxt->last_in_flank_timer += delta;
  } else {
    ctxt->last_in_flank_timer = MAX_TIMER;
  }
  if (ctxt->last_out_flank_timer < MAX_TIMER) {
    ctxt->last_out_flank_timer += delta;
  } else {
    ctxt->last_out_flank_timer = MAX_TIMER;
  }

  ctxt->in_changed = ctxt->last_in_level != ctxt->in_level;
  ctxt->timed_out = delta >= ctxt->timeout;
  if (!ctxt->timed_out) {
    ctxt->timeout -= delta;
  }
  if (!ctxt->timed_out && !ctxt->in_changed && !ctxt->notify_send) return 0;

  // Run the state machine
  (void)PT_SCHEDULE(main_loop(ctxt));
  
  if (ctxt->in_changed) {
    ctxt->last_in_flank_timer = 0;
    ctxt->last_in_level = ctxt->in_level;
  }
  if (ctxt->reply_ready) {
    ctxt->reply_ready = 0;
    return DALI_FRAME_RECEIVED;
  } else if (ctxt->send_done) {
    ctxt->send_done = 0;
    return DALI_FRAME_SENT;
  } else {
    return 0;
  }
  return 0;
}



void
dali_send_msg(struct DaliContext *ctxt, const struct dali_msg *msg)
{
  ctxt->send_msg = *msg;
  if (msg->flags & DALI_FLAGS_RETRY) {
    ctxt->send_left = 1 + ctxt->send_retries;
  } else {
    ctxt->send_left = 1; // Try once
  }
}
