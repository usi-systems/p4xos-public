// Parser for paxos packet headers

parser parse_paxos {
    extract(paxos);
    return ingress;
}
