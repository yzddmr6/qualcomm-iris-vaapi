// SPDX-License-Identifier: MIT
#include "../src/va/driver.h"
#include "../src/va/client_policy.h"
#include "../src/util/error.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <sys/prctl.h>

// config.cc's error translation, without linking a hardware capability probe.
namespace va {
VAStatus report(VADriverContextP, const char*, const std::exception& error)
{
    const auto* e = dynamic_cast<const iris::Error*>(&error);
    return e ? e->status : VA_STATUS_ERROR_OPERATION_FAILED;
}
}

int main(int argc, char** argv)
{
    if (argc < 2) return 2;
    const bool blocked = std::strcmp(argv[1], "blocked") == 0;
    // A child can change its visible title/argv[0]; the executable still wins.
    if (argc > 2) {
        const char* title = blocked ? "firefox" : "chrome";
        if (std::strlen(argv[0]) < std::strlen(title)) return 3;
        std::strcpy(argv[0], title);
        if (prctl(PR_SET_NAME, title, 0, 0, 0) != 0) return 4;
    }
    va::DriverData d;
    d.capabilities.nv12 = d.capabilities.p010 = true;
    d.capabilities.profiles = {VAProfileH264High, VAProfileHEVCMain,
                              VAProfileHEVCMain10, VAProfileVP9Profile0};
    va::apply_client_profile_policy(d.capabilities.profiles);
    VADriverContext ctx{};
    ctx.pDriverData = &d;
    VAProfile profiles[va::kMaxProfiles];
    int count = 0;
    if (va::queryConfigProfiles(&ctx, profiles, &count) != VA_STATUS_SUCCESS) return 5;
    for (const auto profile : {VAProfileH264High, VAProfileVP9Profile0,
                               VAProfileHEVCMain, VAProfileHEVCMain10}) {
        const bool hevc = profile == VAProfileHEVCMain || profile == VAProfileHEVCMain10;
        const bool allowed = !(blocked && hevc);
        const bool advertised = std::find(profiles, profiles + count, profile) != profiles + count;
        VAConfigID id = VA_INVALID_ID;
        const auto status = va::createConfig(&ctx, profile, VAEntrypointVLD, nullptr, 0, &id);
        VAEntrypoint entry{};
        int entries = -1;
        va::queryConfigEntrypoints(&ctx, profile, &entry, &entries);
        VAConfigAttrib attribute{VAConfigAttribRTFormat, 0};
        va::getConfigAttributes(&ctx, profile, VAEntrypointVLD, &attribute, 1);
        if (advertised != allowed || entries != int(allowed) ||
            status != (allowed ? VA_STATUS_SUCCESS : VA_STATUS_ERROR_UNSUPPORTED_PROFILE) ||
            (attribute.value != VA_ATTRIB_NOT_SUPPORTED) != allowed) return 6;
        if (allowed && va::destroyConfig(&ctx, id) != VA_STATUS_SUCCESS) return 7;
    }
    std::printf("HEVC=%s H264=yes VP9=yes executable-policy OK\n", blocked ? "no" : "yes");
    return 0;
}
