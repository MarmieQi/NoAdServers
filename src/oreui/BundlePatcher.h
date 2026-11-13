#pragma once

#include "config/ServerUiPolicy.h"

#include <string>
#include <string_view>

namespace noadservers::oreui {

struct PatchReport {
    bool        bypassedServersV2{false};
    bool        wrappedServersTab{false};
    bool        patchedHighPingWarning{false};
    bool        injectedRuntime{false};
    std::string missingSymbols;
};

PatchReport patchBundleJs(
    std::string&                  bundleJs,
    config::ServerUiPolicy const& policy,
    std::string_view              frontendScriptJs
);

} // namespace noadservers::oreui
