# CMakeList.txt : CMake project for FastNoise2
cmake_minimum_required(VERSION 3.7.1)

project(FastNoise2 VERSION 0.10.0)
set(CMAKE_CXX_STANDARD 17)

message("FastNoise2 Arch: ${CMAKE_SYSTEM_PROCESSOR}") 

if(CMAKE_SYSTEM_PROCESSOR MATCHES armv7)

    set(FASTSIMD_COMPILE_ARMV7 true)
    set(FASTSIMD_COMPILE_ARM true)
    set(FASTSIMD_COMPILE_HAVE_NEON true)

elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)

    set(FASTSIMD_COMPILE_AARCH64 true)
    set(FASTSIMD_COMPILE_ARM true)
    set(FASTSIMD_COMPILE_HAVE_NEON true)

elseif(CMAKE_SYSTEM_PROCESSOR MATCHES arm64)

    set(FASTSIMD_COMPILE_ARM true)
    set(FASTSIMD_COMPILE_HAVE_NEON true)

elseif(CMAKE_SYSTEM_PROCESSOR MATCHES arm)

    set(FASTSIMD_COMPILE_ARM true)

endif()


# determine whether this is a standalone project or included by other projects
if (NOT DEFINED FASTNOISE2_STANDALONE_PROJECT)
    if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        set(FASTNOISE2_STANDALONE_PROJECT ON)
    else()
        set(FASTNOISE2_STANDALONE_PROJECT OFF)
    endif ()
endif()

# Build DLL
#set(BUILD_SHARED_LIBS ON) 

option(FASTNOISE2_NOISETOOL "Build NoiseTool application" ${FASTNOISE2_STANDALONE_PROJECT})
option(FASTNOISE2_TESTS "Build tests" OFF)

if(MSVC)
    #setup pdb target location
    set(pdb_output_dir "${CMAKE_CURRENT_BINARY_DIR}/pdb-files")

    set(CMAKE_PDB_OUTPUT_DIRECTORY "${pdb_output_dir}")
    set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY "${pdb_output_dir}") 

    #need to sync pdb files
    add_compile_options("/FS")
endif()

# Introduce variables:
# * CMAKE_INSTALL_LIBDIR
# * CMAKE_INSTALL_BINDIR
include(GNUInstallDirs) 
set(install_targets "")

add_subdirectory(src)

if(FASTNOISE2_NOISETOOL)
    include(cmake/CPM.cmake)
    add_subdirectory(NoiseTool)
endif()

if(FASTNOISE2_TESTS)
    include(cmake/CPM.cmake)  
    add_subdirectory(tests)
endif()


#Install -----------------------------------------------------------

# Layout. This works for all platforms:
#   * <prefix>/lib*/cmake/<PROJECT-NAME>
#   * <prefix>/lib*/
#   * <prefix>/include/
set(config_install_dir "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")

# Configuration
set(version_config "${generated_dir}/${PROJECT_NAME}ConfigVersion.cmake")
set(project_config "${generated_dir}/${PROJECT_NAME}Config.cmake")
set(targets_export_name "${PROJECT_NAME}Targets")
set(namespace "${PROJECT_NAME}::")

# Include module with fuction 'write_basic_package_version_file'
include(CMakePackageConfigHelpers)

# Configure '<PROJECT-NAME>ConfigVersion.cmake'
# Use:
#   * PROJECT_VERSION
write_basic_package_version_file(
    "${version_config}" COMPATIBILITY SameMajorVersion
)

# Configure '<PROJECT-NAME>Config.cmake'
# Use variables:
#   * TARGETS_EXPORT_NAME
#   * PROJECT_NAME
configure_package_config_file(
    "cmake/Config.cmake.in"
    "${project_config}"
    INSTALL_DESTINATION "${config_install_dir}"
)

# Targets:
#   * <prefix>/lib/libname.a
#   * header location after install: <prefix>/include/${PROJECT_NAME}/include.hpp
#   * headers can be included by C++ code `#include <${PROJECT_NAME}/include.hpp>`
install(
    TARGETS ${install_targets}
    EXPORT "${targets_export_name}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# Headers:
install(
    FILES ${install_fastsimd_headers}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/FastSIMD"
)
install(
    FILES ${install_fastnoise_headers}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/FastNoise"
)
install(
    FILES ${install_fastnoise_generators_headers}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/FastNoise/Generators"
)

# Config
#   * <prefix>/lib/cmake/${PROJECT_NAME}/${PROJECT_NAME}Config.cmake
#   * <prefix>/lib/cmake/${PROJECT_NAME}/${PROJECT_NAME}ConfigVersion.cmake
install(
    FILES "${project_config}" "${version_config}"
    DESTINATION "${config_install_dir}"
)

# Config
#   * <prefix>/lib/cmake/${PROJECT_NAME}/${PROJECT_NAME}Targets.cmake
install(
    EXPORT "${targets_export_name}"
    NAMESPACE "${namespace}"
    DESTINATION "${config_install_dir}"
)

if(MSVC)
    #install pdbs
    get_cmake_property(is_multi GENERATOR_IS_MULTI_CONFIG)

    if(is_multi)
      set(config_suffix "$<CONFIG>")
    else()
      set(config_suffix "")
    endif() 

    if(BUILD_SHARED_LIBS)
      set(pdb_dst ${CMAKE_INSTALL_BINDIR})
    else()
      set(pdb_dst ${CMAKE_INSTALL_LIBDIR})
    endif() 

    install(
        DIRECTORY "${pdb_output_dir}/${config_suffix}/"
        DESTINATION ${pdb_dst}
    )
endif()
