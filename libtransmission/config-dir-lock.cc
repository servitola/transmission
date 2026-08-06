// This file Copyright © Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include <string_view>

#include "libtransmission/config-dir-lock.h"
#include "libtransmission/transmission.h"
#include "libtransmission/tr-strbuf.h"

using namespace std::literals;

namespace
{
// Every client has to agree on this name: two that spell it differently each hold a
// lock the other never looks at, and both start on one config dir.
auto constexpr LockFilename = "lock"sv;
} // namespace

tr_config_dir_lock::tr_config_dir_lock(std::string_view const config_dir)
    : lock_{ tr_pathbuf{ config_dir, '/', LockFilename } }
{
    (void)lock_.try_acquire();
}

bool tr_configDirIsContended(std::string_view const config_dir)
{
    // Taking the lock is how the question is asked, so this holds it for the length
    // of the call. Releasing it again is what makes the answer a snapshot.
    return tr_config_dir_lock{ config_dir }.status() == tr_config_dir_lock::Status::Contended;
}
