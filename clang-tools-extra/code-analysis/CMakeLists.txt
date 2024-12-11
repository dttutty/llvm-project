set(LLVM_LINK_COMPONENTS support)

add_clang_executable(code-analysis
  CodeAnalysis.cpp
  )
install(TARGETS code-analysis RUNTIME DESTINATION bin)
target_link_libraries(code-analysis                                             
  PRIVATE                                                                                     
  clangTooling                                                                                
  clangBasic                                                                                  
  clangASTMatchers                      
  )

