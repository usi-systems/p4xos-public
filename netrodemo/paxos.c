#include <pif_plugin.h>
#include <nfp.h>

#define MAX_INST 400000
#define SWITCH_ID 0x0001
__declspec(mem export) uint32_t cur_instance;
__declspec(mem export) uint32_t pkt_inst;
__declspec(mem export) uint16_t rnds[MAX_INST];
__declspec(mem export) uint16_t vrnds[MAX_INST];
__declspec(mem export) uint32_t values[MAX_INST];

__gpr uint32_t timestamp_high, timestamp_low;

enum Paxos {
    Phase0 = 0,
    Phase1A,
    Phase1B,
    Phase2A,
    Phase2B
};

int pif_plugin_get_forwarding_start_time(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data)
{
    PIF_PLUGIN_paxos_T *paxos;
    timestamp_low = local_csr_read(local_csr_timestamp_low);
    timestamp_high = local_csr_read(local_csr_timestamp_high);

    if (! pif_plugin_hdr_paxos_present(headers)) {
        return PIF_PLUGIN_RETURN_FORWARD;
    }

    paxos = pif_plugin_hdr_get_paxos(headers);

    paxos->fsl = timestamp_low;
    paxos->fsh = timestamp_high;
    return PIF_PLUGIN_RETURN_FORWARD;
}

int pif_plugin_get_forwarding_end_time(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data)
{
    PIF_PLUGIN_paxos_T *paxos;
    timestamp_low = local_csr_read(local_csr_timestamp_low);
    timestamp_high = local_csr_read(local_csr_timestamp_high);

    if (! pif_plugin_hdr_paxos_present(headers)) {
        return PIF_PLUGIN_RETURN_FORWARD;
    }

    paxos = pif_plugin_hdr_get_paxos(headers);

    paxos->fel = timestamp_low;
    paxos->feh = timestamp_high;
    return PIF_PLUGIN_RETURN_FORWARD;
}

int pif_plugin_get_coordinator_start_time(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data)
{
    PIF_PLUGIN_paxos_T *paxos;
    timestamp_low = local_csr_read(local_csr_timestamp_low);
    timestamp_high = local_csr_read(local_csr_timestamp_high);

    if (! pif_plugin_hdr_paxos_present(headers)) {
        return PIF_PLUGIN_RETURN_DROP;
    }

    paxos = pif_plugin_hdr_get_paxos(headers);

    paxos->csl = timestamp_low;
    paxos->csh = timestamp_high;
    return PIF_PLUGIN_RETURN_FORWARD;
}

int pif_plugin_get_coordinator_end_time(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data)
{
    PIF_PLUGIN_paxos_T *paxos;
    timestamp_low = local_csr_read(local_csr_timestamp_low);
    timestamp_high = local_csr_read(local_csr_timestamp_high);

    if (! pif_plugin_hdr_paxos_present(headers)) {
        return PIF_PLUGIN_RETURN_DROP;
    }

    paxos = pif_plugin_hdr_get_paxos(headers);

    paxos->cel = timestamp_low;
    paxos->ceh = timestamp_high;
    return PIF_PLUGIN_RETURN_FORWARD;
}

int pif_plugin_get_acceptor_start_time(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data)
{
    PIF_PLUGIN_paxos_T *paxos;
    timestamp_low = local_csr_read(local_csr_timestamp_low);
    timestamp_high = local_csr_read(local_csr_timestamp_high);

    if (! pif_plugin_hdr_paxos_present(headers)) {
        return PIF_PLUGIN_RETURN_DROP;
    }

    paxos = pif_plugin_hdr_get_paxos(headers);

    paxos->asl = timestamp_low;
    paxos->ash = timestamp_high;
    return PIF_PLUGIN_RETURN_FORWARD;
}

int pif_plugin_get_acceptor_end_time(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data)
{
    PIF_PLUGIN_paxos_T *paxos;
    timestamp_low = local_csr_read(local_csr_timestamp_low);
    timestamp_high = local_csr_read(local_csr_timestamp_high);

    if (! pif_plugin_hdr_paxos_present(headers)) {
        return PIF_PLUGIN_RETURN_DROP;
    }

    paxos = pif_plugin_hdr_get_paxos(headers);

    paxos->ael = timestamp_low;
    paxos->aeh = timestamp_high;
    return PIF_PLUGIN_RETURN_FORWARD;
}


int pif_plugin_reset_registers(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data)
{
    int i;
    cur_instance = 0;
    for (i = 0; i < MAX_INST; i++) {
        rnds[i] = 0;
        vrnds[i] = 0;
        values[i] = 0;
    }
    return PIF_PLUGIN_RETURN_FORWARD;
}


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

    if (! pif_plugin_hdr_paxos_present(headers)) {
        return PIF_PLUGIN_RETURN_DROP;
    }

    paxos = pif_plugin_hdr_get_paxos(headers);

    pkt_inst = paxos->inst;

    if (paxos->rnd < rnds[pkt_inst]) {
        return PIF_PLUGIN_RETURN_DROP;
    }
    rnds[pkt_inst]  = paxos->rnd;
    paxos->vrnd = vrnds[pkt_inst];
    paxos->acpt = SWITCH_ID;
    paxos->msgtype = Phase1B;
    paxos->val = values[pkt_inst];

    return PIF_PLUGIN_RETURN_FORWARD;
}


int pif_plugin_paxos_phase2a(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data)
{
    PIF_PLUGIN_paxos_T *paxos;

    if (! pif_plugin_hdr_paxos_present(headers)) {
        return PIF_PLUGIN_RETURN_DROP;
    }

    paxos = pif_plugin_hdr_get_paxos(headers);

    pkt_inst = paxos->inst;

    if (paxos->rnd < rnds[pkt_inst]) {
        return PIF_PLUGIN_RETURN_DROP;
    }
    rnds[pkt_inst]  = paxos->rnd;
    vrnds[pkt_inst] = paxos->rnd;
    values[pkt_inst] = paxos->val;
    paxos->acpt = SWITCH_ID;
    paxos->msgtype = Phase2B;

    return PIF_PLUGIN_RETURN_FORWARD;
}