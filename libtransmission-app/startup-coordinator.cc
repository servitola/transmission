// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include <chrono>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <fmt/format.h>

#include <libtransmission/file.h>
#include <libtransmission/lock-file.h>
#include <libtransmission/log.h>
#include <libtransmission/tr-assert.h>
#include <libtransmission/tr-strbuf.h>

#include "libtransmission-app/interop-names.h"
#include "libtransmission-app/interop.h"
#include "libtransmission-app/startup-coordinator.h"

namespace tr::interop
{
namespace
{

enum class StartupDecision : uint8_t
{
    Start,
    Delegated,
    Refused,
    Busy
};

[[nodiscard]] bool matches(Instance& other, std::string_view const config_dir)
{
    auto const theirs = other.config_dir();

    // Cannot say: a release without the method.
    // startup-coordinator.h's contract says take it.
    if (std::empty(theirs))
    {
        return true;
    }

    // Filesystem identity first: when both dirs exist it sees through bind mounts and hard links,
    // not just the symlinks a resolved compare covers.
    if (tr_sys_path_is_same(theirs.c_str(), std::string{ config_dir }.c_str()))
    {
        return true;
    }

    return canonical_config_dir(theirs) == canonical_config_dir(config_dir);
}

// One attempt to hand this launch over. Start means nobody took it.
// StartupCoordinator states the policy this carries out.
[[nodiscard]] StartupDecision try_delegate(
    Transport& transport,
    std::string_view const config_dir,
    Intent const intent,
    MetainfoProvider const& metainfos)
{
    if (intent == Intent::Standalone)
    {
        return StartupDecision::Start;
    }

    auto const other = transport.find_other_instance();
    if (!other || !matches(*other, config_dir))
    {
        return StartupDecision::Start;
    }

    tr_logAddDebug(fmt::format("handing this launch to {:s}", other->description()));

    // An answer -- yes or no -- takes the launch; only silence leaves it here.
    // The contract in startup-coordinator.h says why.
    switch (intent)
    {
    case Intent::AddTorrents:
        {
            auto decision = StartupDecision::Start;

            // Read here and not before: the arguments are worth encoding only now that
            // an instance is known to be both there and serving this config dir.
            auto const encoded = metainfos ? metainfos() : std::vector<std::string>{};

            for (auto const& metainfo : encoded)
            {
                auto const answer = other->add_metainfo(metainfo);
                if (!answer)
                {
                    // Gone mid-handoff.
                    // Offering the remaining files would only wait on a process that is no longer there.
                    break;
                }

                // One refusal decides the launch: it went nowhere for a file the user named,
                // and saying so matters more than the ones that landed.
                if (!*answer)
                {
                    decision = StartupDecision::Refused;
                }
                else if (decision == StartupDecision::Start)
                {
                    decision = StartupDecision::Delegated;
                }
            }

            return decision;
        }

    case Intent::Present:
        {
            auto const answer = other->present_window();
            if (!answer)
            {
                return StartupDecision::Start;
            }

            return *answer ? StartupDecision::Delegated : StartupDecision::Refused;
        }

    case Intent::Standalone:
        break;
    }

    return StartupDecision::Start;
}

[[nodiscard]] std::optional<int> report_startup_decision(
    StartupDecision const decision,
    Intent const intent,
    std::string_view const config_dir)
{
    switch (decision)
    {
    case StartupDecision::Delegated:
        if (intent == Intent::Present)
        {
            fmt::print(stderr, "Already running on '{:s}'; presenting it.\n", config_dir);
        }

        return 0;

    case StartupDecision::Refused:
        fmt::print(stderr, "The client already running on '{:s}' could not do that.\n", config_dir);
        return 1;

    case StartupDecision::Busy:
        return report_config_dir_busy(config_dir);

    case StartupDecision::Start:
        break;
    }

    return {};
}

} // namespace

class StartupCoordinator::Impl
{
public:
    Impl(std::string config_dir, std::unique_ptr<Transport> transport)
        : config_dir_{ std::move(config_dir) }
        , transport_{ std::move(transport) }
        , lock_{ tr_pathbuf{ config_dir_, '/', StartupLockFilename } }
    {
        TR_ASSERT(transport_ != nullptr);

        // A dir must exist before a file in it can be locked,
        // so a launch on a brand-new config dir claims it too.
        tr_sys_dir_create(config_dir_, TR_SYS_DIR_CREATE_PARENTS, 0755);
        (void)lock_.try_acquire();
    }

    Impl(Impl&&) = delete;
    Impl(Impl const&) = delete;
    Impl& operator=(Impl&&) = delete;
    Impl& operator=(Impl const&) = delete;
    ~Impl() = default;

    [[nodiscard]] StartupDecision arbitrate(
        Intent const intent,
        MetainfoProvider const& metainfos,
        std::chrono::milliseconds const patience)
    {
        using namespace std::chrono_literals;

        auto const delegate = [&]
        {
            return try_delegate(*transport_, config_dir_, intent, metainfos);
        };

        if (auto const decision = delegate(); decision != StartupDecision::Start)
        {
            return decision;
        }

        if (lock_.status() != tr_lock_file::Status::Contended)
        {
            return StartupDecision::Start;
        }

        // Another launch is between delegation and publication;
        // wait for it to publish -- release follows publication -- or to give up and exit.
        auto const deadline = std::chrono::steady_clock::now() + patience;
        while (!lock_.try_acquire())
        {
            if (std::chrono::steady_clock::now() >= deadline)
            {
                return StartupDecision::Busy;
            }

            std::this_thread::sleep_for(50ms);
        }

        // Whoever released the guard published first; give that peer one final
        // chance before starting.
        return delegate();
    }

    [[nodiscard]] std::optional<int> delegate(
        Intent const intent,
        MetainfoProvider const& metainfos,
        std::chrono::milliseconds const patience)
    {
        return report_startup_decision(arbitrate(intent, metainfos, patience), intent, config_dir_);
    }

    void publish(Instance& self)
    {
        transport_->publish(self);
        lock_.release();
    }

    void publish_nothing()
    {
        lock_.release();
    }

private:
    std::string const config_dir_;
    std::unique_ptr<Transport> const transport_;
    tr_lock_file lock_;
};

StartupCoordinator::StartupCoordinator(std::string config_dir, std::unique_ptr<Transport> transport)
    : impl_{ std::make_unique<Impl>(std::move(config_dir), std::move(transport)) }
{
}

StartupCoordinator::~StartupCoordinator() = default;

std::optional<int> StartupCoordinator::delegate(
    Intent const intent,
    MetainfoProvider const& metainfos,
    std::chrono::milliseconds const patience)
{
    return impl_->delegate(intent, metainfos, patience);
}

int report_config_dir_busy(std::string_view const config_dir)
{
    fmt::print(stderr, "Another process is already using '{:s}'.\n", config_dir);
    return 1;
}

void StartupCoordinator::publish(Instance& self)
{
    impl_->publish(self);
}

void StartupCoordinator::publish_nothing()
{
    impl_->publish_nothing();
}

} // namespace tr::interop
