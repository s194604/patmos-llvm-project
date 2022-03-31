add_llvm_component_library(LLVMPatmosDisassembler
  PatmosDisassembler.cpp
  
  LINK_COMPONENTS
  MCDisassembler
  PatmosInfo
  Support
  
  ADD_TO_COMPONENT
  Patmos 
  )
