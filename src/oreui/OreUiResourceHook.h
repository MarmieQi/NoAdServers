#pragma once

#include "config/ServerUiPolicy.h"

namespace noadservers::oreui {

bool installResourceHook(config::ServerUiPolicy const& policy);
void uninstallResourceHook();

} // namespace noadservers::oreui
