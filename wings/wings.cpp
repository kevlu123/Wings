#include "impl.h"
#include <iostream>

namespace wings {

    static thread_local Error werror;

    extern "C" {

        Error WErrorGet() {
            return werror;
        }

        void WContextGetConfig(const WContext* context, Config* config) {
            WASSERT(context && config);
            *config = context->config;
        }

        void WContextSetConfig(WContext* context, const Config* config) {
            WASSERT(context);
            if (config) {
                WASSERT(config->maxAlloc >= 0);
                context->config = *config;
            } else {
                context->config.maxAlloc = 100'000;
                context->config.maxRecursion = 100;
                context->config.log = [](const char* message) { std::cout << message << '\n'; };
            }
        }

        WContext* WContextCreate(const Config* config) {
            WContext* context = new WContext();
            WContextSetConfig(context, config);
            return context;
        }

        void WContextDestroy(WContext* context) {
            if (context) {
                delete context;
            }
        }

        void WContextLog(const WContext* context, const char* message) {
            WASSERT(context && message);
            if (context->config.log)
                context->config.log(message);
        }

    } // extern "C"

} // namespace wings
