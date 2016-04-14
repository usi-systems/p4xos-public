#!/usr/bin/env python

from ryu.base import app_manager
from ryu.controller.handler import MAIN_DISPATCHER
from ryu.controller.handler import set_ev_cls
from ryu.ofproto import ofproto_v1_3
from ryu.lib import addrconv
from ryu.lib import igmplib
from ryu.lib.dpid import str_to_dpid
from ryu.lib.packet import packet
from ryu.lib.packet import ethernet
from ryu.controller import ofp_event
from ryu.controller.handler import CONFIG_DISPATCHER
import signal
import sys

def signal_handler(signal, frame):
    print 'You pressed Ctrl+C!'
    sys.exit(0)

class SimpleSwitchIgmp(app_manager.RyuApp):
    OFP_VERSIONS = [ofproto_v1_3.OFP_VERSION]
    _CONTEXTS = {'igmplib': igmplib.IgmpLib}

    def __init__(self, *args, **kwargs):
        super(SimpleSwitchIgmp, self).__init__(*args, **kwargs)
        self.mac_to_port = {}
        signal.signal(signal.SIGINT, signal_handler)


    def get_match(self, in_port, ipv4_dst):
        return parser.OFPMatch(in_port=in_port, ipv4_dst=ipv4_dst)

    def send_group_del(self, datapath):
        ofp = datapath.ofproto
        ofp_parser = datapath.ofproto_parser
        req = ofp_parser.OFPGroupMod(datapath, ofp.OFPGC_DELETE,
                                     ofp.OFPGT_FF, ofp.OFPG_ALL, None)
        datapath.send_msg(req)


    def send_group_mod(self, datapath):
        ofp = datapath.ofproto
        ofp_parser = datapath.ofproto_parser

        ports = [2, 3]
        buckets = []
        for port in ports:
            max_len = 65535
            actions = [ofp_parser.OFPActionOutput(port, max_len)]

            weight = 0
            watch_port = port
            watch_group = 0
            buckets.append(ofp_parser.OFPBucket(weight, watch_port, watch_group,
                                            actions))

        group_id = 1
        req = ofp_parser.OFPGroupMod(datapath, ofp.OFPGC_ADD,
                                     ofp.OFPGT_FF, group_id, buckets)
        datapath.send_msg(req)


    def add_flow(self, datapath, priority, match, actions, buffer_id=None):
        ofproto = datapath.ofproto
        parser = datapath.ofproto_parser

        inst = [parser.OFPInstructionActions(ofproto.OFPIT_APPLY_ACTIONS,
                                             actions)]
        if buffer_id:
            mod = parser.OFPFlowMod(datapath=datapath, buffer_id=buffer_id,
                                    priority=priority, match=match,
                                    instructions=inst)
        else:
            mod = parser.OFPFlowMod(datapath=datapath, priority=priority,
                                    match=match, instructions=inst)
        datapath.send_msg(mod)

    @set_ev_cls(ofp_event.EventOFPSwitchFeatures, CONFIG_DISPATCHER)
    def switch_features_handler(self, ev):
        msg = ev.msg
        self.logger.info('OFPSwitchFeatures received: '
                          'datapath_id=0x%016x n_buffers=%d '
                          'n_tables=%d auxiliary_id=%d '
                          'capabilities=0x%08x',
                            msg.datapath_id, msg.n_buffers, msg.n_tables,
                            msg.auxiliary_id, msg.capabilities) 
        self.send_group_del(msg.datapath)
        self.send_group_mod(msg.datapath)


    @set_ev_cls(ofp_event.EventOFPPacketIn, MAIN_DISPATCHER)
    def _packet_in_handler(self, ev):
        msg = ev.msg
        datapath = msg.datapath
        ofproto = datapath.ofproto

        pkt = packet.Packet(msg.data)
        eth = pkt.get_protocols(ethernet.ethernet)[0]

        dst = eth.dst
        src = eth.src

        dpid = datapath.id
        self.mac_to_port.setdefault(dpid, {})
        in_port = msg.match['in_port']
        parser = datapath.ofproto_parser

        # learn a mac address to avoid FLOOD next time.
        self.mac_to_port[dpid][src] = in_port

        if dst in self.mac_to_port[dpid]:
            out_port = self.mac_to_port[dpid][dst]
        else:
            out_port = ofproto.OFPP_FLOOD

        actions = [datapath.ofproto_parser.OFPActionOutput(out_port)]

        # install a flow to avoid packet_in next time
        if out_port != ofproto.OFPP_FLOOD:
            match = parser.OFPMatch(in_port=in_port, eth_dst=dst)
            self.add_flow(datapath, 1, match, actions)

        out = datapath.ofproto_parser.OFPPacketOut(
            datapath=datapath, buffer_id=msg.buffer_id, in_port=in_port,
            actions=actions)
        datapath.send_msg(out)

    @set_ev_cls(igmplib.EventMulticastGroupStateChanged,
               MAIN_DISPATCHER)
    def _status_changed(self, ev):
        msg = {
            igmplib.MG_GROUP_ADDED: 'Multicast Group Added',
            igmplib.MG_MEMBER_CHANGED: 'Multicast Group Member Changed',
            igmplib.MG_GROUP_REMOVED: 'Multicast Group Removed',
        }
        self.logger.info("%s: [%s] querier:[%s] hosts:%s",
                         msg.get(ev.reason), ev.address, ev.src,
                         ev.dsts)