//
// Created by skinman95 on 24/09/19.
//

#ifndef POET_CODE_POET_NODE_T_H
#define POET_CODE_POET_NODE_T_H

typedef unsigned int uint;


struct node {
    uint arrival_time;
    uint sgx_time;
    uint n_leadership;
    uint time_left;
};

typedef struct node node_t;

#endif //POET_CODE_POET_NODE_T_H