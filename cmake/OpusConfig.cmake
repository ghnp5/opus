if(__opus_config)
  return()
endif()
set(__opus_config INCLUDED)

include(OpusFunctions)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/config.h.cmake.in config.h @ONLY)
add_definitions(-DHAVE_CONFIG_H)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY C_STANDARD 99)

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

include(CheckLibraryExists)
check_library_exists(m floor "" HAVE_LIBM)
if(HAVE_LIBM)
  list(APPEND OPUS_REQUIRED_LIBRARIES m)
endif()

include(CFeatureCheck)
c_feature_check(VLA)

include(CheckIncludeFile)
check_include_file(alloca.h HAVE_ALLOCA_H)

include(CheckSymbolExists)
if(HAVE_ALLOCA_H)
  add_definitions(-DHAVE_ALLOCA_H)
  check_symbol_exists(alloca "alloca.h" USE_ALLOCA_SUPPORTED)
else()
  check_symbol_exists(alloca "stdlib.h;malloc.h" USE_ALLOCA_SUPPORTED)
endif()

include(CheckFunctionExists)
check_function_exists(lrintf HAVE_LRINTF)
check_function_exists(lrint HAVE_LRINT)

if(CMAKE_SYSTEM_PROCESSOR MATCHES "(i[0-9]86|x86|X86|amd64|AMD64|x86_64)")
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(OPUS_CPU_X64 1)
  else()
    set(OPUS_CPU_X86 1)
  endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "(arm|aarch64)")
  set(OPUS_CPU_ARM 1)
endif()

if(NOT OPUS_DISABLE_INTRINSICS)
  opus_supports_cpu_detection(RUNTIME_CPU_CAPABILITY_DETECTION)
endif()

if(OPUS_CPU_X86 OR OPUS_CPU_X64 AND NOT OPUS_DISABLE_INTRINSICS)
  opus_detect_sse(COMPILER_SUPPORT_SIMD)
elseif(OPUS_CPU_ARM AND NOT OPUS_DISABLE_INTRINSICS)
  opus_detect_neon(COMPILER_SUPPORT_NEON)
  if(COMPILER_SUPPORT_NEON)
    option(OPUS_USE_NEON "Option to enable NEON" ON)
    option(OPUS_MAY_HAVE_NEON "Does runtime check for neon support" ON)
    option(OPUS_PRESUME_NEON "Assume target CPU has NEON support" OFF)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
      set(OPUS_PRESUME_NEON ON)
    elseif(CMAKE_SYSTEM_NAME MATCHES "iOS")
      set(OPUS_PRESUME_NEON ON)
    endif()
  endif()
endif()

if(MSVC)
  check_flag(FAST_MATH /fp:fast)
  check_flag(STACK_PROTECTOR /GS)
  check_flag(STACK_PROTECTOR_DISABLED /GS-)
else()
  check_flag(FAST_MATH -ffast-math)
  check_flag(STACK_PROTECTOR -fstack-protector-strong)
  check_flag(HIDDEN_VISIBILITY -fvisibility=hidden)
endif()
