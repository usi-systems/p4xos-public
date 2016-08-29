#include "controller.h"
#include "messages.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

// DEFAULT PORT
#define DEF_PORT 0

// NETPAXOS CONTROLLER
controller c;

///////////////////////////////////////////////////////////////////////////////////////////////////
///                         DIGEST HANDLER FUNCTION
///////////////////////////////////////////////////////////////////////////////////////////////////

void dhf(void* b) {
    struct p4_header* h = netconv_p4_header(unpack_p4_header(b, 0));
    if (h->type != P4T_DIGEST) {
        printf("Method is not implemented\n");
        return;
    }

    struct p4_digest* d = unpack_p4_digest(b,0);
    printf("Unknown digest received: X%sX\n", d->field_list_name);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///                                 NETPAXOS FILL TABLES:
///////////////////////////////////////////////////////////////////////////////////////////////////

/* FORWARD TABLE */

// ENUM
typedef enum {FORWARD_TBL_DROP, FORWARD_TBL_FORWARD} forward_tbl_action;

// FILL FUNCTION
void fill_forward_tbl(forward_tbl_action taction, uint8_t ingress_port, uint8_t fwd_port)
{
    char buffer[2048];
    struct p4_header* h;
    struct p4_add_table_entry* te;
    struct p4_action* a;
    struct p4_action_parameter* ap;
    struct p4_field_match_exact* exact;

    printf("Fill table forward_tbl...\n");

    h = create_p4_header(buffer, 0, 2048);

    // 1. Set the table name in which you want to insert a new entry
    te = create_p4_add_table_entry(buffer,0,2048);
    strcpy(te->table_name, "forward_tbl");

    // 2. According to the match type, fill the key values.
    // If there are multiple "reads" in the table,
    // further keys should be added similarly
    exact = add_p4_field_match_exact(te, 2048);
    strcpy(exact->header.name, "standard_metadata.ingress_port");
    memcpy(exact->bitmap, &ingress_port, 1);
    exact->length = 1*8+0;

    // 3. Define the action for the match rules added previously
    a = add_p4_action(h, 2048);

    switch(taction) {
	    case FORWARD_TBL_DROP:
		// 4. Fill the action name and parameters. Drop has no params.
		strcpy(a->description.name, "_drop");
		break;
	    case FORWARD_TBL_FORWARD:
		// 4. Fill the action name and parameters.
		strcpy(a->description.name, "forward");
		// 4b. Add action parameters. 
		ap = add_p4_action_parameter(h, a, 2048);   
		strcpy(ap->name, "port");
		memcpy(ap->bitmap, &fwd_port, 1);
		ap->length = 1*8+0;
    }

    // 5. Byte order conversions
    netconv_p4_header(h);
    netconv_p4_add_table_entry(te);
    netconv_p4_field_match_exact(exact);
    netconv_p4_action(a);
    if (taction == FORWARD_TBL_FORWARD) netconv_p4_action_parameter(ap);

    // 6. Sending the message to the switch
    send_p4_msg(c, buffer, 2048);
}

/* IGMP TABLE */

// ENUM
typedef enum {IGMP_TBL_HANDLE_QUERY} igmp_tbl_action;

// FILL FUNCTION
void fill_igmp_tbl(igmp_tbl_action taction, uint8_t ingress_port, uint8_t fwd_port)
{
    char buffer[2048];
    struct p4_header* h;
    struct p4_add_table_entry* te;
    struct p4_action* a;
    struct p4_action_parameter* ap;
    struct p4_field_match_exact* exact;

    printf("Fill table forward_tbl...\n");

    h = create_p4_header(buffer, 0, 2048);

    // 1. Set the table name in which you want to insert a new entry
    te = create_p4_add_table_entry(buffer,0,2048);
    strcpy(te->table_name, "forward_tbl");

    // 2. According to the match type, fill the key values.
    // If there are multiple "reads" in the table,
    // further keys should be added similarly
    exact = add_p4_field_match_exact(te, 2048);
    strcpy(exact->header.name, "standard_metadata.ingress_port");
    memcpy(exact->bitmap, &ingress_port, 1);
    exact->length = 1*8+0;

    // 3. Define the action for the match rules added previously
    a = add_p4_action(h, 2048);

    switch(taction) {
        case FORWARD_TBL_DROP:
        // 4. Fill the action name and parameters. Drop has no params.
        strcpy(a->description.name, "_drop");
        break;
        case FORWARD_TBL_FORWARD:
        // 4. Fill the action name and parameters.
        strcpy(a->description.name, "forward");
        // 4b. Add action parameters. 
        ap = add_p4_action_parameter(h, a, 2048);   
        strcpy(ap->name, "port");
        memcpy(ap->bitmap, &fwd_port, 1);
        ap->length = 1*8+0;
    }

    // 5. Byte order conversions
    netconv_p4_header(h);
    netconv_p4_add_table_entry(te);
    netconv_p4_field_match_exact(exact);
    netconv_p4_action(a);
    if (taction == FORWARD_TBL_FORWARD) netconv_p4_action_parameter(ap);

    // 6. Sending the message to the switch
    send_p4_msg(c, buffer, 2048);
}

/*

// DROP_TBL
void fill_drop_tbl(uint8_t port, uint8_t mac[6])

// ROUND_TBL
void fill_round_tbl(uint8_t port, uint8_t mac[6])

// ACCEPTOR_TBL
void fill_acceptor_tbl(uint8_t port, uint8_t mac[6])

// SEQUENCE_TBL
void fill_sequence_tbl(uint8_t port, uint8_t mac[6])

*/

///////////////////////////////////////////////////////////////////////////////////////////////////
///                                 NETPAXOS DEFAULT ACTIONS:
///////////////////////////////////////////////////////////////////////////////////////////////////

// FORWARD_TBL --> forward
void set_default_action_forward_tbl()
{
    char buffer[2048];
    struct p4_header* h;
    struct p4_set_default_action* sda;
    struct p4_action* a;
    struct p4_action_parameter* ap;

    // UPDATE PORT NUMBER IF NEEDED
    uint8_t default_port = DEF_PORT;

    printf("Generate set_default_action message for table NetPaxos-->forward_tbl\n");
    
    h = create_p4_header(buffer, 0, sizeof(buffer));

    // 1. Set the table name
    sda = create_p4_set_default_action(buffer,0,sizeof(buffer));
    strcpy(sda->table_name, "forward_tbl");

    // 2. Set the name of default action
    a = &(sda->action);
    strcpy(a->description.name, "forward");

    // 3. Add the parameters (for more than one parameters this section should be repeated...)
    ap = add_p4_action_parameter(h, a, 2048);
    strcpy(ap->name, "port");
    memcpy(ap->bitmap, &default_port, 1);
    ap->length = 1*8+0;

    // 4. Byte order conversions
    netconv_p4_header(h);
    netconv_p4_set_default_action(sda);
    netconv_p4_action(a);
    netconv_p4_action_parameter(ap);

    // 5. Sending the message
    send_p4_msg(c, buffer, sizeof(buffer));
}

// IGMP_TBL --> handle_query
void set_default_action_igmp_tbl()
{
    char buffer[2048];
    struct p4_header* h;
    struct p4_set_default_action* sda;
    struct p4_action* a;
    struct p4_action_parameter* ap;

    // UPDATE PORT NUMBER IF NEEDED
    uint8_t default_port = DEF_PORT;

    printf("Generate set_default_action message for table NetPaxos-->igmp_tbl\n");
    
    h = create_p4_header(buffer, 0, sizeof(buffer));

    // 1. Set the table name
    sda = create_p4_set_default_action(buffer,0,sizeof(buffer));
    strcpy(sda->table_name, "igmp_tbl");

    // 2. Set the name of default action
    a = &(sda->action);
    strcpy(a->description.name, "handle_query");

    // 3. Add the parameters (for more than one parameters this section should be repeated...)
    ap = add_p4_action_parameter(h, a, 2048);
    strcpy(ap->name, "port");
    memcpy(ap->bitmap, &default_port, 1);
    ap->length = 1*8+0;

    // 4. Byte order conversions
    netconv_p4_header(h);
    netconv_p4_set_default_action(sda);
    netconv_p4_action(a);
    netconv_p4_action_parameter(ap);

    // 5. Sending the message
    send_p4_msg(c, buffer, sizeof(buffer));
}

// TODO: check if drop action works
// DROP_TBL --> _drop
void set_default_action_drop_tbl()
{
    char buffer[2048];
    struct p4_header* h;
    struct p4_set_default_action* sda;
    struct p4_action* a;
    struct p4_action_parameter* ap;

    // UPDATE PORT NUMBER IF NEEDED
    uint8_t default_port = DEF_PORT;

    printf("Generate set_default_action message for table NetPaxos-->drop_tbl\n");
    
    h = create_p4_header(buffer, 0, sizeof(buffer));

    // 1. Set the table name
    sda = create_p4_set_default_action(buffer,0,sizeof(buffer));
    strcpy(sda->table_name, "drop_tbl");

    // 2. Set the name of default action
    a = &(sda->action);
    strcpy(a->description.name, "_drop");

    // 3. Add the parameters (for more than one parameters this section should be repeated...)
    ap = add_p4_action_parameter(h, a, 2048);
    strcpy(ap->name, "port");
    memcpy(ap->bitmap, &default_port, 1);
    ap->length = 1*8+0;

    // 4. Byte order conversions
    netconv_p4_header(h);
    netconv_p4_set_default_action(sda);
    netconv_p4_action(a);
    netconv_p4_action_parameter(ap);

    // 5. Sending the message
    send_p4_msg(c, buffer, sizeof(buffer));
}

// ROUND_TBL --> read_round
void set_default_action_round_tbl()
{
    char buffer[2048];
    struct p4_header* h;
    struct p4_set_default_action* sda;
    struct p4_action* a;
    struct p4_action_parameter* ap;

    // UPDATE PORT NUMBER IF NEEDED
    uint8_t default_port = DEF_PORT;

    printf("Generate set_default_action message for table NetPaxos-->round_tbl\n");
    
    h = create_p4_header(buffer, 0, sizeof(buffer));

    // 1. Set the table name
    sda = create_p4_set_default_action(buffer,0,sizeof(buffer));
    strcpy(sda->table_name, "round_tbl");

    // 2. Set the name of default action
    a = &(sda->action);
    strcpy(a->description.name, "read_round");

    // 3. Add the parameters (for more than one parameters this section should be repeated...)
    ap = add_p4_action_parameter(h, a, 2048);
    strcpy(ap->name, "port");
    memcpy(ap->bitmap, &default_port, 1);
    ap->length = 1*8+0;

    // 4. Byte order conversions
    netconv_p4_header(h);
    netconv_p4_set_default_action(sda);
    netconv_p4_action(a);
    netconv_p4_action_parameter(ap);

    // 5. Sending the message
    send_p4_msg(c, buffer, sizeof(buffer));
}

// ACCEPTOR_TBL --> handle_2a
void set_default_action_acceptor_tbl()
{
    char buffer[2048];
    struct p4_header* h;
    struct p4_set_default_action* sda;
    struct p4_action* a;
    struct p4_action_parameter* ap;

    // UPDATE PORT NUMBER IF NEEDED
    uint8_t default_port = DEF_PORT;

    printf("Generate set_default_action message for table NetPaxos-->acceptor_tbl\n");
    
    h = create_p4_header(buffer, 0, sizeof(buffer));

    // 1. Set the table name
    sda = create_p4_set_default_action(buffer,0,sizeof(buffer));
    strcpy(sda->table_name, "acceptor_tbl");

    // 2. Set the name of default action
    a = &(sda->action);
    strcpy(a->description.name, "handle_2a");

    // 3. Add the parameters (for more than one parameters this section should be repeated...)
    ap = add_p4_action_parameter(h, a, 2048);
    strcpy(ap->name, "port");
    memcpy(ap->bitmap, &default_port, 1);
    ap->length = 1*8+0;

    // 4. Byte order conversions
    netconv_p4_header(h);
    netconv_p4_set_default_action(sda);
    netconv_p4_action(a);
    netconv_p4_action_parameter(ap);

    // 5. Sending the message
    send_p4_msg(c, buffer, sizeof(buffer));
}

// SEQUENCE_TBL --> increase_instance
void set_default_action_sequence_tbl()
{
    char buffer[2048];
    struct p4_header* h;
    struct p4_set_default_action* sda;
    struct p4_action* a;
    struct p4_action_parameter* ap;

    // UPDATE PORT NUMBER IF NEEDED
    uint8_t default_port = DEF_PORT;

    printf("Generate set_default_action message for table NetPaxos-->sequence_tbl\n");
    
    h = create_p4_header(buffer, 0, sizeof(buffer));

    // 1. Set the table name
    sda = create_p4_set_default_action(buffer,0,sizeof(buffer));
    strcpy(sda->table_name, "sequence_tbl");

    // 2. Set the name of default action
    a = &(sda->action);
    strcpy(a->description.name, "increase_instance");

    // 3. Add the parameters (for more than one parameters this section should be repeated...)
    ap = add_p4_action_parameter(h, a, 2048);
    strcpy(ap->name, "port");
    memcpy(ap->bitmap, &default_port, 1);
    ap->length = 1*8+0;

    // 4. Byte order conversions
    netconv_p4_header(h);
    netconv_p4_set_default_action(sda);
    netconv_p4_action(a);
    netconv_p4_action_parameter(ap);

    // 5. Sending the message
    send_p4_msg(c, buffer, sizeof(buffer));
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///                             INIT FUNCTION
///////////////////////////////////////////////////////////////////////////////////////////////////




void init() {
    printf("Set default actions.\n");

    // SET DEFAULT ACTIONS
    set_default_action_forward_tbl(); //forward
    set_default_action_igmp_tbl; //igmp
    set_default_action_drop_tbl; //drop
    set_default_action_round_tbl; //round
    set_default_action_acceptor_tbl; //acceptor
    set_default_action_sequence_tbl; //sequence

    // FILL TABLES
    fill_forward_tbl(FORWARD_TBL_FORWARD, 0, 1); //ingress port=0; port parameter of action is 1
    fill_igmp_tbl(IGMP_TBL_HANDLE_QUERY, 0, 1); //ingress port=0; port parameter of action is 1
    fill_drop_tbl(DROP_TBL_DROP, 0, 1); //ingress port=0; port parameter of action is 1
    fill_round_tbl(ROUND_TBL_READ_ROUND, 0, 1); //ingress port=0; port parameter of action is 1
    fill_acceptor_tbl(ACCEPTOR_TBL_HANDLE_2A, 0, 1); //ingress port=0; port parameter of action is 1
    fill_sequence_tbl(SEQUENCE_TBL_INCREASE_INSTANCE, 0, 1); //ingress port=0; port parameter of action is 1
    
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///                             MAIN FUNCTION
///////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) 
{
        if (argc>1) {
                        printf("Too many arguments...\nUsage: %s <filename(optional)>\n", argv[0]);
                        return -1;
        }

    printf("Create and configure NetPaxos controller...\n");
    c = create_controller_with_init(11111, 3, dhf, init);

    printf("Launching controller's main loop...\n");
    execute_controller(c);

    printf("Destroy NetPaxos controller\n");
    destroy_controller(c);

    return 0;
}

