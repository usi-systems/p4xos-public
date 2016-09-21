#include <stdio.h>
#include <getopt.h>
#include "args.h"
#include "utils.h"
/* Display usage instructions */
void print_usage(const char *prgname)
{
    PRINT_INFO("\nUsage: %s [EAL options] -- [-l] [-i learner_id] [-a nb_acceptors] [-n nb_learners]\n"
               "  -i\t\t 0 for the first learner\n"
               "  -l\t\t enable LevelDB\n",
       prgname);
}

/* Parse the arguments given in the command line of the application */
void parse_args(int argc, char **argv)
{
        int opt;
        const char *prgname = argv[0];
        learner_config.learner_id = 0;
        learner_config.nb_learners = 1;
        learner_config.nb_acceptors = 1;
        learner_config.level_db = 0;
        /* Disable printing messages within getopt() */
        /* Parse command line */
        while ((opt = getopt(argc, argv, "i:a:n:l")) != EOF) {
                switch (opt) {
                case 'i':
                        learner_config.learner_id = atoi(optarg);
                        break;
                case 'a':
                        learner_config.nb_acceptors = atoi(optarg);
                        break;
                case 'n':
                        learner_config.nb_learners = atoi(optarg);
                        break;
                case 'l':
                        learner_config.level_db = 1;
                        break;
                default:
                        print_usage(prgname);
                        FATAL_ERROR("Invalid option specified");
                }
        }
        /* Check that options were parsed ok */
        if (learner_config.learner_id >= learner_config.nb_learners) {
                print_usage(prgname);
                FATAL_ERROR("Learner ID is invalid");
        }
}