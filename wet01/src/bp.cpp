/* 046267 Computer Architecture - Winter 20/21 - HW #1                  */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"

#include <cstdint>
#include <iostream>
#include <vector>
#include <map>
#include <math.h>
#include <assert.h>

#define MAX_HISTORY_BITS 8
#define MAX_TABLE_ENTRIES 32

/* uncomment to see
 *  debug messeges*/
#define DEBUG_CODE_ON


/*///////////////////////////////////////////////
  _____                     _        __  _    
 |_   _|_  _  _ __  ___  __| | ___  / _|( )___
   | | | || || '_ \/ -_)/ _` |/ -_)|  _||/(_-<
   |_|  \_, || .__/\___|\__,_|\___||_|    /__/
        |__/ |_|                              

*////////////////////////////////////////////////

/* local/global history */
typedef enum { 
	LOCAL_HIST,
	GLOBAL_HIST
} t_histType;
	

/* local/global fsm table */
typedef enum {
	LOCAL_TABLE,
	GLOBAL_TABLE
} t_fsmTableType;


/* share type */
typedef enum{
	LSHARE,
	GSHARE
} t_tableShare;


/* bimodal states */
typedef enum {
	SNT=0,	// strongly not taken
	WNT=1,	// weakly not taken
	WT =2,	// weakly taken
	ST =3	// strongly taken
} t_bimodalFSM;


/* branch predictor identity info */
typedef struct {
	uint8_t 		numOfEntries;	// 2^[0:5]
	uint8_t			historySize;	// [1:8]
	uint32_t	 	tagSize;		// [0:(30-log2(btbSize))]
	t_bimodalFSM    initialState;	// [SNT, WNT, WT, ST]
	t_histType		histType;  		// [LOCAL, GLOBAL]
	t_fsmTableType	tableType;		// [LOCAL, GLOBAL]
	t_tableShare	tableShare;		// [LSHARE, GSHARE] exists only if tableType is global
} t_btbIdentity;


/* taken/not taken typdef */
typedef enum {
	TAKEN=1,
	NOT_TAKEN=0
} t_branchJump;


/* vector for a single history table */
typedef std::vector<t_bimodalFSM> t_fsmVector;


/* history typdef holds history bits */
typedef uint8_t t_history;


/* branch prediction struct,
 * holds speculative jump 
 * result and jump target
 * pointer to a history vector*/
typedef struct {
	t_branchJump result; // TAKEN/NOT TAKEN;
	uint32_t 	 target; // 32 bit Address;
} t_prediction;


/* btb entry holds 
 * Tag feild
 * jump target feild
 * and address of history table */
typedef struct {
	uint32_t tag;
	uint32_t target;
} t_btbEntry;


/*/////////////////////////
   ___  _               
  / __|| | __ _  ___ ___
 | (__ | |/ _` |(_-<(_-<
  \___||_|\__,_|/__//__/
                        
*//////////////////////////


class BranchPredictor {
	public:
		t_btbIdentity 			 id;
		std::vector<t_btbEntry>  btbVector;		   // size = number of entries
		std::vector<t_history> 	 historyVector;    // size = one dimention when global, multiple when local.
		std::vector<t_fsmVector> histTableVectors; // size = one dimention when global, multiple when local.
		
		/* with param's constructor */
		BranchPredictor(t_btbIdentity id_){ id = id_; };
};


/*/////////////////////////////////////////////
  ___                 _    _                
 | __|_  _  _ _   __ | |_ (_) ___  _ _   ___
 | _|| || || ' \ / _||  _|| |/ _ \| ' \ (_-<
 |_|  \_,_||_||_|\__| \__||_|\___/|_||_|/__/
                                            
*//////////////////////////////////////////////


/* declaring bp object of
 * class branchPredictor */
BranchPredictor BP({0});


int BP_init(unsigned btbSize, unsigned historySize, unsigned tagSize, unsigned fsmState,
			bool isGlobalHist, bool isGlobalTable, int Shared){
	
	/* build BP identity needs casting*/
	t_btbIdentity id;
	id.numOfEntries = uint8_t(btbSize);
	id.historySize  = uint8_t(historySize);
	id.tagSize 	    = uint8_t(tagSize);
	id.initialState = t_bimodalFSM(fsmState);
	id.histType 	= (isGlobalHist)  ? GLOBAL_HIST : LOCAL_HIST;
	id.tableType  	= (isGlobalTable) ? GLOBAL_TABLE : LOCAL_TABLE;
	id.tableShare   = (Shared) 	      ? GSHARE : LSHARE;
	BP = BranchPredictor(id);
	
/* debug code */
#ifdef DEBUG_CODE_ON 
	std::cout << int(BP.id.numOfEntries) << " ";
	std::cout << int(BP.id.historySize)  << " ";
	std::cout << BP.id.tagSize 		<< " ";
	std::cout << BP.id.initialState << " ";
	std::cout << BP.id.histType 	<< " ";
	std::cout << BP.id.tableType    << " ";
	std::cout << BP.id.tableShare   <<std::endl;
#endif	  
	
	
	return 0;
}

bool BP_predict(uint32_t pc, uint32_t *dst){
	return false;
}

void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){
	return;
}

void BP_GetStats(SIM_stats *curStats){
	return;
}

