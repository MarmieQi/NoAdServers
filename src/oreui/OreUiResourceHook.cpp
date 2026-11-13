#include "oreui/OreUiResourceHook.h"

#include "EmbeddedFrontendJs.h"
#include "oreui/BundlePatcher.h"

#include "ll/api/memory/Hook.h"
#include "mc/client/gui/oreui/resources/ResourceRequest.h"
#include "mc/client/gui/oreui/resources/ResourceResponse.h"
#include "mc/client/gui/oreui/resources/handlers/HybridResourceHandler.h"
#include "mc/external/gameface/cohtml/IAsyncResourceResponse.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace noadservers::oreui {
namespace {

constexpr std::string_view kBundleUrlPrefix = "ui://./hbui/index-";

bool                   gHookInstalled{false};
config::ServerUiPolicy gPolicy{};

Gameface::ResourceHandlerStatus writeResponse(
    Gameface::ResourceRequest const& request,
    Gameface::ResourceResponse&      response,
    std::string_view                 contents,
    char const*                      contentType
) {
    auto* target = response.mResponse;
    if (!target || request.mRange.get().has_value()) {
        return Gameface::ResourceHandlerStatus::Failure;
    }
    auto* destination = target->GetSpace(contents.size());
    if (!destination) {
        return Gameface::ResourceHandlerStatus::Failure;
    }
    std::memcpy(destination, contents.data(), contents.size());
    target->SetStatus(200);
    target->SetHeader("Content-Type", contentType);
    target->SetHeader("Cache-Control", "no-store");
    return Gameface::ResourceHandlerStatus::Success;
}

bool readDiskFile(std::filesystem::path const& path, std::string& outContents) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) return false;
    std::ostringstream ss;
    ss << ifs.rdbuf();
    outContents = ss.str();
    return !outContents.empty();
}

LL_TYPE_INSTANCE_HOOK(
    ServerUiResourceRequestHook,
    ll::memory::HookPriority::Normal,
    OreUI::HybridResourceHandler,
    &OreUI::HybridResourceHandler::$onResourceRequest,
    Gameface::ResourceHandlerStatus,
    Gameface::ResourceRequest const& request,
    Gameface::ResourceResponse&      response
) {
    auto const& url = request.mAbsoluteUrl.get();
    if (url.starts_with(kBundleUrlPrefix) && url.ends_with(".js")) {
        std::string_view const fileName = std::string_view{url}.substr(std::string_view{"ui://./"}.size());
        std::string            bundle;
        if (!readDiskFile(std::filesystem::path{"data/gui/dist"} / fileName, bundle)) {
            return origin(request, response);
        }

        patchBundleJs(bundle, gPolicy, kEmbeddedFrontendJs);
        return writeResponse(request, response, bundle, "application/javascript; charset=utf-8");
    }

    return origin(request, response);
}

} // namespace

bool installResourceHook(config::ServerUiPolicy const& policy) {
    gPolicy = policy;
    if (gHookInstalled) return true;
    gHookInstalled = ServerUiResourceRequestHook::hook() == 0;
    return gHookInstalled;
}

void uninstallResourceHook() {
    if (!gHookInstalled) return;
    ServerUiResourceRequestHook::unhook();
    gHookInstalled = false;
}

} // namespace noadservers::oreui
