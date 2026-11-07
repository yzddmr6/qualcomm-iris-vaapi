// SPDX-License-Identifier: MIT
#pragma once

#include <set>
#include <string_view>
#include <sys/auxv.h>
#include <va/va.h>

namespace va {

// Official Linux Chrome executes a binary named "chrome", including its
// GPU/zygote children. AT_EXECFN survives argv/process-title changes and needs
// no /proc access or new sandbox permission. This is a compatibility rule,
// not an executable authentication or security boundary.
inline void apply_client_profile_policy(std::set<VAProfile>& profiles)
{
    const auto* executable = reinterpret_cast<const char*>(getauxval(AT_EXECFN));
    std::string_view name = executable ? executable : "";
    const auto slash = name.find_last_of('/');
    if (slash != std::string_view::npos)
        name.remove_prefix(slash + 1);
    if (name == "chrome") {
        // Chromium 563075803: keep bitstream validation and browser defaults
        // intact. Firefox and other clients retain HEVC capability.
        profiles.erase(VAProfileHEVCMain);
        profiles.erase(VAProfileHEVCMain10);
    }
}

} // namespace va
