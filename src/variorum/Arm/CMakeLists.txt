set(variorum_arm_headers
  CACHE INTERNAL "")

set(variorum_arm_sources
  CACHE INTERNAL "")

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${variorum_includes})

add_library(variorum_arm OBJECT
            ${variorum_arm_sources}
            ${variorum_arm_headers})

### Shared libraries need PIC
set_property(TARGET ${variorum_arm} PROPERTY POSITION_INDEPENDENT_CODE 1)
