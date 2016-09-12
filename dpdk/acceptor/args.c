#include <stdio.h>
#include <getopt.h>
#include "args.h"
#include "utils.h"
/* Display usage instructions */
void print_usage(const char *prgname)
{
        PRINT_INFO("\nUsage: %s [EAL options] -- [-i acceptor_id]\n",
            prgname);
}

/* Parse the arguments given in the command line of the application */
void parse_args(int argc, char **argv)
{
        int opt;
        const char *prgname = argv[0];
        acceptor_config.acceptor_id = 0;
        /* Disable printing messages within getopt() */
        /* Parse command line */
        while ((opt = getopt(argc, argv, "i:")) != EOF) {
                switch (opt) {
                case 'i':
                        acceptor_config.acceptor_id = atoi(optarg);
                        break;
                default:
                        print_usage(prgname);
                        FATAL_ERROR("Invalid option specified");
                }
        }
        /* Check that options were parsed ok */
        if (acceptor_config.acceptor_id < 0) {
                print_usage(prgname);
                FATAL_ERROR("acceptor_id is invalid");
        }
}