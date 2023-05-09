#pragma once
#include "rack.hpp"
#include "../dep/oscpack/ip/IpEndpointName.h"
#include "../dep/oscpack/osc/OscOutboundPacketStream.h"

#include "VCVStructure.hpp"

#include <unordered_map>
#include <vector>
#include <thread>
#include <queue>
#include <chrono>
#include <condition_variable>

using Time = std::chrono::steady_clock;
using float_sec = std::chrono::duration<float>;
using float_time_point = std::chrono::time_point<Time, float_sec>;
using ms = std::chrono::milliseconds;

#define OSC_BUFFER_SIZE (1024 * 16)

//  Q:
enum CommandType {
  /* SyncCable, */
  /* SyncModule, */
  /* CheckModuleSync, */
  /* FinalizeModule, */
  /* UpdateLights, */
  /* UpdateDisplays, */

  TestCheck,
  Noop
};
struct Payload {
  int64_t pid;
  int cid, gcid;

  float_time_point lastCheck;
  float wait;

  int retries = 0;
  int limit = 10;
};
typedef std::pair<CommandType, Payload> Command;

typedef std::unordered_map<int, VCVLight*> LightReferenceMap;

struct OscController {
  OscController();
  ~OscController();

  IpEndpointName endpoint;

  char* oscBuffer = new char[OSC_BUFFER_SIZE];

  /* std::thread syncworker; */
  //  Q:
  std::thread queueWorker;
  std::atomic<bool> queueWorkerRunning;
  std::queue<Command> commandQueue;
  std::mutex qmutex;
  std::condition_variable queueLockCondition;
  float_time_point getCurrentTime();

  bool isSynced();
  void ensureSynced();
  void processQueue();

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

  void bundleLight(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVLight* light, int paramId = -1);
  void bundleParam(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVParam* param);
  void bundleInput(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVPort* input);
  void bundleOutput(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVPort* output);
  void bundlePort(osc::OutboundPacketStream& bundle, VCVPort* port);
  void bundleDisplay(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVDisplay* display);
  void bundleModule(osc::OutboundPacketStream& bundle, VCVModule* module);

  void syncModule(VCVModule* module);
  void syncModules();

  void syncCable(VCVCable* cable);
  void syncCables();

  void syncAll();

  void sendInitialSyncComplete();

  void registerLightReference(int64_t moduleId, VCVLight* light);

  void sendMessage(osc::OutboundPacketStream packetStream);
  void sendLightUpdates();
  void bundleLightUpdate(osc::OutboundPacketStream& bundle, int64_t moduleId, int lightId, NVGcolor color);

  // UE callbacks
	void UERx(const char* path, int64_t outerId, int innerId);
};
