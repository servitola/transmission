// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#pragma once

#include <string_view>

// The names by which one Transmission instance reaches another.
// They are a wire contract shared with separately-built clients:
// changing a value here makes such a client silently unreachable.
//
// This is the whole inventory. Four of the shared names have to live in
// `libtransmission/transmission.h` instead, because `transmission-cli` and
// `transmission-daemon` need them and do not link libtransmission-app:
//
//   TR_CONFIG_DIR_NAME            the default config dir the clients share
//   TR_RPC_SESSION_ID_HEADER      the CSRF header a third-party RPC client sends
//   TR_RPC_RPC_VERSION_HEADER     the version header it reads back
//   TrHttpServerDefaultBasePath   with TrHttpServerRpcRelativePath, the RPC URL
//
// and one in `libtransmission/config-dir-lock.cc`, for the same reason:
//
//   the config dir lock filename, which is what stops two sessions at once
//
// A fork renaming these has to visit all three files. A fork keeping interop
// with Transmission has to leave every one of them alone.

// Macros, because Q_CLASSINFO and QStringLiteral require a literal token rather than a constexpr value.
// The interface is spelled out rather than defined as the service name, so that a fork
// taking its own service name decides separately whether the interface follows it.
#define TR_INTEROP_DBUS_SERVICE_NAME "com.transmissionbt.Transmission"
#define TR_INTEROP_DBUS_INTERFACE_NAME "com.transmissionbt.Transmission"
#define TR_INTEROP_DBUS_OBJECT_PATH "/com/transmissionbt/Transmission"

// The class id a running Qt client registers on Windows.
// transmission-qt.idl and dist/msi/components/QtClient.wxs share this;
// so if you change the text here, remember to change it there too.
#define TR_INTEROP_COM_CLASS_ID "{0e2c952c-0597-491f-ba26-249d7e6fab49}"

#define TR_INTEROP_METHOD_ADD_METAINFO "AddMetainfo"
#define TR_INTEROP_METHOD_PRESENT_WINDOW "PresentWindow"
#define TR_INTEROP_METHOD_CONFIG_DIR "ConfigDir"

namespace tr::interop
{

inline constexpr std::string_view DBusIntrospectionXml =
    "<node>"
    "<interface name='" TR_INTEROP_DBUS_INTERFACE_NAME
    "'>"
    "<method name='" TR_INTEROP_METHOD_ADD_METAINFO
    "'><arg type='s' direction='in'/><arg type='b' direction='out'/></method>"
    "<method name='" TR_INTEROP_METHOD_PRESENT_WINDOW
    "'><arg type='b' direction='out'/></method>"
    "<method name='" TR_INTEROP_METHOD_CONFIG_DIR
    "'><arg type='s' direction='out'/></method>"
    "</interface>"
    "</node>";

// New clients register an OLE item moniker, delimiter "!", whose item is this prefix
// followed by the canonical config dir (defined below).
// That gives every independently-running config dir its own deterministic ROT entry.
inline constexpr std::string_view ComConfigMonikerPrefix = "Transmission.ConfigDir:";

// Method names are part of the contract. Signatures in D-Bus notation:
//   AddMetainfo(s) -> b   PresentWindow() -> b   ConfigDir() -> s
//
// - AddMetainfo's string carries a URL or magnet link as itself,
//   or a torrent file's contents base64'd.
// - PresentWindow is called by third-party scripts and must keep its name.
// - ConfigDir must respond with the canonical config dir, defined below.
inline constexpr std::string_view MethodAddMetainfo = TR_INTEROP_METHOD_ADD_METAINFO;
inline constexpr std::string_view MethodPresentWindow = TR_INTEROP_METHOD_PRESENT_WINDOW;
inline constexpr std::string_view MethodConfigDir = TR_INTEROP_METHOD_CONFIG_DIR;

// THE CANONICAL CONFIG DIR
//
// A rule, not a description of what one implementation happens to produce: two clients
// that normalize differently disagree about whether they serve the same dir, and which
// of the two answers you get depends on which of them launched second.
//
// The canonical form of a config dir is the path the OS reports for the directory itself:
//   - POSIX:   realpath(3) -- absolute, every symlink resolved, no trailing slash.
//   - Windows: GetFinalPathNameByHandleW() with FILE_NAME_NORMALIZED | VOLUME_NAME_DOS on
//              a handle to the dir, with the `\\?\` prefix removed and `\\?\UNC\` rewritten
//              to `\\`. That is the on-disk spelling, so its case is the filesystem's
//              rather than the caller's.
// A dir that does not exist yet cannot be resolved. It canonicalizes to itself made
// absolute against the working directory, and to nothing further.
//
// `tr::interop::canonical_config_dir()` is this rule; nothing else should reimplement it.
//
// How much the rule carries differs by transport, and Windows is the strict one:
//   - D-Bus: a caller canonicalizes both its own dir and the ConfigDir() reply before
//     comparing them, so a peer that answers a differently-spelled path still matches
//     as long as the two resolve alike.
//   - COM: the canonical dir is concatenated into the moniker item above and matched as
//     an exact string. Nothing re-canonicalizes it, so a client that spells it
//     differently is not found at all.

// A running session stores its D-Bus info in `${config_dir}/dbus-peer`.
// See `dbus-peer-record.h` for more info about that file and its use.
inline constexpr std::string_view PeerRecordFilename = "dbus-peer";

// Held from the moment a launch gives up on delegating until the moment it can itself
// be delegated to, so that two launches racing on one config dir cannot both start.
// Shared like the rest: clients naming this differently do not serialize against each
// other, and both win a race that neither can lose.
inline constexpr std::string_view StartupLockFilename = "lock.startup";

} // namespace tr::interop
