// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace tr::interop
{

enum class Intent : uint8_t
{
    // This launch asks for something no instance already running can do for it --
    // a session on another host, or a window that opens minimized.
    // Handing it over would drop what it asked for and report success.
    Standalone,
    Present,
    AddTorrents
};

// An instance you can talk to: this process, or one reached over a transport.
// The caller doesn't need to know which -- nor whether it is even this program.
// A separately-built client that shares a config dir answers here too,
// so what follows binds any implementation, not just these clients.
class Instance
{
public:
    virtual ~Instance() = default;

    // returns `std::nullopt` for unreachable remote instances.
    // false means reached, but the instance couldn't do the thing.
    [[nodiscard]] virtual std::optional<bool> present_window() = 0;
    [[nodiscard]] virtual std::optional<bool> add_metainfo(std::string_view metainfo) = 0;

    // The canonical config dir this instance serves, as interop-names.h defines canonical.
    // Empty when the instance did not say, which a release without the method never can.
    // An instance that serves no config dir -- one attached to a session on another host --
    // is not a candidate for delegation at all and must not be published.
    [[nodiscard]] virtual std::string config_dir() = 0;

    // For log messages. Never parsed.
    [[nodiscard]] virtual std::string description() const = 0;
};

class Transport
{
public:
    virtual ~Transport() = default;

    // `self` must answer until this Transport is destroyed.
    // Whoever owns the Instance must destroy the Transport before releasing it.
    virtual void publish(Instance& self) = 0;

    // Does NOT filter by config dir: which instance counts as a match is
    // StartupCoordinator's decision (startup-coordinator.h), so every transport
    // answers this the same way. Must never return this process.
    [[nodiscard]] virtual std::unique_ptr<Instance> find_other_instance() = 0;
};

// Returns the canonical form of `config_dir`, as interop-names.h defines it.
// That comment is the contract; this is its one implementation. Every ConfigDir()
// answerer, every comparison, and every COM moniker item goes through here.
[[nodiscard]] std::string canonical_config_dir(std::string_view config_dir);

// The canonical form of `config_dir`, creating the directory first if it is not there.
// A launch resolves the dir before it can know whether one is already running on it, and
// a dir that does not exist cannot be resolved -- so a first launch would otherwise settle
// on a spelling that every later launch, resolving a dir that by then exists, disagrees
// with. D-Bus survives that, since both ends re-canonicalize before comparing; the COM
// moniker, which matches the config dir as an exact string, does not.
[[nodiscard]] std::string canonical_config_dir_created(std::string_view config_dir);

// The running-object-table item a Windows client registers for `config_dir`, and the one
// a launch looks up. Interop contract: the lookup matches this string exactly and nothing
// re-canonicalizes it, so a client that builds it differently is never found. Lives here,
// off Windows, because that makes it the one thing about the COM path that every platform
// can test. Canonicalizes `config_dir` itself rather than trusting the caller to have.
[[nodiscard]] std::string com_config_moniker_item(std::string_view config_dir);

// Returns true if `arg` is a URL or a magnet link.
// Returns false if `arg` is a base64-encoded .torrent.
[[nodiscard]] bool is_metainfo_link(std::string_view arg);

// Converts `arg` to a wire form.
// Filenames and file:// URIs load the .torrent file and convert to base64.
// torrent URLs and magnet links are passed through as-is.
// Returns empty when the argument is none of the above.
[[nodiscard]] std::optional<std::string> encode_metainfo_arg(std::string_view arg);

// Reads back the wire form encode_metainfo_arg() writes, so that what counts as a
// torrent is decided here rather than once per client.
// Returns the .torrent contents `metainfo` carries, or nothing when it carries none:
// it is a link, it is not base64, or what it decodes to is not a torrent.
// Ask is_metainfo_link() first -- a link is the other form, not a malformed torrent.
[[nodiscard]] std::optional<std::string> decode_metainfo_torrent(std::string_view metainfo);

} // namespace tr::interop
