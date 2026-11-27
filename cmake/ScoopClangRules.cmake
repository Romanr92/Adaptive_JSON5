# Loaded after platform defaults, including during compiler try-compiles.
string(CONCAT CMAKE_CXX_LINK_EXECUTABLE
    "\"${adaptive_json5_scoop_root}/apps/gcc/current/bin/g++.exe\" "
    "<FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
