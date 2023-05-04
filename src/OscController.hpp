#pragma once
#include "rack.hpp"
#include "../dep/oscpack/ip/IpEndpointName.h"
#include "../dep/oscpack/osc/OscOutboundPacketStream.h"

#include "VCVStructure.hpp"

#include <unordered_map>
#include <vector>
#include <thread>

typedef std::unordered_map<int, VCVLight*> LightReferenceMap;

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
  std::unordered_map<int64_t, LightReferenceMap> LightReferences;

  void init();

  rack::math::Vec ueCorrectPos(rack::math::Vec parentSize, rack::math::Vec pos, rack::math::Vec size);
  rack::math::Rect box2cm(rack::math::Rect pxBox);
  rack::math::Vec vec2cm(rack::math::Vec pxVec);
  int randomId();
  bool isRectangleLight(rack::app::MultiLightWidget* light);

  void collectRackModules();
	void printRackModules();

  void collectCables();
  void printCables();

  void syncModuleLight(int64_t moduleId, VCVLight* light, int paramId = -1);
  void syncModuleLights(int64_t moduleId);
  void syncParamLights(int64_t moduleId, VCVParam* param);

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

  void registerLightReference(int64_t moduleId, VCVLight* light);

  void sendMessage(osc::OutboundPacketStream packetStream);
  void sendLightUpdates();
  void sendLightUpdate(int64_t moduleId, int lightId, NVGcolor color);

  // UE callbacks
	void UERx(const char* path, int64_t outerId, int innerId);
};
