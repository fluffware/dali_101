#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dali_101.h>
#include <sim_seq.h>

#define MAX_STACK 20
struct ParserContext
{
  int64_t current_ts;
  int64_t group_stack[MAX_STACK];
  int stack_top;
  struct SimItem *sequence;
  struct SimItem **sequence_tail;
};

void
sim_parser_init(struct ParserContext *ctxt);

int
sim_parser_parse(struct ParserContext *ctxt, const char **pp);
