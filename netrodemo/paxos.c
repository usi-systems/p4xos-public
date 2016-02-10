#include <pif_plugin.h>

#define MAX_INST 65535
#define MAJORITY 1
#define SWITCH_ID 0x12345678
__shared uint32_t cur_instance;
__shared uint32_t pkt_inst;
__declspec(mem export) uint32_t rnds[MAX_INST];
__declspec(mem export) uint32_t vrnds[MAX_INST];
__declspec(mem export) uint16_t valsizes[MAX_INST];
__declspec(mem export) uint16_t majorities[MAX_INST];
__declspec(mem export) uint32_t values[MAX_INST];

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

int pif_plugin_paxos_phase1a(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data)
{
    PIF_PLUGIN_paxos_T *paxos;
    PIF_PLUGIN_value_T *value;

    if (! pif_plugin_hdr_paxos_present(headers)) {
        return PIF_PLUGIN_RETURN_DROP;
    }

    if (! pif_plugin_hdr_value_present(headers)) {
        return PIF_PLUGIN_RETURN_DROP;
    }

    paxos = pif_plugin_hdr_get_paxos(headers);
    value = pif_plugin_hdr_get_value(headers);

    pkt_inst = paxos->inst;

    if (paxos->rnd < rnds[pkt_inst]) {
        return PIF_PLUGIN_RETURN_DROP;
    }
    rnds[pkt_inst]  = paxos->rnd;
    paxos->vrnd = vrnds[pkt_inst];
    paxos->valsize = valsizes[pkt_inst];
    paxos->acpt = SWITCH_ID;
    paxos->msgtype = Phase1B;
    value->content = values[pkt_inst];

    return PIF_PLUGIN_RETURN_FORWARD;
}

int pif_plugin_paxos_phase1b(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data)
{
    PIF_PLUGIN_paxos_T *paxos;
    PIF_PLUGIN_value_T *value;

    if (! pif_plugin_hdr_paxos_present(headers)) {
        return PIF_PLUGIN_RETURN_DROP;
    }

    if (! pif_plugin_hdr_value_present(headers)) {
        return PIF_PLUGIN_RETURN_DROP;
    }

    paxos = pif_plugin_hdr_get_paxos(headers);
    value = pif_plugin_hdr_get_value(headers);

    pkt_inst = paxos->inst;

    if (paxos->rnd != rnds[pkt_inst]) {
        return PIF_PLUGIN_RETURN_DROP;
    }

    if (paxos->vrnd > vrnds[pkt_inst]) {
        valsizes[pkt_inst] = paxos->valsize;
        values[pkt_inst] = value->content;
    }

    majorities[pkt_inst]++;
    // count < > MAJORITY: Sending Phase2A only when Reaching Majority, not after that.
    if (majorities[pkt_inst] != MAJORITY) {
        return PIF_PLUGIN_RETURN_DROP;
    }

    // Craft Phase2A message
    // inst and rnd are already equal to the ones stored in the switch state
    paxos->msgtype = Phase2A;
    paxos->valsize = valsizes[pkt_inst];
    value->content = values[pkt_inst];
    // Send phase2A message to acceptors
    return PIF_PLUGIN_RETURN_FORWARD;
}

int pif_plugin_paxos_phase2a(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data)
{
    PIF_PLUGIN_paxos_T *paxos;
    PIF_PLUGIN_value_T *value;

    if (! pif_plugin_hdr_paxos_present(headers)) {
        return PIF_PLUGIN_RETURN_DROP;
    }

    if (! pif_plugin_hdr_value_present(headers)) {
        return PIF_PLUGIN_RETURN_DROP;
    }

    paxos = pif_plugin_hdr_get_paxos(headers);
    value = pif_plugin_hdr_get_value(headers);

    pkt_inst = paxos->inst;

    if (paxos->rnd < rnds[pkt_inst]) {
        return PIF_PLUGIN_RETURN_DROP;
    }
    rnds[pkt_inst]  = paxos->rnd;
    vrnds[pkt_inst] = paxos->rnd;
    valsizes[pkt_inst] = paxos->valsize;
    paxos->acpt = SWITCH_ID;
    paxos->msgtype = Phase2B;
    values[pkt_inst] = value->content;

    return PIF_PLUGIN_RETURN_FORWARD;
}