#ifndef __SIM_XML_H__LX8V0XNS0P__
#define __SIM_XML_H__LX8V0XNS0P__

#include <libxml/xmlreader.h>
#include <sim_seq.h>

int
sim_xml_parse(xmlTextReaderPtr reader, struct SimItem **events);

#endif /* __SIM_XML_H__LX8V0XNS0P__ */
