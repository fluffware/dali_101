#include <vcd_seq.h>
#include <time.h>
#include <dali_101.h>
#include <inttypes.h>


void
vcd_write_prolog(FILE *f)
{
  time_t now; 
  struct tm date_time;
  char date_buffer[50];
  time(&now);
  localtime_r(&now, &date_time);
  strftime(date_buffer, sizeof(date_buffer), "%F %T", &date_time);
  fprintf(f,"$date\n%s\n$end\n", date_buffer);
  fputs("$timescale 1ns $end\n"
	"$scope sim $end\n"
	"$var wire 1 B bus $end\n"
	"$var wire 1 O output $end\n"
	"$var wire 1 E external $end\n"
	"$var event 1 S send $end\n"
	"$var event 1 r result_recv $end\n"
	"$var event 1 s result_send $end\n"
	"$upscope $end\n"
	"$enddefinitions $end\n"
	"$dumpvars\n"
	"1E\n"
	"1B\n"
	"1O\n"
	"$end\n"
	,f);
}

void
vcd_write_seq(FILE *f, struct SimItem *seq)
{
  int64_t last_ts = INT64_MIN;
  while(seq) {
    if (seq->type >= ExternalHigh && seq->type <= ResultRecv) {
      if (last_ts != seq->ts) {
	fprintf(f, ("#%" PRId64 "\n"), seq->ts * (1000000000 / TICKS_PER_SEC));
	last_ts = seq->ts;
      }
      switch(seq->type) {
      case ExternalLow:
	fputs("0E\n",f);
	break;
      case ExternalHigh:
	fputs("1E\n",f);
	break;
      case OutLow:
	fputs("0O\n",f);
	break;
      case OutHigh:
	fputs("1O\n",f);
	break;
      case BusLow:
	fputs("0B\n",f);
	break;
      case BusHigh:
	fputs("1B\n",f);
	break;
      case Send:
	fputs("1S\n",f);
	break;
      case ResultSend:
	fputs("1s\n",f);
	break;
      case ResultRecv:
	fputs("1r\n",f);
	break;
      default:
	break;
      }
    }
    seq = seq->next;
  }
}

