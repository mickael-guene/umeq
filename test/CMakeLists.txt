add_subdirectory (gtest-1.7.0)

include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/src/jitter ${CMAKE_SOURCE_DIR}/src/cache)

SET(GTEST_SOURCE_FILES jitter/const.cpp jitter/add.cpp jitter/sub.cpp jitter/xor.cpp jitter/and.cpp jitter/or.cpp jitter/shl.cpp jitter/shr.cpp jitter/asr.cpp jitter/ite.cpp jitter/cmpeq.cpp jitter/cmpne.cpp jitter/cast.cpp jitter/context.cpp jitter/call.cpp jitter/exit.cpp cache/cache.cpp)

add_executable(testes ${GTEST_SOURCE_FILES})
target_link_libraries(testes -Wl,-z,execstack gtest gtest_main jitter cache)

#Following macro only present in cmake 3, so code below to the same
#GTEST_ADD_TESTS(testes "" ${GTEST_SOURCE_FILES})
FOREACH(GTEST_SOURCE_FILE ${GTEST_SOURCE_FILES})
  FILE(STRINGS ${GTEST_SOURCE_FILE} GTEST_NAMES REGEX ^TEST)
  FOREACH(GTEST_NAME ${GTEST_NAMES})
    STRING(REGEX REPLACE ["\) \(,"] ";" GTEST_NAME ${GTEST_NAME})
    LIST(GET GTEST_NAME 1 GTEST_GROUP_NAME)
    LIST(GET GTEST_NAME 3 GTEST_NAME)
    ADD_TEST(${GTEST_GROUP_NAME}.${GTEST_NAME} testes --gtest_filter=${GTEST_GROUP_NAME}.${GTEST_NAME})
  ENDFOREACH()
ENDFOREACH()
