#pragma once

namespace noadservers::app {

class AppController {
public:
    static AppController& getInstance();

    bool load();
    bool enable();
    bool disable();
};

} // namespace noadservers::app
