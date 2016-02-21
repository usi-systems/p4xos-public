#include "includes/paxos_headers.p4"
#include "includes/paxos_parser.p4"


register instance_register {
    width : INSTANCE_SIZE;
    instance_count : 1;
}


action _nop() {

}

//  This function read num_inst stored in the register and copy it to
//  the current packet. Then it increased the num_inst by 1, and write
//  it back to the register
action increase_instance() {
    register_read(paxos.instance, instance_register, 0);
    add_to_field(paxos.instance, 1);
    register_write(instance_register, 0, paxos.instance);
}

table sequence_tbl {
    reads   { paxos.msgtype : exact; }
    actions { increase_instance; _nop; }
    size : 1;
}
