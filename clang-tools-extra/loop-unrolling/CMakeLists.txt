set(LLVM_LINK_COMPONENTS support)

add_clang_executable(loop-unrolling
  LoopUnrolling.cpp
)
install(TARGETS loop-unrolling RUNTIME DESTINATION bin)
target_link_libraries(loop-unrolling
  PRIVATE                                                                                              
  clangTooling                                                                                
  clangBasic                                                                                  
  clangASTMatchers                                   
  )
