#ifndef _ROUTING_HEADER_H_
#define _ROUTING_HEADER_H_

header_type routing_metadata_t {
   fields {
       ipv4Length : 16;
       udpLength : 16;
   }
}

metadata routing_metadata_t routing_metadata;

#endif
