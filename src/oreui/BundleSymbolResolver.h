#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace noadservers::oreui {

struct TextRange {
    std::size_t start{0};
    std::size_t end{0};
};

struct ServersV2SwitchMatch {
    TextRange   range{};
    std::string switchVar;
    std::string reactVar;
    std::string classicComponent;
};

struct ServersTabMatch {
    std::size_t signatureStart{0};
    std::size_t bodyStart{0}; // points to '{' of function body
    std::string componentName;
    std::string paramList;    // e.g. "{routePrefix:e=\"/play\",showFeaturedServers:n=!0,showCreatorServers:a=!0}"
    std::string sidebarComponent;
};

struct ResolvedBundleSymbols {
    std::optional<ServersV2SwitchMatch> serversV2Switch;
    std::optional<ServersTabMatch>      serversTab;
    std::optional<TextRange>            thirdPartyDoneExpr;
    std::optional<TextRange>            serversLoadingDefaultExpr;
    std::optional<TextRange>            noServiceContentReturnExpr;
    std::optional<TextRange>            loadThirdPartyDetailsCall;
    std::optional<TextRange>            highPingReturnExpr;
    std::optional<std::size_t>          rootRenderInsertPos;

    std::string reactVar;
    std::string useSharedFacet;
    std::string useFacetMap;
    std::string useFacetUnwrap;
    std::string noValue;
    std::string externalServersFacet;
    std::string networkDetailsFacet;
    std::string thirdPartyWorldListFacet;
    std::string thirdPartyDoneValue;
    std::string serverTypeEnum;
    std::string useHistory;
    std::string formatRouteType;

    [[nodiscard]] bool hasCompleteRuntime() const {
        return serversTab.has_value() && rootRenderInsertPos.has_value() && !reactVar.empty() &&
               !useSharedFacet.empty() && !useFacetMap.empty() && !useFacetUnwrap.empty() && !noValue.empty() &&
               !externalServersFacet.empty() && !networkDetailsFacet.empty() && !serverTypeEnum.empty() &&
               !useHistory.empty() && !formatRouteType.empty();
    }
};

ResolvedBundleSymbols resolveBundleSymbols(std::string_view bundleJs);

} // namespace noadservers::oreui
