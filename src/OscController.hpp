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

#define OSC_BUFFER_SIZE (1024 * 16)

using Time = std::chrono::steady_clock;
using float_sec = std::chrono::duration<float>;
using float_time_point = std::chrono::time_point<Time, float_sec>;

enum CommandType {
  SyncCable,
  SyncModule,
  CheckModuleSync,
  UpdateLights,
  /* UpdateDisplays, */
  Noop
};
struct Payload {
  int64_t pid;
  int cid, gcid;

  float_time_point lastCheck;
  float wait;

  int retries = 0;
  int limit = 10;

  Payload() {}
  Payload(int64_t _pid) : pid(_pid) {} 
  Payload(int64_t _pid, float_time_point _lastCheck, float _wait) : pid(_pid), lastCheck(_lastCheck), wait(_wait) {} 
};
typedef std::pair<CommandType, Payload> Command;

typedef std::unordered_map<int, VCVLight*> LightReferenceMap;

struct OscController {
  OscController();
  ~OscController();

  IpEndpointName endpoint;

  char* oscBuffer = new char[OSC_BUFFER_SIZE];
  void sendMessage(osc::OutboundPacketStream packetStream);

  std::thread queueWorker;
  std::atomic<bool> queueWorkerRunning;
  std::queue<Command> commandQueue;
  std::mutex qmutex;
  std::condition_variable queueLockCondition;
  float_time_point getCurrentTime();
  void enqueueCommand(Command command);
  void processQueue();


  bool isModuleSynced(int64_t moduleId);

  std::unordered_map<int64_t, VCVModule> Modules;
  std::unordered_map<int64_t, VCVCable> Cables;

  std::mutex lmutex;
  std::unordered_map<int64_t, LightReferenceMap> LightReferences;

  void init();

  rack::math::Vec ueCorrectPos(rack::math::Vec parentSize, rack::math::Vec pos, rack::math::Vec size);
  rack::math::Rect box2cm(rack::math::Rect pxBox);
  rack::math::Vec vec2cm(rack::math::Vec pxVec);
  int randomId();
  bool isRectangleLight(rack::app::MultiLightWidget* light);

  void collectModule(int64_t moduleId);
  void collectModules();
	void printModules();

  void collectCable(int64_t cableId);
  void collectCables();
  void printCables();

  void bundleLight(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVLight* light, int paramId = -1);
  void bundleParam(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVParam* param);
  void bundleInput(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVPort* input);
  void bundleOutput(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVPort* output);
  void bundlePort(osc::OutboundPacketStream& bundle, VCVPort* port);
  void bundleDisplay(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVDisplay* display);
  void bundleModule(osc::OutboundPacketStream& bundle, VCVModule* module);

  void enqueueSyncModule(int64_t moduleId);
  void syncModule(VCVModule* module);

  void enqueueSyncCable(int64_t cableId);
  void syncCable(VCVCable* cable);

  void sendInitialSyncComplete();
  void sendModuleSyncComplete(int64_t moduleId);

  void registerLightReference(int64_t moduleId, VCVLight* light);

  void sendLightUpdates();
  void enqueueLightUpdates();
  void bundleLightUpdate(osc::OutboundPacketStream& bundle, int64_t moduleId, int lightId, NVGcolor color);

  // UE callbacks
	void UERx(const char* path, int64_t outerId, int innerId);
};
