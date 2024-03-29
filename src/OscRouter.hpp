#pragma once

#include "../dep/oscpack/osc/OscPacketListener.h"
#include "../dep/oscpack/ip/IpEndpointName.h"
#include "../dep/oscpack/osc/OscReceivedElements.h"

#include <string>
#include <unordered_map>

struct OscController;
  
typedef void(OscController::*CableAction)(int64_t, int64_t, int, int);
typedef std::unordered_map<std::string, CableAction> CableActionMap;

typedef void(OscController::*RouteAction)(int64_t, int, float);
typedef std::unordered_map<std::string, RouteAction> RouteMap;

class OscRouter : public osc::OscPacketListener {
public:
  OscController* controller = NULL;
  void SetController(OscController* controller);

  RouteMap routes;
  void AddRoute(std::string address, RouteAction action);

  virtual void ProcessMessage(const osc::ReceivedMessage& message, const IpEndpointName& remoteEndpoint) override;
};
