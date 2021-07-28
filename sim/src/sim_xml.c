#include "sim_xml.h"
#include <stdio.h>
#include <ctype.h>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)



#define MAX_STACK 20
struct _ParserContext
{
  int64_t current_ts;  // Timestamp of last event (reference for rel)
  int64_t group_ts; // Timestamp of parent (reference for abs)
  int64_t group_stack[MAX_STACK];
  int stack_top;
  struct SimItem *sequence;
  struct SimItem **sequence_tail;

  struct CheckItem *check;
  struct CheckItem **check_tail;
};

typedef struct _ParserContext ParserContext;

static void
push_ts(ParserContext *ctxt)
{
  if (ctxt->stack_top == MAX_STACK) {
    eprintf("Too many nested events\n");
    return;
  }
  ctxt->group_stack[ctxt->stack_top++] = ctxt->current_ts;
  ctxt->group_ts = ctxt->current_ts;
}

static void
pop_ts(ParserContext *ctxt)
{
  if (ctxt->stack_top == 0) {
    eprintf("Stack empty\n");
    return;
  }
  ctxt->current_ts = ctxt->group_stack[--ctxt->stack_top];
  if (ctxt->stack_top >= 1) { 
    ctxt->group_ts = ctxt->group_stack[ctxt->stack_top - 1];;
  } else {
    ctxt->group_ts = 0;
  }
}

/* Returns 1 on success, 0 if not found, -1 on error */
static int 
get_long_attr(xmlTextReaderPtr reader, const xmlChar *attr_name, long *value)
{
  xmlChar *val_str = xmlTextReaderGetAttribute(reader, attr_name);
  if (val_str) {
    char *end;
    long v = strtol((const char*)val_str, &end, 10);
    if (val_str == (const xmlChar*)end || *end != '\0') {
      xmlFree(val_str);
      return -1;
    }
    xmlFree(val_str);
    *value = v;
    return 1;
  } else {
    return 0;
  }
}

/* Returns 1 on success, 0 if not found, -1 on error */
static int
get_time_attr(xmlTextReaderPtr reader, const xmlChar *attr_name, int64_t *value)
{
  xmlChar *val_str = xmlTextReaderGetAttribute(reader, attr_name);
  if (val_str) {
    char *pos;
    int64_t time;
    long v = strtol((const char*)val_str, &pos, 10);
    if (val_str == (const xmlChar*)pos) {
      eprintf("Invalid time value\n");
      xmlFree(val_str);
      return -1;
    }
    while(isspace(*pos)) pos++;
    if (*pos != '\0')  {
      if (pos[0] == 's') {
	time = v * TICKS_PER_SEC;
	pos += 1;
      } else if (pos[0] == 'n' && pos[1] == 's') {
	time = ((int64_t)v * TICKS_PER_SEC) / 1000000000;
	pos += 2;
      } else if (pos[0] == 'u' && pos[1] == 's') {
	time = ((int64_t)v * TICKS_PER_SEC) / 1000000;
	pos += 2;
      } else if (pos[0] == 'm' && pos[1] == 's') {
	time = ((int64_t)v * TICKS_PER_SEC) / 1000;
	pos += 2;
      } else {
	eprintf("Unknown time unit\n");
	xmlFree(val_str);
	return -1;
      }
      while(isspace(*pos)) pos++;
      if (*pos != '\0') {
	eprintf("Extra charachters at end of time\n");
	xmlFree(val_str);
	return -1;
      }
    } else {
      time = v;
    }
    xmlFree(val_str);
    *value = time;
    return 1;
  } else {
    return 0;
  }
}

/* Returns 1 on success, 0 if not found, -1 on error */
static int
get_twostate_attr(xmlTextReaderPtr reader, const xmlChar *attr_name,
		  const xmlChar *true_str, const xmlChar *false_str,
		  int *state)
{
  xmlChar *val_str = xmlTextReaderGetAttribute(reader, attr_name);
  if (!val_str) return 0;
  if (xmlStrEqual(val_str, true_str)) {
    *state = 1;
  } else if (xmlStrEqual(val_str, false_str)) {
    *state = 0;
  } else {
    eprintf("The value of attribute '%s' must be either '%s' or '%s'\n",
	    attr_name, true_str, false_str);
    xmlFree(val_str);
    return -1;
  }
  xmlFree(val_str);
  return 1;
}		    

static int
get_yesno_attr(xmlTextReaderPtr reader, const xmlChar *attr_name,
	       int *state) 
{
  return get_twostate_attr(reader, attr_name, BAD_CAST "yes", BAD_CAST "no",
			   state);
}

static int
parse_events(ParserContext *ctxt, xmlTextReaderPtr reader);

static int
get_time_attrs(ParserContext *ctxt, xmlTextReaderPtr reader, 
	       int64_t *value)
{
  int ret;
  int64_t time;
  
  ret = get_time_attr(reader, BAD_CAST "abs", &time);
  if (ret == -1) return -1;
  if (ret == 1) {
    time += ctxt->group_ts;
  } else {
    ret = get_time_attr(reader, BAD_CAST "rel", &time);
    if (ret == -1) return -1;
    if (ret == 1) {
      time += ctxt->current_ts;
    } else {
      eprintf("Either 'abs' or 'rel' is required\n");
      return -1;
    }
  }
  *value = time;
  return 1;
}


static int
parse_high_low(ParserContext *ctxt, xmlTextReaderPtr reader, int high)
{
  int ret;
  int64_t time;
  ret = get_time_attrs(ctxt, reader, &time);
  if (ret != 1) return ret;
  sim_seq_add_simple(&ctxt->sequence_tail, 
		     high ? ExternalHigh : ExternalLow,
		     time);
  ctxt->current_ts = time;
  push_ts(ctxt);
  ret = parse_events(ctxt, reader);
  pop_ts(ctxt);
  return ret;
}

static int
parse_check_high_low(ParserContext *ctxt, xmlTextReaderPtr reader, int high)
{
  int ret;
  int64_t time;
  ret = get_time_attrs(ctxt, reader, &time);
  if (ret != 1) return ret;
  check_seq_add_simple(&ctxt->check_tail, 
		     high ? OutHigh : OutLow,
		       time,time);
  ctxt->current_ts = time;
  push_ts(ctxt);
  ret = parse_events(ctxt, reader);
  pop_ts(ctxt);
  return ret;
}

static int
parse_bit_seq(ParserContext *ctxt, xmlTextReaderPtr reader)
{
  int ret;
  int64_t time;
  int64_t interval;
  ret = get_time_attrs(ctxt, reader, &time);
  if (ret != 1) return ret;
  ret = get_time_attr(reader, BAD_CAST "interval", &interval);
  if (ret != 1) return ret;
  xmlChar *bits = xmlTextReaderGetAttribute(reader, BAD_CAST "bits");
  xmlChar *pos = bits;
  while(*pos != '\0') {
    if (*pos != '0' && *pos != '1') {
      eprintf("'bits' attribute may only contain '0' an '1'\n");
      return -1;
    }
    sim_seq_add_simple(&ctxt->sequence_tail, 
		       *pos == '1' ? ExternalHigh : ExternalLow,
		       time);
    pos++;
    time += interval;
  }
  ctxt->current_ts = time;
  push_ts(ctxt);
  ret = parse_events(ctxt, reader);
  pop_ts(ctxt);
  return ret;
}

/* Returns hex value of char or -1 if none */
static int
ascii_as_hex(xmlChar ch)
{
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  return -1;
}

static int
pair_as_hex(xmlChar *str)
{
  int high = ascii_as_hex(str[0]);
  if (high < 0) return -1;
  int low = ascii_as_hex(str[1]);
  if (low < 0) return -1;
  return high * 16 + low;
}


static int
parse_send(ParserContext *ctxt, xmlTextReaderPtr reader)
{
  int ret;
  int64_t time;
  long priority = 1;
  int answer = 0;
  int twice = 0;
  int retry = 0;
  int ignore_collision = 0;
  long seq_no = 1;
  ret = get_time_attrs(ctxt, reader, &time);
  if (ret != 1) return -1;
  
  ret = get_yesno_attr(reader, BAD_CAST "answer", &answer);
  if (ret == -1) return -1;
  ret = get_yesno_attr(reader, BAD_CAST "twice", &twice);
  if (ret == -1) return -1;
  ret = get_yesno_attr(reader, BAD_CAST "retry", &retry);
  if (ret == -1) return -1;
  
  ret = get_twostate_attr(reader, BAD_CAST "collision",
			  BAD_CAST "ignore", BAD_CAST "handle",
			  &ignore_collision);
  if (ret == -1) return -1;

  ret = get_long_attr(reader, BAD_CAST "priority", &priority);
  if (ret == -1) return -1;
  if (priority < 0 || priority > 5) {
    eprintf("'priority' attribute must be between 0 and 5 (inclusive).");
    return -1;
  }
  ret = get_long_attr(reader, BAD_CAST "seq", &seq_no);
  if (ret != 1) return -1;
  
  uint16_t flags = 0;
  if (answer) flags |= DALI_FLAGS_EXPECT_ANSWER;
  if (twice) flags |= DALI_FLAGS_SEND_TWICE;
  if (retry) flags |= DALI_FLAGS_RETRY;
  if (ignore_collision) flags |= DALI_FLAGS_NO_COLLISIONS;

  struct dali_msg msg;
  xmlChar *hex = xmlTextReaderGetAttribute(reader, BAD_CAST "hex");
  switch (xmlStrlen(hex)) {
  case 2:
    flags |= DALI_FLAGS_LENGTH_8;
    break;
  case 4:
    flags |= DALI_FLAGS_LENGTH_16;
    break;
  case 6:
    flags |= DALI_FLAGS_LENGTH_24;
    break;
  default:
    eprintf("Invalid length of 'hex' attribute\n");
    xmlFree(hex);
    return -1;
  }

  flags |= priority;
  xmlChar *pos = hex;
  for (int i = 0; i < 4; i++) {
    if (*pos == '\0') break;
    int v = pair_as_hex(pos);
    if (v < 0) {
      eprintf("Invalid hex number\n");
      xmlFree(hex);
      return -1;
    }
    msg.frame[i] = v;
    pos += 2;
  }
  
  xmlFree(hex);
  msg.flags = flags;
  msg.result = 0;
  msg.seq = seq_no;
  sim_seq_add_msg(&ctxt->sequence_tail, Send, time, &msg);
		  
  ctxt->current_ts = time;
  push_ts(ctxt);
  ret = parse_events(ctxt, reader);
  pop_ts(ctxt);
  
  return ret;
}

static int
parse_events(ParserContext *ctxt, xmlTextReaderPtr reader)
{
  int ret = xmlTextReaderIsEmptyElement(reader);
  if (ret != 0) return ret;
  int parent_depth = xmlTextReaderDepth(reader);
  ret = xmlTextReaderRead(reader);
  while(ret == 1) {
    if (parent_depth >= xmlTextReaderDepth(reader)) {
      return 1;
    }
    int depth = xmlTextReaderDepth(reader);
    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      const xmlChar *name =  xmlTextReaderConstLocalName(reader);
      // printf("Name: %s\n", name);
      if (xmlStrEqual(BAD_CAST "high", name)) {
	const int ret = parse_high_low(ctxt, reader, 1);
	if (ret != 1) return ret;
      } else if (xmlStrEqual(BAD_CAST "low", name)) {
	const int ret = parse_high_low(ctxt, reader, 0);
	if (ret != 1) return ret;
      } else if (xmlStrEqual(BAD_CAST "send", name)) {
	const int ret = parse_send(ctxt, reader);
	if (ret != 1) return ret;
      } else if (xmlStrEqual(BAD_CAST "bit-seq", name)) {
	const int ret = parse_bit_seq(ctxt, reader);
	if (ret != 1) return ret;
      } else if (xmlStrEqual(BAD_CAST "check-high", name)) {
	const int ret = parse_check_high_low(ctxt, reader,1);
	if (ret != 1) return ret;
      } else if (xmlStrEqual(BAD_CAST "check-low", name)) {
	const int ret = parse_check_high_low(ctxt, reader,0);
	if (ret != 1) return ret;
      } else {
	eprintf("Unknown element %s\n", name);
	return -1;
      }
    }
    do {
      const int ret = xmlTextReaderRead(reader);
      if (ret != 1) return ret;
    } while(depth < xmlTextReaderDepth(reader));
    //printf("Next: %d\n", xmlTextReaderNodeType(reader));
}
  return ret;
}

static int 
parse_simulation(ParserContext *ctxt, xmlTextReaderPtr reader)
{
  if (!xmlStrEqual(BAD_CAST "simulation", xmlTextReaderConstLocalName(reader))) {
    eprintf("No simulation element\n");
    return 0;
  }
  int ret = parse_events(ctxt, reader);
  return ret;
}

int
sim_xml_parse(xmlTextReaderPtr reader, struct SimItem **events, struct CheckItem **checks)
{
  ParserContext ctxt;
  ctxt.current_ts = 0;
  ctxt.group_ts = 0;
  ctxt.stack_top = 0;
  ctxt.sequence = NULL;
  ctxt.sequence_tail = &ctxt.sequence;
  ctxt.check = NULL;
  ctxt.check_tail = &ctxt.check;
  while (1) {
    int ret = xmlTextReaderRead(reader);
    if (ret != 1) break;
    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_ELEMENT) {
      int ret = parse_simulation(&ctxt, reader);
      if (ret == -1) {
	sim_seq_destroy(ctxt.sequence);
      } else {
	*events = ctxt.sequence;
	*checks = ctxt.check;
      }
      return ret;
    }
  }
  return -1;
}
