#pragma once

namespace noadservers::config {

struct ServerUiPolicy {
    bool hideFeaturedServers{true};
    bool hideCreatorServers{true};
    bool hideConnectionWarning{true};
};

} // namespace noadservers::config
