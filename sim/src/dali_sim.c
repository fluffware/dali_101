#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sim_xml.h>
#include <vcd_seq.h>
#include <simulate.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <libxml/xmlreader.h>

#define eprintf(...) fprintf(stderr, __VA_ARGS__)


static struct SimItem *
read_sim_file(const char *filename)
{
  struct SimItem *seq = NULL;
  struct CheckItem *checks = NULL;
  //sim_parser_init(&parser);
  xmlTextReaderPtr reader = xmlReaderForFile(filename, NULL, XML_PARSE_NONET);
  if (!reader) {
    eprintf("Failed to open file %s: %s\n", filename, strerror(errno));
    return NULL;
  }
  if (sim_xml_parse(reader, &seq, &checks) == -1) {
    eprintf("Failed to parse file %s\n", filename);
    return NULL;
  }
  sim_seq_sort(&seq);
  check_seq_sort(&checks);
  return seq;
}

static
void usage()
{
  eprintf("dali_sim [options] <simulation file> ...\n");
  eprintf("Options:\n"
	  "-w <wcd file or dir>     Write resulting waveforms to file\n"
	  );
}
	  
int
main(int argc, char * const *argv)
{
  int opt;
  const char *vcd_dest = NULL;
  while((opt = getopt(argc, argv, "w:")) != -1) {
    switch(opt) {
    case 'w':
      vcd_dest = optarg;
      break;
    default:
      usage();
      return EXIT_FAILURE;
    }
  }

  int vcd_dir = 0;
  int arg_pos = optind;
  if (vcd_dest) {
    struct stat statbuf;
    if (stat(vcd_dest, &statbuf) != 0) {
      eprintf("Failed to stat file %s: %s", vcd_dest, strerror(errno));
      return EXIT_FAILURE;
    }
    if ((statbuf.st_mode & S_IFMT) == S_IFDIR) {
      vcd_dir = 1;
    } else {
      if (arg_pos + 1 < argc) { // More than one simulation file
	eprintf("Argument for -w must be a directory"
		" if more than one simulation file is supplied\n");
	return EXIT_FAILURE;
      }
    }
  }
  while(arg_pos < argc) {
    const char *sim_file = argv[arg_pos];
    struct SimItem *sim_input = read_sim_file(sim_file);
    if (!sim_input) {
      return EXIT_FAILURE;
    }
    //sim_seq_dump(sim_input);
    struct SimItem *sim_res = simulate(sim_input, INT64_MAX);
    sim_seq_destroy(sim_input);
    char *vcd_file = NULL;
    if (vcd_dest) {
      if (vcd_dir) {
	char *file = strdup(sim_file);
	char *base = basename(file);
	char *ext_start = rindex(base, '.');
	if (ext_start) *ext_start = '\0';
	int dest_len = strlen(vcd_dest);
	int base_len = strlen(base);
	vcd_file = malloc(dest_len + base_len + (1+5));
	strcpy(vcd_file, vcd_dest);
	char *p = vcd_file + dest_len;
	if (p[-1] != '/') {
	  *p++ = '/';
	}
	strcpy(p, base);
	p += base_len;
	strcpy(p, ".vcd");
	free(file);
      } else {
	vcd_file = strdup(vcd_dest);
      }
      
      FILE *vcd = fopen(vcd_file,"w");
      if (!vcd) {
	eprintf("Failed to open file %s: %s\n", vcd_file, strerror(errno));
	free(vcd_file);
	return EXIT_FAILURE;
      }
      free(vcd_file);
      vcd_write_prolog(vcd);
      vcd_write_seq(vcd, sim_res);
      fclose(vcd);
    }
    sim_seq_destroy(sim_res);
    arg_pos++;
  }
  return EXIT_SUCCESS;
}
