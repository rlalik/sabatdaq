install(
    TARGETS sabatdaq_exe
    RUNTIME COMPONENT sabatdaq_Runtime
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
