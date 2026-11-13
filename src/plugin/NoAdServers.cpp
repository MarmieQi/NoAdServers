#include "plugin/NoAdServers.h"

#include "app/AppController.h"

#include "ll/api/mod/RegisterHelper.h"

namespace noadservers {

NoAdServers& NoAdServers::getInstance() {
    static NoAdServers instance;
    return instance;
}

NoAdServers::NoAdServers() : mSelf(*ll::mod::NativeMod::current()) {}

ll::mod::NativeMod& NoAdServers::getSelf() const { return mSelf; }

bool NoAdServers::load() { return app::AppController::getInstance().load(); }

bool NoAdServers::enable() { return app::AppController::getInstance().enable(); }

bool NoAdServers::disable() { return app::AppController::getInstance().disable(); }

} // namespace noadservers

LL_REGISTER_MOD(noadservers::NoAdServers, noadservers::NoAdServers::getInstance());
