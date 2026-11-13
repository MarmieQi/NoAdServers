#include "app/AppController.h"

#include "config/ServerUiPolicy.h"
#include "oreui/OreUiResourceHook.h"

namespace noadservers::app {

AppController& AppController::getInstance() {
    static AppController instance;
    return instance;
}

bool AppController::load() { return true; }

bool AppController::enable() {
    config::ServerUiPolicy const policy{};
    return oreui::installResourceHook(policy);
}

bool AppController::disable() {
    oreui::uninstallResourceHook();
    return true;
}

} // namespace noadservers::app
