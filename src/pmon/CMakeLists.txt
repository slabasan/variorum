target_link_libraries(variorum ${variorum_deps})

set(CMAKE_C_COMPILER "gcc")
set(CMAKE_C_FLAGS "-fomit-frame-pointer -Wall -std=c99")

set(pmon_sources
  pmon.c
)
message(STATUS " [*] Adding demoapp: pmon")
add_executable(pmon ${pmon_sources})
target_link_libraries(pmon variorum ${variorum_deps})

install(TARGETS pmon EXPORT variorum
        DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")

include_directories(${CMAKE_SOURCE_DIR}/variorum
                    ${CMAKE_SOURCE_DIR}/variorum/Intel)
