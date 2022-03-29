add_llvm_component_library(LLVMPatmosAsmPrinter
  PatmosInstPrinter.cpp
  
  LINK_COMPONENTS
  MC
  Support
 
  ADD_TO_COMPONENT
  Patmos   
  
  )
