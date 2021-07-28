#ifndef __VCD_SEQ_H__PE1I3K1502__
#define __VCD_SEQ_H__PE1I3K1502__
#include <sim_seq.h>
#include <stdio.h>

void
vcd_write_prolog(FILE *f);

void
vcd_write_seq(FILE *f, struct SimItem *seq);

#endif /* __VCD_SEQ_H__PE1I3K1502__ */
