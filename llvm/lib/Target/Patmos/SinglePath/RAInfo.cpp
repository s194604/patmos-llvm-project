//===-- RAInfo.cpp - Patmos LLVM single-path predicate register allocator -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The RAInfo class handles predicate register allocation for single-path code.
//
//===----------------------------------------------------------------------===//
#include "RAInfo.h"

#include "llvm/Support/Debug.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/DepthFirstIterator.h"

#include <llvm/ADT/BitVector.h>
#include <sstream>

using namespace llvm;
using namespace std;

#define DEBUG_TYPE "patmos-singlepath"

STATISTIC( SPNumPredicates, "Number of predicates for single-path code");
STATISTIC( PredSpillLocs, "Number of required spill bits for predicates");
STATISTIC( NoSpillScopes,
                  "Number of SPScopes (loops) where S0 spill can be omitted");

///////////////////////////////////////////////////////////////////////////////

/// LiveRange - Class to hold live range information for a predicate in
/// an RAInfo object.
/// A live range is a set of position, each of which is associated with a
/// basic block in the scope being described. The first position in the
/// range matches the header block in the scope. The rest of the blocks
/// are indexed in topological ordering.
/// There is one more position than there are blocks, so the last position
/// is not associated with any block.
/// At any location, the predicate can be used and/or defined.
/// Defining a predicate at a location means it gets its runtime value there,
/// i.e. whether it is true or false.
class LiveRange {
friend class RAInfo;
private:

  // Where each predicate is used.
  // The position is the index of the block in the scope
  // except for the last one which doesn't have an associated block
  BitVector uses;

  // Where each predicate is defined.
  // The position is the index of the block in the scope
  // except for the last one which doesn't have an associated block
  BitVector defs;
public:
  // Add a use of the predicate associated with this range
  // at the position given.
  void addUse(long pos) { uses.set(pos); }

  // Add a use of the predicate associated with this range
  // at the position given.
  void addDef(long pos) { defs.set(pos); }

  /// Constructs a new live range for a scope.
  /// Must be given the number of FCFG blocks in the scope.
  LiveRange(unsigned range){
    range++; // We use 1 extra position that is not associated with a block
    uses = BitVector(range);
    defs = BitVector(range);
  }
  bool isUse(long pos) const { return uses.test(pos); }
  bool isDef(long pos) const { return defs.test(pos); }
  bool lastUse(long pos) const {
    // test whether shifting out up to this use will result in an empty
    // bitvector
    // return (uses >> (pos+1LL)) == 0;
    for (unsigned i = pos+1; i < uses.size(); i++) {
      if (uses.test(i)) return false;
    }
    return true;
  }
  bool hasDefBefore(long pos) const {
    // 00000100000 pos
    // 00000011111 before
    // -> any common?
    //return (defs & ((1LL << pos)-1LL)) != 0;
    unsigned i = pos;
    while (i-- > 0) {
      if (defs.test(i)) return true;
    }
    return false;
  }

  // check if there is any use before (and including) pos
  bool anyUseBefore(long pos) const {
    //return (uses & ((1LL << (pos+1LL))-1LL)) != 0;
    for (unsigned i = 0; i <= pos; i++) {
      if (uses.test(i)) return true;
    }
    return false;
  }
  bool hasNextUseBefore(long pos, const LiveRange &other) const {
    assert(uses.size() == other.uses.size());
    // this   ....10000|...
    // other ......1000|...   -> no
    //                ^pos
    for (unsigned i = pos; i < uses.size(); i++) {
      if (other.uses.test(i)) break;
      if (uses.test(i)) return true;
    }
    return false;
  }
  string str(void) const {
    stringbuf buf;
    char kind[] = { '-', 'u', 'd', 'x' };
    for (unsigned long i = 0; i < uses.size(); i++) {
      int x = 0;
      if (uses.test(i)) x += 1;
      if (defs.test(i)) x += 2;
      buf.sputc(kind[x]);
    }
    return buf.str();
  }
};

///////////////////////////////////////////////////////////////////////////////

// A class defining a predicate location in memory.
// A location is either a register or a stack spill slot, i.e. the 'type'.
// The 'loc' field specifies the index of the register or stack spill slot
// used by this location.
// E.g. Location{Register, 1} specifies that this location is the second
// register, while Location{Stack, 3} specifies this location is the
// fourth stack spill slot.
// Location indices start at 0 for both registers and stack spill slots.
class Location {

  public:
    friend bool operator<(const Location &, const Location &);
    friend llvm::raw_ostream &operator<<(llvm::raw_ostream&, const Location &);

    Location(): type(RAInfo::LocType::Register), loc(0){}
    Location(const Location &o): type(o.type), loc(o.loc){}
    Location(RAInfo::LocType type, unsigned loc): type(type), loc(loc){}

    RAInfo::LocType getType() const { return type;}
    unsigned getLoc() const { return loc;}
    bool isRegister() const { return type == RAInfo::LocType::Register;}
    bool isStack() const { return type == RAInfo::LocType::Stack;}

  private:
    RAInfo::LocType type;
    unsigned loc;

};

bool operator<(const Location&l, const Location &r){
  if( l.getType() == r.getType()){
    return l.getLoc() < r.getLoc();
  } else {
    return false;
  }
}

llvm::raw_ostream &operator<<(llvm::raw_ostream& os, const Location &m) {
    os << "Location{" << (m.getType() == RAInfo::Register? "Register":"Stack") <<", " << m.getLoc() <<"}";
    return os;
}

// The private implementation of RAInfo using the PIMPL pattern.
class RAInfo::Impl {
public:
  // A reference to the RAInfo that uses this instance
  // to implement its private members. (I.e. the public part
  // of the implementation)
  RAInfo &Pub;

  /// Number of available registers for use by the function.
  /// Not necessarily all of these registers are usable by the
  /// scope associated with this instance, since the parent scope
  /// may be using some of them.
  /// See 'firstUsableReg'.
  const unsigned MaxRegs;

  // The live ranges of predicates.
  // Given a predicate x, then its live range is LRs[x]
  std::map<unsigned, LiveRange> LRs;

  // The definition location of each predicate.
  // Given the predicate x, its definition is DefLocs[x].
  std::map<unsigned, Location> DefLocs;

  // The total number of predicate locations used by this instance.
  unsigned NumLocs;

  // The maximum number of location used by any child.
  unsigned ChildrenMaxCumLocs;

  /// The index of the first register this instance can use.
  /// The registers below the index are used by a parent scope.
  unsigned FirstUsableReg;

  /// The index of the first stack spill slot this instance can use.
  /// The slots below the index are used by a parent scope.
  unsigned FirstUsableStackSlot;

  /// Record to hold predicate use information for a MBB.
  struct UseLoc {
    /// Which register location to use as the predicate
    /// to an MBB
    unsigned loc;

    /// From which spill location to load the predicate
    /// before using it (load it into 'loc').
    /// If the boolean is false, does not need to load before use and the integer is undefined.
    std::pair<bool, unsigned> load;

    /// To which spill location to spill the predicate (from 'loc')
    /// after the MBB is done.
    /// If the boolean is false, does not need to spill after use and the integer is undefined.
    std::pair<bool, unsigned> spill;
    UseLoc(unsigned loc) : loc(loc), load(std::make_pair(false, 0)),
        spill(std::make_pair(false, 0))
    {}
  };

  // Map of MBB -> (map of Predicate ->UseLoc), for an SPScope
  map<const MachineBasicBlock*, std::map<unsigned, UseLoc>> UseLocs;

  bool NeedsScopeSpill;

  Impl(RAInfo *pub, SPScope *S, unsigned availRegs):
    Pub(*pub), MaxRegs(availRegs), NumLocs(0), ChildrenMaxCumLocs(0),
    FirstUsableReg(0), FirstUsableStackSlot(0),NeedsScopeSpill(true)
  {
    createLiveRanges();
    assignLocations();
  }

  // Returns the first available location in the given set, removing it from the set.
  // If the set is empty, a new Location is created and returned.
  Location getAvailLoc(set<Location> &FreeLocs) {
    if (!FreeLocs.empty()) {
      set<Location>::iterator it = FreeLocs.begin();
      FreeLocs.erase(it);
      return *it;
    }
    // Create a new location
    unsigned oldNumLocs = NumLocs++;

    return (oldNumLocs < MaxRegs)?
      Location(Register, oldNumLocs)
      : Location(Stack, oldNumLocs - MaxRegs)
    ;
  }

  // Returns whether either there is a free register location available
  // in the given set, or one can be created.
  // if true, the next call to getAvailLoc is guaranteed to produce a Register
  // location (assuming the given set or the fields don't change).
  bool hasFreeRegister(set<Location> &FreeLocs) {
    return (!FreeLocs.empty() && (FreeLocs.begin()->getType() == Register))
        || (NumLocs < MaxRegs);
  }

  // getCumLocs - Get the maximum number of locations
  // used by this scope and any of its children
  unsigned getCumLocs(void) const { return NumLocs + ChildrenMaxCumLocs; }

  void createLiveRanges(void) {

    auto
    getOrCreateLRFor = [&](unsigned predicate){
      if(LRs.find(predicate) == LRs.end()){
        LRs.insert(std::make_pair(predicate, LiveRange(Pub.Scope->getNumberOfFcfgBlocks())));
      }
      assert(LRs.find(predicate) != LRs.end());
      return &LRs.at(predicate);
    };

    // create live range information for each predicate
    LLVM_DEBUG(dbgs() << " Create live-ranges for [MBB#"
                 << Pub.Scope->getHeader()->getMBB()->getNumber() << "]\n");

    auto blocks = Pub.Scope->getBlocksTopoOrd();

    for (unsigned i = 0, e = blocks.size(); i < e; i++) {
      auto block = blocks[i];
      auto preds = block->getBlockPredicates();
      // insert uses
      for(auto pred: preds){
        getOrCreateLRFor(pred)->addUse(i);
      }
      // insert defs
      for(auto def: block->getDefinitions()){
        getOrCreateLRFor(def.predicate)->addDef(i);
      }
    }
    // add a use for header predicate
    // TODO:(Emad) is that because its a loop and P0 is used when we jump back to the start
    // TODO:(Emad) of the loop, therefore we say that the last block also uses P0? i.e. connecting
    // TODO:(Emad) the loop end with the start?
    if (!Pub.Scope->isTopLevel()) {
      auto preds = Pub.Scope->getHeader()->getBlockPredicates();
      for(auto pred: preds){
        getOrCreateLRFor(pred)->addUse(blocks.size());
      }
    }
  }

  void assignLocations(void) {
    LLVM_DEBUG(dbgs() << " Assign locations for [MBB#"
                 << Pub.Scope->getHeader()->getMBB()->getNumber() << "]\n");
    SPNumPredicates += Pub.Scope->getNumPredicates(); // STATISTIC

    set<Location> FreeLocs;

    // map to keep track of locations of predicates during the scan
    map<unsigned, Location> curLocs;

    auto blocks = Pub.Scope->getBlocksTopoOrd();
    for (unsigned i = 0, e = blocks.size(); i < e; i++) {
      auto block = blocks[i];
      MachineBasicBlock *MBB = block->getMBB();

      LLVM_DEBUG( dbgs() << "  MBB#" << MBB->getNumber() << ": " );

      // (1) handle use
      handlePredUse(i, block, curLocs, FreeLocs);

      // (3) handle definitions in this basic block.
      //     if we need to get new locations for predicates (loc==-1),
      //     assign new ones in nearest-next-use order
      auto definitions = block->getDefinitions();
      if (!definitions.empty()) {
        vector<unsigned> order;
        for(auto def: definitions){
          auto pred = def.predicate;
          if (curLocs.find(pred) == curLocs.end()) {
            // need to get a new loc for predicate
            order.push_back(pred);
          }
        }

        sortFurthestNextUse(i, order);

        // nearest use is in front
        for (auto pred: order) {
          Location l = getAvailLoc(FreeLocs);
          map<unsigned, Location>::iterator findCurUseLoc = curLocs.find(pred);
          if(findCurUseLoc == curLocs.end()){
            curLocs.insert(make_pair(pred, l));
          }else{
            findCurUseLoc->second = l;
          }
          if(DefLocs.find(pred) == DefLocs.end()){
            DefLocs.insert(std::make_pair(pred, l));
          }else{
            DefLocs.find(pred)->second = l;
          }
          assert(curLocs.find(pred)->second.getLoc() == DefLocs.at(pred).getLoc());
          LLVM_DEBUG(
              dbgs() << "def " << pred << " in " << DefLocs.at(pred) << ", "
          );
        }
      }
      LLVM_DEBUG(dbgs() << "\n");
    } // end of forall MBB

    // What is the location of the header predicate after handling all blocks?
    // We store this location, as it is where the next iteration has to get it
    // from (if different from its use location)
    // Code for loading the predicate is placed before the back-branch,
    // generated in LinearizeWalker::exitSubscope().
    if (!Pub.Scope->isTopLevel()) {
      auto headerUseLocs = UseLocs.find(Pub.Scope->getHeader()->getMBB());
      assert(headerUseLocs != UseLocs.end());

      for(auto headerPred: Pub.Scope->getHeader()->getBlockPredicates()){
        auto predUseLocPair = headerUseLocs->second.find(headerPred);
        assert(predUseLocPair != headerUseLocs->second.end());
        auto predUseLoc = &predUseLocPair->second;

        auto curPredLoc = curLocs.at(headerPred).getLoc();
        if (predUseLoc->loc != curPredLoc) {
          predUseLoc->load = std::make_pair(true, curPredLoc);
        }
      }
    }
  }

  /// Converts a register index into a global index that takes parent
  /// into account.
  unsigned unifyRegister(unsigned idx){
    LLVM_DEBUG(dbgs() << "Unifying register: (" << idx << ") with (" << FirstUsableReg << ")\n");
    // We don't have to check whether the result is larger that the number
    // of available registers, because we know the parent will spill
    // if that is the case.
    return idx + FirstUsableReg;
  }

  /// Converts a Stack spill slot index into a global index that takes parent
  /// into account.
  unsigned unifyStack(unsigned idx){
    return idx + FirstUsableStackSlot;
  }

  /// Unifies with parent, such that this RAInfo knows which registers it can use
  /// and where its spill slots are.
  void unifyWithParent(const RAInfo::Impl &parent, int parentSpillLocCnt, bool topLevel){

      // We can avoid a spill if the total number of locations
      // used by the parent, this instance, and any child is less
      // than/equal to the number of registers available to the function.
      if ( !topLevel && parent.NumLocs + getCumLocs() <= MaxRegs ) {

        // Compute the first register not used by an ancestor.
        FirstUsableReg = parent.FirstUsableReg + parent.NumLocs;

        // If the total number of locations the parent, myself, and my children need
        // are less than/equal to the number of available registers
        // we do not have to spill any predicates.
        NeedsScopeSpill = false;
      }

    if (NumLocs > MaxRegs) {
      FirstUsableStackSlot = parentSpillLocCnt;
    }
  }

  /// Unifies with child, such that this RAInfo knows how many locations will
  /// be used by the given child.
  void unifyWithChild(const RAInfo::Impl &child){
    ChildrenMaxCumLocs = std::max(child.getCumLocs(), ChildrenMaxCumLocs);
  }

  UseLoc calculateNotHeaderUseLoc(unsigned blockIndex, unsigned usePred,
      map<unsigned, Location>& curLocs, set<Location>& FreeLocs)
  {
    map<unsigned, Location>::iterator findCurUseLoc = curLocs.find(usePred);
    assert(findCurUseLoc != curLocs.end());

    // each use must be preceded by a location assignment
    Location& curUseLoc = findCurUseLoc->second;

    // if previous location was not a register, we have to allocate
    // a register and/or possibly spill
    if (curUseLoc.isStack()) {
      auto useloc_newloc = handleIfNotInRegister(
          blockIndex, FreeLocs, curLocs, curUseLoc.getLoc());
      LLVM_DEBUG(
        dbgs() << "Moving current location of predicate " << usePred <<" to " << useloc_newloc.second << "\n"
      );
      curUseLoc = useloc_newloc.second;
      return useloc_newloc.first;
    } else {
      // everything stays as is
      return UseLoc(curUseLoc.getLoc());
    }
  }

  UseLoc calculateHeaderUseLoc(set<Location>& FreeLocs, map<unsigned, Location>& curLocs) {

    // we get a loc for the header predicate
    Location loc = getAvailLoc(FreeLocs);
    UseLoc UL(loc.getLoc());
    auto headerPred = getHeaderPred();
    // TODO: can't handle headers that have been merged yet
    assert(Pub.Scope->getHeader()->getBlockPredicates().size() == 1);
    assert(DefLocs.find(headerPred) == DefLocs.end());
    DefLocs.insert(std::make_pair(headerPred,loc));
    map<unsigned, Location>::iterator curLoc0 = curLocs.find(headerPred);
    if(curLoc0 == curLocs.end()){
      curLocs.insert(make_pair(headerPred, loc));
    }else{
      curLoc0->second = loc;
    }
    assert(UL.loc == 0);
    return UL;
  }

  void handlePredUse(unsigned i, PredicatedBlock* block,
      map<unsigned, Location>& curLocs, set<Location>& FreeLocs)
  {
    for(auto usePred: block->getBlockPredicates()){
      LLVM_DEBUG(dbgs() << "Allocating predicate " << usePred << "\n");

      // for the top-level entry of a single-path root,
      // we don't need to assign a location, as we will use p0
      if (!(usePred == getHeaderPred() && Pub.Scope->isRootTopLevel())) {
        assert(block == Pub.Scope->getHeader() || i > 0);
        assert(UseLocs[block->getMBB()].count(usePred) == 0
            && "Block was already assigned a use predicate");

        auto useLoc = (Pub.Scope->isHeader(block)) ?
              calculateHeaderUseLoc(FreeLocs, curLocs)
            : calculateNotHeaderUseLoc(i, usePred, curLocs, FreeLocs);

        assert(!UseLocs[block->getMBB()].count(usePred) && "Predicate shouldn't have any use locations set");
        UseLocs[block->getMBB()].insert(std::make_pair(usePred, useLoc));
      } else {
        LLVM_DEBUG(
          dbgs() << "MBB#" << block->getMBB()->getNumber() << " uses P0\n"
        );
      }
    }

    // retire locations
    for(auto usePred: block->getBlockPredicates()){
      if (!(usePred == getHeaderPred() && Pub.Scope->isRootTopLevel())) {
        assert(block == Pub.Scope->getHeader() || i > 0);

        if (LRs.at(usePred).lastUse(i)) {
          LLVM_DEBUG(dbgs() << "retire " << usePred << ". ");
          map<unsigned, Location>::iterator findCurUseLoc = curLocs.find(usePred);
          assert(findCurUseLoc != curLocs.end());
          Location& curUseLoc = findCurUseLoc->second;

          // free location, also removing it from the current one is use
          assert(!FreeLocs.count(curUseLoc));
          FreeLocs.insert(curUseLoc);
          curLocs.erase(findCurUseLoc);
        }
      }
    }
  }

  std::pair<UseLoc, Location> handleIfNotInRegister(unsigned blockIndex, set<Location>& FreeLocs,
      map<unsigned, Location>& curLocs, unsigned stackLoc)
  {
    if (hasFreeRegister(FreeLocs)) {
      Location newLoc = getAvailLoc(FreeLocs);
      UseLoc UL(newLoc.getLoc());
      UL.load = std::make_pair(true, stackLoc);
      assert(UL.loc <= MaxRegs);
      return std::make_pair(UL, newLoc);
    } else {
      // spill and reassign
      // order predicates wrt furthest next use
      vector<unsigned> order;
      for(auto pair: LRs){
        auto pred = pair.first;
        map<unsigned, Location>::iterator cj = curLocs.find(pred);
        if (cj != curLocs.end() && cj->second.isRegister()) {
          order.push_back(pred);
        }
      }
      sortFurthestNextUse(blockIndex, order);
      unsigned furthestPred = order.back();

      Location newStackLoc = getAvailLoc(FreeLocs); // guaranteed to be a stack location, since there are no physicals free
      assert(newStackLoc.isStack());

      map<unsigned, Location>::iterator findFurthest =
          curLocs.find(furthestPred);
      assert(findFurthest != curLocs.end());

      UseLoc UL(findFurthest->second.getLoc());
      UL.load = std::make_pair(true, stackLoc);

      // differentiate between already used and not yet used
      if (LRs.at(furthestPred).anyUseBefore(blockIndex)) {
        UL.spill = std::make_pair(true, newStackLoc.getLoc());

        LLVM_DEBUG( dbgs() << "Spilling predicate " << furthestPred << " to " << newStackLoc << "\n" );
      } else {
        // if it has not been used, we change the initial
        // definition location
        assert(DefLocs.count(furthestPred) && "Predicate should already have a definition");
        DefLocs[furthestPred] = newStackLoc;

        LLVM_DEBUG( dbgs() << "Moving initial definition of predicate " << furthestPred <<
            " to " << newStackLoc << "\n" );
      }

      auto replacement = findFurthest->second;
      assert(replacement.isRegister() && "Should use a register location");

      // Move the current location of the spilled register to stack
      findFurthest->second = newStackLoc;

      return std::make_pair(UL, replacement);
    }
  }

  /// Sorts the given vector of predicates according to the
  /// furthest next use from the given MBB position.
  void sortFurthestNextUse(unsigned pos, vector<unsigned>& order) {
    std::sort(order.begin(), order.end(), [this, pos](int a, int b){
      return LRs.at(a).hasNextUseBefore(pos, LRs.at(b));
    });
  }

  /// Returns the predicate used by the header of the scope that is represented
  /// by this instance.
  unsigned getHeaderPred(){
    return *Pub.Scope->getHeader()->getBlockPredicates().begin();
  }

  /// Gets the use location of the given mbb.
  ///
  /// The given function handles extracting the correct location type needed.
  /// If the location returned is negative, it means the location type has no value.
  std::map<unsigned, unsigned> getAnyLoc(const MachineBasicBlock *MBB, std::function<std::pair<bool, unsigned> (UseLoc)> f)
  {
    std::map<unsigned, unsigned> result;
    if (UseLocs.count(MBB)) {
      for(auto ul: UseLocs[MBB]){
        auto opLoc = f(ul.second);
        if(opLoc .first){
          result[ul.first] = unifyRegister(opLoc.second);
        }
      }
    }
    return result;
  }

};

///////////////////////////////////////////////////////////////////////////////
//  RAInfo methods
///////////////////////////////////////////////////////////////////////////////

RAInfo::RAInfo(SPScope *S, unsigned availRegs) :
  Scope(S), Priv(spimpl::make_unique_impl<Impl>(this, S, availRegs))
  {}

bool RAInfo::needsScopeSpill(void) const {
  return Priv->NeedsScopeSpill;
}

bool RAInfo::isFirstDef(const MachineBasicBlock *MBB, unsigned pred) const {
  auto blocks = Scope->getBlocksTopoOrd();
  for(unsigned i=0; i< blocks.size(); i++) {
    if (blocks[i]->getMBB() == MBB) {
      return !Priv->LRs.at(pred).hasDefBefore(i);
    }
  }
  return false;
}

bool RAInfo::hasSpillLoad(const MachineBasicBlock *MBB) const {
  if (Priv->UseLocs.count(MBB)) {
    for(auto ul: Priv->UseLocs[MBB]){
      if(ul.second.spill.first || ul.second.load.first){
        return true;
      }
    }
  }
  return false;
}

std::map<unsigned, unsigned> RAInfo::getUseLocs(const MachineBasicBlock *MBB) const {
  std::map<unsigned, unsigned> result;
  if (Priv->UseLocs.count(MBB)) {
    for(auto ul: Priv->UseLocs[MBB]){
      auto loc = Priv->unifyRegister(ul.second.loc);
      assert( loc < Priv->MaxRegs );
      result[ul.first] = loc;
    }
  }
  return result;
}

std::map<unsigned, unsigned> RAInfo::getLoadLocs(const MachineBasicBlock *MBB) const {
  return Priv->getAnyLoc(MBB, [](Impl::UseLoc ul){return ul.load;});
}

std::map<unsigned, unsigned> RAInfo::getSpillLocs(const MachineBasicBlock *MBB) const {
  return Priv->getAnyLoc(MBB, [](Impl::UseLoc ul){return ul.spill;});
}

tuple<RAInfo::LocType, unsigned> RAInfo::getDefLoc(unsigned pred) const {
  auto pair = Priv->DefLocs.find(pred);
  assert(pair != Priv->DefLocs.end());
  Location loc = pair->second;
  if( loc.getType() == RAInfo::Register){
    return make_tuple(loc.getType(), Priv->unifyRegister(loc.getLoc()));
  }else{
    return make_tuple(loc.getType(), Priv->unifyStack(loc.getLoc()));
  }
}

unsigned RAInfo::neededSpillLocs(){
  if(Priv->NumLocs < Priv->MaxRegs) {
    return 0;
  }else{
    return Priv->NumLocs - Priv->MaxRegs;
  }
}

void RAInfo::dump() const {
  dump(dbgs(), 0);
}

void RAInfo::dump(raw_ostream& os, unsigned indent) const{
  os.indent(indent) << "[MBB#"     << Scope->getHeader()->getMBB()->getNumber()
         <<  "] depth=" << Scope->getDepth() << "\n";

  for(auto pair: Priv->LRs){
    os.indent(indent) << "  LR(p" << pair.first << ") = [" << pair.second.str() << "]\n";
  }

  auto blocks = Scope->getBlocksTopoOrd();
  for (unsigned i=0, e=blocks.size(); i<e; i++) {
    MachineBasicBlock *MBB = blocks[i]->getMBB();
    os.indent(indent) << "  " << i << "| MBB#" << MBB->getNumber();
    os << " UseLocs{\n";
    for(auto predUl: Priv->UseLocs[MBB]){
      os << "    (Pred: " << predUl.first << ", loc=" << predUl.second.loc << ", load=";
      if (predUl.second.load.first) {
        os << predUl.second.load.second;
      }else{
        os << "none";
      }
      dbgs() << ", spill=";
      if (predUl.second.spill.first) {
        os << predUl.second.spill.second;
      }else{
        os << "none";
      }
      os << "), ";
    }
    os << "}\n";
  }

  os.indent(indent) << "  DefLocs:     ";
  for(auto pair: Priv->DefLocs){
    os << " p" << pair.first << "=" << pair.second << ", ";
  }
  os << "\n";

  os.indent(indent) << "  NumLocs:      " << Priv->NumLocs << "\n"
            "  CumLocs:      " << Priv->getCumLocs() << "\n"
            "  Offset:       " << Priv->FirstUsableReg  << "\n"
            "  SpillOffset:  " << Priv->FirstUsableStackSlot  << "\n";
}

std::map<const SPScope*, RAInfo> RAInfo::computeRegAlloc(SPScope *rootScope, unsigned AvailPredRegs){

  std::map<const SPScope*, RAInfo> RAInfos;
  // perform reg-allocation in post-order to compute cumulative location
  // numbers in one go
  for (auto iter = po_begin(rootScope), end = po_end(rootScope);
      iter!=end; ++iter) {
    auto scope = *iter;
    // create RAInfo for SPScope
    RAInfos.insert(std::make_pair(scope, RAInfo(scope,  AvailPredRegs)));
    RAInfo &RI = RAInfos.at(scope);

    // Because this is a post-order traversal, we have already visited
    // all children of the current scope (S). Synthesize the cumulative number of locations
    for(SPScope::child_iterator CI = scope->child_begin(), CE = scope->child_end();
        CI != CE; ++CI) {
      SPScope *CN = *CI;
      RI.Priv->unifyWithChild(*(RAInfos.at(CN).Priv));
    }
  } // end of PO traversal for RegAlloc


  // Visit all scopes in depth-first order to compute offsets:
  // - Offset is inherited during traversal
  // - SpillOffset is assigned increased depth-first, from left to right
  unsigned spillLocCnt = 0;
  for (auto iter = df_begin(rootScope), end = df_end(rootScope);
        iter!=end; ++iter) {
    auto scope = *iter;
    RAInfo &RI = RAInfos.at(scope);

    if (!scope->isTopLevel()) {
       RI.Priv->unifyWithParent(*(RAInfos.at(scope->getParent()).Priv), spillLocCnt, scope->isTopLevel());
      if (!RI.needsScopeSpill()) NoSpillScopes++; // STATISTIC
    }
    spillLocCnt += RI.neededSpillLocs();
    LLVM_DEBUG( RI.dump() );
  } // end df

  PredSpillLocs += spillLocCnt; // STATISTIC
  return RAInfos;
}

