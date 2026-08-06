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
// Called at most once, and only once there is an instance that will take them,
// so the common launch that finds nobody never reads a torrent file.
using MetainfoProvider = std::function<std::vector<std::string>()>;

// Owns startup delegation and publication, including the transport and the lock
// that serializes the interval between them. The session's separate lifetime
// lock remains authoritative after startup.
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

    // Makes `self` reachable before releasing the startup lock. `self` must
    // outlive this coordinator; destruction stops the transport answering.
    void publish(Instance& self);

    // For a launch that goes on to start but serves no config dir -- a client attached
    // to a session on another host. Nothing here can be delegated to, so nothing is
    // published; the startup lock still has to be released, or the next launch on this
    // dir waits out its patience for a publication that is never coming.
    void publish_nothing();

private:
    class Impl;
    std::unique_ptr<Impl> const impl_;
};

// The Busy report alone,
// for a launch that started and then found the config dir held by a process delegation cannot reach.
[[nodiscard]] int report_config_dir_busy(std::string_view config_dir);

} // namespace tr::interop
