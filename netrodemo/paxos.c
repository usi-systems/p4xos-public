#include <pif_plugin.h>

__shared uint32_t cur_instance;

int pif_plugin_paxos_func(EXTRACTED_HEADERS_T *headers, MATCH_DATA_T *data)
{
    PIF_PLUGIN_paxos_T *paxos;

    if (! pif_plugin_hdr_paxos_present(headers)) {
        return PIF_PLUGIN_RETURN_DROP;
    }

    paxos = pif_plugin_hdr_get_paxos(headers);
    cur_instance++;
    paxos->instance = cur_instance;
    return PIF_PLUGIN_RETURN_FORWARD;
}