# CMakeList.txt : CMake project for FastNoise
cmake_minimum_required(VERSION 3.7.1)

project(FastNoise2 VERSION 0.5.0)
set(CMAKE_CXX_STANDARD 17)

option(FASTNOISE2_NOISETOOL "Build Noise Tool" ON)
option(FASTNOISE2_TESTS "Build Test" OFF)

if(MSVC)
    #setup pdb target location
    set(pdb_output_dir "${CMAKE_CURRENT_BINARY_DIR}/pdb-files")

    set(CMAKE_PDB_OUTPUT_DIRECTORY "${pdb_output_dir}")
    set(CMAKE_COMPILE_PDB_OUTPUT_DIRECTORY "${pdb_output_dir}") 

    #need to sync pdp files
    add_compile_options("/FS")
endif()

set(install_targets "")

add_subdirectory(src)

if(FASTNOISE2_NOISETOOL)
	add_subdirectory(NoiseTool)
endif()

if(FASTNOISE2_TESTS)
	add_subdirectory(tests)
endif()


#Install -----------------------------------------------------------

# Introduce variables:
# * CMAKE_INSTALL_LIBDIR
# * CMAKE_INSTALL_BINDIR
include(GNUInstallDirs) 

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

if(FASTNOISE2_NOISETOOL)
	if(WIN32)
		#need sdl2 dll on windows, linux its expected to be installed
		if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "4") #32bit
			install(
				FILES NoiseTool/ThirdParty/SDL2-2.0.12/lib/x86/SDL2.dll
				DESTINATION "${CMAKE_INSTALL_BINDIR}"
			)
		else()
			install(
				FILES NoiseTool/ThirdParty/SDL2-2.0.12/lib/x64/SDL2.dll
				DESTINATION "${CMAKE_INSTALL_BINDIR}"
			)
		endif()
	endif()
endif()

# Headers:
install(
    FILES ${install_fastsimd_headers}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/FastSIMD"
)
install(
    FILES ${install_fastnoise_headers}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/FastNoise"
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
