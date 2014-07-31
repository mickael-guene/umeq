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

#add binary testes
ADD_TEST(Static.Nolib.Hello.a.out.O0.arm ../src/umeq ${CMAKE_SOURCE_DIR}/test/static/nolib/hello/a.out.O0.arm)
ADD_TEST(Static.Nolib.Hello.a.out.O1.arm ../src/umeq ${CMAKE_SOURCE_DIR}/test/static/nolib/hello/a.out.O1.arm)
ADD_TEST(Static.Nolib.Hello.a.out.O2.arm ../src/umeq ${CMAKE_SOURCE_DIR}/test/static/nolib/hello/a.out.O2.arm)
ADD_TEST(Static.Nolib.Hello.a.out.O0.thumb ../src/umeq ${CMAKE_SOURCE_DIR}/test/static/nolib/hello/a.out.O0.thumb)
ADD_TEST(Static.Nolib.Hello.a.out.O1.thumb ../src/umeq ${CMAKE_SOURCE_DIR}/test/static/nolib/hello/a.out.O1.thumb)
ADD_TEST(Static.Nolib.Hello.a.out.O2.thumb ../src/umeq ${CMAKE_SOURCE_DIR}/test/static/nolib/hello/a.out.O2.thumb)
ADD_TEST(Static.Nolib.Helloloop.a.out.O0.arm ../src/umeq ${CMAKE_SOURCE_DIR}/test/static/nolib/helloloop/a.out.O0.arm)
ADD_TEST(Static.Nolib.Helloloop.a.out.O1.arm ../src/umeq ${CMAKE_SOURCE_DIR}/test/static/nolib/helloloop/a.out.O1.arm)
ADD_TEST(Static.Nolib.Helloloop.a.out.O2.arm ../src/umeq ${CMAKE_SOURCE_DIR}/test/static/nolib/helloloop/a.out.O2.arm)
ADD_TEST(Static.Nolib.Helloloop.a.out.O0.thumb ../src/umeq ${CMAKE_SOURCE_DIR}/test/static/nolib/helloloop/a.out.O0.thumb)
ADD_TEST(Static.Nolib.Helloloop.a.out.O1.thumb ../src/umeq ${CMAKE_SOURCE_DIR}/test/static/nolib/helloloop/a.out.O1.thumb)
ADD_TEST(Static.Nolib.Helloloop.a.out.O2.thumb ../src/umeq ${CMAKE_SOURCE_DIR}/test/static/nolib/helloloop/a.out.O2.thumb)
ADD_TEST(Static.libc.Hello.a.out.O0.arm ../src/umeq ${CMAKE_SOURCE_DIR}/test/static/libc/hello/a.out.O0.arm)
