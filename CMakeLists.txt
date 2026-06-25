cmake_minimum_required(VERSION 3.25)

project(TmfAdditiveSynth
    VERSION 0.1.0
    LANGUAGES C CXX
    DESCRIPTION "JUCE module for additive synth voice interceptors")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

option(TMF_ADDITIVE_SYNTH_BUILD_TESTS "Build tmf_additive_synth tests" ${PROJECT_IS_TOP_LEVEL})
option(TMF_ADDITIVE_SYNTH_USE_IPP "Use Intel IPP for FFT support when available" ON)
set(TMF_ADDITIVE_SYNTH_JUCE_PATH "" CACHE PATH "Optional path to a JUCE checkout for standalone builds")
set(TMF_ADDITIVE_SYNTH_INTERCEPT_SYNTH_PATH "" CACHE PATH "Optional path to tmf_intercept_synth for standalone builds")
set(TMF_ADDITIVE_SYNTH_IPP_ROOT "" CACHE PATH "Optional path to an Intel IPP installation")

if(PROJECT_IS_TOP_LEVEL)
    include(FetchContent)

    if(TMF_ADDITIVE_SYNTH_JUCE_PATH)
        add_subdirectory("${TMF_ADDITIVE_SYNTH_JUCE_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/JUCE")
    else()
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG develop
            GIT_PROGRESS TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()
endif()

if(NOT COMMAND juce_add_module)
    message(FATAL_ERROR "JUCE must be added to your project before tmf_additive_synth")
endif()

if(NOT TARGET tmf_intercept_synth)
    if(TMF_ADDITIVE_SYNTH_INTERCEPT_SYNTH_PATH)
        add_subdirectory("${TMF_ADDITIVE_SYNTH_INTERCEPT_SYNTH_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/tmf_intercept_synth")
    else()
        add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../tmf_intercept_synth" "${CMAKE_CURRENT_BINARY_DIR}/tmf_intercept_synth")
    endif()
endif()

juce_add_module("${CMAKE_CURRENT_LIST_DIR}")
add_library(tmf::additive_synth ALIAS tmf_additive_synth)

function(tmf_additive_synth_configure_ipp target)
    if(NOT TMF_ADDITIVE_SYNTH_USE_IPP OR APPLE)
        return()
    endif()

    set(ipp_root_candidates)
    if(TMF_ADDITIVE_SYNTH_IPP_ROOT)
        list(APPEND ipp_root_candidates "${TMF_ADDITIVE_SYNTH_IPP_ROOT}")
    endif()

    if(DEFINED ENV{IPP_DIR})
        get_filename_component(ipp_root_from_dir "$ENV{IPP_DIR}/../../.." ABSOLUTE)
        list(APPEND ipp_root_candidates "${ipp_root_from_dir}")
    endif()

    if(WIN32)
        list(APPEND ipp_root_candidates
            "$ENV{USERPROFILE}/.nuget/packages/intelipp.static.win-x64/2022.3.0.387"
            "$ENV{IPP_ROOT}"
            "$ENV{IPPROOT}"
            "C:/Program Files (x86)/Intel/oneAPI/ipp/latest")
    elseif(UNIX)
        list(APPEND ipp_root_candidates
            "$ENV{IPP_ROOT}"
            "$ENV{IPPROOT}"
            "/opt/intel/oneapi/ipp/latest")
    endif()

    foreach(candidate IN LISTS ipp_root_candidates)
        if(candidate AND IS_DIRECTORY "${candidate}")
            set(ipp_root "${candidate}")
            break()
        endif()
    endforeach()

    if(NOT ipp_root)
        message(STATUS "Intel IPP not found; tmf_additive_synth will use its portable FFT fallback")
        return()
    endif()

    if(EXISTS "${ipp_root}/build/native/include/ipp/ipps.h")
        set(ipp_include "${ipp_root}/build/native/include")
        set(ipp_library_dir "${ipp_root}/build/native/win-x64")
    elseif(EXISTS "${ipp_root}/include/ipps.h")
        set(ipp_include "${ipp_root}/include")
        if(WIN32)
            foreach(candidate
                    "${ipp_root}/lib/intel64"
                    "${ipp_root}/lib/intel64_win"
                    "${ipp_root}/lib")
                if(IS_DIRECTORY "${candidate}")
                    set(ipp_library_dir "${candidate}")
                    break()
                endif()
            endforeach()
        else()
            foreach(candidate
                    "${ipp_root}/lib/intel64"
                    "${ipp_root}/lib")
                if(IS_DIRECTORY "${candidate}")
                    set(ipp_library_dir "${candidate}")
                    break()
                endif()
            endforeach()
        endif()
    endif()

    if(NOT ipp_include OR NOT ipp_library_dir)
        message(STATUS "Intel IPP not found at ${ipp_root}; tmf_additive_synth will use its portable FFT fallback")
        return()
    endif()

    find_library(ipp_signal_library NAMES ippsmt ipps PATHS "${ipp_library_dir}" NO_DEFAULT_PATH)
    find_library(ipp_core_library NAMES ippcoremt ippcore PATHS "${ipp_library_dir}" NO_DEFAULT_PATH)

    if(NOT ipp_signal_library OR NOT ipp_core_library)
        message(STATUS "Intel IPP libraries not found at ${ipp_library_dir}; tmf_additive_synth will use its portable FFT fallback")
        return()
    endif()

    message(STATUS "Intel IPP found for tmf_additive_synth at ${ipp_root}")
    target_include_directories(${target} INTERFACE "${ipp_include}")
    target_link_libraries(${target} INTERFACE "${ipp_signal_library}" "${ipp_core_library}")
    target_compile_definitions(${target} INTERFACE TMF_ADDITIVE_SYNTH_HAS_IPP=1)
endfunction()

tmf_additive_synth_configure_ipp(tmf_additive_synth)

file(GLOB_RECURSE TMF_ADDITIVE_SYNTH_TEST_SOURCE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/tests/*.cpp")
file(GLOB_RECURSE TMF_ADDITIVE_SYNTH_TEST_HEADER_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/tests/*.h")
set(TMF_ADDITIVE_SYNTH_TEST_FILES
    ${TMF_ADDITIVE_SYNTH_TEST_SOURCE_FILES}
    ${TMF_ADDITIVE_SYNTH_TEST_HEADER_FILES})

get_target_property(TMF_ADDITIVE_SYNTH_MODULE_SOURCES tmf_additive_synth INTERFACE_SOURCES)
if(TMF_ADDITIVE_SYNTH_MODULE_SOURCES)
    list(REMOVE_ITEM TMF_ADDITIVE_SYNTH_MODULE_SOURCES ${TMF_ADDITIVE_SYNTH_TEST_SOURCE_FILES})
    set_target_properties(tmf_additive_synth PROPERTIES
        INTERFACE_SOURCES "${TMF_ADDITIVE_SYNTH_MODULE_SOURCES}")
endif()

get_target_property(TMF_ADDITIVE_SYNTH_MODULE_JUCE_SOURCES tmf_additive_synth INTERFACE_JUCE_MODULE_SOURCES)
if(TMF_ADDITIVE_SYNTH_MODULE_JUCE_SOURCES)
    list(REMOVE_ITEM TMF_ADDITIVE_SYNTH_MODULE_JUCE_SOURCES ${TMF_ADDITIVE_SYNTH_TEST_SOURCE_FILES})
    set_target_properties(tmf_additive_synth PROPERTIES
        INTERFACE_JUCE_MODULE_SOURCES "${TMF_ADDITIVE_SYNTH_MODULE_JUCE_SOURCES}")
endif()

get_target_property(TMF_ADDITIVE_SYNTH_MODULE_JUCE_HEADERS tmf_additive_synth INTERFACE_JUCE_MODULE_HEADERS)
if(TMF_ADDITIVE_SYNTH_MODULE_JUCE_HEADERS)
    list(REMOVE_ITEM TMF_ADDITIVE_SYNTH_MODULE_JUCE_HEADERS ${TMF_ADDITIVE_SYNTH_TEST_HEADER_FILES})
    set_target_properties(tmf_additive_synth PROPERTIES
        INTERFACE_JUCE_MODULE_HEADERS "${TMF_ADDITIVE_SYNTH_MODULE_JUCE_HEADERS}")
endif()

if(NOT PROJECT_IS_TOP_LEVEL)
    set(TMF_ADDITIVE_SYNTH_TEST_SOURCE_FILES ${TMF_ADDITIVE_SYNTH_TEST_SOURCE_FILES} PARENT_SCOPE)
    set(TMF_ADDITIVE_SYNTH_TEST_HEADER_FILES ${TMF_ADDITIVE_SYNTH_TEST_HEADER_FILES} PARENT_SCOPE)
    set(TMF_ADDITIVE_SYNTH_TEST_FILES ${TMF_ADDITIVE_SYNTH_TEST_FILES} PARENT_SCOPE)
endif()

if(PROJECT_IS_TOP_LEVEL AND TMF_ADDITIVE_SYNTH_BUILD_TESTS)
    enable_testing()

    include(FetchContent)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.8.1
        GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(Catch2)

    set_source_files_properties(${TMF_ADDITIVE_SYNTH_TEST_SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY FALSE)

    add_executable(tmf_additive_synth_tests ${TMF_ADDITIVE_SYNTH_TEST_FILES})
    target_compile_features(tmf_additive_synth_tests PRIVATE cxx_std_20)
    target_link_libraries(tmf_additive_synth_tests PRIVATE
        tmf_additive_synth
        tmf_intercept_synth
        Catch2::Catch2WithMain
        juce::juce_audio_processors
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    target_compile_definitions(tmf_additive_synth_tests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1)

    include(${Catch2_SOURCE_DIR}/extras/Catch.cmake)
    catch_discover_tests(tmf_additive_synth_tests DISCOVERY_MODE PRE_TEST)
endif()
