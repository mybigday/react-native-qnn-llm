cmake_minimum_required(VERSION 3.4.1)
project(QnnLlm)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 17)

if (DEFINED ENV{QNN_SDK_ROOT} AND NOT QNN_SDK_ROOT)
  set(QNN_SDK_ROOT $ENV{QNN_SDK_ROOT})
endif()

option(USE_QNN_SDK_LIB "Use QNN SDK Lib" OFF)

include_directories(
  ${QNN_SDK_ROOT}/include/Genie
  ${QNN_SDK_ROOT}/include/QNN
)

file(GLOB_RECURSE SRC_FILES "../cpp/*.cpp" "cpp-adapter.cpp")

if (NOT QNN_SDK_ROOT)
  message(WARNING "QNN_SDK_ROOT is not set, not building QNN LLM")
endif()

if (CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a" AND QNN_SDK_ROOT)
  add_library(qnn-llm SHARED ${SRC_FILES})

  include(FetchContent)

  find_package(ZLIB REQUIRED)

  set(ZSTD_BUILD_STATIC ON)
  set(ZSTD_BUILD_SHARED OFF)
  set(ZSTD_BUILD_EXAMPLES OFF)
  set(ZSTD_BUILD_TESTS OFF)

  FetchContent_Declare(
    zstd
    URL https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz
  )
  FetchContent_MakeAvailable(zstd)
  add_subdirectory(${zstd_SOURCE_DIR}/build/cmake)

  set(QNN_LIB_DIR ${QNN_SDK_ROOT}/lib)
  set(QNN_PLAT_LIB_DIR ${QNN_LIB_DIR}/aarch64-android)

  target_include_directories(qnn-llm PRIVATE ../cpp ${ZLIB_INCLUDE_DIRS})

  target_link_libraries(
    qnn-llm
    ${QNN_PLAT_LIB_DIR}/libGenie.so
    log
    ZLIB::ZLIB
    libzstd_static
  )

  file(GLOB QNN_PLAT_LIBS
    ${QNN_PLAT_LIB_DIR}/libGenie.so
    ${QNN_PLAT_LIB_DIR}/libQnnGenAiTransformerCpuOpPkg.so
    ${QNN_PLAT_LIB_DIR}/libQnnGenAiTransformerModel.so
    ${QNN_PLAT_LIB_DIR}/libQnnGenAiTransformer.so
    ${QNN_PLAT_LIB_DIR}/libQnnCpu.so
    ${QNN_PLAT_LIB_DIR}/libQnnDspNetRunExtensions.so
    ${QNN_PLAT_LIB_DIR}/libQnnHtpNetRunExtensions.so
  )

  if (USE_QNN_SDK_LIB)
    file(GLOB QNN_HTP_LIBS
      ${QNN_PLAT_LIB_DIR}/libQnnSystem.so
      ${QNN_PLAT_LIB_DIR}/libQnnGpu.so
      ${QNN_PLAT_LIB_DIR}/libQnnHtp.so
      ${QNN_PLAT_LIB_DIR}/libQnnHtpPrepare.so
      ${QNN_PLAT_LIB_DIR}/libQnnHtpV68Stub.so
      ${QNN_PLAT_LIB_DIR}/libQnnHtpV69Stub.so
      ${QNN_PLAT_LIB_DIR}/libQnnHtpV73Stub.so
      ${QNN_PLAT_LIB_DIR}/libQnnHtpV75Stub.so
      ${QNN_PLAT_LIB_DIR}/libQnnHtpV79Stub.so
      ${QNN_LIB_DIR}/hexagon-v68/unsigned/libQnnHtpV68Skel.so
      ${QNN_LIB_DIR}/hexagon-v69/unsigned/libQnnHtpV69Skel.so
      ${QNN_LIB_DIR}/hexagon-v73/unsigned/libQnnHtpV73Skel.so
      ${QNN_LIB_DIR}/hexagon-v75/unsigned/libQnnHtpV75Skel.so
      ${QNN_LIB_DIR}/hexagon-v79/unsigned/libQnnHtpV79Skel.so
    )
    list(APPEND QNN_LIBS ${QNN_HTP_LIBS})
  endif()

  # concatenate two lists
  list(APPEND QNN_LIBS ${QNN_PLAT_LIBS})

  add_custom_command(TARGET qnn-llm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${QNN_LIBS}
    $<TARGET_FILE_DIR:qnn-llm>)
endif()
