CFLAGS= -Wall -g -pedantic -I../../include -I../../src $(XML_CPPFLAGS) $(COVERAGE_FLAGS) -DTICKS_PER_SEC=1000000L

bin_PROGRAMS = dali_sim
dali_sim_SOURCES = dali_sim.c sim_xml.c sim_seq.c check_seq.c list.c vcd_seq.c simulate.c msg.c ../../src/dali_101.c
dali_sim_LDADD= $(XML_LIBS)
