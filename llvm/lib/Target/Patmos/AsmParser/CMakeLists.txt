add_llvm_component_library(LLVMPatmosAsmParser
  PatmosAsmParser.cpp
 
  LINK_COMPONENTS
  MC
  MCParser
  PatmosDesc
  PatmosInfo
 
  ADD_TO_COMPONENT
  Patmos 
  )

