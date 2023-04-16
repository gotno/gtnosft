#pragma once
#include "rack.hpp"
#include "../dep/oscpack/ip/IpEndpointName.h"
#include "../dep/oscpack/osc/OscOutboundPacketStream.h"

#include "VCVStructure.hpp"

#include <unordered_map>
#include <thread>

struct OscController {
  OscController();
  ~OscController();

  IpEndpointName endpoint;
  std::thread syncworker;
  bool isSynced();
  void ensureSynced();

  int syncCheckCount = 0;

  std::unordered_map<int64_t, VCVModule> RackModules;
  std::unordered_map<int64_t, VCVCable> Cables;

  void init();

  rack::math::Rect box2cm(rack::math::Rect pxBox);

  void collectRackModules();
	void printRackModules();

  void collectCables();

  void syncModuleParam(int64_t moduleId, VCVParam* param);
  void syncModuleParams(int64_t moduleId);

  void syncModuleInput(int64_t moduleId, VCVPort* input);
  void syncModuleInputs(int64_t moduleId);

  void syncModuleOutput(int64_t moduleId, VCVPort* output);
  void syncModuleOutputs(int64_t moduleId);

  void syncModuleDisplay(int64_t moduleId, VCVDisplay* display);
  void syncModuleDisplays(int64_t moduleId);

  void syncRackModule(VCVModule* module);
  void syncRackModuleComponents(int64_t moduleId);
  void syncRackModules();
  void syncCable(VCVCable* cable);
  void syncCables();
  void syncAll();

  void sendInitialSyncComplete();

	/* osc::OutboundPacketStream getPacketStream(); */
  void sendMessage(osc::OutboundPacketStream packetStream);

  // UE callbacks
	void UERx(const char* path, int64_t outerId, int innerId);
};
