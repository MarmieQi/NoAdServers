#include "oreui/BundlePatcher.h"

#include "oreui/BundleSymbolResolver.h"

#include <algorithm>
#include <vector>

namespace noadservers::oreui {
namespace {

struct TextEdit {
    std::size_t start{0};
    std::size_t length{0};
    std::string replacement;
};

std::string formatPolicyJs(config::ServerUiPolicy const& policy) {
    std::string out = "window.__NAS_POLICY__={hideFeaturedServers:";
    out.append(policy.hideFeaturedServers ? "true" : "false");
    out.append(",hideCreatorServers:");
    out.append(policy.hideCreatorServers ? "true" : "false");
    out.append(",hideConnectionWarning:");
    out.append(policy.hideConnectionWarning ? "true" : "false");
    out.append("};");
    return out;
}

std::string formatRuntimeExport(ResolvedBundleSymbols const& sym, std::string_view origAlias) {
    std::string out = "window.__NoAdServersRuntime={React:";
    out.append(sym.reactVar);
    out.append(",Facet:{useSharedFacet:");
    out.append(sym.useSharedFacet);
    out.append(",useFacetMap:");
    out.append(sym.useFacetMap);
    out.append(",useFacetUnwrap:");
    out.append(sym.useFacetUnwrap);
    out.append(",NO_VALUE:");
    out.append(sym.noValue);
    out.append("},Facets:{externalServers:");
    out.append(sym.externalServersFacet);
    out.append(",networkDetails:");
    out.append(sym.networkDetailsFacet);
    out.append(",serverType:");
    out.append(sym.serverTypeEnum);
    out.append("},useHistory:");
    out.append(sym.useHistory);
    out.append(",formatRouteType:");
    out.append(sym.formatRouteType);
    out.append(",OriginalServersTab:");
    out.append(origAlias);
    out.append("};");
    return out;
}

} // namespace

PatchReport patchBundleJs(
    std::string&                  bundleJs,
    config::ServerUiPolicy const& policy,
    std::string_view              frontendScriptJs
) {
    PatchReport report{};
    auto const  sym = resolveBundleSymbols(bundleJs);

    std::vector<TextEdit> edits;

    if (sym.serversV2Switch.has_value() && policy.hideFeaturedServers) {
        auto const& sw = *sym.serversV2Switch;
        edits.push_back({
            sw.range.start,
            sw.range.end - sw.range.start,
            sw.switchVar + "=()=>" + sw.reactVar + ".createElement(" + sw.classicComponent + ",null)",
        });
        report.bypassedServersV2 = true;
    }

    std::string const origAlias = "_nasOrigServersTab";
    if (sym.hasCompleteRuntime()) {
        auto const& tab = *sym.serversTab;
        edits.push_back({
            tab.signatureStart,
            tab.bodyStart - tab.signatureStart,
            "function " + tab.componentName + "(e){if(window.__NoAdServersPage)return " + sym.reactVar +
                ".createElement(window.__NoAdServersPage,e);return " + origAlias + "(e);}function " + origAlias +
                "(" + tab.paramList + ")",
        });
        report.wrappedServersTab = true;

        std::string injection = formatPolicyJs(policy);
        if (!sym.thirdPartyDoneValue.empty()) {
            injection.append("var _nasEmptyTP={featuredExperiences:[],creatorExperiences:[],fetchThirdPartyWorldsTaskState:");
            injection.append(sym.thirdPartyDoneValue);
            injection.append(
                "};var _nasEmptyTPFacet={get:function(){return _nasEmptyTP;},observe:function(e){e(_nasEmptyTP);return function(){};}};"
            );
        }
        injection.append(formatRuntimeExport(sym, origAlias));
        injection.append(frontendScriptJs);
        injection.push_back(';');
        edits.push_back({*sym.rootRenderInsertPos, 0, std::move(injection)});
        report.injectedRuntime = true;
    } else {
        auto appendMissing = [&](char const* name, bool ok) {
            if (!ok) {
                if (!report.missingSymbols.empty()) report.missingSymbols.push_back(',');
                report.missingSymbols.append(name);
            }
        };
        appendMissing("serversTab", sym.serversTab.has_value());
        appendMissing("rootRender", sym.rootRenderInsertPos.has_value());
        appendMissing("react", !sym.reactVar.empty());
        appendMissing("useSharedFacet", !sym.useSharedFacet.empty());
        appendMissing("useFacetMap", !sym.useFacetMap.empty());
        appendMissing("useFacetUnwrap", !sym.useFacetUnwrap.empty());
        appendMissing("noValue", !sym.noValue.empty());
        appendMissing("externalServers", !sym.externalServersFacet.empty());
        appendMissing("networkDetails", !sym.networkDetailsFacet.empty());
        appendMissing("serverType", !sym.serverTypeEnum.empty());
        appendMissing("useHistory", !sym.useHistory.empty());
        appendMissing("formatRouteType", !sym.formatRouteType.empty());
    }

    constexpr std::string_view kHideBoth =
        "window.__NAS_POLICY__&&window.__NAS_POLICY__.hideFeaturedServers&&window.__NAS_POLICY__.hideCreatorServers";
    bool const hideBoth = policy.hideFeaturedServers && policy.hideCreatorServers;

    if (sym.thirdPartyDoneExpr.has_value() && hideBoth) {
        auto const& expr = *sym.thirdPartyDoneExpr;
        auto const  orig = bundleJs.substr(expr.start, expr.end - expr.start);
        edits.push_back({expr.start, expr.end - expr.start, "(" + std::string{kHideBoth} + ")||(" + orig + ")"});
    }

    if (sym.serversTab.has_value() && !sym.reactVar.empty() && hideBoth) {
        auto const tabEl = sym.reactVar + ".createElement(" + sym.serversTab->componentName + ",null)";
        for (auto const& maybeRange : {sym.serversLoadingDefaultExpr, sym.noServiceContentReturnExpr}) {
            if (!maybeRange.has_value()) continue;
            auto const& expr = *maybeRange;
            auto const  orig = bundleJs.substr(expr.start, expr.end - expr.start);
            edits.push_back({expr.start, expr.end - expr.start, std::string{kHideBoth} + "?" + tabEl + ":" + orig});
        }
    }

    if (!sym.useSharedFacet.empty() && !sym.thirdPartyWorldListFacet.empty() && !sym.thirdPartyDoneValue.empty() &&
        sym.hasCompleteRuntime() && hideBoth) {
        std::string const call = sym.useSharedFacet + "(" + sym.thirdPartyWorldListFacet + ")";
        std::string const repl = "(" + std::string{kHideBoth} + "?_nasEmptyTPFacet:" + call + ")";
        std::size_t       pos  = 0;
        while ((pos = bundleJs.find(call, pos)) != std::string::npos) {
            edits.push_back({pos, call.size(), repl});
            pos += call.size();
        }
    }

    if (sym.loadThirdPartyDetailsCall.has_value() && hideBoth) {
        auto const& expr = *sym.loadThirdPartyDetailsCall;
        auto const  orig = bundleJs.substr(expr.start, expr.end - expr.start);
        edits.push_back({expr.start, expr.end - expr.start, "(" + std::string{kHideBoth} + "?void 0:" + orig + ")"});
    }

    if (sym.highPingReturnExpr.has_value() && policy.hideConnectionWarning) {
        auto const& expr = *sym.highPingReturnExpr;
        auto const  orig = bundleJs.substr(expr.start, expr.end - expr.start);
        edits.push_back({
            expr.start,
            expr.end - expr.start,
            "window.__NAS_POLICY__&&window.__NAS_POLICY__.hideConnectionWarning?null:" + orig,
        });
        report.patchedHighPingWarning = true;
    }

    // Apply edits from back to front so earlier byte offsets remain exact.
    std::sort(edits.begin(), edits.end(), [](TextEdit const& a, TextEdit const& b) {
        return a.start > b.start;
    });
    for (auto const& edit : edits) {
        bundleJs.replace(edit.start, edit.length, edit.replacement);
    }

    return report;
}

} // namespace noadservers::oreui
