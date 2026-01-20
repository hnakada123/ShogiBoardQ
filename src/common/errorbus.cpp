#include "errorbus.h"

ErrorBus& ErrorBus::instance() {
    static ErrorBus inst;
    return inst;
}
