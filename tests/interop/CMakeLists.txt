# Drives the built client binaries as delegating launchers against a mock
# instance owning the well-known name -- the cross-toolkit handoff nothing
# else exercises. The launchers exit before window creation, so no display is
# needed; the script skips itself (exit 77) when the bus or python-gi is
# missing.

if(NOT (UNIX AND NOT APPLE))
    return()
endif()

if(NOT (ENABLE_QT AND ENABLE_QT_DBUS_INTEROP) AND NOT ENABLE_GTK)
    return()
endif()

find_program(DBUS_RUN_SESSION dbus-run-session)
find_package(Python3 COMPONENTS Interpreter)
if(NOT DBUS_RUN_SESSION OR NOT Python3_FOUND)
    return()
endif()

# interop-names.h keeps the wire names in one place because a client spelling one
# differently is silently unreachable. The mock is a client here, so it is told the
# names rather than repeating them: a rename should move the test with the code.
# A name that cannot be read out is fatal -- guessing would give the mock a name
# nothing calls, which looks exactly like a mock that failed to start.
set(INTEROP_NAMES_H ${CMAKE_SOURCE_DIR}/libtransmission-app/interop-names.h)

function(tr_read_interop_name macro_name out_var)
    file(STRINGS ${INTEROP_NAMES_H} matched REGEX "^#define +${macro_name} +\"")
    if(NOT matched MATCHES "\"([^\"]+)\"")
        message(FATAL_ERROR "couldn't read ${macro_name} from ${INTEROP_NAMES_H}")
    endif()
    set(${out_var} ${CMAKE_MATCH_1} PARENT_SCOPE)
endfunction()

# The value of an `inline constexpr std::string_view Foo = "bar";` in interop-names.h.
function(tr_read_interop_constant name out_var)
    file(STRINGS ${INTEROP_NAMES_H} matched REGEX "${name} = \"")
    if(NOT matched MATCHES "\"([^\"]+)\"")
        message(FATAL_ERROR "couldn't read ${name} from ${INTEROP_NAMES_H}")
    endif()
    set(${out_var} ${CMAKE_MATCH_1} PARENT_SCOPE)
endfunction()

tr_read_interop_name(TR_INTEROP_DBUS_SERVICE_NAME INTEROP_SERVICE_NAME)
tr_read_interop_name(TR_INTEROP_DBUS_INTERFACE_NAME INTEROP_INTERFACE_NAME)
tr_read_interop_name(TR_INTEROP_DBUS_OBJECT_PATH INTEROP_OBJECT_PATH)
tr_read_interop_name(TR_INTEROP_METHOD_ADD_METAINFO INTEROP_METHOD_ADD_METAINFO)
tr_read_interop_name(TR_INTEROP_METHOD_PRESENT_WINDOW INTEROP_METHOD_PRESENT_WINDOW)
tr_read_interop_name(TR_INTEROP_METHOD_CONFIG_DIR INTEROP_METHOD_CONFIG_DIR)
tr_read_interop_constant(PeerRecordFilename INTEROP_PEER_RECORD_FILENAME)

# The interface is read separately from the service name rather than assumed equal to it:
# assuming would put the one assumption the peer record exists to remove into the test
# that is supposed to catch it.
set(INTEROP_NAME_ENV
    "TR_INTEROP_SERVICE_NAME=${INTEROP_SERVICE_NAME}"
    "TR_INTEROP_INTERFACE_NAME=${INTEROP_INTERFACE_NAME}"
    "TR_INTEROP_OBJECT_PATH=${INTEROP_OBJECT_PATH}"
    "TR_INTEROP_METHOD_ADD_METAINFO=${INTEROP_METHOD_ADD_METAINFO}"
    "TR_INTEROP_METHOD_PRESENT_WINDOW=${INTEROP_METHOD_PRESENT_WINDOW}"
    "TR_INTEROP_METHOD_CONFIG_DIR=${INTEROP_METHOD_CONFIG_DIR}"
    "TR_INTEROP_PEER_RECORD_FILENAME=${INTEROP_PEER_RECORD_FILENAME}")

set(QT_LAUNCHER -)
if(ENABLE_QT AND ENABLE_QT_DBUS_INTEROP)
    set(QT_LAUNCHER $<TARGET_FILE:${TR_NAME}-qt>)
endif()

set(GTK_LAUNCHER -)
if(ENABLE_GTK)
    set(GTK_LAUNCHER $<TARGET_FILE:${TR_NAME}-gtk>)
endif()

add_test(
    NAME interop-delegation
    COMMAND
        bash ${CMAKE_CURRENT_SOURCE_DIR}/delegation-test.sh
        ${QT_LAUNCHER}
        ${GTK_LAUNCHER}
        ${CMAKE_CURRENT_SOURCE_DIR}/mock-instance.py)

set_tests_properties(interop-delegation
    PROPERTIES
        SKIP_RETURN_CODE 77
        TIMEOUT 180
        ENVIRONMENT "${INTEROP_NAME_ENV}")

# A daemon holds the config dir but answers no interop interface, so a launch
# has nobody to delegate to and must refuse to start a second session on it.
if(ENABLE_DAEMON AND ENABLE_QT AND ENABLE_QT_DBUS_INTEROP)
    add_test(
        NAME interop-busy
        COMMAND
            bash ${CMAKE_CURRENT_SOURCE_DIR}/busy-test.sh
            $<TARGET_FILE:${TR_NAME}-daemon>
            ${QT_LAUNCHER})

    set_tests_properties(interop-busy
        PROPERTIES
            SKIP_RETURN_CODE 77
            TIMEOUT 180)
endif()
