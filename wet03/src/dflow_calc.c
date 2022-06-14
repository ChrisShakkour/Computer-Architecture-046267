/* 046267 Computer Architecture - Winter 20/21 - HW #3               */
/* Implementation (skeleton)  for the dataflow statistics calculator */

#include "dflow_calc.h"


// global variables
uint32_t instCount;
int* exitArr = NULL;


// struct that holds 
// Opcode information
struct node{
     struct node *link1;
     struct node *link2;
     int id;
     unsigned int opcodeLatency;
};
typedef struct node Node;


// function to free Node and
// exit array allocated memory.
void freeProgCtx(ProgCtx ctx) {
    free((Node*)ctx);
    free(exitArr);
}


//
int getInstDeps(ProgCtx ctx, unsigned int theInst, int *src1DepInst, int *src2DepInst) {
    if(ctx != NULL || instCount >= theInst) {
        Node *graph = (Node *) ctx;
        *src1DepInst = graph[theInst+1].link1->id - 1;
        *src2DepInst = graph[theInst+1].link2->id - 1;
        return 0;
    }
    return -1;
}


// returns deepest InstDepth 
// for a given node.
int getProgDepth(ProgCtx ctx) {
    if(ctx!=NULL) {
        int max = 0;
        Node *graph = (Node *) ctx;
        for (int i = 0; i < instCount; i++) {
            int temp = getInstDepth(ctx, i)+graph[i+1].opcodeLatency;
            if (temp > max) {
                max = temp;
            }
        }
        return max;
    }
    return 0;
}


//
ProgCtx analyzeProg(const unsigned int opsLatency[], const InstInfo progTrace[], unsigned int numOfInsts) {
    instCount = numOfInsts;
    Node* graph = (Node*)malloc(sizeof(Node)*(numOfInsts+1)); //graph[0]=entry
    if(graph != NULL) {
        exitArr = (int *) malloc(sizeof(int) * (numOfInsts + 1));
        graph[0].id = 0;
        for (int i = 1; i < numOfInsts + 1; i++) {
            exitArr[i] = 0;
            graph[i].id = -1;
            graph[i].link1 = NULL;
            graph[i].link2 = NULL;
        }
        int RAT[32] = {0};
        for (int i = 1; i < numOfInsts + 1; i++) {
            graph[i].id = i;
            graph[i].opcodeLatency = opsLatency[progTrace[i-1].opcode];
            graph[i].link1 = &graph[RAT[progTrace[i-1].src1Idx]];
            exitArr[graph[RAT[progTrace[i-1].src1Idx]].id]++;
            graph[i].link2 = &graph[RAT[progTrace[i-1].src2Idx]];
            exitArr[graph[RAT[progTrace[i-1].src2Idx]].id]++;
            RAT[progTrace[i-1].dstIdx] = i;
        }
        return graph;
    }
    return PROG_CTX_NULL;
}


// Recursive function to go over the
// tree taking the Right node first.
unsigned int getInstDepth_inner(ProgCtx ctx, unsigned int theInst){
    if(theInst == 0 ){
        return 0;
    }
    Node* graph = (Node*)ctx;
    unsigned int R = getInstDepth_inner(ctx,graph[theInst].link1->id);
    unsigned int L = getInstDepth_inner(ctx,graph[theInst].link2->id);
    if(R>L){
        return R + graph[theInst].opcodeLatency;
    }
    return L+graph[theInst].opcodeLatency;
}


// calls recursive function to iterate over 
// the tree starting from the righ node
int getInstDepth(ProgCtx ctx, unsigned int theInst) {
    if(ctx!=NULL || instCount >= theInst) {
        Node *graph = (Node *) ctx;
        unsigned int R = getInstDepth_inner(ctx, graph[theInst+1].link1->id);
        unsigned int L = getInstDepth_inner(ctx, graph[theInst+1].link2->id);
        if (R > L) {
            return (int)R;
        }
        return (int)L;
    }
    return -1;
}




