enable_language(C)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../../scripts/build-softfloat.py" --verify-only
  RESULT_VARIABLE softfloat_status)
if(NOT softfloat_status EQUAL 0)
  message(FATAL_ERROR "Pinned SoftFloat source hashes differ")
endif()
file(GLOB softfloat_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/source/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/source/ARM-VFPv2/*.c")
add_library(reverse_softfloat STATIC ${softfloat_sources})
set_target_properties(reverse_softfloat PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE ON)
target_include_directories(reverse_softfloat PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/source/ARM-VFPv2")
target_include_directories(reverse_softfloat PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/source/include")
target_compile_definitions(reverse_softfloat PRIVATE SOFTFLOAT_FAST_INT64 SOFTFLOAT_ROUND_ODD INLINE_LEVEL=5 THREAD_LOCAL=_Thread_local)
