#pragma once

#include <string>

#include "CommonTypes.h"

namespace rgc {

class IApp
{
public:
    enum class LOG_TYPE
    {
        MSG,
        WARN,
        ERR,
        DEBUG
    };

    virtual ~IApp() {};
    virtual void deliverMessage(MessageId msgId, payload_t const &payload) const = 0;
    virtual void run() = 0;
    virtual void log(LOG_TYPE, std::string const &msg) = 0;
};

}