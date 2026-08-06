// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <libtransmission-app/interop.h>
#include <libtransmission-app/interop-names.h>
#include <libtransmission-app/startup-coordinator.h>

#include <libtransmission/lock-file.h>

#include "test-fixtures.h"

namespace tr::test
{

using StartupCoordinatorTest = libtransmission::test::SandboxedTest;

namespace
{

using interop::StartupLockFilename;

class NullInstance final : public interop::Instance
{
public:
    [[nodiscard]] std::optional<bool> present_window() override
    {
        return true;
    }

    [[nodiscard]] std::optional<bool> add_metainfo(std::string_view const /*metainfo*/) override
    {
        return true;
    }

    [[nodiscard]] std::string config_dir() override
    {
        return {};
    }

    [[nodiscard]] std::string description() const override
    {
        return "null instance";
    }
};

class GuardProbingTransport final : public interop::Transport
{
public:
    GuardProbingTransport(std::string config_dir, bool& guard_was_held_during_publish, bool& destroyed)
        : config_dir_{ std::move(config_dir) }
        , guard_was_held_during_publish_{ guard_was_held_during_publish }
        , destroyed_{ destroyed }
    {
    }

    ~GuardProbingTransport() override
    {
        destroyed_ = true;
    }

    void publish(interop::Instance& /*self*/) override
    {
        auto const filename = config_dir_ + '/' + std::string{ StartupLockFilename };
        auto prober = tr_lock_file{ filename };
        (void)prober.try_acquire();
        guard_was_held_during_publish_ = prober.status() == tr_lock_file::Status::Contended;
    }

    [[nodiscard]] std::unique_ptr<interop::Instance> find_other_instance() override
    {
        return {};
    }

private:
    std::string const config_dir_;
    bool& guard_was_held_during_publish_;
    bool& destroyed_;
};

} // namespace

TEST_F(StartupCoordinatorTest, publishesBeforeReleasing)
{
    auto guard_was_held_during_publish = false;
    auto destroyed = false;
    auto instance = NullInstance{};
    auto transport = std::make_unique<GuardProbingTransport>(sandboxDir(), guard_was_held_during_publish, destroyed);
    auto coordinator = interop::StartupCoordinator{ sandboxDir(), std::move(transport) };
    coordinator.publish(instance);

    EXPECT_TRUE(guard_was_held_during_publish);

    auto const filename = sandboxDir() + '/' + std::string{ StartupLockFilename };
    auto prober = tr_lock_file{ filename };
    EXPECT_TRUE(prober.try_acquire());
}

// A launch that starts without publishing still has to end the interval it holds the
// guard for. Leaving it held would make the next launch on this dir wait out its full
// patience and then report a busy dir that nobody is reachable on.
TEST_F(StartupCoordinatorTest, publishNothingReleasesTheGuard)
{
    auto published = false;
    auto destroyed = false;
    auto transport = std::make_unique<GuardProbingTransport>(sandboxDir(), published, destroyed);
    auto coordinator = interop::StartupCoordinator{ sandboxDir(), std::move(transport) };
    coordinator.publish_nothing();

    EXPECT_FALSE(published);

    auto const filename = sandboxDir() + '/' + std::string{ StartupLockFilename };
    auto prober = tr_lock_file{ filename };
    EXPECT_TRUE(prober.try_acquire());
}

TEST_F(StartupCoordinatorTest, destroysTransportWhenDestroyed)
{
    auto guard_was_held_during_publish = false;
    auto destroyed = false;
    auto instance = NullInstance{};

    {
        auto transport = std::make_unique<GuardProbingTransport>(sandboxDir(), guard_was_held_during_publish, destroyed);
        auto coordinator = interop::StartupCoordinator{ sandboxDir(), std::move(transport) };
        coordinator.publish(instance);
        EXPECT_FALSE(destroyed);
    }

    EXPECT_TRUE(destroyed);
}

} // namespace tr::test
