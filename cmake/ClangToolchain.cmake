# Use Clang to compile and Scoop GCC's driver to link its MinGW runtime.
if(CMAKE_HOST_WIN32)
    if(DEFINED ENV{SCOOP})
        file(TO_CMAKE_PATH "$ENV{SCOOP}" adaptive_json5_scoop_root)
    else()
        file(TO_CMAKE_PATH "$ENV{USERPROFILE}/scoop" adaptive_json5_scoop_root)
    endif()
    set(CMAKE_CXX_COMPILER "${adaptive_json5_scoop_root}/apps/llvm/current/bin/clang++.exe")
    set(CMAKE_CXX_COMPILER_TARGET x86_64-w64-windows-gnu)
    set(CMAKE_SYSROOT "${adaptive_json5_scoop_root}/apps/gcc/current")
    set(CMAKE_RC_COMPILER "${adaptive_json5_scoop_root}/apps/gcc/current/bin/windres.exe")
    # Scoop GCC bundles its exception runtime in libgcc; let its driver select
    # the matching libraries rather than Clang's generic MinGW runtime list.
    set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX "${CMAKE_CURRENT_LIST_DIR}/ScoopClangRules.cmake")
    set(ENV{PATH} "${adaptive_json5_scoop_root}/apps/gcc/current/bin;${adaptive_json5_scoop_root}/apps/llvm/current/bin;$ENV{PATH}")
else()
    set(CMAKE_CXX_COMPILER clang++)
endif()
