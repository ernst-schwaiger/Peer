#pragma once

#include "CommonTypes.h"

namespace rgc {

class IApp
{
public:
    virtual ~IApp() {};
    virtual void deliverMessage(MessageId msgId, payload_t const &payload) const = 0;
    virtual void run() = 0;
};

}