#include "oreui/BundleSymbolResolver.h"

#include <algorithm>
#include <cctype>

namespace noadservers::oreui {
namespace {

bool isIdentChar(char ch) {
    auto const u = static_cast<unsigned char>(ch);
    return std::isalnum(u) != 0 || ch == '_' || ch == '$';
}

std::string readIdentBackward(std::string_view s, std::size_t endExclusive) {
    if (endExclusive == 0 || endExclusive > s.size()) return {};
    std::size_t pos = endExclusive;
    while (pos > 0 && isIdentChar(s[pos - 1])) {
        --pos;
    }
    return std::string{s.substr(pos, endExclusive - pos)};
}

std::string readIdentForward(std::string_view s, std::size_t start) {
    if (start >= s.size()) return {};
    std::size_t pos = start;
    while (pos < s.size() && isIdentChar(s[pos])) {
        ++pos;
    }
    return std::string{s.substr(start, pos - start)};
}

std::size_t findMatchingBrace(std::string_view s, std::size_t openBracePos) {
    if (openBracePos >= s.size() || s[openBracePos] != '{') return std::string_view::npos;
    int depth = 0;
    for (std::size_t i = openBracePos; i < s.size(); ++i) {
        char const ch = s[i];
        if (ch == '"' || ch == '\'' || ch == '`') {
            char const quote = ch;
            ++i;
            while (i < s.size() && s[i] != quote) {
                if (s[i] == '\\') ++i;
                ++i;
            }
            continue;
        }
        if (ch == '{') {
            ++depth;
        } else if (ch == '}') {
            --depth;
            if (depth == 0) return i;
        }
    }
    return std::string_view::npos;
}

std::string resolveFacetConstByKey(std::string_view s, std::string_view keyLiteral) {
    std::size_t pos = 0;
    while ((pos = s.find(keyLiteral, pos)) != std::string_view::npos) {
        // Expect `=fn("key")` right before keyLiteral: look back up to 16 chars for `=`
        std::size_t const searchStart = pos > 24 ? pos - 24 : 0;
        auto const        eqPos       = s.rfind('=', pos);
        if (eqPos != std::string_view::npos && eqPos >= searchStart) {
            auto ident = readIdentBackward(s, eqPos);
            if (!ident.empty()) return ident;
        }
        pos += keyLiteral.size();
    }
    return {};
}

std::optional<ServersV2SwitchMatch> resolveServersV2Switch(std::string_view s) {
    constexpr std::string_view kFlag = R"("mc-servers-tab-v2")";
    auto const                 flagPos = s.find(kFlag);
    if (flagPos == std::string_view::npos) return std::nullopt;

    auto const arrowPos = s.rfind("=()=>", flagPos);
    if (arrowPos == std::string_view::npos || flagPos - arrowPos > 80) return std::nullopt;

    auto const switchVar = readIdentBackward(s, arrowPos);
    if (switchVar.empty()) return std::nullopt;

    auto const openBrace = s.find('{', arrowPos);
    if (openBrace == std::string_view::npos) return std::nullopt;

    auto const closeBrace = findMatchingBrace(s, openBrace);
    if (closeBrace == std::string_view::npos) return std::nullopt;

    auto const body = s.substr(openBrace, closeBrace - openBrace + 1);
    auto const cePos = body.rfind(".createElement(");
    if (cePos == std::string_view::npos) return std::nullopt;

    auto const reactVar    = readIdentBackward(body, cePos);
    auto const classicComp = readIdentForward(body, cePos + std::string_view{".createElement("}.size());
    if (reactVar.empty() || classicComp.empty()) return std::nullopt;

    ServersV2SwitchMatch match;
    match.range            = {arrowPos - switchVar.size(), closeBrace + 1};
    match.switchVar        = switchVar;
    match.reactVar         = reactVar;
    match.classicComponent = classicComp;
    return match;
}

std::optional<ServersTabMatch> resolveServersTab(std::string_view s) {
    constexpr std::string_view kPropAnchor = R"(="/play",showFeaturedServers:)";
    std::size_t                searchPos   = 0;
    while (true) {
        auto const anchorPos = s.find(kPropAnchor, searchPos);
        if (anchorPos == std::string_view::npos) return std::nullopt;
        searchPos = anchorPos + kPropAnchor.size();

        auto const fnPos = s.rfind("function ", anchorPos);
        if (fnPos == std::string_view::npos || anchorPos - fnPos > 40) continue;

        auto const parenPos = s.find('(', fnPos + 9);
        if (parenPos == std::string_view::npos || parenPos > anchorPos) continue;
        if (s.substr(parenPos + 1, 13) != "{routePrefix:") continue;

        auto const compName = std::string{s.substr(fnPos + 9, parenPos - (fnPos + 9))};
        auto const bracePos = s.find("){", anchorPos);
        if (bracePos == std::string_view::npos || bracePos - anchorPos > 80) continue;

        auto const paramList = std::string{s.substr(parenPos + 1, bracePos - (parenPos + 1))};
        auto const closePos  = findMatchingBrace(s, bracePos + 1);
        if (closePos == std::string_view::npos) continue;

        auto const  body = s.substr(bracePos + 1, closePos - bracePos);
        std::string sidebarComp;
        auto const  loadingProp = body.find("loading:");
        if (loadingProp != std::string_view::npos) {
            auto const callPos = body.rfind(".createElement(", loadingProp);
            if (callPos != std::string_view::npos) {
                sidebarComp = readIdentForward(body, callPos + std::string_view{".createElement("}.size());
            }
        } else if (auto const cePos = body.find(".createElement("); cePos != std::string_view::npos) {
            sidebarComp = readIdentForward(body, cePos + std::string_view{".createElement("}.size());
        }

        ServersTabMatch match;
        match.signatureStart   = fnPos;
        match.bodyStart        = bracePos + 1;
        match.componentName    = compName;
        match.paramList        = paramList;
        match.sidebarComponent = sidebarComp;
        return match;
    }
}

void resolveFacetRuntime(std::string_view s, ResolvedBundleSymbols& out) {
    constexpr std::string_view kNoValueKey = R"(=Symbol.for("NoValue"))";
    auto const                 nvPos       = s.find(kNoValueKey);
    if (nvPos == std::string_view::npos) return;

    out.noValue = readIdentBackward(s, nvPos);

    auto const searchEnd = std::min(s.size(), nvPos + 4500);
    auto const window    = s.substr(nvPos, searchEnd - nvPos);
    // Locate useFacetUnwrap: `function <name>(e,n=<eq>){...useMemo...e.observe`
    std::size_t pos = 0;
    while ((pos = window.find("function ", pos)) != std::string_view::npos) {
        auto const openParen = window.find('(', pos + 9);
        if (openParen == std::string_view::npos) break;
        auto const fnName   = std::string{window.substr(pos + 9, openParen - (pos + 9))};
        auto const openBr   = window.find('{', openParen);
        if (openBr == std::string_view::npos) break;
        auto const closeBr = findMatchingBrace(window, openBr);
        if (closeBr == std::string_view::npos) break;
        auto const fnBody = window.substr(openBr, closeBr - openBr + 1);
        if (fnBody.find("useMemo") != std::string_view::npos &&
            fnBody.find("e.observe") != std::string_view::npos &&
            fnBody.find("e.get()") != std::string_view::npos) {
            out.useFacetUnwrap = fnName;
            break;
        }
        pos = openParen + 1;
    }
}

std::optional<TextRange> resolveThirdPartyDoneExpr(std::string_view s, ServersTabMatch const& tab) {
    auto const closePos = findMatchingBrace(s, tab.bodyStart);
    if (closePos == std::string_view::npos) return std::nullopt;
    auto const body = s.substr(tab.bodyStart, closePos - tab.bodyStart + 1);
    auto const pos  = body.find("e.fetchThirdPartyWorldsTaskState===");
    if (pos == std::string_view::npos) return std::nullopt;
    auto const comma = body.find(',', pos);
    if (comma == std::string_view::npos) return std::nullopt;
    return TextRange{tab.bodyStart + pos, tab.bodyStart + comma};
}

struct FunctionBody {
    std::size_t      openBrace{0};
    std::string_view text;
};

std::optional<FunctionBody> findZeroArgFunctionBody(std::string_view s, std::string_view fnName) {
    if (fnName.empty()) return std::nullopt;
    auto const decl  = "function " + std::string{fnName} + "()";
    auto const fnPos = s.find(decl);
    if (fnPos == std::string_view::npos) return std::nullopt;
    auto const openBr  = s.find('{', fnPos);
    auto const closeBr = findMatchingBrace(s, openBr);
    if (openBr == std::string_view::npos || closeBr == std::string_view::npos) return std::nullopt;
    return FunctionBody{openBr, s.substr(openBr, closeBr - openBr + 1)};
}

std::optional<TextRange> resolveServersLoadingDefaultExpr(std::string_view s, std::string_view classicComp) {
    auto const fn = findZeroArgFunctionBody(s, classicComp);
    if (!fn) return std::nullopt;
    auto const defPos = fn->text.find("default:return ");
    if (defPos == std::string_view::npos) return std::nullopt;
    auto const exprStart = defPos + std::string_view{"default:return "}.size();
    auto const switchEnd = fn->text.find("}})", exprStart);
    if (switchEnd == std::string_view::npos) return std::nullopt;
    return TextRange{fn->openBrace + exprStart, fn->openBrace + switchEnd};
}

std::optional<TextRange> resolveNoServiceContentReturnExpr(std::string_view s, std::string_view classicComp) {
    auto const fn = findZeroArgFunctionBody(s, classicComp);
    if (!fn) return std::nullopt;
    constexpr std::string_view kCase = R"(case"no-service-content":return )";
    auto const                 pos   = fn->text.find(kCase);
    if (pos == std::string_view::npos) return std::nullopt;
    auto const exprStart = pos + kCase.size();
    auto const defPos    = fn->text.find(";default:", exprStart);
    if (defPos == std::string_view::npos) return std::nullopt;
    return TextRange{fn->openBrace + exprStart, fn->openBrace + defPos};
}

std::optional<TextRange> resolveHighPingExpr(std::string_view s) {
    constexpr std::string_view kCaseSig = R"(case"high-ping":return )";
    auto const                 pos      = s.find(kCaseSig);
    if (pos == std::string_view::npos) return std::nullopt;

    std::size_t const exprStart = pos + kCaseSig.size();
    int               parenDepth = 0;
    for (std::size_t i = exprStart; i < s.size(); ++i) {
        char const ch = s[i];
        if (ch == '"' || ch == '\'' || ch == '`') {
            char const quote = ch;
            ++i;
            while (i < s.size() && s[i] != quote) {
                if (s[i] == '\\') ++i;
                ++i;
            }
            continue;
        }
        if (ch == '(') {
            ++parenDepth;
        } else if (ch == ')') {
            --parenDepth;
        } else if (ch == ';' && parenDepth == 0) {
            return TextRange{exprStart, i};
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> resolveRootRenderInsertPos(std::string_view s) {
    constexpr std::string_view kRootAnchor = R"(document.getElementById("root"))";
    auto const                 rootPos     = s.rfind(kRootAnchor);
    if (rootPos == std::string_view::npos) return std::nullopt;

    auto const fnPos = s.rfind("(function(", rootPos);
    if (fnPos == std::string_view::npos || rootPos - fnPos > 200) return std::nullopt;
    return fnPos;
}

} // namespace

ResolvedBundleSymbols resolveBundleSymbols(std::string_view s) {
    ResolvedBundleSymbols out;
    out.serversV2Switch    = resolveServersV2Switch(s);
    out.serversTab         = resolveServersTab(s);
    if (out.serversTab.has_value()) {
        out.thirdPartyDoneExpr = resolveThirdPartyDoneExpr(s, *out.serversTab);
    }
    if (out.serversV2Switch.has_value()) {
        out.serversLoadingDefaultExpr  = resolveServersLoadingDefaultExpr(s, out.serversV2Switch->classicComponent);
        out.noServiceContentReturnExpr = resolveNoServiceContentReturnExpr(s, out.serversV2Switch->classicComponent);
    }
    out.highPingReturnExpr = resolveHighPingExpr(s);
    out.rootRenderInsertPos = resolveRootRenderInsertPos(s);

    out.externalServersFacet     = resolveFacetConstByKey(s, R"("vanilla.externalServerWorldList")");
    out.networkDetailsFacet      = resolveFacetConstByKey(s, R"("vanilla.networkWorldDetails")");
    out.thirdPartyWorldListFacet = resolveFacetConstByKey(s, R"("vanilla.thirdPartyWorldList")");
    resolveFacetRuntime(s, out);

    if (out.thirdPartyDoneExpr.has_value()) {
        auto const exprText = s.substr(out.thirdPartyDoneExpr->start, out.thirdPartyDoneExpr->end - out.thirdPartyDoneExpr->start);
        if (auto const eqPos = exprText.find("==="); eqPos != std::string_view::npos) {
            out.thirdPartyDoneValue = std::string{exprText.substr(eqPos + 3)};
        }
    }

    if (out.serversTab.has_value()) {
        auto const& tab      = *out.serversTab;
        auto const  closePos = findMatchingBrace(s, tab.bodyStart);
        if (closePos != std::string_view::npos) {
            auto const body = s.substr(tab.bodyStart, closePos - tab.bodyStart + 1);

            if (!out.externalServersFacet.empty()) {
                auto const needle = "(" + out.externalServersFacet + ")";
                if (auto const p = body.find(needle); p != std::string_view::npos) {
                    out.useSharedFacet = readIdentBackward(body, p);
                }
            }

            if (auto const p = body.find("e.externalServerWorlds"); p != std::string_view::npos) {
                if (auto const openParen = body.rfind('(', p); openParen != std::string_view::npos) {
                    out.useFacetMap = readIdentBackward(body, openParen);
                }
            }

            if (auto const cePos = body.find(".createElement("); cePos != std::string_view::npos) {
                out.reactVar = readIdentBackward(body, cePos);
            }

            if (auto const extPos = body.find(".External)"); extPos != std::string_view::npos) {
                out.serverTypeEnum = readIdentBackward(body, extPos);
                auto const openParen = body.rfind('(', extPos);
                if (openParen != std::string_view::npos) {
                    out.formatRouteType = readIdentBackward(body, openParen);
                }
            }

            if (!out.serverTypeEnum.empty()) {
                auto const tpNeedle = "," + out.serverTypeEnum + ".ThirdParty):";
                if (auto const tpPos = body.find(tpNeedle); tpPos != std::string_view::npos) {
                    if (auto const qPos = body.rfind('?', tpPos); qPos != std::string_view::npos && tpPos - qPos < 24) {
                        out.loadThirdPartyDetailsCall = TextRange{
                            tab.bodyStart + qPos + 1,
                            tab.bodyStart + tpPos + tpNeedle.size() - 1,
                        };
                    }
                }
            }
        }

        if (!tab.sidebarComponent.empty()) {
            auto const sideDecl = "function " + tab.sidebarComponent + "(";
            if (auto const sidePos = s.find(sideDecl); sidePos != std::string_view::npos) {
                auto const paramClose = s.find("){", sidePos);
                auto const openBr     = paramClose != std::string_view::npos ? paramClose + 1 : std::string_view::npos;
                auto const closeBr    = findMatchingBrace(s, openBr);
                if (openBr != std::string_view::npos && closeBr != std::string_view::npos) {
                    auto const sideBody = s.substr(openBr, closeBr - openBr + 1);
                    if (auto const addPos = sideBody.find("/servers/add"); addPos != std::string_view::npos) {
                        if (auto const pushPos = sideBody.rfind(".push(", addPos);
                            pushPos != std::string_view::npos) {
                            auto const histVar = readIdentBackward(sideBody, pushPos);
                            if (!histVar.empty()) {
                                auto const declNeedle = histVar + "=";
                                if (auto const declPos = sideBody.find(declNeedle);
                                    declPos != std::string_view::npos) {
                                    auto const callEnd = sideBody.find("()", declPos + declNeedle.size());
                                    if (callEnd != std::string_view::npos) {
                                        out.useHistory = std::string{
                                            sideBody.substr(declPos + declNeedle.size(),
                                                            callEnd - (declPos + declNeedle.size()))};
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return out;
}

} // namespace noadservers::oreui
