//===-- PatmosSPReduce.cpp - Reduce the CFG for Single-Path code ----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#include "PatmosSPReduce.h"
#include "PredicateDefinition.h"
#include "Patmos.h"
#include "PatmosInstrInfo.h"
#include "PatmosMachineFunctionInfo.h"
#include "PatmosSubtarget.h"
#include "PatmosTargetMachine.h"
#include "llvm/IR/Function.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/GraphTraits.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/CodeGen/MachinePostDominators.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/GraphWriter.h"
#include "llvm/Support/DOTGraphTraits.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <set>
#include <queue>
#include <algorithm>
#include <sstream>
#include <iostream>

using namespace llvm;

#define DEBUG_TYPE "patmos-singlepath"

#define USE_BCOPY
#define NOSPILL_OPTIMIZATION
//#define BOUND_UNDEREST_PROTECTION

STATISTIC( RemovedBranchInstrs, "Number of branch instructions removed");
STATISTIC( InsertedInstrs,      "Number of instructions inserted");
STATISTIC( LoopCounters,        "Number of loop counters introduced");
STATISTIC( ElimLdStCnt,         "Number of eliminated redundant loads/stores");

namespace llvm {
  /// LinearizeWalker - Class to linearize the CFG during a walk of the SPScope
  /// tree.
  class LinearizeWalker : public SPScopeWalker {
    private:
      virtual void nextMBB(MachineBasicBlock *);
      virtual void enterSubscope(SPScope *);
      virtual void exitSubscope(SPScope *);

      // reference to the pass, to get e.g. RAInfos
      PatmosSPReduce &Pass;
      // reference to the machine function, for inserting MBBs
      MachineFunction &MF;

      MachineBasicBlock *LastMBB; // state: last MBB re-inserted

      /// Inserts into the given MBB predicate loads or copies for the
      /// predicates used by the header of the given scope.
      void insertHeaderPredLoadOrCopy(const SPScope *scope, MachineBasicBlock *PrehdrMBB, DebugLoc DL){
        const RAInfo &RI = Pass.RAInfos.at(scope),
                     &RP = Pass.RAInfos.at(scope->getParent());
        auto headerBlock = scope->getHeader();
        auto HeaderMBB = headerBlock->getMBB();

        // copy/load the header predicate for the subloop
        auto parentLoadLocs = RP.getLoadLocs(HeaderMBB); // In the parent RAInfo, which preds should be loaded
        auto parentPredRegs = Pass.getPredicateRegisters(RP, headerBlock); // In the parent RAInfo, which registers does the block use
        auto predRegs = Pass.getPredicateRegisters(RI, headerBlock); // Which registers does the block actually use
        for(auto pred: headerBlock->getBlockPredicates()){
          if(parentLoadLocs.count(pred)){
            // The predicate needs to be loaded from a spill slot
            Pass.insertPredicateLoad(PrehdrMBB, PrehdrMBB->end(),
                parentLoadLocs[pred], predRegs[pred]);
            InsertedInstrs++; // STATISTIC
          } else {
            // The predicate does not need to be loaded.

            // Find the register the parent uses for the predicate
            unsigned parentReg = parentPredRegs.count(pred)? parentPredRegs[pred] : Patmos::P0;

            // if the registers used for the predicate don't match between
            // parent and this scope, move the value of the predicate
            // from the parent register to this scope's register
            if(predRegs[pred] != parentPredRegs[pred]){
              AddDefaultPred(BuildMI(*PrehdrMBB, PrehdrMBB->end(), DL,
                    Pass.TII->get(Patmos::PMOV), predRegs[pred]))
                .addReg( parentReg ).addImm(0);
              InsertedInstrs++; // STATISTIC
            }
          }
        }
      }
    public:
      explicit LinearizeWalker(PatmosSPReduce &pass, MachineFunction &mf)
        : Pass(pass), MF(mf), LastMBB(NULL) {}
  };

  /// RedundantLdStEliminator - Class that implements the removal
  /// of redundant loads and stores (to a tracked register), which are
  /// inserted in the course of the transformation.
  /// This includes predicate spill code and loop counters.
  class RedundantLdStEliminator {
    public:
      explicit RedundantLdStEliminator(MachineFunction &mf,
          const PatmosRegisterInfo *tri, unsigned int tgtreg,
          const PatmosMachineFunctionInfo &PMFI)
        : MF(mf), TRI(tri), TgtReg(tgtreg), NumFIs(PMFI.getSinglePathFICnt()),
          OffsetFIs(PMFI.getSinglePathLoopCntFI(0)) {}

      void addRemovableInst(MachineInstr *MI) {
        Removables.insert(MI);
      }


      unsigned int process(void) {
        LLVM_DEBUG( dbgs() << "Eliminate redundant loads/stores to " <<
            TRI->getName(TgtReg) << "\n" );

        unsigned int count = 0;
        // create the container with the bitvectors for each basic block
        // for the data-flow analyses
        for (MachineFunction::iterator MBB = MF.begin(), MBBe = MF.end();
            MBB != MBBe; ++MBB) {
          BlockInfos.insert(std::make_pair(&*MBB, Blockinfo(NumFIs)));
        }

        LLVM_DEBUG(dbgs() << "Removing redundant loads:\n");
        findRedundantLoads();
        count += remove();

        // Having redundant loads eliminated enables simpler removal
        // of redundant stores
        LLVM_DEBUG(dbgs() << "Removing redundant stores:\n");
        // FIXME the analysis is erroneous.
        //findRedundantStores();
        count += remove();

        return count;
      }

      unsigned int remove(void) {
        unsigned int cnt = Removables.size();
        for (std::set<MachineInstr *>::iterator I = Removables.begin(),
            E = Removables.end(); I != E; ++I) {
          LLVM_DEBUG(dbgs() << "  " << **I);
          (*I)->eraseFromParent();
        }
        Removables.clear();
        return cnt;
      }

    private:
      MachineFunction &MF;
      const PatmosRegisterInfo *TRI;
      const unsigned int TgtReg;
      const unsigned int NumFIs;
      const unsigned int OffsetFIs;

      std::set<MachineInstr *> Removables;

      struct Blockinfo {
        // required for elimination of redundant loads
        BitVector LiveFIExit, LiveFIEntry;
        // required for elimination of redundant stores
        BitVector SubseqStoresEntry, SubseqStoresExit;
        BitVector FutureLoadsEntry, FutureLoadsExit;

        Blockinfo(unsigned int size)
          : LiveFIExit(size), LiveFIEntry(size),
            SubseqStoresEntry(size), SubseqStoresExit(size),
            FutureLoadsEntry(size), FutureLoadsExit(size) {}
      };

      std::map<const MachineBasicBlock *, Blockinfo> BlockInfos;

      inline unsigned int normalizeFI(int fi) const {
        unsigned norm = fi - OffsetFIs;
        assert((fi >= 0 && norm < NumFIs) && "FI out of bounds");
        return norm;
      }

      inline int denormalizeFI(unsigned int fi) const {
        assert(fi < NumFIs && "FI out of bounds");
        return fi + OffsetFIs;
      }

      void printFISet(const BitVector &BV, raw_ostream &os) const {
        for (int i = BV.find_first(); i != -1; i = BV.find_next(i)) {
          os << denormalizeFI(i) << " ";
        }
      }

      bool isUncondLoad(const MachineInstr *MI, int &fi) const {
        if ((MI->getOpcode() == Patmos::LBC || MI->getOpcode() == Patmos::LWC)
            && MI->getOperand(0).getReg() == TgtReg
            && (MI->getOperand(1).getReg() == Patmos::NoRegister ||
              MI->getOperand(1).getReg() == Patmos::P0)
            && MI->getOperand(2).getImm() == 0
            && MI->getOperand(3).isFI()) {
          fi = MI->getOperand(3).getIndex();
          return true;
        }
        return false;
      }

      bool isUncondStore(const MachineInstr *MI, int &fi) const {
        if ((MI->getOpcode() == Patmos::SBC || MI->getOpcode() == Patmos::SWC)
            && MI->getOperand(4).getReg() == TgtReg
            && (MI->getOperand(0).getReg() == Patmos::NoRegister ||
              MI->getOperand(0).getReg() == Patmos::P0)
            && MI->getOperand(1).getImm() == 0
            && MI->getOperand(2).isFI()) {
          fi = MI->getOperand(2).getIndex();
          return true;
        }
        return false;
      }


      void findRedundantLoads(void) {
        // forward DF problem
        std::map<MachineInstr *, BitVector> collected_loads;
        // operate in reverse-postorder
        ReversePostOrderTraversal<MachineFunction *> RPOT(&MF);
        bool changed;
        do {
          changed = false;
          for (ReversePostOrderTraversal<MachineFunction *>::rpo_iterator
              RI = RPOT.begin(),  RE = RPOT.end(); RI != RE; ++RI) {
            MachineBasicBlock *MBB = *RI;
            Blockinfo &BI = this->BlockInfos.at(MBB);

            BitVector livein = BitVector(NumFIs, true);
            // join from predecessors
            if (MBB->pred_size() > 0) {
              for (MachineBasicBlock::pred_iterator PI = MBB->pred_begin();
                  PI != MBB->pred_end(); ++PI) {
                 livein &= this->BlockInfos.at(*PI).LiveFIExit;
              }
            } else {
              livein.reset();
            }
            if (BI.LiveFIEntry != livein) {
              BI.LiveFIEntry = livein;
              changed = true;
            }

            // transfer
            BitVector livefi(livein);
            for (MachineBasicBlock::iterator MI = MBB->begin(),
                MIe = MBB->end(); MI != MIe; ++MI) {
              // check for unconditional load to TgtReg
              int fi;
              if (isUncondLoad(&*MI, fi)) {
                // remember load with livefi at entry
                collected_loads[&*MI] = livefi;
                // update
                livefi.reset();
                livefi.set(normalizeFI(fi));
              }
            }
            // was an update?
            if (BI.LiveFIExit != livefi) {
              BI.LiveFIExit = livefi;
              changed = true;
            }
          }
        } while (changed);

        // now inspect the livefi at entry of each load. if it is equal to
        // the fi of the load, the load is redundant and we can remove it.
        for (std::map<MachineInstr *, BitVector>::iterator
            I = collected_loads.begin(), E = collected_loads.end();
            I != E; ++I) {
          int fi;
          (void) isUncondLoad(I->first, fi);
          if (I->second.test(normalizeFI(fi))) {
            Removables.insert(I->first);
          }
        }
      }

      void findRedundantStores(void) {
        // backward DF problems
        std::map<MachineInstr *,
                 std::pair<BitVector, BitVector> > collected_stores;
        std::queue<MachineBasicBlock *> worklist;

        // fill worklist initially in dfs postorder
        for (po_iterator<MachineBasicBlock *> POI = po_begin(&MF.front()),
            POIe = po_end(&MF.front());  POI != POIe; ++POI) {
          worklist.push(*POI);
        }

        // iterate.
        while (!worklist.empty()) {
          // pop first element
          MachineBasicBlock *MBB = worklist.front();
          worklist.pop();
          Blockinfo &BI = this->BlockInfos.at(MBB);

          BitVector subseqstores(NumFIs, true);
          BitVector futureloads(NumFIs);
          if (MBB->succ_size() > 0) {
            for (MachineBasicBlock::succ_iterator SI = MBB->succ_begin();
                SI != MBB->succ_end(); ++SI) {
              Blockinfo &BISucc = this->BlockInfos.at(*SI);
              futureloads  |= BISucc.FutureLoadsEntry;
              subseqstores &= BISucc.SubseqStoresEntry;
            }
          } else {
            subseqstores.reset();
          }
          BI.FutureLoadsExit = futureloads;
          BI.SubseqStoresExit = subseqstores;

          // transfer
          for (MachineBasicBlock::reverse_iterator MI = MBB->rbegin(),
              MIe = MBB->rend(); MI != MIe; ++MI) {
            int fi;
            if (isUncondLoad(&*MI, fi)) {
              int nfi = normalizeFI(fi);
              futureloads.set(nfi);
              if (!subseqstores.test(nfi)) subseqstores.reset();
              continue;
            }
            if (isUncondStore(&*MI, fi)) {
              // remember store-inst with futureloads/subseq-st at exit
              collected_stores[&*MI] = std::make_pair(futureloads,
                                                      subseqstores);
              // update
              subseqstores.reset();
              subseqstores.set(normalizeFI(fi));
              continue;
            }
          }

          // was an update?
          if (BI.FutureLoadsEntry  != futureloads ||
              BI.SubseqStoresEntry != subseqstores) {
            BI.FutureLoadsEntry  = futureloads;
            BI.SubseqStoresEntry = subseqstores;
            // add predecessors to worklist
            for (MachineBasicBlock::pred_iterator PI=MBB->pred_begin();
                PI!=MBB->pred_end(); ++PI) {
              worklist.push(*PI);
            }
          }
        }

        // Now iterate through the collected store instructions.
        // If the fi of a store is covered by a subsequent store, or the
        // fi is never loaded again in the future, the store can be removed.
        for (std::map<MachineInstr *,
                      std::pair<BitVector, BitVector> >::iterator
             I = collected_stores.begin(), E = collected_stores.end();
             I != E; ++I) {
          int fi, nfi;
          (void) isUncondStore(I->first, fi);
          nfi = normalizeFI(fi);
          BitVector futureloads  = I->second.first;
          BitVector subseqstores = I->second.second;
          if (subseqstores.test(nfi) || !futureloads.test(nfi)) {
            Removables.insert(I->first);
          }
        }
      }

  };

}

char PatmosSPReduce::ID = 0;

/// createPatmosSPReducePass - Returns a new PatmosSPReduce
/// \see PatmosSPReduce
FunctionPass *llvm::createPatmosSPReducePass(const PatmosTargetMachine &tm) {
  return new PatmosSPReduce(tm);
}

///////////////////////////////////////////////////////////////////////////////
//  PatmosSPReduce methods
///////////////////////////////////////////////////////////////////////////////

void PatmosSPReduce::doReduceFunction(MachineFunction &MF) {

  LLVM_DEBUG( dbgs() << "BEFORE Single-Path Reduce\n"; MF.dump() );

  MachineRegisterInfo &RegInfo = MF.getRegInfo();

  AvailPredRegs.clear();
  UnavailPredRegs.clear();
  // Get the unused predicate registers
  LLVM_DEBUG( dbgs() << "Available PRegs:" );
  for (TargetRegisterClass::iterator I=Patmos::PRegsRegClass.begin(),
      E=Patmos::PRegsRegClass.end(); I!=E; ++I ) {
    if (RegInfo.reg_empty(*I) && *I!=Patmos::P0) {
      AvailPredRegs.push_back(*I);
      LLVM_DEBUG( dbgs() << " " << TRI->getName(*I) );
    } else {
      UnavailPredRegs.push_back(*I);
    }
  }
  LLVM_DEBUG( dbgs() << "\n" );

  GuardsReg = Patmos::R26;
  // Get a temporary predicate register, which must not be used for allocation
  PRTmp     = AvailPredRegs.back();
  AvailPredRegs.pop_back();

  LLVM_DEBUG( dbgs() << "RegAlloc\n" );
  RAInfos.clear();
  RAInfos = RAInfo::computeRegAlloc(RootScope, AvailPredRegs.size());

  // before inserting code, we need to obtain additional instructions that are
  // spared from predication (i.e. need to execute unconditionally)
  // -> instructions that store/restore return information
  // NB: we execute the whole frame setup unconditionally!
  //collectReturnInfoInsts(MF);

  // Guard the instructions (no particular order necessary)
  for (auto iter = df_begin(RootScope), end = df_end(RootScope);
        iter != end; ++iter) {
    applyPredicates(*iter, MF);
  }
  // Insert predicate definitions (no particular order necessary)
  for (auto iter = df_begin(RootScope), end = df_end(RootScope);
        iter != end; ++iter) {
    auto scope = *iter;
    insertPredDefinitions(scope);
    insertStackLocInitializations(scope);
  }

  // After all scopes are handled, perform some global fixups

  // Fixup kill flag of condition predicate registers
  fixupKillFlagOfCondRegs();

  // we create an instance of the eliminator here, such that we can
  // insert dummy instructions for analysis and mark them as 'to be removed'
  // with the eliminator
  GuardsLdStElim = new RedundantLdStEliminator(MF, TRI, GuardsReg, *PMFI);

  // Following walk of the SPScope tree linearizes the CFG structure,
  // inserting MBBs as required (preheader, spill/restore, loop counts, ...)
  LLVM_DEBUG( dbgs() << "Linearize MBBs\n" );
  LinearizeWalker LW(*this, MF);
  RootScope->walk(LW);

  // Following function merges MBBs in the linearized CFG in order to
  // simplify it
  mergeMBBs(MF);

  // Perform the elimination of LD/ST on the large basic blocks
  ElimLdStCnt += GuardsLdStElim->process();
  delete GuardsLdStElim;


  // Remove frame index operands from inserted loads and stores to stack
  eliminateFrameIndices(MF);

  // Finally, we assign numbers in ascending order to MBBs again.
  MF.RenumberBlocks();

  LLVM_DEBUG( dbgs() << "AFTER Single-Path Reduce\n"; MF.dump() );
  LLVM_DEBUG({
      dbgs() << "Scope tree after Reduction:\n";
      RootScope->dump(dbgs(), 0, true);
  });
}

SmallVector<MachineOperand, 2> PatmosSPReduce::getEdgeCondition(
    const PredicatedBlock* sourceBlock,
    PredicatedBlock::Definition def) {

  MachineBasicBlock *SrcMBB = sourceBlock->getMBB();

  SmallVector<MachineOperand, 2> condition;
  condition.push_back(def.condPred);
  condition.push_back(def.condFlag);

  if (condition[0].isKill()) {
    condition[0].setIsKill(false);
    // remember MBBs which have their final branch condition killed
    if (!KilledCondRegs.count(SrcMBB)) {
      KilledCondRegs.insert(std::make_pair(SrcMBB, condition[0]));
    }
  }
  return condition;
}

void PatmosSPReduce::insertStackLocInitializations(SPScope *S) {
  LLVM_DEBUG( dbgs() << " Insert StackLoc Initializations in [MBB#"
                << S->getHeader()->getMBB()->getNumber() << "]\n");

  // register allocation information
  RAInfo &R = RAInfos.at(S);

  // Create the masks
  std::map<int, uint32_t> masks;
  LLVM_DEBUG(dbgs() << "  - Stack Loc: " );
  for(auto pred: S->getAllPredicates()){
    // We don't clear the header predicate
    if(pred == *S->getHeader()->getBlockPredicates().begin()) continue;
    RAInfo::LocType type; unsigned stloc;
    std::tie(type, stloc) = R.getDefLoc(pred);

    if (type == RAInfo::Stack) {
      int fi; unsigned bitpos;
      getStackLocPair(fi, bitpos, stloc);
      LLVM_DEBUG(dbgs() << "p" << pred << " " << stloc
          << " ("  << fi  << "/" << bitpos << "); ");
      if (!masks.count(fi)) {
        masks[fi] = 0;
      }
      masks[fi] |= (1 << bitpos);
    }
  }
  LLVM_DEBUG(dbgs() << "\n");

  // Clear stack locations according to masks, at the beginning of the header
  MachineBasicBlock *MBB = S->getHeader()->getMBB();
  MachineBasicBlock::iterator MI = MBB->begin();
  if (S->isTopLevel()) {
    // skip frame setup
    while (MI->getFlag(MachineInstr::FrameSetup)) ++MI;
  }

  LLVM_DEBUG(dbgs() << "  - Masks:\n" );
  DebugLoc DL;
  for (std::map<int, uint32_t>::iterator I = masks.begin(), E = masks.end();
      I != E; ++I) {
    int fi = I->first;
    uint32_t mask = I->second;
    LLVM_DEBUG(dbgs() << "    fi " << fi  << " mask " << mask << "\n");
    // load from stack slot
    AddDefaultPred(BuildMI(*MBB, MI, DL, TII->get(Patmos::LWC), GuardsReg))
      .addFrameIndex(fi).addImm(0); // address
    // insert AND instruction to clear predicates according to mask
    AddDefaultPred(BuildMI(*MBB, MI, DL, TII->get(Patmos::ANDl),
          GuardsReg))
      .addReg(GuardsReg)
      .addImm(~mask);
    // store to stack slot
    AddDefaultPred(BuildMI(*MBB, MI, DL, TII->get(Patmos::SWC)))
      .addFrameIndex(fi).addImm(0) // address
      .addReg(GuardsReg, RegState::Kill);
    InsertedInstrs += 3; // STATISTIC
  }
}

void PatmosSPReduce::insertPredDefinitions(SPScope *S) {
  LLVM_DEBUG( dbgs() << " Insert Predicate Definitions in [MBB#"
                << S->getHeader()->getMBB()->getNumber() << "]\n");

  auto blocks = S->getScopeBlocks();
  RAInfo &R = RAInfos.at(S);// local scope of definitions
  for(auto block: blocks){

    // inner scope
    RAInfo &RI = S->isSubheader(block) ? RAInfos.at(S->findScopeOf(block))
                                       : R;
    // get the guard register from the source block
    auto useLocs = getPredicateRegisters(RI, block);
    auto defs = block->getDefinitions();

    LLVM_DEBUG(
      dbgs() << " - MBB#" << block->getMBB()->getNumber() << ":\n";
      dbgs() << "  Definitions before sorting: [";
      for(auto def: defs){
        auto tuple = R.getDefLoc(def.predicate);
        dbgs() << "(" << (std::get<0>(tuple) == RAInfo::Register ? "R ": "") << 
        AvailPredRegs[std::get<1>(tuple)] << ", " << 
        (useLocs.count(def.guard)? useLocs[def.guard] : Patmos::P0) << "), ";
      }
      dbgs() << "]\n";
    );
    
	// We sort the definitions, such that no definition
    // overwrite the guard predicate of a succeeding definition.
    // We also track for each definition if it's the first definition
    // of the given predicate (the bool).
    std::vector<std::tuple< 
      PredicateDefinition,
      // If not swap, whether this definition is the first for this predicate
      // If swap, undefined
      bool>
    > defs_sorted;

    for(auto def: defs){
      RAInfo::LocType x_pred_type; unsigned x_pred_loc;
      std::tie(x_pred_type, x_pred_loc) = R.getDefLoc(def.predicate);
	  x_pred_loc = x_pred_type == RAInfo::Register ? AvailPredRegs[x_pred_loc] : x_pred_loc;
      unsigned x_guard_reg = useLocs.count(def.guard)? useLocs[def.guard] : Patmos::P0;
	  
	  auto insert_at = defs_sorted.end();
      auto is_first_pred_def = R.isFirstDef(block->getMBB(), def.predicate);
      auto cond = getEdgeCondition(block, def);

      PredicateDefinition x_def(x_pred_type, x_pred_loc, x_guard_reg, cond);

      auto iter = defs_sorted.begin();
      for(auto end=defs_sorted.end(); iter<end; iter++) {
		assert(!x_def.is_swap() && "Shouldn't be possible");
		
		auto y_def = std::get<0>(*iter);
        
        if( y_def.is_swap() ) {
          if( y_def.overwrites_guard_of(x_def) ) {
            insert_at = iter; 
          }
          assert( 
            !x_def.overwrites_guard_of(y_def) &&
            "Definition for predicate that is already part of a swap definition"
          );
        } else if( x_def.merge_into_swap(y_def) ) {
          iter = defs_sorted.erase(iter);
          insert_at = defs_sorted.end();
          end=insert_at; //update to make its valid.
          break; // end loop to ensure swap is insertet at the end
        } else if( y_def.overwrites_guard_of(x_def) && insert_at == end ){
          insert_at = iter; 

          if( x_def.share_predicate_target(y_def)){
            // If it was the first, it no longer is.
            std::get<1>(*iter) = false; 
          }
        } else if( x_def.share_predicate_target(y_def) ){	
          if( insert_at == end ) {
            // We will insert 'x_def' after the other
            is_first_pred_def = false;
          } else {
            // We have already inserted 'x_def' before this definition
            std::get<1>(*iter) = false; 
          }
        }
      }
      defs_sorted.insert(insert_at, std::make_tuple(x_def, is_first_pred_def));
    }

    LLVM_DEBUG(
      dbgs() << "  Definitions after sorting: [";
      for(auto entry: defs_sorted){
		auto def = std::get<0>(entry);
        dbgs() << "(";
		dbgs().flush();
        if(	def.is_swap() ){
          dbgs() << "SWP ";
		  dbgs() << std::get<0>(entry).swap.predicate_1 << ", " << 
          std::get<0>(entry).swap.predicate_2 << "), ";
        } else if( def.is_simple() ){
          dbgs() << (def.simple.type == RAInfo::Register ? "R": "") 
                 << (std::get<1>(entry) ? "F ": " ");
		  dbgs() << std::get<0>(entry).simple.predicate << ", " << 
          std::get<0>(entry).simple.guard << "), ";
        }
      }
      dbgs() << "]\n";
    );
	
    for(auto entry: defs_sorted){
      auto definition = std::get<0>(entry);
	  SmallVector<MachineOperand, 2> cond1;
	  cond1.push_back(definition.conditions[0]);
	  cond1.push_back(definition.conditions[1]);
      if( definition.is_swap() ){
        auto pred1 = definition.swap.predicate_1, pred2 = definition.swap.predicate_2;
        LLVM_DEBUG(dbgs() << "Insert Swap Definition Pred(" << pred1 << ") Pred2(" << pred2 << ")\n");

        auto insertPXOR = [&](auto r1, auto r2){
          MachineBasicBlock::iterator MI = block->getMBB()->getFirstTerminator();
          DebugLoc DL(MI->getDebugLoc());

          AddDefaultPred(BuildMI(*block->getMBB(), MI, DL,
            TII->get(Patmos::PXOR), r1)
          )
          .addReg(r1).addImm(0)
          .addReg(r2).addImm(0);

          InsertedInstrs++; // STATISTIC
        };

        // We first swap the values of the two predicates
        insertPXOR(pred1, pred2);
        insertPXOR(pred2, pred1);
        insertPXOR(pred1, pred2);
		
		SmallVector<MachineOperand, 2> cond2;
		cond2.push_back(definition.conditions[2]);
		cond2.push_back(definition.conditions[3]);

        // Then define them using the swapped guards, 
        // i.e. each register becomes its own guard
        insertDefEdge(S, block, RAInfo::Register, pred1, pred1, cond1, true);
        insertDefEdge(S, block, RAInfo::Register, pred2, pred2, cond2, true);
      }	else if( definition.is_simple() ) {	
        insertDefEdge(S, block, definition.simple.type, definition.simple.predicate, definition.simple.guard, cond1, std::get<1>(entry));
      } else {
		report_fatal_error("PatmosSPReduce::insertPredDefinitions unsupported definition type."); 
	  }
    }
    LLVM_DEBUG(dbgs() << "\n");
  }
}

void PatmosSPReduce::insertDefEdge(SPScope *S, const PredicatedBlock *block,
     RAInfo::LocType predType, unsigned predLoc, unsigned guardLoc, SmallVector<MachineOperand, 2> cond,
     bool first_def)
{  
  // the MBB we need to insert the defining instruction is the edge source
  MachineBasicBlock &SrcMBB = *block->getMBB();
							
  if (predType == RAInfo::Register) {
    RAInfo &RI = S->isSubheader(block) ? RAInfos.at(S->findScopeOf(block))
                                       : RAInfos.at(S);
    if (!S->isSubheader(block) || !RI.needsScopeSpill()) {

      // FIXME assumes direct parent-child relationship, if nested
      assert(!S->isSubheader(block) || (RI.Scope->getParent() == S));

      // The definition location of the predicate is a physical register.
      insertDefToRegLoc(
          SrcMBB, predLoc, guardLoc, cond,
          !first_def || S->isSubheader(block)
          );
    } else {
      // Assume direct parent-child relationship
      assert(RI.Scope->getParent() == S);
	  
      unsigned slot = RI.Scope->getDepth()-1;

      LLVM_DEBUG(dbgs() << "Insert Definition to S0 Spill. Pred(" << predLoc << ") Guard(" << guardLoc << ")\n");

      // set a bit in the appropriate S0 spill slot
      insertDefToS0SpillSlot(SrcMBB, slot, predLoc, guardLoc, cond);
    }
  } else {
    LLVM_DEBUG(dbgs() << "Insert Definition to stack. Loac(" << predLoc << ") Guard(" << guardLoc << ")\n");
    insertDefToStackLoc(SrcMBB, predLoc, guardLoc, cond);
  }
}

void PatmosSPReduce::
insertDefToRegLoc(MachineBasicBlock &MBB, unsigned predReg, unsigned guard,
                  const SmallVectorImpl<MachineOperand> &Cond,
                  bool usePmov) {
  LLVM_DEBUG(dbgs() << "Insert Register Definition Pred(" << predReg << ") Guard(" << guard << ") using ");
  
  // insert the predicate definitions before any branch at the MBB end
  MachineBasicBlock::iterator MI = MBB.getFirstTerminator();
  DebugLoc DL(MI->getDebugLoc());
  MachineInstr *DefMI;
  if (usePmov) {
    LLVM_DEBUG(dbgs() << "PMOV\n");

    DefMI = BuildMI(MBB, MI, DL,
        TII->get(Patmos::PMOV), predReg)
      .addReg(guard).addImm(0) // guard operand
      .add(Cond[0]).add(Cond[1]); // condition
  } else {
    LLVM_DEBUG(dbgs() << "PAND\n");

    // the PAND instruction must not be predicated
    DefMI = AddDefaultPred(BuildMI(MBB, MI, DL,
          TII->get(Patmos::PAND), predReg))
      .addReg(guard).addImm(0) // current guard as source
      .add(Cond[0]).add(Cond[1]); // condition
  }
  InsertedInstrs++; // STATISTIC
}

void PatmosSPReduce::
insertDefToStackLoc(MachineBasicBlock &MBB, unsigned stloc, unsigned guard,
                    const SmallVectorImpl<MachineOperand> &Cond) {

  // insert the predicate definitions before any branch at the MBB end
  MachineBasicBlock::iterator MI = MBB.getFirstTerminator();
  DebugLoc DL(MI->getDebugLoc());

  // The definition location of the predicate is a spill location.
  int fi; unsigned bitpos;
  getStackLocPair(fi, bitpos, stloc);
  unsigned tmpReg = GuardsReg;

  // load from stack slot
  AddDefaultPred(BuildMI(MBB, MI, DL, TII->get(Patmos::LWC), tmpReg))
    .addFrameIndex(fi).addImm(0); // address

#ifdef USE_BCOPY
  // (guard) bcopy R, bitpos, Cond
  BuildMI(MBB, MI, DL, TII->get(Patmos::BCOPY), tmpReg)
    .addReg(guard).addImm(0) // guard
    .addReg(tmpReg)
    .addImm(bitpos)
    .add(Cond[0]).add(Cond[1]); // condition
  InsertedInstrs++; // STATISTIC
#else
  // clear bit on first definition (unconditionally)
  uint32_t or_bitmask = 1 << bitpos;
  // compute combined predicate (guard && condition)
  AddDefaultPred(BuildMI(MBB, MI, DL,
        TII->get(Patmos::PAND), PRTmp))
    .addReg(guard).addImm(0) // guard
    .addOperand(Cond[0]).addOperand(Cond[1]); // condition
  // set bit
  // if (guard && cond) R |= (1 << loc)
  unsigned or_opcode = (isUInt<12>(or_bitmask)) ? Patmos::ORi : Patmos::ORl;
  BuildMI(MBB, MI, DL, TII->get(or_opcode), tmpReg)
    .addReg(PRTmp).addImm(0) // if (guard && cond) == true
    .addReg(tmpReg)
    .addImm(or_bitmask);
  InsertedInstrs += 2; // STATISTIC
#endif
  // store back to stack slot
  AddDefaultPred(BuildMI(MBB, MI, DL, TII->get(Patmos::SWC)))
    .addFrameIndex(fi).addImm(0) // address
    .addReg(tmpReg, RegState::Kill);
  InsertedInstrs += 2; // STATISTIC
}


void PatmosSPReduce::
insertDefToS0SpillSlot(MachineBasicBlock &MBB, unsigned slot, unsigned predReg,
                       unsigned guard,
                       const SmallVectorImpl<MachineOperand> &Cond) {

  // insert the predicate definitions before any branch at the MBB end
  MachineBasicBlock::iterator MI = MBB.getFirstTerminator();
  DebugLoc DL(MI->getDebugLoc());

  int fi = PMFI->getSinglePathS0SpillFI(slot);
  unsigned tmpReg = GuardsReg;
  int bitpos = TRI->getS0Index(predReg);
  assert(bitpos > 0);

  // load from stack slot
  AddDefaultPred(BuildMI(MBB, MI, DL,
        TII->get(Patmos::LBC), tmpReg))
    .addFrameIndex(fi).addImm(0); // address

#ifdef USE_BCOPY
  // (guard) bcopy R, bitpos, Cond
  BuildMI(MBB, MI, DL, TII->get(Patmos::BCOPY), tmpReg)
    .addReg(guard).addImm(0) // guard
    .addReg(tmpReg)
    .addImm(bitpos)
    .add(Cond[0]).add(Cond[1]); // condition
  InsertedInstrs++; // STATISTIC
#else
  uint32_t or_bitmask = 1 << bitpos;
  // compute combined predicate (guard && condition)
  AddDefaultPred(BuildMI(MBB, MI, DL,
        TII->get(Patmos::PAND), PRTmp))
    .addReg(guard).addImm(0) // guard
    .addOperand(Cond[0]).addOperand(Cond[1]); // condition
  // set bit
  // if (guard && cond) R |= (1 << loc)
  assert(isUInt<12>(or_bitmask);
  BuildMI(MBB, MI, DL, TII->get(Patmos::ORi), tmpReg)
    .addReg(PRTmp).addImm(0) // if (guard && cond) == true
    .addReg(tmpReg)
    .addImm(or_bitmask);
  InsertedInstrs += 2; // STATISTIC
#endif
  // store back to stack slot
  AddDefaultPred(BuildMI(MBB, MI, DL, TII->get(Patmos::SBC)))
    .addFrameIndex(fi).addImm(0) // address
    .addReg(tmpReg, RegState::Kill);
  InsertedInstrs += 2; // STATISTIC
}

void PatmosSPReduce::fixupKillFlagOfCondRegs(void) {
  for (std::map<MachineBasicBlock *, MachineOperand>::iterator
        I = KilledCondRegs.begin(), E = KilledCondRegs.end(); I != E; ++I) {

    MachineBasicBlock *MBB = (*I).first;
    MachineOperand CondReg = (*I).second;

    MachineBasicBlock::iterator firstTI = MBB->getFirstTerminator();

    // restore kill flag at the last use
    // To this end, we search the instruction in which it was last used.
    for (auto lastMI = std::prev(firstTI),
        firstMI = MBB->begin();
        lastMI != firstMI; --lastMI) {
      MachineOperand *MO;
      if ((MO = lastMI->findRegisterUseOperand(CondReg.getReg())) != NULL) {
        MO->setIsKill(true);
        break;
      }
    } // end of search

  } // end for all elements in KilledCondRegs
  KilledCondRegs.clear();
}


void PatmosSPReduce::applyPredicates(SPScope *S, MachineFunction &MF) {
  LLVM_DEBUG( dbgs() << " Applying predicates in [MBB#"
                << S->getHeader()->getMBB()->getNumber() << "]\n");

  const RAInfo &R = RAInfos.at(S);

  // Predicate the instructions of blocks in S, also inserting spill/load
  // of predicates not in registers.

  auto blocks = S->getScopeBlocks();
  for(auto block: blocks){
    auto MBB = block->getMBB();
    auto instrPreds = block->getInstructionPredicates();
    auto predRegs = getPredicateRegisters(R, block);

    // apply predicate to all instructions in block
    for( auto MI = MBB->instr_begin(), ME = MBB->getFirstInstrTerminator();
                                     MI != ME; ++MI) {

      if (MI->isReturn()) {
          DEBUG_TRACE( dbgs() << "    skip return: " << *MI );
          continue;
      }
      if (TII->isStackControl(&*MI)) {
          DEBUG_TRACE( dbgs() << "    skip stack control: " << *MI );
          continue;
      }
      if (MI->getFlag(MachineInstr::FrameSetup)) {
          continue;
          DEBUG_TRACE(dbgs() << "    skip frame setup: " << *MI);
      }
      if (ReturnInfoInsts.count(&*MI)) {
          DEBUG_TRACE(dbgs() << "    skip return info (re-)storing: " << *MI);
          continue;
      }

      assert(instrPreds.count(&(*MI)));
      auto instrPred = instrPreds[&(*MI)];
      auto predReg = predRegs.count(instrPred) ? predRegs[instrPred] : Patmos::P0;
      DEBUG_TRACE( dbgs() << "Predicate (" << instrPred << ") set to register: (" << predReg << ")\n");
      if (MI->isCall()) {
          DEBUG_TRACE( dbgs() << "    call: " << *MI );
          assert(!TII->isPredicated(*MI) && "call predicated");
          DebugLoc DL = MI->getDebugLoc();
          // copy actual preg to temporary preg
          AddDefaultPred(BuildMI(*MBB, MI, DL,
                TII->get(Patmos::PMOV), PRTmp))
            .addReg(predReg).addImm(0);

          // store/restore caller saved R9 (gets dirty during frame setup)
          int fi = PMFI->getSinglePathCallSpillFI();
          // store to stack slot
          AddDefaultPred(BuildMI(*MBB, MI, DL, TII->get(Patmos::SWC)))
            .addFrameIndex(fi).addImm(0) // address
            .addReg(Patmos::R9, RegState::Kill);
          // restore from stack slot (after the call MI)
          AddDefaultPred(BuildMI(*MBB, std::next(MI), DL,
                TII->get(Patmos::LWC), Patmos::R9))
            .addFrameIndex(fi).addImm(0); // address
          ++MI; // skip the load operation
          InsertedInstrs += 3; // STATISTIC
          continue;
      }

      if (MI->isPredicable(MachineInstr::QueryType::IgnoreBundle) && predReg != Patmos::P0) {
        auto isPredicated = [&](auto instr){
          int i = instr->findFirstPredOperandIdx();
          if (i != -1) {
            unsigned preg = instr->getOperand(i).getReg();
            int      flag = instr->getOperand(++i).getImm();
            return (preg!=Patmos::NoRegister && preg!=Patmos::P0) || flag;
          }
          // no predicates at all
          return false;
        };
        if (!isPredicated(MI)) {
          // find first predicate operand
          int i = MI->findFirstPredOperandIdx();
          assert(i != -1);
          MachineOperand &PO1 = MI->getOperand(i);
          MachineOperand &PO2 = MI->getOperand(i+1);
          assert(PO1.isReg() && PO2.isImm() &&
                 "Unexpected Patmos predicate operand");
          PO1.setReg(predReg);
          PO2.setImm(0);
        } else {
          DEBUG_TRACE( dbgs() << "    in MBB#" << MBB->getNumber()
                        << ": instruction already predicated: " << *MI );
          // read out the predicate
          int i = MI->findFirstPredOperandIdx();
          assert(i != -1);
          MachineOperand &PO1 = MI->getOperand(i);
          MachineOperand &PO2 = MI->getOperand(i+1);
          if (!(PO1.getReg() == predReg && PO2.getImm() == 0)) {
            // build a new predicate := use_preg & old pred
            AddDefaultPred(BuildMI(*MBB, MI, MI->getDebugLoc(),
                                TII->get(Patmos::PAND), PRTmp))
                  .addReg(predReg).addImm(0)
                  .add(PO1).add(PO2);
            PO1.setReg(PRTmp);
            PO2.setImm(0);
            InsertedInstrs++; // STATISTIC
          }
        }
      }
    } // for each instruction in MBB

    // insert spill and load instructions for the guard register
    if (!S->isHeader(block) && R.hasSpillLoad(MBB)) {
      insertUseSpillLoad(R, block);
    }

    // if this is a reachable function, we need to get the
    // top-level predicate from the caller
    if (S->isTopLevel() && !S->isRootTopLevel() && S->isHeader(block)) {
      // skip unconditionally executed frame setup
      MachineBasicBlock::iterator MI = MBB->begin();
      while (MI->getFlag(MachineInstr::FrameSetup)) ++MI;

      auto headerPreds = block->getBlockPredicates();
      assert(headerPreds.size() == 1);
      // HT
      auto pred = *block->getBlockPredicates().begin();
      assert(predRegs.count(pred));
      auto predReg = predRegs[pred];

      AddDefaultPred(BuildMI(*MBB, MI, MI->getDebugLoc(),
            TII->get(Patmos::PMOV), predReg))
        .addReg(PRTmp).addImm(0);
    }
  }
}

std::map<unsigned, unsigned> PatmosSPReduce::getPredicateRegisters(const RAInfo &R,
                                    const PredicatedBlock *block)
{
  auto uls =  R.getUseLocs(block->getMBB());

  // We replace all location with the register they represent
  for(auto iter = uls.begin(), end = uls.end(); iter != end; iter++){
    assert(iter->second < AvailPredRegs.size());
    iter->second = AvailPredRegs[iter->second];
  }

  return uls;
}

void PatmosSPReduce::getStackLocPair(int &fi, unsigned &bitpos,
                                     const unsigned stloc) const {
  fi = PMFI->getSinglePathExcessSpillFI(stloc / 32);
  bitpos = stloc % 32;
}


void PatmosSPReduce::insertUseSpillLoad(const RAInfo &R,
                                        PredicatedBlock *block) {
  auto MBB = block->getMBB();
  auto spillLocs = R.getSpillLocs(MBB);
  auto loadLocs = R.getLoadLocs(MBB);
  auto useLocs = getPredicateRegisters(R, block);

  // All spills must be followed by a load
  for(auto spillLoc: spillLocs){
    assert(loadLocs.count(spillLoc.first));
  }

  for(auto loadLoc: loadLocs){
    auto pred = loadLoc.first;
    MachineBasicBlock::iterator firstMI = MBB->begin();
    DebugLoc DL;
    assert(useLocs.count(pred));
    auto use_preg = useLocs[pred];

    // insert spill code
    if(spillLocs.count(pred)){
      auto spill = spillLocs[pred];

      int fi; unsigned bitpos;
      getStackLocPair(fi, bitpos, spill);
      // load from stack slot
      AddDefaultPred(BuildMI(*MBB, firstMI, DL,
            TII->get(Patmos::LWC), GuardsReg))
        .addFrameIndex(fi).addImm(0); // address
      // set/clear bit
#ifdef USE_BCOPY
      // (guard) bcopy R, (spill%32), use_preg
      AddDefaultPred(BuildMI(*MBB, firstMI, DL,
            TII->get(Patmos::BCOPY), GuardsReg))
        .addReg(GuardsReg)
        .addImm(bitpos)
        .addReg(use_preg).addImm(0); // condition
      InsertedInstrs++; // STATISTIC
#else
      // if (guard) R |= (1 << spill)
      uint32_t or_bitmask = 1 << bitpos;
      unsigned or_opcode = (isUInt<12>(or_bitmask))? Patmos::ORi : Patmos::ORl;
      BuildMI(*MBB, firstMI, DL, TII->get(or_opcode), GuardsReg)
        .addReg(use_preg).addImm(0) // if guard == true
        .addReg(GuardsReg)
        .addImm( or_bitmask );
      // if (!guard) R &= ~(1 << spill)
      BuildMI(*MBB, firstMI, DL, TII->get(Patmos::ANDl), GuardsReg)
        .addReg(use_preg).addImm(1) // if guard == false
        .addReg(GuardsReg)
        .addImm( ~or_bitmask );
      InsertedInstrs += 2; // STATISTIC
#endif
      // store back to stack slot
      AddDefaultPred(BuildMI(*MBB, firstMI, DL, TII->get(Patmos::SWC)))
        .addFrameIndex(fi).addImm(0) // address
        .addReg(GuardsReg, RegState::Kill);
      InsertedInstrs += 2; // STATISTIC (load/store)
    }

    insertPredicateLoad(MBB, firstMI, loadLoc.second, use_preg);
  }
}


void PatmosSPReduce::insertPredicateLoad(MachineBasicBlock *MBB,
                                         MachineBasicBlock::iterator MI,
                                         int loc, unsigned target_preg) {
  assert(loc != -1);
  DebugLoc DL;
  int fi; unsigned bitpos;
  getStackLocPair(fi, bitpos, loc);
  // load from stack slot
  AddDefaultPred(BuildMI(*MBB, MI, DL, TII->get(Patmos::LWC), GuardsReg))
    .addFrameIndex(fi).addImm(0); // address
  // test bit
  // BTESTI $Guards, loc
  AddDefaultPred(BuildMI(*MBB, MI, DL, TII->get(Patmos::BTESTI), target_preg))
    .addReg(GuardsReg, RegState::Kill).addImm(bitpos);
  InsertedInstrs += 2; // STATISTIC
}


void PatmosSPReduce::mergeMBBs(MachineFunction &MF) {
  LLVM_DEBUG( dbgs() << "Function before block merge:\n"; MF.dump() );
  LLVM_DEBUG({
    dbgs() << "Scope tree before block merge:\n";
    RootScope->dump(dbgs(), 0, true);
  });
  // first, obtain the sequence of MBBs in DF order (as copy!)
  // NB: have to use the version below, as some version of libcxx will not
  // compile it (similar to
  //    http://lists.cs.uiuc.edu/pipermail/cfe-commits/Week-of-Mon-20130325/076850.html)
  //std::vector<MachineBasicBlock*> order(df_begin(&MF.front()),
  //                                      df_end(  &MF.front()));
  std::vector<MachineBasicBlock*> order;
  for (df_iterator<MachineBasicBlock *> I = df_begin(&MF.front()),
       E = df_end(&MF.front()); I != E; ++I) {
      order.push_back(*I);
  }


  std::vector<MachineBasicBlock*>::iterator I = order.begin(),
                                            E = order.end();

  MachineBasicBlock *BaseMBB = *I;
  auto BaseBlock = RootScope->findBlockOf(BaseMBB);
  DEBUG_TRACE( dbgs() << "Base MBB#" << BaseMBB->getNumber() << "\n" );
  // iterate through order of MBBs
  while (++I != E) {
    // get MBB of iterator
    MachineBasicBlock *MBB = *I;

    if (MBB->pred_size() == 1) {
      DEBUG_TRACE( dbgs() << "  Merge MBB#" << MBB->getNumber() << "\n" );
      // transfer the instructions
      BaseMBB->splice(BaseMBB->end(), MBB, MBB->begin(), MBB->end());
      // remove the edge between BaseMBB and MBB
      BaseMBB->removeSuccessor(MBB);
      // BaseMBB gets the successors of MBB instead
      BaseMBB->transferSuccessors(MBB);
      // remove MBB from MachineFunction
      MF.erase(MBB);

      auto MBBBlock = RootScope->findBlockOf(MBB);
      if(BaseBlock){
        if(MBBBlock){
          RootScope->merge(BaseBlock, MBBBlock);
        }
      }else{
        if(MBBBlock){
          MBBBlock->replaceMbb(BaseMBB);
        }
      }

      if (BaseMBB->succ_size() > 1) {
        // we have encountered a backedge
        BaseMBB = *(++I);
        DEBUG_TRACE( dbgs() << "Base MBB#" << BaseMBB->getNumber() << "\n" );
      }
    } else {
      BaseMBB = MBB;
      DEBUG_TRACE( dbgs() << "Base MBB#" << BaseMBB->getNumber() << "\n" );
    }
    BaseBlock = RootScope->findBlockOf(BaseMBB);
  }
  // invalidate order
  order.clear();
}


void PatmosSPReduce::collectReturnInfoInsts(MachineFunction &MF) {
  LLVM_DEBUG( dbgs() << "Collect return info insts\n" );

  ReturnInfoInsts.clear();

  SmallSet<unsigned, 4> SpecialRegs;
  SpecialRegs.insert(Patmos::SRB);
  SpecialRegs.insert(Patmos::SRO);
  SpecialRegs.insert(Patmos::S0);

  for (MachineFunction::iterator MBB = MF.begin(), MBBe = MF.end();
      MBB != MBBe; ++MBB) {
    for (MachineBasicBlock::iterator MI = MBB->begin(), MIe = MBB->end();
        MI != MIe; ++MI) {

      if (!MI->getFlag(MachineInstr::FrameSetup)) continue;

      if (MI->getOpcode() == Patmos::MFS &&
          SpecialRegs.count(MI->getOperand(3).getReg())) {
        // store return info in prologue (reads SRB/SRO)
        ReturnInfoInsts.insert(&*MI);
        LLVM_DEBUG(dbgs() << "   in MBB#" << MBB->getNumber() << ": "; MI->dump());
        // get reg it defines
        unsigned reg = MI->getOperand(0).getReg();
        // search down for first use of reg (store to stack slot)
        MachineBasicBlock::iterator UMI = MI;
        bool found = false;
        while (++UMI != MIe && !found) {
          // if UMI uses reg
          for (unsigned i = 0; i < UMI->getNumOperands(); i++) {
            const MachineOperand &MO = UMI->getOperand(i);
            if ( MO.isReg() && MO.getReg() == reg) {

              assert(UMI->getFlag(MachineInstr::FrameSetup));
              ReturnInfoInsts.insert(&*UMI);
              LLVM_DEBUG(dbgs() << "         #" << MBB->getNumber() << ": ";
                  UMI->dump());
              found = true;
              break;
            }
          }
        } // end inner loop
        continue;
      }
      if (MI->getOpcode() == Patmos::MTS &&
          SpecialRegs.count(MI->getOperand(0).getReg())) {
        // restore return info in epilogue (writes SRB/SRO)
        ReturnInfoInsts.insert(&*MI);
        LLVM_DEBUG(dbgs() << "   in MBB#" << MBB->getNumber() << ": "; MI->dump());
        // get reg it uses
        unsigned reg = MI->getOperand(3).getReg();
        // search up for def of reg (load from stack slot)
        MachineBasicBlock::iterator DMI = std::prev(MI);
        bool found = false;
        while (!found) {
          // if DMI defines reg
          if (DMI->definesRegister(reg)) {
            assert(DMI->getFlag(MachineInstr::FrameSetup));
            ReturnInfoInsts.insert(&*DMI);
            LLVM_DEBUG(dbgs() << "         #" << MBB->getNumber() << ": ";
                DMI->dump());
            found = true;
            break;
          }
          if (DMI == MBB->begin()) break;
          --DMI;
        } // end inner loop
        continue;
      }
    }

  }
}


void PatmosSPReduce::eliminateFrameIndices(MachineFunction &MF) {

  for (MachineFunction::iterator MBB = MF.begin(), MBBe = MF.end();
      MBB != MBBe; ++MBB) {
    for (MachineBasicBlock::iterator MI = MBB->begin(), MIe = MBB->end();
        MI != MIe; ++MI) {
      if (MI->mayStore() && MI->getOperand(2).isFI()) {
        TRI->eliminateFrameIndex(MI, 0, 2);
      }
      if (MI->mayLoad() && MI->getOperand(3).isFI()) {
        TRI->eliminateFrameIndex(MI, 0, 3);
      }
    }
  }
}


void PatmosSPReduce::getLoopLiveOutPRegs(const SPScope *S,
                                         std::vector<unsigned> &pregs) const {

  auto SuccMBBs = S->getSucceedingBlocks();

  pregs.clear();
  for(auto succ: SuccMBBs) {
    for (unsigned i=0; i<UnavailPredRegs.size(); i++) {
      if (succ->getMBB()->isLiveIn(UnavailPredRegs[i])) {
        LLVM_DEBUG(dbgs() << "LiveIn: " << TRI->getName(UnavailPredRegs[i])
            << " into MBB#" << succ->getMBB()->getNumber() << "\n");
        pregs.push_back(UnavailPredRegs[i]);
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
//  LinearizeWalker methods
///////////////////////////////////////////////////////////////////////////////

void LinearizeWalker::nextMBB(MachineBasicBlock *MBB) {
  DEBUG_TRACE( dbgs() << "| MBB#" << MBB->getNumber() << "\n" );

  // remove all successors
  for ( MachineBasicBlock::succ_iterator SI = MBB->succ_begin();
        SI != MBB->succ_end();
        SI = MBB->removeSuccessor(SI) )
        ; // no body

  // remove the branch at the end of MBB
  // (update statistic counter)
  RemovedBranchInstrs += Pass.TII->removeBranch(*MBB);

  if (LastMBB) {
    // add to the last MBB as successor
    LastMBB->addSuccessor(MBB);
    // move in the code layout
    MBB->moveAfter(LastMBB);
  }
  // keep track of tail
  LastMBB = MBB;
}

void LinearizeWalker::enterSubscope(SPScope *S) {

  // We don't create a preheader for entry.
  if (S->isTopLevel()) return;

  // insert loop preheader to spill predicates / load loop bound
  MachineBasicBlock *PrehdrMBB = MF.CreateMachineBasicBlock();
  MF.push_back(PrehdrMBB);

  const RAInfo &RI = Pass.RAInfos.at(S);

  DebugLoc DL;

  if (RI.needsScopeSpill()) {
    // load the predicate registers to GuardsReg, and store them to the
    // allocated stack slot for this scope depth.
    int fi = Pass.PMFI->getSinglePathS0SpillFI(S->getDepth() - 1);
    Pass.TII->copyPhysReg(*PrehdrMBB, PrehdrMBB->end(), DL,
        Pass.GuardsReg, Patmos::S0, false);
    // we insert a dummy load for the RedundantLdStEliminator
    MachineInstr *Dummy = AddDefaultPred(BuildMI(*PrehdrMBB, PrehdrMBB->end(),
          DL, Pass.TII->get(Patmos::LBC), Pass.GuardsReg))
          .addFrameIndex(fi).addImm(0); // address
    Pass.GuardsLdStElim->addRemovableInst(Dummy);
    AddDefaultPred(BuildMI(*PrehdrMBB, PrehdrMBB->end(), DL,
            Pass.TII->get(Patmos::SBC)))
      .addFrameIndex(fi).addImm(0) // address
      .addReg(Pass.GuardsReg, RegState::Kill);
    InsertedInstrs += 3; // STATISTIC
  }

  insertHeaderPredLoadOrCopy(S, PrehdrMBB, DL);


  // Initialize the loop bound and store it to the stack slot
  if (S->hasLoopBound()) {
    unsigned tmpReg = Pass.GuardsReg;
    uint32_t loop = S->getLoopBound();
    // Create an instruction to load the loop bound
    // TODO try to find an unused register
    AddDefaultPred(BuildMI(*PrehdrMBB, PrehdrMBB->end(), DL,
          Pass.TII->get( (isUInt<12>(loop)) ? Patmos::LIi : Patmos::LIl),
          tmpReg))
      .addImm(loop); // the loop bound

    int fi = Pass.PMFI->getSinglePathLoopCntFI(S->getDepth()-1);
    // we insert a dummy load for the RedundantLdStEliminator
    MachineInstr *Dummy = AddDefaultPred(BuildMI(*PrehdrMBB, PrehdrMBB->end(),
          DL, Pass.TII->get(Patmos::LWC), Pass.GuardsReg))
          .addFrameIndex(fi).addImm(0); // address
    Pass.GuardsLdStElim->addRemovableInst(Dummy);
    // store the initialized loop bound to its stack slot
    AddDefaultPred(BuildMI(*PrehdrMBB, PrehdrMBB->end(), DL,
            Pass.TII->get(Patmos::SWC)))
      .addFrameIndex(fi).addImm(0) // address
      .addReg(tmpReg, RegState::Kill);
    InsertedInstrs += 2; // STATISTIC
    LoopCounters++; // STATISTIC
  }

  // append the preheader
  nextMBB(PrehdrMBB);
}


void LinearizeWalker::exitSubscope(SPScope *S) {

  auto headerBlock = S->getHeader();
  MachineBasicBlock *HeaderMBB = headerBlock->getMBB();
  DEBUG_TRACE( dbgs() << "ScopeRange [MBB#" <<  HeaderMBB->getNumber()
                <<  ", MBB#" <<  LastMBB->getNumber() << "]\n" );

  if (S->isTopLevel()) return;

  const RAInfo &RI = Pass.RAInfos.at(S);
  DebugLoc DL;

  // insert backwards branch to header at the last block
  MachineBasicBlock *BranchMBB = MF.CreateMachineBasicBlock();
  MF.push_back(BranchMBB);
  // weave in before inserting the branch (otherwise it'll be removed again)
  nextMBB(BranchMBB);

  // now we can fill the MBB with instructions:
  //
  // load the header predicate, if necessary
  const auto predRegs = Pass.getPredicateRegisters(RI, headerBlock);
  auto neededLoads = RI.getLoadLocs(HeaderMBB);
  for(auto load: neededLoads){
    Pass.insertPredicateLoad(BranchMBB, BranchMBB->end(),
        load.second, predRegs.at(load.first));
  }

  assert(!S->isTopLevel());
  assert(S->hasLoopBound());
  // load the branch predicate:
  // load the loop counter, decrement it by one, and if it is not (yet)
  // zero, we enter the loop again.
  // TODO is the loop counter in a register?!
  int fi = Pass.PMFI->getSinglePathLoopCntFI(S->getDepth() - 1);
  unsigned tmpReg = Pass.GuardsReg;
  AddDefaultPred(BuildMI(*BranchMBB, BranchMBB->end(), DL,
          Pass.TII->get(Patmos::LWC), tmpReg))
    .addFrameIndex(fi).addImm(0); // address

  // decrement
  AddDefaultPred(BuildMI(*BranchMBB, BranchMBB->end(), DL,
          Pass.TII->get(Patmos::SUBi), tmpReg))
    .addReg(tmpReg).addImm(1);
  // compare with 0, PRTmp as predicate register
  unsigned branch_preg = Pass.PRTmp;
  AddDefaultPred(BuildMI(*BranchMBB, BranchMBB->end(), DL,
          Pass.TII->get(Patmos::CMPLT), branch_preg))
    .addReg(Patmos::R0).addReg(tmpReg);
  // store back
  AddDefaultPred(BuildMI(*BranchMBB, BranchMBB->end(), DL,
          Pass.TII->get(Patmos::SWC)))
    .addFrameIndex(fi).addImm(0) // address
    .addReg(tmpReg, RegState::Kill);
  InsertedInstrs += 4; // STATISTIC

  // insert branch to header
  assert(branch_preg != Patmos::NoRegister);

#ifdef BOUND_UNDEREST_PROTECTION
  if (branch_preg != header_preg) {
    AddDefaultPred(BuildMI(*BranchMBB, BranchMBB->end(), DL,
          Pass.TII->get(Patmos::POR), branch_preg))
      .addReg(branch_preg).addImm(0)
      .addReg(header_preg).addImm(0);
    InsertedInstrs++; // STATISTIC
  }
#endif

  // branch condition: not(<= zero)
  BuildMI(*BranchMBB, BranchMBB->end(), DL, Pass.TII->get(Patmos::BR))
    .addReg(branch_preg).addImm(0)
    .addMBB(HeaderMBB);
  BranchMBB->addSuccessor(HeaderMBB);
  InsertedInstrs++; // STATISTIC

  // create a post-loop MBB to restore the spill predicates, if necessary
  if (RI.needsScopeSpill()) {
    MachineBasicBlock *PostMBB = MF.CreateMachineBasicBlock();
    MF.push_back(PostMBB);
    // we create a LBC instruction here; TRI->eliminateFrameIndex() will
    // convert it to a stack cache access, if the stack cache is enabled.
    int fi = Pass.PMFI->getSinglePathS0SpillFI(S->getDepth()-1);
    unsigned tmpReg = Pass.GuardsReg;
    AddDefaultPred(BuildMI(*PostMBB, PostMBB->end(), DL,
            Pass.TII->get(Patmos::LBC), tmpReg))
      .addFrameIndex(fi).addImm(0); // address

    // if there are any PRegs to be preserved, do it now
    std::vector<unsigned> liveouts;
    Pass.getLoopLiveOutPRegs(S, liveouts);
    for(unsigned i = 0; i < liveouts.size(); i++) {
      AddDefaultPred(BuildMI(*PostMBB, PostMBB->end(), DL,
            Pass.TII->get(Patmos::BCOPY), tmpReg))
        .addReg(tmpReg)
        .addImm(Pass.TRI->getS0Index(liveouts[i]))
        .addReg(liveouts[i]).addImm(0);
      InsertedInstrs++; // STATISTIC
    }

    // assign to S0
    Pass.TII->copyPhysReg(*PostMBB, PostMBB->end(), DL,
                          Patmos::S0, tmpReg, true);
    nextMBB(PostMBB);
    InsertedInstrs += 2; // STATISTIC
  }
}

///////////////////////////////////////////////////////////////////////////////

