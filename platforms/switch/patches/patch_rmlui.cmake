# Run in RmlUi's source tree as its FetchContent PATCH_COMMAND: runtime dependency sets cannot be
# configured on an MSYS host, and a static NRO installs nothing.
set(_f "CMake/RuntimeUtilities.cmake")
file(READ "${_f}" _c)
string(REPLACE "if(\${CMAKE_VERSION} VERSION_GREATER_EQUAL \"3.21\")"
               "if(\${CMAKE_VERSION} VERSION_GREATER_EQUAL \"3.21\" AND NOT NX)" _c "${_c}")
file(WRITE "${_f}" "${_c}")
