#include "OscRouter.hpp"
#include "OscController.hpp"

#include <cstring>

void OscRouter::ProcessMessage(const osc::ReceivedMessage& message, const IpEndpointName& remoteEndpoint) {
  (void) remoteEndpoint; // suppress unused parameter warning

  if (!controller) {
    WARN("`OscController* controller` not set, cannot process message.");
    return;
  }

  /* DEBUG("OscRouter::ProcessMessage: %s", message.AddressPattern()); */
  try {
    for (std::pair<std::string, RouteAction> route : routes) {
      const char* path = route.first.c_str();
      /* DEBUG("checking match for %s", path); */

      if (std::strcmp(path, message.AddressPattern()) < 0) {
        /* DEBUG("%s route matches %s", message.AddressPattern(), path); */

        osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();

        osc::uint64 outerId = (arg++)->AsInt64();
        osc::uint32 innerId = (arg++)->AsInt32();

        if (arg != message.ArgumentsEnd()) throw osc::ExcessArgumentException();

        RouteAction action = route.second;
        (controller->*action)(message.AddressPattern(), outerId, innerId);
      }
    }
  } catch(osc::Exception& e) {
    DEBUG("Error parsing OSC message %s: %s", message.AddressPattern(), e.what());
  }
}

void OscRouter::SetController(OscController* controller) {
  this->controller = controller;
}

void OscRouter::AddRoute(std::string address, RouteAction action) {
  routes.insert(std::unordered_map<std::string, RouteAction>::value_type(address, action));
}
