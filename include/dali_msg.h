#ifndef __DALI_MSG_H__FOKLKPJJ07__
#define __DALI_MSG_H__FOKLKPJJ07__


#define DALI_OK 0
#define DALI_ERR_FRAMING 10 // Received a frame with a framing error
#define DALI_ERR_BUS_LOW 20 // The bus has been low for a long time
#define DALI_INFO_BUS_HIGH 24 // The bus has returned to a high level
#define DALI_ERR_BUS_BUSY 30 /* Failed to send a frame, including
			      * retries, due to bus activity.
			      */
#define DALI_NO_REPLY 40 /* Timeout when waiting for a backward frame */
#define DALI_ERR_DRIVER 50 /* Hardware or software error */
#define DALI_ERR_TIMING 55 /* A timeout wasn't handled quickly enough */

// Frame length
#define DALI_FLAGS_LENGTH 0x700
#ifdef ENABLE_FRAME25
#define DALI_FLAGS_LENGTH_25 0x500
#endif
#define DALI_FLAGS_LENGTH_24 0x300
#define DALI_FLAGS_LENGTH_16 0x200
#define DALI_FLAGS_LENGTH_8 0x0100
#define DALI_FLAGS_DRIVER 0x000 // Driver command, don't send a frame

// Repeat frame
#define DALI_FLAGS_SEND_TWICE 0x20
#define DALI_FLAGS_SEND_ONCE 0x00

// Retry
#define DALI_FLAGS_RETRY 0x10

#define DALI_FLAGS_EXPECT_ANSWER 0x08

// Ignore collision. Must be used for backward frames.
#define DALI_FLAGS_NO_COLLISIONS 0x40

// Priority
#define DALI_FLAGS_PRIORITY 0x07
#define DALI_FLAGS_PRIORITY_0 0x00 // Backward frame
#define DALI_FLAGS_PRIORITY_1 0x01
#define DALI_FLAGS_PRIORITY_2 0x02
#define DALI_FLAGS_PRIORITY_3 0x03
#define DALI_FLAGS_PRIORITY_4 0x04
#define DALI_FLAGS_PRIORITY_5 0x05

struct dali_msg
{
	uint8_t seq;/* Sequence number. Replies have the same
		      * sequence number as the corresponding sent
		      * frame or command. Received frame not
		      * corresponding to a sent frame has sequence
		      * number 0 */
	uint8_t result; /* Set when a request is returned. */
	uint16_t flags;
	uint8_t frame[4]; /* Frame data or driver command */
};

#endif /* __DALI_MSG_H__FOKLKPJJ07__ */
