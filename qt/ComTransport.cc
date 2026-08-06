// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <windows.h>
#include <objbase.h>
#include <oleauto.h>

#include <QAxFactory>
#include <QAxObject>
#include <QDebug>
#include <QString>
#include <QUuid>
#include <QVariant>

#include <fmt/format.h>

#include <libtransmission-app/interop-names.h>

#include "InteropObject.h"
#include "Transports.h"
#include "Utils.h"

// The typelib and app ids are ActiveQt registration internals, not wire contract:
// no launcher looks them up.
// The class id in interop-names.h is the one other builds must agree on.
// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
QAXFACTORY_BEGIN("{1e405fc2-1a3a-468b-8bd6-bfbb58770390}", "{792d1aac-53cc-4dc9-bc29-e5295fdb93a9}")
QAXCLASS(InteropObject)
QAXFACTORY_END() // NOLINT

// These are ActiveQt internals; declaring here as I don't like their WinMain much...
extern bool qAxOutProcServer; // NOLINT
extern wchar_t qAxModuleFilename[MAX_PATH]; // NOLINT
extern QString qAxInit(); // NOLINT
extern QAxFactory* qAxFactory(); // NOLINT

namespace
{

class ComTransport final : public tr::interop::Transport
{
public:
    // COM and the ActiveQt out-of-process machinery are initialized here,
    // so build the transport before QApplication.
    explicit ComTransport(QString const& config_dir);
    ~ComTransport() override;

    void publish(tr::interop::Instance& self) override;

    [[nodiscard]] std::unique_ptr<tr::interop::Instance> find_other_instance() override;

private:
    class Impl;
    std::unique_ptr<Impl> const impl_;
};

[[nodiscard]] HRESULT initializeActiveQt()
{
    static auto const Result = []
    {
        qAxOutProcServer = true;
        ::GetModuleFileNameW(nullptr, qAxModuleFilename, MAX_PATH);

        auto const result = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (SUCCEEDED(result))
        {
            qAxInit();
        }

        return result;
    }();

    return Result;
}

// Takes ownership of the caller's reference: QAxObject adds its own.
[[nodiscard]] std::unique_ptr<QAxObject> wrap(IUnknown* const unknown)
{
    auto client = std::make_unique<QAxObject>(unknown);
    unknown->Release();
    return client;
}

// How one machine routes to the right client: the running object table is keyed by moniker,
// so an item moniker naming the config dir gives every dir its own entry.
// GetActiveObject() -- the fallback below -- is keyed by class id alone,
// which is why it can only ever find "some" client.
//
// The prefix is part of the wire contract,
// so a separately-built client that shares it is findable here and finds this one.
// That is the point of sharing it, and the cost is below, at publish.
[[nodiscard]] IMoniker* createConfigMoniker(QString const& config_dir)
{
    auto* moniker = static_cast<IMoniker*>(nullptr);
    auto const item = Utils::toQString(tr::interop::com_config_moniker_item(config_dir.toStdString()));
    return SUCCEEDED(::CreateItemMoniker(L"!", reinterpret_cast<LPCOLESTR>(item.utf16()), &moniker)) ? moniker : nullptr;
}

// Returns the object registered for this exact config dir.
// Unlike GetActiveObject(CLSID),
// the item moniker remains unambiguous when several clients are running.
[[nodiscard]] std::unique_ptr<QAxObject> connectToConfigClient(QString const& config_dir)
{
    auto* table = static_cast<IRunningObjectTable*>(nullptr);
    if (FAILED(::GetRunningObjectTable(0, &table)) || table == nullptr)
    {
        return {};
    }

    auto* moniker = createConfigMoniker(config_dir);
    if (moniker == nullptr)
    {
        table->Release();
        return {};
    }

    auto* unknown = static_cast<IUnknown*>(nullptr);
    auto const result = table->GetObject(moniker, &unknown);
    moniker->Release();
    table->Release();

    if (FAILED(result) || unknown == nullptr)
    {
        return {};
    }

    return wrap(unknown);
}

// Compatibility fallback for a client that predates config-specific monikers.
// GetActiveObject reads the ROT and never starts a registered LocalServer32.
[[nodiscard]] std::unique_ptr<QAxObject> connectToLegacyClient()
{
    auto* unknown = static_cast<IUnknown*>(nullptr);

    if (::GetActiveObject(QUuid{ QStringLiteral(TR_INTEROP_COM_CLASS_ID) }, nullptr, &unknown) != S_OK || unknown == nullptr)
    {
        return {};
    }

    return wrap(unknown);
}

class RemoteInstance final : public tr::interop::Instance
{
public:
    explicit RemoteInstance(std::unique_ptr<QAxObject> client)
        : client_{ std::move(client) }
    {
    }

    [[nodiscard]] std::optional<bool> present_window() override
    {
        auto const reply = call(tr::interop::MethodPresentWindow);
        return reply.isValid() ? std::optional{ reply.toBool() } : std::nullopt;
    }

    [[nodiscard]] std::optional<bool> add_metainfo(std::string_view const metainfo) override
    {
        auto const reply = call(tr::interop::MethodAddMetainfo, Utils::toQString(metainfo));
        return reply.isValid() ? std::optional{ reply.toBool() } : std::nullopt;
    }

    [[nodiscard]] std::string config_dir() override
    {
        auto const reply = call(tr::interop::MethodConfigDir);
        return reply.isValid() ? reply.toString().toStdString() : std::string{};
    }

    [[nodiscard]] std::string description() const override
    {
        return "COM running object " TR_INTEROP_COM_CLASS_ID;
    }

private:
    // An invalid reply covers both a release without the method and an object whose process has gone;
    // COM cannot tell either from "reached, and it declined".
    // Treat silence as silence and let tr::interop::StartupCoordinator draw the line.
    [[nodiscard]] QVariant call(std::string_view const method, QVariant const& arg = {})
    {
        if (!arg.isValid())
        {
            return client_->dynamicCall(fmt::format("{:s}()", method).c_str());
        }

        return client_->dynamicCall(fmt::format("{:s}(QString)", method).c_str(), arg);
    }

    std::unique_ptr<QAxObject> client_;
};

} // namespace

class ComTransport::Impl
{
public:
    explicit Impl(QString config_dir)
        : config_dir_{ std::move(config_dir) }
        , co_initialize_result_{ initializeActiveQt() }
    {
    }

    ~Impl()
    {
        // Cleared first: a factory-built InteropObject answers for whatever this named,
        // and COM can hand one out until the revocations below have all landed.
        InteropObject::publish_instance(nullptr);

        if (config_cookie_ != 0U && table_ != nullptr)
        {
            table_->Revoke(config_cookie_);
        }

        if (legacy_cookie_ != 0U)
        {
            ::RevokeActiveObject(legacy_cookie_, nullptr);
        }

        if (table_ != nullptr)
        {
            table_->Release();
        }

        if (wrapper_ != nullptr)
        {
            wrapper_->Release();
        }
    }

    void publish(tr::interop::Instance& self)
    {
        if (FAILED(co_initialize_result_))
        {
            return;
        }

        InteropObject::publish_instance(&self);

        QAxFactory::startServer();
        object_ = std::make_unique<InteropObject>(self);
        auto* const factory = qAxFactory();
        if (factory == nullptr || !factory->createObjectWrapper(object_.get(), &wrapper_) || wrapper_ == nullptr)
        {
            qWarning() << "couldn't create the COM interop wrapper";
            return;
        }

        // Two products publishing for one config dir diverge by platform by nature, not oversight:
        // the D-Bus record is a file, so its last writer holds it and a caller reaches that one.
        // Here both registrations stand, and COM does not say which a later lookup reaches --
        // not the first, not the last, undefined.
        // Do not build on whichever one a given Windows happens to return.
        //
        // The config dir lock is what keeps a second session off the dir in the first place,
        // so this is the degenerate case, not the shared one.
        if (auto* moniker = createConfigMoniker(config_dir_); moniker != nullptr)
        {
            if (SUCCEEDED(::GetRunningObjectTable(0, &table_)) && table_ != nullptr &&
                FAILED(table_->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, wrapper_, moniker, &config_cookie_)))
            {
                config_cookie_ = 0U;
                qWarning() << "couldn't register the config-specific COM interop object";
            }

            moniker->Release();
        }

        // Preserve the fixed-CLSID entry for older launchers,
        // but do not add another ambiguous entry when any client already owns it.
        if (connectToLegacyClient() == nullptr &&
            FAILED(
                ::RegisterActiveObject(
                    wrapper_,
                    QUuid{ QStringLiteral(TR_INTEROP_COM_CLASS_ID) },
                    ACTIVEOBJECT_STRONG,
                    &legacy_cookie_)))
        {
            legacy_cookie_ = 0U;
            qWarning() << "couldn't register the legacy COM interop object";
        }
    }

    [[nodiscard]] std::unique_ptr<tr::interop::Instance> find_other_instance() const
    {
        if (FAILED(co_initialize_result_))
        {
            return {};
        }

        // The moniker is what picks out this config dir;
        // the class id knows nothing about dirs,
        // so what it finds is checked by tr::interop::StartupCoordinator like any other candidate.
        auto client = connectToConfigClient(config_dir_);
        if (client == nullptr)
        {
            client = connectToLegacyClient();
        }

        return client != nullptr ? std::make_unique<RemoteInstance>(std::move(client)) : nullptr;
    }

private:
    QString const config_dir_;
    std::unique_ptr<InteropObject> object_;
    IDispatch* wrapper_ = nullptr;
    IRunningObjectTable* table_ = nullptr;
    DWORD config_cookie_ = 0U;
    DWORD legacy_cookie_ = 0U;
    HRESULT const co_initialize_result_;
};

ComTransport::ComTransport(QString const& config_dir)
    : impl_{ std::make_unique<Impl>(config_dir) }
{
}

ComTransport::~ComTransport() = default;

void ComTransport::publish(tr::interop::Instance& self)
{
    impl_->publish(self);
}

std::unique_ptr<tr::interop::Instance> ComTransport::find_other_instance()
{
    // The running object table cannot hold this process's entry
    // while this process is still deciding whether to start:
    // registration happens in publish(), after delegation has been ruled out.
    return impl_->find_other_instance();
}

namespace tr::interop::detail
{

std::unique_ptr<Transport> make_com_transport(QString const& config_dir)
{
    return std::make_unique<ComTransport>(config_dir);
}

} // namespace tr::interop::detail
