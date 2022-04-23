/* 046267 Computer Architecture - Winter 20/21 - HW #1                  */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"

#include <cstdint>
#include <iostream>
#include <vector>
#include <map>
#include <math.h>
#include <assert.h>


/* uncomment to see
 *  debug messeges*/
//#define DEBUG_CODE_ON

#define MAX_HISTORY_BITS 	8
#define MAX_TABLE_ENTRIES 	32
#define ADDR_WIDTH 			32 // instruction address is 32 bits


uint32_t tagMask = 0xFFFFFFFF;



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


/* share policy */
typedef enum {
	LSB_SHARE=1,
	MID_SHARE=2
} t_sharePolicy;


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
	t_sharePolicy   sharePolicy;	// [LSB, MID]
} t_btbIdentity;


/* taken/not taken typdef */
typedef enum {
	TAKEN=true,
	NOT_TAKEN=false
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
	bool 	 valid;
} t_btbEntry;


/*/////////////////////////
   ___  _               
  / __|| | __ _  ___ ___
 | (__ | |/ _` |(_-<(_-<
  \___||_|\__,_|/__//__/
                        
*//////////////////////////


class BranchPredictor {
	public:
		
	// ####################################
	// ### class ADT
	// ####################################
		/* input parameters are stored here */
		t_btbIdentity 			 id;
		
		/* constructed btb Data types */
		std::vector<t_btbEntry>  btbVector;		   // size = number of entries
		std::vector<t_history> 	 historyVector;    // size = one dimention when global, multiple when local.
		std::vector<t_fsmVector> histTableVectors; // size = one dimention when global, multiple when local.
		
		/* addr masks */
		uint32_t btbEntryAddrMask;
		uint32_t tagMask;
		
	// ####################################
	// ### class functions
	// ####################################
		
		/* with param's constructor */
		BranchPredictor(t_btbIdentity id_){ id = id_; };
	
		
		/* creates a mask that looks like this 0b00001100 for entry size of 4 */
		void construct_entry_addr_mask(void){
			btbEntryAddrMask = 0x0;
			for(int i=0; i<log2(id.numOfEntries); i++)
				btbEntryAddrMask = (btbEntryAddrMask << 1) +1; 
			btbEntryAddrMask = btbEntryAddrMask << 2; // two lower bits are always 0;
		};
		
		
		
		/* creates a mask for tag extraction */
		void construct_tag_mask(void){
			tagMask = 0x3;
			for(int i=0; i<log2(id.numOfEntries); i++)
				tagMask = (tagMask << 1) +1; 
			tagMask = ~tagMask;
		};
		
		
		/* push Zeros into each entry in the vector */
		void construct_btb_vector(void){
			for(int i=0; i<id.numOfEntries; i++)
				btbVector.push_back({0});
		};
		
		
		/* single entry if GLOBAL. else every entry has a history */
		void construct_history_vector(void){
			/* if LOCAL create a history for each entry */
			if(id.histType == LOCAL_HIST){
				for(int i=0; i<id.numOfEntries; i++)
					historyVector.push_back({0});
			}
			/* single history element for all entries*/
			else historyVector.push_back({0}); 
		};
		
		
		/* create and fill fsm vector table 
		 * with initial prediction fsm state */
		void construct_fsm_vectors(void) {
			int twoPowerHist = pow(2, id.historySize);
			/* if LOCAL create a table for each entry */
			if(id.tableType == LOCAL_TABLE){
				for(int i=0; i<id.numOfEntries; i++){
					std::vector<t_bimodalFSM> fsmHistVector(twoPowerHist, id.initialState);
					histTableVectors.push_back(fsmHistVector);
				}
			}
			/* single table for all entries*/
			else {
				std::vector<t_bimodalFSM> fsmHistVector(twoPowerHist, id.initialState);
				histTableVectors.push_back(fsmHistVector); 
			}
		};
			
		
		/*  */
		t_branchJump get_branch_resolution(uint8_t btbEntryAddr){
			return NOT_TAKEN;
		};
		
		
		/*  */
		void set_branch_resolution(uint8_t btbEntryAddr, t_branchJump branchJump){
			
		}
		
		
		/* calculates the size in bits of this predictor */
		uint32_t get_branch_predictor_size(void){
			uint32_t totalBitSize;
			uint32_t btbBitSize;
			uint32_t historyBitSize;
			uint32_t fsmTablesBitsize;
			uint8_t  numOfEntries;
			uint8_t  tagSize;
			uint32_t histSize;
			
			// btb table calculation
			numOfEntries = id.numOfEntries;
			tagSize = (ADDR_WIDTH - 2) - log2(numOfEntries); 
			btbBitSize = numOfEntries * (ADDR_WIDTH + tagSize);
			
			// history calculation
			histSize = id.historySize;
			if(id.histType == LOCAL_HIST)
				historyBitSize = histSize * numOfEntries;
			else
				historyBitSize = histSize;
			
			// fsm tables calculation
			if(id.tableType == LOCAL_TABLE)
				fsmTablesBitsize = numOfEntries * 2 * pow(2, histSize);
			else
				fsmTablesBitsize = 2 * pow(2, histSize);
				
			totalBitSize = btbBitSize + historyBitSize + fsmTablesBitsize;
			return totalBitSize; // return size in bits
		}
			
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
	
	/* build BP identity with casting*/
	t_btbIdentity id;
	id.numOfEntries = uint8_t(btbSize);
	id.historySize  = uint8_t(historySize);
	id.tagSize 	    = uint8_t(tagSize);
	id.initialState = t_bimodalFSM(fsmState);
	id.histType 	= (isGlobalHist)  ? GLOBAL_HIST : LOCAL_HIST;
	id.tableType  	= (isGlobalTable) ? GLOBAL_TABLE : LOCAL_TABLE;
	id.tableShare   = (Shared) 	      ? GSHARE : LSHARE;
	id.sharePolicy  = (Shared) 		  ? t_sharePolicy(Shared) : LSB_SHARE;
	BP = BranchPredictor(id);

	/* BTB vector construction */
	BP.construct_btb_vector();
	BP.construct_history_vector();
	BP.construct_fsm_vectors();
	
	/* address masks construction */
	BP.construct_entry_addr_mask();
	BP.construct_tag_mask();
	
	/* debug code */
	#ifdef DEBUG_CODE_ON 
		// input parameters
		std::cout << int(BP.id.numOfEntries) << " ";
		std::cout << int(BP.id.historySize)  << " ";
		std::cout << BP.id.tagSize 		<< " ";
		std::cout << BP.id.initialState << " ";
		std::cout << BP.id.histType 	<< " ";
		std::cout << BP.id.tableType    << " ";
		std::cout << BP.id.tableShare   << " ";
		std::cout << BP.id.sharePolicy  <<std::endl;
		std::cout << "Shared value: " << Shared << std::endl;
		// post construction 
		std::cout << "btb vector size: " 	   << BP.btbVector.size() << std::endl;
		std::cout << "history vector size: "   << BP.historyVector.size() << std::endl;
		std::cout << "fsm Table vector size: " << BP.histTableVectors.size() << std::endl;
		// mask values
		std::cout << "entry address mask: " << BP.btbEntryAddrMask << std::endl;
		std::cout << "tag address mask: " << BP.tagMask << std::endl;
	#endif	  
	return 0;
}

bool BP_predict(uint32_t pc, uint32_t *dst){
	
	uint8_t 	 btbEntryAddr; // [31:0]
	uint32_t 	 pcTag;
	uint32_t 	 entryTag;
	t_branchJump branchJump;
	
	// calc entry address	
	btbEntryAddr = (pc & BP.btbEntryAddrMask) >> 2;
	
	// entry unvalid no branch command
	if(!BP.btbVector[btbEntryAddr].valid){
		(*dst) = pc + 4;
		return NOT_TAKEN;
	}

	// calculate pc tag
	pcTag = pc & BP.tagMask;
	
	// get entry tag
	entryTag = BP.btbVector[btbEntryAddr].tag;
	
	// if branch command hit
	if(pcTag == entryTag){
		branchJump = BP.get_branch_resolution(btbEntryAddr);
		if(branchJump == TAKEN){
			(*dst) = BP.btbVector[btbEntryAddr].target;
			return TAKEN;
		}
		else{
			(*dst) = pc + 4;
			return NOT_TAKEN;
		}
	}
	
	/* debug code */
	#ifdef DEBUG_CODE_ON
		std::cout << std::endl;
		std::cout << "entry addr: " << std::hex << int(btbEntryAddr) << std::endl;
		std::cout << "IF: ";
		std::cout << "pc: 0x" << std::hex << pc <<std::endl;
	#endif	  
	
	return false;
}


void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){
	
	t_btbEntry btbEntry;
	uint8_t 	 btbEntryAddr; // [31:0]

	// calc entry address	
	btbEntryAddr = (pc & BP.btbEntryAddrMask) >> 2;
		
	// calculate pc tag
	btbEntry.tag    = pc & BP.tagMask;
	btbEntry.target = targetPc;
	btbEntry.valid  = true;
	
	// update entry table
	BP.btbVector[btbEntryAddr] = btbEntry;
	
	// update history vector
	// update branch resolution
	// flush if needed
	
	/* debug code */
	#ifdef DEBUG_CODE_ON 
		//std::cout << std::endl;
		std::cout << "EXE: ";
		std::cout << "pc: 0x" << std::hex << pc <<std::endl;
		std::cout << std::endl;
	#endif	 
		
	return;
}

void BP_GetStats(SIM_stats *curStats){
	
	
	curStats->flush_num = 5;
	curStats->br_num = 2;
	curStats->size = BP.get_branch_predictor_size();
	
	return;
}

