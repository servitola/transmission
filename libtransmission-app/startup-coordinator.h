// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "libtransmission-app/interop.h" // Instance, Intent, Transport

namespace tr::interop
{

// Produces the launch's torrent arguments in wire form (encode_metainfo_arg).
// We call this at most once, and only when an instance will take them,
// so the common launch that finds nobody never reads a torrent file.
using MetainfoProvider = std::function<std::vector<std::string>()>;

// Owns startup delegation and publication: the transport, and the startup lock we hold
// between them. Do not confuse that lock with the config dir lock a session takes.
// The config dir lock is the one that matters once startup is over.
class StartupCoordinator
{
public:
    StartupCoordinator(std::string config_dir, std::unique_ptr<Transport> transport);
    StartupCoordinator(StartupCoordinator&&) = delete;
    StartupCoordinator(StartupCoordinator const&) = delete;
    StartupCoordinator& operator=(StartupCoordinator&&) = delete;
    StartupCoordinator& operator=(StartupCoordinator const&) = delete;
    ~StartupCoordinator();

    // Returns an exit code when another instance took the launch or startup
    // timed out. Empty means this process should continue starting.
    [[nodiscard]] std::optional<int> delegate(
        Intent intent,
        MetainfoProvider const& metainfos,
        std::chrono::milliseconds patience = std::chrono::minutes{ 2 });

    // Makes `self` reachable before we release the startup lock.
    // `self` must outlive this coordinator.
    // Destroying the coordinator stops the transport answering.
    void publish(Instance& self);

    // For a launch that starts but serves no config dir, such as a client attached to a
    // session on another host. We publish nothing, because nothing here can be delegated to.
    // We still have to release the startup lock: the next launch on this dir would otherwise
    // wait out its patience for a publication that never comes.
    void publish_nothing();

private:
    class Impl;
    std::unique_ptr<Impl> const impl_;
};

// Reports that the config dir is busy and returns the exit code to leave with.
// We call this when a launch has already started and then finds the dir held by a
// process it cannot delegate to.
[[nodiscard]] int report_config_dir_busy(std::string_view config_dir);

} // namespace tr::interop
