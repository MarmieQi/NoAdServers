#pragma once

#include "ll/api/mod/NativeMod.h"

namespace noadservers {

class NoAdServers {
public:
    static NoAdServers& getInstance();

    NoAdServers();
    ll::mod::NativeMod& getSelf() const;

    bool load();
    bool enable();
    bool disable();

private:
    ll::mod::NativeMod& mSelf;
};

} // namespace noadservers
