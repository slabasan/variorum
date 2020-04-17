# Copyright 2019-2020 Lawrence Livermore National Security, LLC and other
# Variorum Project Developers. See the top-level LICENSE file for details.
#
# SPDX-License-Identifier: MIT

set(MSR-TOOLS
    rdmsr
    wrmsr
)

message(STATUS "Adding msr-tool utilities")
foreach(UTIL ${MSR-TOOLS})
    message(STATUS " [*] Adding utility: ${UTIL}")
    add_executable(${UTIL} ${UTIL}.c)
    target_link_libraries(${UTIL})
endforeach()

install(TARGETS ${MSR-TOOLS}
        EXPORT  variorum
        LIBRARY DESTINATION utilities/msr-tools
        ARCHIVE DESTINATION utilities/msr-tools
        RUNTIME DESTINATION utilities/msr-tools)
