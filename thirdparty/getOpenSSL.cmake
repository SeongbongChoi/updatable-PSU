set(GIT_REPOSITORY      "https://github.com/openssl/openssl.git")
set(DEP_NAME            OpenSSL)
set(CLONE_DIR "${VOLE_PSI_THIRDPARTY_CLONE_DIR}/${DEP_NAME}")

set(OPENSSL_LIB_PATH "${VOLEPSI_THIRDPARTY_DIR}/lib/libcrypto.a")
set(OPENSSL_HEADER_PATH "${VOLEPSI_THIRDPARTY_DIR}/include/openssl/ssl.h")

if(EXISTS ${OPENSSL_LIB_PATH} AND EXISTS ${OPENSSL_HEADER_PATH})
    message(STATUS "OpenSSL already installed at ${VOLEPSI_THIRDPARTY_DIR}")
    if(NOT TARGET OpenSSL::Crypto)
        add_library(OpenSSL::Crypto STATIC IMPORTED)
        set_target_properties(OpenSSL::Crypto PROPERTIES
            IMPORTED_LOCATION ${OPENSSL_LIB_PATH}
        )
        target_include_directories(OpenSSL::Crypto SYSTEM INTERFACE ${VOLEPSI_THIRDPARTY_DIR}/include)
    endif()
    return()
endif()

message("============= Building ${DEP_NAME} =============")

find_program(GIT git REQUIRED)

if(NOT EXISTS ${CLONE_DIR})
    execute_process(
        COMMAND ${GIT} clone ${GIT_REPOSITORY} ${CLONE_DIR}
        RESULT_VARIABLE CLONE_RESULT
        COMMAND_ECHO STDOUT
    )
    
    if(NOT CLONE_RESULT EQUAL 0)
        message(FATAL_ERROR "OpenSSL clone failed")
    endif()
endif()

set(CURVE25519_FILE "${CLONE_DIR}/crypto/ec/curve25519.c")

if(EXISTS ${CURVE25519_FILE})
    file(READ ${CURVE25519_FILE} CURVE25519_CONTENT)
    
    if(CURVE25519_CONTENT MATCHES "static void x25519_scalar_mult")
        string(REPLACE 
            "static void x25519_scalar_mult" 
            "void x25519_scalar_mult" 
            CURVE25519_CONTENT_PATCHED 
            "${CURVE25519_CONTENT}"
        )
        file(WRITE ${CURVE25519_FILE} "${CURVE25519_CONTENT_PATCHED}")
        message(STATUS "OpenSSL curve25519.c patched")
    endif()
endif()

execute_process(
    COMMAND ${CLONE_DIR}/Configure 
            no-shared 
            enable-ec_nistp_64_gcc_128 
            no-ssl2 
            no-ssl3 
            no-comp 
            --prefix=${VOLEPSI_THIRDPARTY_DIR}
            --libdir=lib
    WORKING_DIRECTORY ${CLONE_DIR}
    RESULT_VARIABLE CONFIG_RESULT
    COMMAND_ECHO STDOUT
)

if(NOT CONFIG_RESULT EQUAL 0)
    message(FATAL_ERROR "OpenSSL Configure failed")
endif()

execute_process(
    COMMAND make -j${PARALLEL_FETCH}
    WORKING_DIRECTORY ${CLONE_DIR}
    RESULT_VARIABLE BUILD_RESULT
    COMMAND_ECHO STDOUT
)

if(NOT BUILD_RESULT EQUAL 0)
    message(FATAL_ERROR "OpenSSL Build failed")
endif()

execute_process(
    COMMAND make install
    WORKING_DIRECTORY ${CLONE_DIR}
    RESULT_VARIABLE INSTALL_RESULT
    COMMAND_ECHO STDOUT
)

if(NOT INSTALL_RESULT EQUAL 0)
    message(FATAL_ERROR "OpenSSL Install failed")
endif()

if(NOT EXISTS ${OPENSSL_LIB_PATH} OR NOT EXISTS ${OPENSSL_HEADER_PATH})
    message(FATAL_ERROR "OpenSSL build failed:\n  Lib: ${OPENSSL_LIB_PATH}\n  Header: ${OPENSSL_HEADER_PATH}")
endif()

message("${DEP_NAME} installed successfully")