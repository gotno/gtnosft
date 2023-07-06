#include "OscRouter.hpp"
#include "OscController.hpp"

#include <cstring>

void OscRouter::ProcessMessage(const osc::ReceivedMessage& message, const IpEndpointName& remoteEndpoint) {
  (void) remoteEndpoint; // suppress unused parameter warning

  if (!controller) {
    WARN("`OscController* controller` not set, cannot process message.");
    return;
  }

  std::string path(message.AddressPattern());
  if (path.compare(std::string("/create/cable")) == 0) {
    osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();
    osc::uint64 inputModuleId, outputModuleId;
    osc::uint32 inputPortId, outputPortId;
    inputModuleId = (arg++)->AsInt64();
    outputModuleId = (arg++)->AsInt64();
    inputPortId = (arg++)->AsInt32();
    outputPortId = (arg++)->AsInt32();
    controller->addCable(inputModuleId, outputModuleId, inputPortId, outputPortId);
    return;
  } else if (path.compare(std::string("/destroy/cable")) == 0) {
    osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();
    osc::uint64 cableId;
    cableId = (arg++)->AsInt64();
    controller->destroyCable(cableId);
    return;
  } else if (path.compare(std::string("/sync")) == 0) {
    controller->needsSync = true;
    return;
  } else if (routes.find(path) == routes.end()) {
    DEBUG("no route for %s", path.c_str());
    return;
  }

  try {
    osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();

    // potentially skip getting args if none are present
    // todo: implement basic route vs "full" route routemaps
    osc::uint64 outerId = -1;
    if (arg != message.ArgumentsEnd()) {
      outerId = (arg++)->AsInt64();
    }
    osc::uint32 innerId = -1;
    if (arg != message.ArgumentsEnd()) {
      innerId = (arg++)->AsInt32();
    }
    float value = -1.f;
    if (arg != message.ArgumentsEnd()) {
      value = (arg++)->AsFloat();
    }

    /* if (arg != message.ArgumentsEnd()) throw osc::ExcessArgumentException(); */

    RouteAction action = routes[path];
    (controller->*action)(outerId, innerId, value);
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
