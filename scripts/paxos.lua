-- create paxos protocol and its fields
p_paxos = Proto ("paxos","Paxos Protocol")
local f_msgtype = ProtoField.uint16("paxos.msgtype", "MsgType", base.HEX)
local f_instance = ProtoField.uint32("paxos.instance", "Instance", base.HEX)
local f_round = ProtoField.uint16("paxos.round", "Round", base.HEX)
local f_vround = ProtoField.uint16("paxos.vround", "VRound", base.HEX)
local f_datapath = ProtoField.uint16("paxos.datapath", "Datapath", base.HEX)
local f_value = ProtoField.string("paxos.value", "Value", FT_STRING)

p_paxos.fields = {f_instance, f_round, f_vround, f_datapath, f_msgtype, f_value}

-- paxos dissector function
function p_paxos.dissector (buf, pkt, root)
	-- validate packet length is adequate, otherwise quit
	if buf:len() == 0 then return end
	pkt.cols.protocol = p_paxos.name

	-- create subtree for paxos
	subtree = root:add(p_paxos, buf(0))
	-- add protocol fields to subtree
	subtree:add(f_msgtype, buf(0,2)):append_text(" [MsgType]")
	subtree:add(f_instance, buf(2,4)):append_text(" [Instance]")
	subtree:add(f_round, buf(6,2)):append_text(" [Round]")
	subtree:add(f_vround, buf(8,2)):append_text(" [VRound]")
	subtree:add(f_datapath, buf(10,2)):append_text(" [Datapath]")
	subtree:add(f_value, buf(12,32)):append_text(" [Value]")

	-- description of payload
	subtree:append_text("")
end

-- Initialization routine
function p_paxos.init()
end

-- register a chained dissector for port 8002
local udp_dissector_table = DissectorTable.get("udp.port")
dissector = udp_dissector_table:get_dissector(34952)
-- you can call dissector from function p_paxos.dissec
-- so that the previous dissector gets called
udp_dissector_table:add(34952, p_paxos)
