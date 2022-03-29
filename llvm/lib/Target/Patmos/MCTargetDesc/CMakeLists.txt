add_llvm_component_library(LLVMPatmosDesc
  PatmosMCTargetDesc.cpp
  PatmosMCAsmInfo.cpp
  PatmosAsmBackend.cpp
  PatmosMCCodeEmitter.cpp
  PatmosELFObjectWriter.cpp
  PatmosTargetStreamer.cpp
  
  LINK_COMPONENTS
  PatmosAsmPrinter 
  PatmosInfo 
  MC 
  Support
 
  ADD_TO_COMPONENT
  Patmos    
  
  
  )
