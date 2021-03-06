# Copyright (c) 2014,2015 DeNA Co., Ltd., Kazuho Oku, Brian Stanback, Laurentiu Nicola, Masanori Ogino, Ryosuke Matsumoto,
#                         David Carlier
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

CMAKE_MINIMUM_REQUIRED(VERSION 2.8.11)
CMAKE_POLICY(SET CMP0003 NEW)

PROJECT(h2o)

SET(VERSION_MAJOR "2")
SET(VERSION_MINOR "1")
SET(VERSION_PATCH "0")
SET(VERSION_PRERELEASE "")
SET(VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}${VERSION_PRERELEASE}")
SET(LIBRARY_VERSION_MAJOR "0")
SET(LIBRARY_VERSION_MINOR "12")
SET(LIBRARY_VERSION_PATCH "0")
SET(LIBRARY_VERSION "${LIBRARY_VERSION_MAJOR}.${LIBRARY_VERSION_MINOR}.${LIBRARY_VERSION_PATCH}${VERSION_PRERELEASE}")
SET(LIBRARY_SOVERSION "${LIBRARY_VERSION_MAJOR}.${LIBRARY_VERSION_MINOR}")

INCLUDE(GNUInstallDirs)

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/version.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/h2o/version.h)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/libh2o.pc.in ${CMAKE_CURRENT_BINARY_DIR}/libh2o.pc @ONLY)
CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/libh2o-evloop.pc.in ${CMAKE_CURRENT_BINARY_DIR}/libh2o-evloop.pc @ONLY)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

FIND_PACKAGE(PkgConfig)
FIND_PACKAGE(Threads REQUIRED)

FIND_PACKAGE(ZLIB REQUIRED)

OPTION(WITH_BUNDLED_SSL "whether or not to use the bundled libressl" ON)

OPTION(WITHOUT_LIBS "skip building libs even when possible" OFF)
OPTION(BUILD_SHARED_LIBS "whether to build a shared library" OFF)

IF (WITH_BUNDLED_SSL)
    SET(BUNDLED_SSL_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/libressl-build/include")
    SET(H2O_SSL_INCLUDE_DIR ${BUNDLED_SSL_INCLUDE_DIR} PARENT_SCOPE)
    SET(BUNDLED_SSL_LIBRARIES "${CMAKE_CURRENT_BINARY_DIR}/libressl-build/lib/libssl.a" "${CMAKE_CURRENT_BINARY_DIR}/libressl-build/lib/libcrypto.a")
    ADD_CUSTOM_TARGET(bundled-ssl make -f ${CMAKE_CURRENT_SOURCE_DIR}/misc/libressl.mk SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/misc
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    message(STATUS "custom target bundled-ssl")
ELSE (WITH_BUNDLED_SSL)
    IF (NOT OPENSSL_FOUND)
        IF ((NOT UNIX) OR CYGWIN)
            MESSAGE(FATAL_ERROR "OpenSSL not found (and the bundled libressl cannot be used on this platform)")
        ENDIF ((NOT UNIX) OR CYGWIN)
        MESSAGE(FATAL_ERROR "OpenSSL not found (nor H2O was configured to used the bundled libressl)")
    ENDIF (NOT OPENSSL_FOUND)
    IF (OPENSSL_VERSION VERSION_LESS "1.0.2")
        MESSAGE(WARNING "*********************************************************************** OpenSSL 1.0.2 is required for HTTP/2 interoperability with web browsers ***********************************************************************")
    ENDIF (OPENSSL_VERSION VERSION_LESS "1.0.2")
ENDIF (WITH_BUNDLED_SSL)

INCLUDE_DIRECTORIES(
    include
    deps/cloexec
    deps/brotli/enc
    deps/golombset
    deps/libgkc
    deps/libyrmcds
    deps/klib
    deps/neverbleed
    deps/picohttpparser
    deps/picotest
    deps/yaml/include
    deps/yoml)

IF (PKG_CONFIG_FOUND)
    PKG_CHECK_MODULES(LIBUV libuv>=1.0.0)
    IF (LIBUV_FOUND)
        INCLUDE_DIRECTORIES(${LIBUV_INCLUDE_DIRS})
        LINK_DIRECTORIES(${LIBUV_LIBRARY_DIRS})
    ENDIF (LIBUV_FOUND)
ENDIF (PKG_CONFIG_FOUND)
IF (NOT LIBUV_FOUND)
    FIND_PACKAGE(LibUV)
    IF (LIBUV_FOUND AND LIBUV_VERSION VERSION_LESS "1.0.0")
        MESSAGE(STATUS "libuv found but ignored; is too old")
        UNSET(LIBUV_FOUND)
    ENDIF ()
    IF (LIBUV_FOUND)
        INCLUDE_DIRECTORIES(${LIBUV_INCLUDE_DIR})
    ENDIF (LIBUV_FOUND)
ENDIF (NOT LIBUV_FOUND)
IF (NOT LIBUV_FOUND)
    SET(LIBUV_LIBRARIES -luv)
ENDIF (NOT LIBUV_FOUND)
IF (DISABLE_LIBUV)
    MESSAGE(STATUS "ignoring found libuv because of DISABLE_LIBUV")
    SET(LIBUV_FOUND FALSE)
ENDIF(DISABLE_LIBUV)

IF (ZLIB_FOUND)
    INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIRS})
    LINK_DIRECTORIES(${ZLIB_LIBRARY_DIRS})
ENDIF (ZLIB_FOUND)

SET(CC_WARNING_FLAGS "-Wall -Wno-unused-value -Wno-unused-function")
IF ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    IF (NOT ("${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "4.6"))
        SET(CC_WARNING_FLAGS "${CC_WARNING_FLAGS} -Wno-unused-but-set-variable")
    ENDIF ()
    IF (NOT ("${CMAKE_C_COMPILER_VERSION}" VERSION_LESS "4.5"))
        SET(CC_WARNING_FLAGS "${CC_WARNING_FLAGS} -Wno-unused-result")
    ENDIF ()
ENDIF ()

SET(CMAKE_C_FLAGS "-O2 -g ${CC_WARNING_FLAGS} ${CMAKE_C_FLAGS} -DH2O_ROOT=\"${CMAKE_INSTALL_PREFIX}\" -DH2O_CONFIG_PATH=\"${CMAKE_INSTALL_SYSCONFDIR}/h2o.conf\"")

SET(LIBYAML_SOURCE_FILES
    deps/yaml/src/api.c
    deps/yaml/src/dumper.c
    deps/yaml/src/emitter.c
    deps/yaml/src/loader.c
    deps/yaml/src/parser.c
    deps/yaml/src/reader.c
    deps/yaml/src/scanner.c
    deps/yaml/src/writer.c)

SET(BROTLI_SOURCE_FILES
    deps/brotli/enc/backward_references.cc
    deps/brotli/enc/block_splitter.cc
    deps/brotli/enc/brotli_bit_stream.cc
    deps/brotli/enc/compress_fragment.cc
    deps/brotli/enc/compress_fragment_two_pass.cc
    deps/brotli/enc/dictionary.cc
    deps/brotli/enc/encode.cc
    deps/brotli/enc/entropy_encode.cc
    deps/brotli/enc/histogram.cc
    deps/brotli/enc/literal_cost.cc
    deps/brotli/enc/metablock.cc
    deps/brotli/enc/static_dict.cc
    deps/brotli/enc/streams.cc
    deps/brotli/enc/utf8_util.cc
    lib/handler/compress/brotli.cc)

SET(LIB_SOURCE_FILES
    deps/cloexec/cloexec.c
    deps/libgkc/gkc.c
    deps/libyrmcds/close.c
    deps/libyrmcds/connect.c
    deps/libyrmcds/recv.c
    deps/libyrmcds/send.c
    deps/libyrmcds/send_text.c
    deps/libyrmcds/socket.c
    deps/libyrmcds/strerror.c
    deps/libyrmcds/text_mode.c
    deps/picohttpparser/picohttpparser.c

    lib/common/cache.c
    lib/common/file.c
    lib/common/filecache.c
    lib/common/hostinfo.c
    lib/common/http1client.c
    lib/common/memcached.c
    lib/common/memory.c
    lib/common/multithread.c
    lib/common/serverutil.c
    lib/common/socket.c
    lib/common/socketpool.c
    lib/common/string.c
    lib/common/time.c
    lib/common/timeout.c
    lib/common/url.c

    lib/core/config.c
    lib/core/configurator.c
    lib/core/context.c
    lib/core/headers.c
    lib/core/logconf.c
    lib/core/proxy.c
    lib/core/request.c
    lib/core/token.c
    lib/core/util.c

    lib/handler/access_log.c
    lib/handler/chunked.c
    lib/handler/compress.c
    lib/handler/compress/gzip.c
    lib/handler/errordoc.c
    lib/handler/expires.c
    lib/handler/fastcgi.c
    lib/handler/file.c
    lib/handler/headers.c
    lib/handler/mimemap.c
    lib/handler/proxy.c
    lib/handler/redirect.c
    lib/handler/reproxy.c
    lib/handler/throttle_resp.c
    lib/handler/status.c
    lib/handler/status/events.c
    lib/handler/status/requests.c
    lib/handler/http2_debug_state.c
    lib/handler/status/durations.c
    lib/handler/configurator/access_log.c
    lib/handler/configurator/compress.c
    lib/handler/configurator/errordoc.c
    lib/handler/configurator/expires.c
    lib/handler/configurator/fastcgi.c
    lib/handler/configurator/file.c
    lib/handler/configurator/headers.c
    lib/handler/configurator/proxy.c
    lib/handler/configurator/redirect.c
    lib/handler/configurator/reproxy.c
    lib/handler/configurator/throttle_resp.c
    lib/handler/configurator/status.c
    lib/handler/configurator/http2_debug_state.c

    lib/http1.c

    lib/tunnel.c

    lib/http2/cache_digests.c
    lib/http2/casper.c
    lib/http2/connection.c
    lib/http2/frame.c
    lib/http2/hpack.c
    lib/http2/scheduler.c
    lib/http2/stream.c
    lib/http2/http2_debug_state.c)

SET(UNIT_TEST_SOURCE_FILES
    ${LIB_SOURCE_FILES}
    ${LIBYAML_SOURCE_FILES}
    ${BROTLI_SOURCE_FILES}
    deps/picotest/picotest.c
    t/00unit/test.c
    t/00unit/lib/common/cache.c
    t/00unit/lib/common/hostinfo.c
    t/00unit/lib/common/multithread.c
    t/00unit/lib/common/serverutil.c
    t/00unit/lib/common/socket.c
    t/00unit/lib/common/string.c
    t/00unit/lib/common/time.c
    t/00unit/lib/common/url.c
    t/00unit/lib/core/headers.c
    t/00unit/lib/core/proxy.c
    t/00unit/lib/core/util.c
    t/00unit/lib/handler/compress.c
    t/00unit/lib/handler/fastcgi.c
    t/00unit/lib/handler/file.c
    t/00unit/lib/handler/headers.c
    t/00unit/lib/handler/mimemap.c
    t/00unit/lib/handler/redirect.c
    t/00unit/lib/http2/cache_digests.c
    t/00unit/lib/http2/casper.c
    t/00unit/lib/http2/hpack.c
    t/00unit/lib/http2/scheduler.c
    t/00unit/src/ssl.c
    t/00unit/issues/293.c
    t/00unit/issues/percent-encode-zero-byte.c)
LIST(REMOVE_ITEM UNIT_TEST_SOURCE_FILES
    lib/common/cache.c
    lib/common/hostinfo.c
    lib/common/multithread.c
    lib/common/serverutil.c
    lib/common/socket.c
    lib/common/string.c
    lib/common/time.c
    lib/common/url.c
    lib/core/headers.c
    lib/core/proxy.c
    lib/core/util.c
    lib/handler/compress.c
    lib/handler/compress/gzip.c
    lib/handler/fastcgi.c
    lib/handler/file.c
    lib/handler/headers.c
    lib/handler/mimemap.c
    lib/handler/redirect.c
    lib/http2/cache_digests.c
    lib/http2/casper.c
    lib/http2/hpack.c
    lib/http2/scheduler.c)

SET(EXTRA_LIBS ${EXTRA_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${CMAKE_DL_LIBS})

IF (ZLIB_FOUND)
    LIST(INSERT EXTRA_LIBS 0 ${ZLIB_LIBRARIES})
ENDIF (ZLIB_FOUND)

IF (WSLAY_FOUND)
    ADD_LIBRARY(libh2o lib/websocket.c ${LIB_SOURCE_FILES})
    ADD_LIBRARY(libh2o-evloop lib/websocket.c ${LIB_SOURCE_FILES})
ELSE ()
    ADD_LIBRARY(libh2o ${LIB_SOURCE_FILES})
    ADD_LIBRARY(libh2o-evloop ${LIB_SOURCE_FILES})
ENDIF (WSLAY_FOUND)
ADD_DEPENDENCIES(libh2o bundled-ssl)
ADD_DEPENDENCIES(libh2o-evloop bundled-ssl)
TARGET_INCLUDE_DIRECTORIES(libh2o BEFORE PUBLIC ${BUNDLED_SSL_INCLUDE_DIR})
TARGET_INCLUDE_DIRECTORIES(libh2o-evloop BEFORE PUBLIC ${BUNDLED_SSL_INCLUDE_DIR})

SET_TARGET_PROPERTIES(libh2o PROPERTIES
    OUTPUT_NAME h2o
    VERSION ${LIBRARY_VERSION}
    SOVERSION ${LIBRARY_SOVERSION})
TARGET_LINK_LIBRARIES(libh2o ${LIBUV_LIBRARIES} ${EXTRA_LIBS} ${BUNDLED_SSL_LIBRARIES})
SET_TARGET_PROPERTIES(libh2o-evloop PROPERTIES
    OUTPUT_NAME h2o-evloop
    COMPILE_FLAGS "-DH2O_USE_LIBUV=0"
    VERSION ${LIBRARY_VERSION}
    SOVERSION ${LIBRARY_SOVERSION})
TARGET_LINK_LIBRARIES(libh2o-evloop ${EXTRA_LIBS} ${BUNDLED_SSL_LIBRARIES})

IF (OPENSSL_FOUND)
    TARGET_INCLUDE_DIRECTORIES(libh2o PUBLIC ${OPENSSL_INCLUDE_DIR})
    TARGET_INCLUDE_DIRECTORIES(libh2o-evloop PUBLIC ${OPENSSL_INCLUDE_DIR})
    TARGET_LINK_LIBRARIES(libh2o ${OPENSSL_LIBRARIES})
    TARGET_LINK_LIBRARIES(libh2o-evloop ${OPENSSL_LIBRARIES})
    IF (LIBUV_FOUND AND NOT WITHOUT_LIBS)
        INSTALL(TARGETS libh2o DESTINATION ${CMAKE_INSTALL_LIBDIR})
    ELSE ()
        SET_TARGET_PROPERTIES(libh2o PROPERTIES EXCLUDE_FROM_ALL 1)
    ENDIF ()
    IF (NOT WITHOUT_LIBS)
	INSTALL(TARGETS libh2o-evloop DESTINATION ${CMAKE_INSTALL_LIBDIR})
    ELSE ()
        SET_TARGET_PROPERTIES(libh2o-evloop PROPERTIES EXCLUDE_FROM_ALL 1)
    ENDIF()
ELSE (OPENSSL_FOUND)
    SET_TARGET_PROPERTIES(libh2o PROPERTIES EXCLUDE_FROM_ALL 1)
    SET_TARGET_PROPERTIES(libh2o-evloop PROPERTIES EXCLUDE_FROM_ALL 1)
ENDIF (OPENSSL_FOUND)

IF (NOT WITHOUT_LIBS)
    INSTALL(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} FILES_MATCHING PATTERN "*.h")
    IF (LIBUV_FOUND)
        INSTALL(FILES "${CMAKE_BINARY_DIR}/libh2o.pc" DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
    ENDIF ()
    INSTALL(FILES "${CMAKE_BINARY_DIR}/libh2o-evloop.pc" DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
ENDIF ()

INSTALL(PROGRAMS share/h2o/annotate-backtrace-symbols share/h2o/fastcgi-cgi share/h2o/fetch-ocsp-response share/h2o/kill-on-close share/h2o/setuidgid share/h2o/start_server DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/h2o)
INSTALL(FILES share/h2o/ca-bundle.crt DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/h2o)
INSTALL(FILES share/h2o/status/index.html DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/h2o/status)
INSTALL(DIRECTORY doc/ DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/h2o PATTERN "Makefile" EXCLUDE PATTERN "README.md" EXCLUDE)
INSTALL(DIRECTORY examples/ DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/doc/h2o/examples)

# tests
ADD_EXECUTABLE(t-00unit-evloop.t ${UNIT_TEST_SOURCE_FILES})
SET_TARGET_PROPERTIES(t-00unit-evloop.t PROPERTIES
    COMPILE_FLAGS "-DH2O_USE_LIBUV=0 -DH2O_USE_BROTLI=1 -DH2O_UNITTEST=1"
    EXCLUDE_FROM_ALL 1)
TARGET_LINK_LIBRARIES(t-00unit-evloop.t ${EXTRA_LIBS})

IF (LIBUV_FOUND)
    ADD_EXECUTABLE(t-00unit-libuv.t ${UNIT_TEST_SOURCE_FILES})
    SET_TARGET_PROPERTIES(t-00unit-libuv.t PROPERTIES
        COMPILE_FLAGS "-DH2O_USE_BROTLI=1 -DH2O_UNITTEST=1"
        EXCLUDE_FROM_ALL 1)
    TARGET_LINK_LIBRARIES(t-00unit-libuv.t ${LIBUV_LIBRARIES} ${EXTRA_LIBS})
ENDIF (LIBUV_FOUND)

IF (WITH_BUNDLED_SSL)
    TARGET_INCLUDE_DIRECTORIES(t-00unit-evloop.t BEFORE PUBLIC ${BUNDLED_SSL_INCLUDE_DIR})
    TARGET_LINK_LIBRARIES(t-00unit-evloop.t ${BUNDLED_SSL_LIBRARIES})
    ADD_DEPENDENCIES(t-00unit-evloop.t bundled-ssl)
    IF (LIBUV_FOUND)
        TARGET_INCLUDE_DIRECTORIES(t-00unit-libuv.t BEFORE PUBLIC ${BUNDLED_SSL_INCLUDE_DIR})
        TARGET_LINK_LIBRARIES(t-00unit-libuv.t ${BUNDLED_SSL_LIBRARIES})
        ADD_DEPENDENCIES(t-00unit-libuv.t bundled-ssl)
    ENDIF (LIBUV_FOUND)
ELSE (WITH_BUNDLED_SSL)
    IF (OPENSSL_FOUND)
        TARGET_INCLUDE_DIRECTORIES(t-00unit-evloop.t PUBLIC ${OPENSSL_INCLUDE_DIR})
        TARGET_LINK_LIBRARIES(t-00unit-evloop.t ${OPENSSL_LIBRARIES})
        IF (LIBUV_FOUND)
            TARGET_INCLUDE_DIRECTORIES(t-00unit-libuv.t PUBLIC ${OPENSSL_INCLUDE_DIR})
            TARGET_LINK_LIBRARIES(t-00unit-libuv.t ${OPENSSL_LIBRARIES})
        ENDIF (LIBUV_FOUND)
    ENDIF (OPENSSL_FOUND)
ENDIF (WITH_BUNDLED_SSL)

ADD_CUSTOM_TARGET(check env H2O_ROOT=. BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR} prove -v t/*.t
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS h2o t-00unit-evloop.t)
IF (LIBUV_FOUND)
    ADD_DEPENDENCIES(check t-00unit-libuv.t lib-examples)
ENDIF (LIBUV_FOUND)

ADD_CUSTOM_TARGET(check-as-root env H2O_ROOT=. BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR} prove -v t/90root-*.t
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

# environment-specific tweaks
IF (APPLE)
    SET_SOURCE_FILES_PROPERTIES(lib/socket.c lib/websocket.c src/main.c examples/simple.c examples/websocket.c PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
ELSEIF (CMAKE_SYSTEM_NAME STREQUAL "Linux")
     SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -D_GNU_SOURCE")
     IF (WITH_BUNDLED_SSL)
         TARGET_LINK_LIBRARIES(t-00unit-evloop.t "rt")
         IF (LIBUV_FOUND)
             TARGET_LINK_LIBRARIES(t-00unit-libuv.t "rt")
         ENDIF (LIBUV_FOUND)
    ENDIF (WITH_BUNDLED_SSL)
ELSEIF ("${CMAKE_SYSTEM_NAME}" MATCHES "SunOS")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthreads -D_POSIX_PTHREAD_SEMANTICS")
    TARGET_LINK_LIBRARIES(t-00unit-evloop.t "socket" "nsl")
    IF (LIBUV_FOUND)
        TARGET_LINK_LIBRARIES(t-00unit-libuv.t "socket" "nsl")
    ENDIF (LIBUV_FOUND)
ELSE ()
    # for FreeBSD, etc.
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
ENDIF ()

SET(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS}")
