#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "learner.h"
#include "stat.h"
#include "proposer.h"

int main(int argc, char* argv[]) {
    int opt = 0;
    int role = 0;
    char *hostname;
    int duration = 10000;
    int verbose = 0;
    while ((opt = getopt(argc, argv, "vplh:d:")) != -1) {
        switch(opt) {
            case 'p':
                role = 1;
                break;
            case 'l':
                role = 2;
                break;
            case 'v':
                verbose = 1;
                break;
            case 'h':
                hostname = optarg;
                break;
            case 'd':
                duration = atoi(optarg);
                break;
        }
    }

    if (role == 1) {
        // start proposer
        start_proposer(hostname, duration, verbose);
    } else if (role == 2) {
        // start learner
        start_learner(verbose);
    } else {
        printf("Role was not set.\n");
    }
    return 0;

}