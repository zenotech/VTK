# catalyst is not installed on el7 image
set(VTK_ENABLE_CATALYST OFF CACHE BOOL "")

include("${CMAKE_CURRENT_LIST_DIR}/configure_fedora_common.cmake")
