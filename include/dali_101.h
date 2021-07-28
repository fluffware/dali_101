#ifndef __DALI_101_H__CI4ED7DNBH__
#define __DALI_101_H__CI4ED7DNBH__

#include <stdint.h>
#include <dali_msg.h>
#include <sys/pt.h>

struct DaliTimingParams
{
	uint32_t max_switch;
	uint32_t half_bit;
	uint32_t double_half_bit;
	uint32_t stop_condition;
	uint32_t min_half_bit;
	uint32_t max_half_bit;
	uint32_t min_double_half_bit;
	uint32_t max_double_half_bit;

	uint32_t destroy_short;
	uint32_t accept_half;
	uint32_t destroy_mid;
	uint32_t accept_double_half;
};

#ifndef TICKS_PER_SEC
#define TICKS_PER_SEC 200000000LL
#endif

#define SEC_TO_TICKS(u) (TICKS_PER_SEC * u)
#define MSEC_TO_TICKS(u) (TICKS_PER_SEC * u / 1000LL)
#define USEC_TO_TICKS(u) (TICKS_PER_SEC * u / 1000000LL)
#define NSEC_TO_TICKS(n) (TICKS_PER_SEC * n / 1000000000LL)


#define MAX_DELTA (UINT32_MAX/2-1)

#if 0
enum DaliState
  {
    Idle, // Wait for start of frame
    Receiving,
    WaitStopCondition, // Wait for the line to become idle after a framing error
    WaitBusHigh, // Wait for the bus to become high again after a long period of low level
    WaitSending, // Wait the desired settling time before sending
    Sending
  };
#endif

struct DaliContext
{
  struct pt pt;
  uint32_t reply_ready:1;
  uint32_t send_done:1;
  uint32_t in_changed:1;
  uint32_t timed_out:1;
  uint32_t notify_send:1; /* The state machine wants to be notified as
			     soon as a message is ready to be sent. */
  uint32_t twice_pending:1; /* Next frame should be sent as second
			       frame of a send twice transaction */
  uint32_t recovery_timing:1; /* Use reduced settling time after collision */
  uint32_t last_in_level:1; /* Previous level on the input*/
  uint32_t in_level:1; /* Current level on the input*/
  uint32_t out_level:1; /* Level sent to the driver */
  uint32_t no_random:1; /* Don't use random timing when sending */
  uint32_t last_in_flank_timer; // Time since the previous change of the input
  uint32_t last_out_flank_timer; // Time since the previous change of the output
  uint32_t timeout; /* Max time to wait before the next invocation of
		       the handler. If zero then wait indefinitely */
  uint32_t frame; // Frame being sent or received
  uint32_t mask; // Used when coding frame
  uint32_t half_bits; /* Half bits to sent or received so far counting
			 from start of start bit. Only confirmed
			 changes on the bus advances the count, so the
			 count corresponds to the last change. */
  struct DaliTimingParams timing;
  uint16_t send_left; /* Number of times left to try to send a frame.
			 If greater than 0 then there is a frame
			 pending for transmission. */
  uint16_t send_retries;  /* Number of times sending a message is retried
			     in case of a collision if the RETRY flag is set. */
  struct dali_msg send_msg;  // The message pending for transmission
  struct dali_msg receive_msg; // Message being received
};

    
#define DALI_FRAME_SENT 1 /* A message has been sent (or discarded)
			     and send_msg.result is set to reflect the
			     result */
#define DALI_FRAME_RECEIVED 2 /* A frame has been received (possibly
				 with errors) and stored in
				 receive_msg */

// Input and output levels
#define DALI_HIGH 1 /* Idle */
#define DALI_LOW 0 /* Pulled low */

/* delta is the time passed since the previous invocation of the handler,
   state->in_level must be updated to reflect the state of the input.
*/
uint32_t
dali_handler(struct DaliContext *ctxt, uint32_t delta);

void
dali_init(struct DaliContext *ctxt);

void
dali_set_input_level(struct DaliContext *ctxt, uint32_t level);


uint32_t
dali_get_output_level(const struct DaliContext *ctxt);

void
dali_send_msg(struct DaliContext *ctxt, const struct dali_msg *msg);

#endif /* __DALI_101_H__CI4ED7DNBH__ */
