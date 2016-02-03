#include <pif_plugin.h>

// Set MAX_INST 10 for debugging.  MAX: 2^32 (Paxos inst width)
#define MAX_INST 10

#define SWITCH_ID 0x12345678
__shared uint32_t cur_instance;
__shared uint32_t pkt_inst;
__shared uint32_t rnds[MAX_INST];
__shared uint32_t vrnds[MAX_INST];
__shared uint32_t valsizes[MAX_INST];

/*
 * an array of type string to store the value (below).
 * The element size may be large, where to should store the values (SRAM, DRAM,...)?
 */
__shared uint32_t values[MAX_INST];

enum Paxos {
    Phase0 = 0,
    Phase1A,
    Phase1B,
    Phase2A,
    Phase2B
};

int pif_plugin_seq_func(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data)
{
    PIF_PLUGIN_paxos_T *paxos;

    if (! pif_plugin_hdr_paxos_present(headers)) {
        return PIF_PLUGIN_RETURN_DROP;
    }

    paxos = pif_plugin_hdr_get_paxos(headers);
    paxos->inst = cur_instance;
    paxos->msgtype = Phase2A;
    cur_instance++;
    return PIF_PLUGIN_RETURN_FORWARD;
}

/*
 * Can we get the remaining packet payload after all parsed headers?
 * For example, after parsing, we have ethernet, ipv4, paxos headers.
 * Can we extract the bytes after the paxos header ?
 */
int pif_plugin_paxos_func(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data)
{
    PIF_PLUGIN_paxos_T *paxos;
    PIF_PLUGIN_value_T *value;

    if (! pif_plugin_hdr_paxos_present(headers)) {
        return PIF_PLUGIN_RETURN_DROP;
    }

    paxos = pif_plugin_hdr_get_paxos(headers);

    pkt_inst = paxos->inst;

    switch (paxos->msgtype) {
        case Phase1A:
            // handle Phase1A
            if (paxos->rnd >= rnds[pkt_inst]) {
                rnds[pkt_inst]  = paxos->rnd;
                paxos->vrnd = vrnds[pkt_inst];
                paxos->valsize = valsizes[pkt_inst];
                paxos->acpt = SWITCH_ID;
                paxos->msgtype = Phase1B;
                /*
                 * How to attach a header (e.g, value header) to the packet ?
                 * add_header(value);
                 * Copy accepted value from switch memory to packet.
                 * value->content = values[pkt_inst];
                */
            }
            break;
        case Phase1B:
            // handle Phase1B
            break;
        case Phase2A:
            // handle Phase2A
            if (paxos->rnd >= rnds[pkt_inst]) {
                rnds[pkt_inst]  = paxos->rnd;
                vrnds[pkt_inst] = paxos->rnd;
                valsizes[pkt_inst] = paxos->valsize;
                paxos->acpt = SWITCH_ID;
                paxos->msgtype = Phase2B;
                if (! pif_plugin_hdr_value_present(headers)) {
                    return PIF_PLUGIN_RETURN_DROP;
                }
                value = pif_plugin_hdr_get_value(headers);
                values[pkt_inst] = value->content;
            }
            break;
        case Phase2B:
            // handle Phase2B
            break;
        default:
            break;
        }

    return PIF_PLUGIN_RETURN_FORWARD;
}