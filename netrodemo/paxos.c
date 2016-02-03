#include <pif_plugin.h>

// Set MAX_INST 10 for debugging.  MAX: 2^32 (Paxos inst width)
#define MAX_INST 10

#define SWITCH_ID 0x12345678
__shared uint32_t cur_instance;
__shared uint32_t pkt_inst;
__shared uint32_t rnds[MAX_INST];
__shared uint32_t vrnds[MAX_INST];
// TODO declare an array of type string to store the value
// e.g __shared string vals[MAX_INST]
__shared uint32_t vals[MAX_INST];

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
    cur_instance++;
    paxos->inst = cur_instance;
    paxos->msgtype = Phase2A;

    return PIF_PLUGIN_RETURN_FORWARD;
}


int pif_plugin_paxos_func(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data)
{
    PIF_PLUGIN_paxos_T *paxos;

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
                paxos->val = vals[pkt_inst];
                paxos->acpt = SWITCH_ID;
                paxos->msgtype = Phase1B;
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
                vals[pkt_inst] = paxos->val;
                paxos->acpt = SWITCH_ID;
                paxos->msgtype = Phase2B;
            }
            break;
        case Phase2B:
            // handle Phase2B
            break;
        default:
            break;
        } // end switch

    return PIF_PLUGIN_RETURN_FORWARD;
}