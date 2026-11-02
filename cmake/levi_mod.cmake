cmake_minimum_required(VERSION 3.22)

# ---------------------------------------------------------------------------
# levi_mod.cmake - shared build support for LeviLaunchroid native mods
#
# Header-only use of the preloader SDK (include/pl) + the SDK's header deps
# (fmt / nlohmann_json / boost.pfr / magic_enum) vendored under refs/third_party.
#
# CRITICAL: the launcher loads a mod with System.load() -> dlopen(RTLD_NOW), and
# mod .so files are built with BIND_NOW, so every undefined symbol must resolve
# at load time. libpreloader.so is loaded by the launcher AFTER System.load(), so
# without a DT_NEEDED entry the load fails with:
#
#   dlopen failed: cannot locate symbol "_ZN2pl3mod9NativeMod7currentEv"
#
# LEVI_PRELOADER_STUB_DIR points at a generated stub named libpreloader.so
# (scripts/gen_preloader_stub.py). Linking it records the DT_NEEDED entry so the
# dynamic linker loads the launcher's real libpreloader.so. The stub itself is
# never shipped.
# ---------------------------------------------------------------------------

get_filename_component(LEVI_WORKSPACE "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(LEVI_PRELOADER_INCLUDE
    "${LEVI_WORKSPACE}/refs/preloader-android/include"
    CACHE PATH "preloader-android public include dir (contains pl/)")
set(LEVI_THIRD_PARTY
    "${LEVI_WORKSPACE}/refs/third_party"
    CACHE PATH "Vendored header dependencies")
set(LEVI_PRELOADER_STUB_DIR
    ""
    CACHE PATH "Directory holding the generated libpreloader.so import stub")

if(NOT EXISTS "${LEVI_PRELOADER_INCLUDE}/pl/Mod.hpp")
    message(FATAL_ERROR
        "preloader SDK not found at ${LEVI_PRELOADER_INCLUDE}. "
        "Run levimods/scripts/bootstrap.sh to fetch it.")
endif()

function(levi_mod_common target)
    target_include_directories("${target}" PRIVATE
        "${LEVI_PRELOADER_INCLUDE}"
        "${LEVI_THIRD_PARTY}/fmt/include"
        "${LEVI_THIRD_PARTY}/json/single_include"
        "${LEVI_THIRD_PARTY}/pfr/include"
        "${LEVI_THIRD_PARTY}/magic_enum/include"
        "${CMAKE_CURRENT_LIST_DIR}/src"
    )
    # fmt is used header-only by pl/Logger.hpp
    target_compile_definitions("${target}" PRIVATE FMT_HEADER_ONLY=1)
    target_compile_options("${target}" PRIVATE
        -fvisibility=hidden -fvisibility-inlines-hidden
        -O2 -fno-rtti -ffunction-sections -fdata-sections
        -Wall -Wextra -Wno-unused-parameter
        -Wno-missing-designated-field-initializers
    )
    target_link_options("${target}" PRIVATE
        "-Wl,--unresolved-symbols=ignore-all"
        "-Wl,--gc-sections"
        "-Wl,-z,max-page-size=16384"
    )
    find_library(LEVI_LOG_LIB log REQUIRED)
    find_library(LEVI_ANDROID_LIB android REQUIRED)
    target_link_libraries("${target}" PRIVATE
        ${LEVI_LOG_LIB} ${LEVI_ANDROID_LIB}
        ${CMAKE_DL_LIBS})

    if(LEVI_PRELOADER_STUB_DIR)
        if(NOT EXISTS "${LEVI_PRELOADER_STUB_DIR}/libpreloader.so")
            message(FATAL_ERROR
                "LEVI_PRELOADER_STUB_DIR is set to '${LEVI_PRELOADER_STUB_DIR}' "
                "but libpreloader.so is missing there.")
        endif()
        target_link_directories("${target}" PRIVATE "${LEVI_PRELOADER_STUB_DIR}")
        # --no-as-needed keeps DT_NEEDED even though no symbol is *defined* here.
        target_link_libraries("${target}" PRIVATE
            "-Wl,--no-as-needed" preloader "-Wl,--as-needed")
        message(STATUS "LeviMod: linking ${target} against libpreloader.so (DT_NEEDED)")
    else()
        message(WARNING
            "LeviMod: ${target} has NO libpreloader.so DT_NEEDED entry. "
            "The mod will fail to load in LeviLaunchroid. "
            "Build through scripts/build.sh, which generates the import stub.")
    endif()
endfunction()

# levi_add_mod(<target> <display name> <library basename> <sources...>)
function(levi_add_mod target display_name library_name)
    add_library("${target}" SHARED ${ARGN})
    set_target_properties("${target}" PROPERTIES
        OUTPUT_NAME "${library_name}"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
        PREFIX "lib")
    levi_mod_common("${target}")
    set_target_properties("${target}" PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON)
    message(STATUS "LeviMod: ${display_name} -> lib${library_name}.so")
endfunction()
