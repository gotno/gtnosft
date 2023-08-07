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
#include <set>

#define OSC_BUFFER_SIZE (1024 * 64)

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

  int retried = 0;
  int retryLimit = 10;

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

  std::mutex pumutex;
  std::set<std::pair<int64_t, int>> pendingParamUpdates;
  void processParamUpdates();

  std::mutex cablemutex;
  std::vector<VCVCable> cablesToAdd;
  std::vector<int64_t> cablesToDestroy;
	void processCableUpdates();

  std::mutex syncmutex;
  bool needsSync = false;
  void collectAndSync();

  std::unordered_map<int64_t, VCVModule> Modules;
  void collectModules(bool printResults = false);
  void collectModule(int64_t moduleId);
	void printModules();

  std::unordered_map<int64_t, VCVCable> Cables;
  void collectCables(bool printResults = false);
  void collectCable(int64_t cableId);
  void printCables();

  bool isModuleSynced(int64_t moduleId);

  std::mutex lmutex;
  std::unordered_map<int64_t, LightReferenceMap> LightReferences;

  rack::math::Vec ueCorrectPos(rack::math::Vec parentSize, rack::math::Vec pos, rack::math::Vec size);
  float px2cm(float px);
  rack::math::Rect box2cm(rack::math::Rect pxBox);
  rack::math::Vec vec2cm(rack::math::Vec pxVec);
  int randomId();
  bool isRectangleLight(rack::app::MultiLightWidget* light);
  std::string getLightSvgPath(rack::app::MultiLightWidget* light);

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
	void rxModule(int64_t outerId, int innerId, float value);
	void rxParam(int64_t outerId, int innerId, float value);
	void rxInput(int64_t outerId, int innerId, float value);
	void rxOutput(int64_t outerId, int innerId, float value);
	void rxModuleLight(int64_t outerId, int innerId, float value);
	void rxDisplay(int64_t outerId, int innerId, float value);
	void rxCable(int64_t outerId, int innerId, float value);

	void updateParam(int64_t outerId, int innerId, float value);

	void addCable(int64_t inputModuleId, int64_t outputModuleId, int inputPortId, int outputPortId);
	void destroyCable(int64_t cableId);
};
