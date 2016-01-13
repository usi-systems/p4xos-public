#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "learner.h"
#include "proposer.h"

int main(int argc, char* argv[]) {
    int opt = 0;
    int role = 0;
    char *hostname;
    while ((opt = getopt(argc, argv, "plh:")) != -1) {
        switch(opt) {
            case 'p':
                role = 1;
                break;
            case 'l':
                role = 2;
                break;
            case 'h':
                hostname = optarg;
                break;
        }
    }

    if (role == 1) {
        // start proposer
        start_proposer(hostname);
    } else if (role == 2) {
        // start learner
        start_learner();
    } else {
        printf("Role was not set.\n");
    }
    return 0;

}