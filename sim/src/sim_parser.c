#include <sim_parser.h>
#include <assert.h>

 #define eprintf(...) fprintf(stderr, __VA_ARGS__)

static void
skip_white(const char **pp)
{
  const char *p = *pp;
  while(1) {
    char c = *p;
    if (c == '\0' || !(c == ' ' || c == '\t' || c == '\n' || c == '\r')) break;
    p++;
  }
  *pp = p;
}

// Return number of characters consumed
static int
parse_uint(const char **pp, int64_t *vp)
{
  const char *p = *pp;
  int64_t v = 0;
  while(*p >= '0' && *p <= '9') {
    v = v * 10 + (*p - '0');
    p++;
  }
  *vp = v;
  int l = p - *pp;
  *pp = p;
  return l;
}

// Return number of characters consumed
static int
parse_uint_hex(const char **pp, uint64_t *vp)
{
  const char *p = *pp;
  int64_t v = 0;
  while(1) {
    if (*p >= '0' && *p <= '9') {
      v = v * 16 + (*p - '0');
    } else if (*p >= 'A' && *p <= 'F') {
      v = v * 16 + (*p - 'A') + 10;
    } else if (*p >= 'a' && *p <= 'f') {
      v = v * 16 + (*p - 'a') + 10;
    } else {
      break;
    }
    p++;
  }
  *vp = v;
  int l = p - *pp;
  *pp = p;
  return l;
}

int
match_token(const char **pp, const char *token)
{
  const char *p = *pp;
  while(*token != '\0') {
    if (*token != *p) return 0;
    p++;
    token++;
  }
  *pp = p;
  return 1;
}

static int
parse_frame(const char **pp, struct dali_msg *msg)
{
  uint64_t frame;
  uint16_t flags = 0;
  skip_white(pp);
  int l = parse_uint_hex(pp, &frame);
  switch(l) {
  case 2:
    l = 8;
    flags |= DALI_FLAGS_LENGTH_8;
    break;
  case 4:
    l = 16;
    flags |= DALI_FLAGS_LENGTH_16;
    break;
  case 6:
    l = 24;
    flags |= DALI_FLAGS_LENGTH_24;
    break;
  default:
    eprintf("Invalid frame length\n");
    return 0;
  }
  msg->flags = (msg->flags &~DALI_FLAGS_LENGTH) | flags;
  frame <<= 32 - l;
  msg->frame[0] = frame >> 24;
  msg->frame[1] = frame >> 16;
  msg->frame[2] = frame >> 8;
  return 1;
}

int
parse_msg_options(const char **pp, struct dali_msg *msg)
{
  while(1) {
    skip_white(pp);
    if (**pp != '-') break;
    (*pp)++;
    switch(**pp) {
    case 'p':
      (*pp)++;
      {
	int64_t p;
	skip_white(pp);
	if (parse_uint(pp, &p) == 0) {
	  eprintf("Missing priority value\n");
	  return 0;
	}
	msg->flags = (msg->flags & ~DALI_FLAGS_PRIORITY) | p;
      }
      break;
    case 'a':
      (*pp)++;
      msg->flags |= DALI_FLAGS_EXPECT_ANSWER;
      break;
    case 't':
      (*pp)++;
      msg->flags |= DALI_FLAGS_SEND_TWICE;
      break;
    case 'r':
      (*pp)++;
      msg->flags |= DALI_FLAGS_RETRY;
      break;
    case 'f':
      (*pp)++;
      if (!(parse_frame(pp, msg))) {
	return 0;
      }
      break;
    case 's':
      (*pp)++;
      {
	int64_t s;
	skip_white(pp);
	if (parse_uint(pp, &s) == 0) {
	  eprintf("Missing sequence value\n");
	  return 0;
	}
	msg->seq = s;
      }
      break;
    case 'c':
      (*pp)++;
      msg->flags |= DALI_FLAGS_NO_COLLISIONS;
      break;
    default:
      eprintf("Unknown option %c\n", **pp);
      return 0;
    }
  }
  return 1;
}

#define STOP_CONDITION USEC_TO_TICKS(2400)
#define HALF_BIT USEC_TO_TICKS(417)
#define FULL_BIT USEC_TO_TICKS(833)

static int64_t
build_frame(struct SimItem ***tail, int64_t ts,
	    uint32_t frame, unsigned int len)
{
  sim_seq_add_simple(tail, ExternalLow, ts);
  ts += HALF_BIT;
  sim_seq_add_simple(tail, ExternalHigh, ts);
  int high = 1;
  frame = (frame << (31 - len)) | 0x80000000;
  frame ^=  frame << 1;
  while(len > 0) {
    if (frame & 0x80000000) {
      ts+= FULL_BIT;
      high = !high;
      sim_seq_add_simple(tail, high ? ExternalHigh : ExternalLow, ts);
    } else {
      ts+= HALF_BIT;
      sim_seq_add_simple(tail, high ? ExternalLow : ExternalHigh, ts);
      ts+= HALF_BIT;
      sim_seq_add_simple(tail, high ? ExternalHigh : ExternalLow, ts);
    }
    len--;
    frame <<= 1;
  }
  if (!high) {
    ts+= HALF_BIT;
    sim_seq_add_simple(tail, ExternalHigh, ts);
  }
  return ts;
}

int
sim_parser_parse(struct ParserContext *ctxt, const char **pp)
{
  while(1) {
    skip_white(pp);
    int64_t ts = ctxt->current_ts;
    switch(**pp) {
    case 'H':
      sim_seq_add_simple(&ctxt->sequence_tail, ExternalHigh, ts);
      (*pp)++;
      break;
    case 'L':
      sim_seq_add_simple(&ctxt->sequence_tail, ExternalLow, ts);
      (*pp)++;
      break;
    case 'T':
      (*pp)++;
      {
	const char s = **pp;
	if (s == '+' || s == '-') {
	  (*pp)++;
	}
	int64_t t;
	if (parse_uint(pp,&t) == 0) {
	  eprintf("Invalid timestamp value\n");
	  return 0;
	}
	if (match_token(pp, "ns")) {
	  t = NSEC_TO_TICKS(t);
	} else if (match_token(pp, "us")) {
	  t = USEC_TO_TICKS(t);
	} else if (match_token(pp, "ms")) {
	  t = MSEC_TO_TICKS(t);
	} else if (match_token(pp, "s")) {
	  t = SEC_TO_TICKS(t);
	}
	if (s == '+') {
	  ctxt->current_ts += t;
	} else if (s == '-') {
	  ctxt->current_ts -= t;
	} else {
	  ctxt->current_ts = t + ctxt->group_stack[ctxt->stack_top];
	}
      }
      break;
    case '>':
      (*pp)++;
      {
	uint64_t frame;
	int l = parse_uint_hex(pp, &frame);
	switch(l) {
	case 2:
	  l = 8;
	  break;
	case 4:
	  l = 16;
	  break;
	case 6:
	  l = 24;
	  break;
	default:
	  eprintf("Invalid frame length\n");
	  return 0;
	}
	ctxt->current_ts = build_frame(&ctxt->sequence_tail, ts, frame,l);
      }
      break;
    case 'S':
      (*pp)++;
      {
	struct SimItemMsg *msg = malloc(sizeof(struct SimItemMsg));
	assert(msg);
	msg->item.next = NULL;
	msg->item.ts = ts;
	msg->item.type = Send;
	msg->msg.seq = 21983;
	msg->msg.flags = DALI_FLAGS_PRIORITY_5;
	msg->msg.result = 0;
	if (!parse_msg_options(pp, &msg->msg)) {
	  free(msg);
	  return 0;
	}
	if (!parse_frame(pp, &msg->msg)) {
	  free(msg);
	  return 0;
	}
	
	*ctxt->sequence_tail = &msg->item;
	ctxt->sequence_tail = &msg->item.next;
      }
      break;
    case '#':
      (*pp)++;
      while(**pp != '\n' && **pp != '\0') (*pp)++;
      break;
    case '\0':
      return 1;
    default:
      eprintf("Unknown event %c\n", **pp);
      return 0;
    }
  }
  return 1;
}



void
sim_parser_init(struct ParserContext *ctxt)
{
  ctxt->current_ts = 0;
  ctxt->group_stack[0] = 0;
  ctxt->stack_top = 0;
  ctxt->sequence = NULL;
  ctxt->sequence_tail = &ctxt->sequence;

  // Start with bus high
  sim_seq_add_simple(&ctxt->sequence_tail, ExternalHigh, 0);
}
