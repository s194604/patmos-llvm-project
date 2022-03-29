add_llvm_component_library(LLVMPatmosInfo
  PatmosTargetInfo.cpp
  
  LINK_COMPONENTS
  Analysis 
  AsmPrinter 
  CodeGen 
  Core 
  MC 
  SelectionDAG 
  Support 
  Target 
  GlobalISel
 
  ADD_TO_COMPONENT
  Patmos
  
  )