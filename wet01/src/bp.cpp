/* 046267 Computer Architecture - Winter 20/21 - HW #1                  */
/* This file should hold your implementation of the predictor simulator */

#include "bp_api.h"

#include <cstdint>
#include <iostream>
#include <vector>
#include <math.h>
#include <bitset>


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


/* btb entry holds 
 * Tag feild
 * jump target feild
 * and valid bit */
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
		uint8_t  historyMask;
		
		/* last prediction */
		t_branchJump lastJump; // TAKEN/NOT TAKEN;		
		
		/* branch predictor stats */
		SIM_stats perfStats;
		
	// ####################################
	// ### class functions
	// ####################################
		
		/* with param's constructor */
		BranchPredictor(t_btbIdentity id_){ id = id_; };
	
		
		/* creats a history mask to mach history size */
		void construct_history_mask(void){
			historyMask = 0x0;
			for(int i=0; i<id.historySize; i++)
				historyMask = (historyMask << 1) +1;
		}
		
		
		/* creates a mask that looks like this 0b00001100 for entry size of 4 */
		void construct_entry_addr_mask(void){
			btbEntryAddrMask = 0x0;
			for(int i=0; i<int(log2(id.numOfEntries)); i++)
				btbEntryAddrMask = (btbEntryAddrMask << 1) +1; 
			btbEntryAddrMask = btbEntryAddrMask << 2; // two lower bits are always 0;
		};
		
		
		
		/* creates a mask for tag extraction */
		void construct_tag_mask(void){
			tagMask = 0x0;
			for(uint8_t i=0; i<id.tagSize; i++)
				tagMask = (tagMask << 1) +1;
			tagMask = tagMask << (int(log2(id.numOfEntries)) + 2);
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
			
		
		/* according to the btb entry deep 
		 * dive into the BP topology to get the 
		 * branch prediction TAKE/NOT_TAKEN result */
		t_branchJump get_branch_resolution(uint8_t btbEntryAddr, uint32_t pc){
			
			t_history history;
			uint8_t fsmTableAddr;
			t_fsmVector fsmTable;
			t_branchJump branchRes;
			
			// get history.
			if(id.histType == LOCAL_HIST)
				history = historyVector[btbEntryAddr];
			else history = historyVector[0];
			history = history & historyMask;
			
			// get fsm Table
			if(id.tableType == LOCAL_TABLE)
				fsmTable = histTableVectors[btbEntryAddr];
			else fsmTable = histTableVectors[0];
			
			// calculate fsm entry addr
			if((id.tableType == GLOBAL_TABLE) && (id.tableShare == GSHARE)){
				if(id.sharePolicy == LSB_SHARE)
					fsmTableAddr = uint8_t((pc >> 2) & historyMask) ^ uint8_t(history);
				else
					fsmTableAddr = uint8_t((pc >> 16) & historyMask) ^ uint8_t(history);
			}
			else fsmTableAddr = uint8_t(history);
							 
			// get branch resolution
			branchRes = t_branchJump(int(fsmTable[fsmTableAddr])>>1);
			/* debug code */
			#ifdef DEBUG_CODE_ON
				std::cout << "fsm stat: " << int(fsmTable[fsmTableAddr]) << std::endl;
			#endif	  						
			return branchRes;
		};
		
		
		/* updates history vector for local and global */
		void update_history_vector(uint32_t pc, bool taken){

			t_history history;
			uint32_t 	 pcTag;
			uint32_t 	 entryTag;
			uint8_t btbEntryAddr;
			
			btbEntryAddr = (pc & btbEntryAddrMask) >> 2;
			
			// get history.
			if(id.histType == LOCAL_HIST) {
				history = historyVector[btbEntryAddr];
				pcTag = pc & tagMask;			
				entryTag = btbVector[btbEntryAddr].tag;
				if(entryTag != pcTag){ // flush history
					/* debug code */
					#ifdef DEBUG_CODE_ON
						std::cout << "history Flush occured" << std::endl;
					#endif	  						
					history = 0x0;
					if(id.tableType == LOCAL_TABLE)
						std::fill(histTableVectors[btbEntryAddr].begin(),
								  histTableVectors[btbEntryAddr].end(), id.initialState);
				}
			}
			else history = historyVector[0];

			/* debug code */
			#ifdef DEBUG_CODE_ON
				std::bitset<MAX_HISTORY_BITS> y(history);
				std::cout << "history: " << y << std::endl;
			#endif	  						
			
			history = history & historyMask;			
			history = (history << 1) + uint8_t(taken);
			if(id.histType == LOCAL_HIST) {
				historyVector[btbEntryAddr] = history;
			}
			else historyVector[0] = history;
			return;
		}
		
		
		/* updates the bimodal fsm given pc and resut */
		void set_branch_resolution(uint32_t pc, bool taken){
			
			t_history history;
			uint8_t fsmTableAddr;
			t_fsmVector fsmTable;
			int status;			
			uint8_t btbEntryAddr;
			
			btbEntryAddr = (pc & btbEntryAddrMask) >> 2;

			// get history.
			if(id.histType == LOCAL_HIST)
				history = historyVector[btbEntryAddr];
			else history = historyVector[0];
			history = history & historyMask;
				
			// get fsm Table
			if(id.tableType == LOCAL_TABLE)
				fsmTable = histTableVectors[btbEntryAddr];
			else fsmTable = histTableVectors[0];
			
			// calculate fsm entry addr
			if((id.tableType == GLOBAL_TABLE) && (id.tableShare == GSHARE)){
				if(id.sharePolicy == LSB_SHARE)
					fsmTableAddr = uint8_t((pc >> 2) & historyMask) ^ uint8_t(history);
				else
					fsmTableAddr = uint8_t((pc >> 16) & historyMask) ^ uint8_t(history);
			}
			else fsmTableAddr = uint8_t(history);
			
			// get branch resolution
			status = int(fsmTable[fsmTableAddr]);
			
			/* debug code */
			#ifdef DEBUG_CODE_ON
				std::cout << "fsm old stat: " << status << " ";
			#endif	  						
			
			if(taken) status += (status < 3);
			else status -= (status > 0);

			/* debug code */
			#ifdef DEBUG_CODE_ON
				std::cout << "fsm new stat: " << status << std::endl;
			#endif	  						
			
			// update bimodal fsm value
			if(id.tableType == LOCAL_TABLE)	
				histTableVectors[btbEntryAddr][fsmTableAddr] =  t_bimodalFSM(status);
			else histTableVectors[0][fsmTableAddr] =  t_bimodalFSM(status);
			
			return;
		};
		
		
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
			tagSize = id.tagSize;
			btbBitSize = numOfEntries * (ADDR_WIDTH + tagSize + 1); // target, tag, valid bit
			
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


/*  */
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
	BP.construct_history_mask();

	/* last jump result */
	BP.lastJump = NOT_TAKEN;
	
	/* perfstats init */
	BP.perfStats.flush_num = 0;
	BP.perfStats.br_num    = 0;
	BP.perfStats.size 	   = BP.get_branch_predictor_size();

	
	
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
		std::cout << "fsm Table size: "        << BP.histTableVectors[0].size() << std::endl;
		// mask values
		std::bitset<32> y1(BP.btbEntryAddrMask);
		std::bitset<32> y2(BP.tagMask);
		std::bitset<8>  y3(BP.historyMask); 
		std::cout << "entry address mask: " << y1 << std::endl;
		std::cout << "tag address mask:   " << y2 << std::endl;
		std::cout << "history mask:       " << y3 << std::endl;
	#endif	  
	return 0;
}


/*  */
bool BP_predict(uint32_t pc, uint32_t *dst){
	
	uint8_t 	 btbEntryAddr; // [31:0]
	uint32_t 	 pcTag;
	uint32_t 	 entryTag;
	t_branchJump branchJump;
	
	/* debug code */
	#ifdef DEBUG_CODE_ON
		std::cout << std::endl;
		std::cout << "IF: ";
		std::cout << "pc: 0x" << std::hex << pc << " ";		
	#endif	  
		
	// calc entry address	
	btbEntryAddr = (pc & BP.btbEntryAddrMask) >> 2;
	
	// entry unvalid no branch command
	if(!BP.btbVector[btbEntryAddr].valid){
		(*dst) = pc + 4;
		BP.lastJump = NOT_TAKEN;
		/* debug code */
		#ifdef DEBUG_CODE_ON
			std::cout << "target: 0x" << std::hex << (*dst) << " ";
			std::cout << "result: N" << std::endl;
		#endif
		return NOT_TAKEN;
	}

	// calculate pc tag
	pcTag = pc & BP.tagMask;
	
	// get entry tag
	entryTag = BP.btbVector[btbEntryAddr].tag;
	
	// if branch command hit
	if(pcTag == entryTag){
		branchJump = BP.get_branch_resolution(btbEntryAddr, pc);
		if(branchJump == TAKEN){
			(*dst) = BP.btbVector[btbEntryAddr].target;
			BP.lastJump = TAKEN;
			/* debug code */
			#ifdef DEBUG_CODE_ON
				std::cout << "target: 0x" << std::hex << (*dst) << " ";
				std::cout << "result: T" << std::endl;
			#endif			
			return TAKEN;
		}
		else{
			(*dst) = pc + 4;
			BP.lastJump = NOT_TAKEN;
			/* debug code */
			#ifdef DEBUG_CODE_ON
				std::cout << "target: 0x" << std::hex << (*dst) << " ";
				std::cout << "result: N" << std::endl;
			#endif	  			
			return NOT_TAKEN;
		}
	}
		
	BP.lastJump = NOT_TAKEN;
	return false;
}


/*  */
void BP_update(uint32_t pc, uint32_t targetPc, bool taken, uint32_t pred_dst){
	
	t_btbEntry btbEntry;
	uint8_t 	 btbEntryAddr; // [31:0]

	// update branch stats
	BP.perfStats.br_num++;
	if((taken && (targetPc != pred_dst)) || 
	   (!taken &&  (pred_dst != pc+4)))
		BP.perfStats.flush_num++;
		
	// calc entry address	
	btbEntryAddr = (pc & BP.btbEntryAddrMask) >> 2;
			
	// update branch resolution for old history
	BP.set_branch_resolution(pc, taken);

	// update history
	BP.update_history_vector(pc, taken);
	
	// update history vector
	btbEntry.tag    = pc & BP.tagMask;
	btbEntry.target = targetPc;
	btbEntry.valid  = true;
	BP.btbVector[btbEntryAddr] = btbEntry;
	
	/* debug code */
	#ifdef DEBUG_CODE_ON 
		//std::cout << std::endl;
		std::cout << "EXE: "; //<< std::endl;
		std::cout << "pc: 0x" << std::hex << pc << " ";//std::endl;
		std::cout << "target: 0x" << std::hex << targetPc << " ";//std::endl;
		std::cout << "branch: " << ((taken) ? "T" : "N") << " ";//std::endl;
		std::cout << "predicted pc: 0x" << std::hex << pred_dst << " ";//std::endl;
		std::cout << "predicted: " << ((BP.lastJump) ? "T" : "N");//std::endl;
		std::cout << std::endl;
	#endif	 
		
	return;
}

void BP_GetStats(SIM_stats *curStats){
	
	curStats->flush_num = BP.perfStats.flush_num;
	curStats->br_num    = BP.perfStats.br_num;
	curStats->size      = BP.get_branch_predictor_size();
	
	return;
}

