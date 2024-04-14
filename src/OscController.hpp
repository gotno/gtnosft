#pragma once
#include <rack.hpp>
#include "../dep/oscpack/ip/IpEndpointName.h"
#include "../dep/oscpack/osc/OscOutboundPacketStream.h"

#include "VCVStructure.hpp"
#include "OSCctrl/collector.hpp"
#include "OSCctrl/bootstrapper.hpp"

#include <unordered_map>
#include <vector>
#include <thread>
#include <queue>
#include <chrono>
#include <condition_variable>
#include <set>

namespace rack {
  namespace plugin {
    struct Plugin;
  }
}

#define OSC_BUFFER_SIZE (1024 * 128)

using Time = std::chrono::steady_clock;
using float_sec = std::chrono::duration<float>;
using float_time_point = std::chrono::time_point<Time, float_sec>;

enum CommandType {
  SyncCable,
  SyncModule,
  SyncLibrary,
  UpdateLights,
  SyncParam,
  SyncMenu,
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
  Payload(int64_t _pid, int _cid) : pid(_pid), cid(_cid) {}
  Payload(int64_t _pid, float_time_point _lastCheck, float _wait) : pid(_pid), lastCheck(_lastCheck), wait(_wait) {}
};
typedef std::pair<CommandType, Payload> Command;

typedef std::unordered_map<int, VCVLight*> LightReferenceMap;

struct OscController {
  OscController();
  ~OscController();

  IpEndpointName unrealServerEndpoint;

  char* oscBuffer = new char[OSC_BUFFER_SIZE];
  void sendMessage(osc::OutboundPacketStream packetStream);

  int64_t ctrlModuleId{-1};
  void setModuleId(const int64_t& moduleId) { ctrlModuleId = moduleId; }
  int ctrlListenPort{-1};
  void setListenPort(const int& listenPort) { ctrlListenPort = listenPort; }
  void setUnrealServerPort(const int& port);

  // filter OSCctrl from ModuleIds
  std::vector<int64_t> getModuleIds() {
    std::vector<int64_t> mids = APP->engine->getModuleIds();
    for (std::vector<int64_t>::iterator it = mids.begin(); it != mids.end();) {
      if (*it == ctrlModuleId) it = mids.erase(it);
      else ++it;
    }
    return mids;
  }

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
  void enqueueSyncParam(int64_t moduleId, int paramId);
  void syncParam(int64_t moduleId, int paramId);

  std::mutex cablemutex;
  std::vector<VCVCable> cablesToCreate;
  std::vector<int64_t> cablesToDestroy;
  void processCableUpdates();

  std::mutex modulemutex;
  std::vector<VCVModule> modulesToCreate;
  std::vector<int64_t> modulesToDestroy;
  void processModuleUpdates();
  void cleanupModule(const int64_t& moduleId);

  std::mutex menuitemmutex;
  std::set<std::tuple<int64_t, int, int>> pendingMenuClicks;
  void processMenuClicks();

  std::mutex menuquantitymutex;
  std::set<std::tuple<int64_t, int, int, float>> pendingMenuQuantityUpdates;
  void processMenuQuantityUpdates();

  std::mutex modulediffmutex;
  std::set<int64_t> pendingModuleDiffs;
  void processModuleDiffs();
  void diffModuleAndCablePresence();

  std::mutex syncmutex;
  bool needsSync = false;
  void reset();
  void collectAndSync();

  Collector Collectr;
  Bootstrapper Bootstrappr;

  std::unordered_map<int64_t, VCVModule> Modules;
  void collectModules(bool printResults = false);
  void printModules();

  std::unordered_map<int64_t, VCVCable> Cables;
  void collectCables(bool printResults = false);
  void printCables();

  std::mutex lmutex;
  std::unordered_map<int64_t, LightReferenceMap> LightReferences;

  void bundleLight(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVLight* light, int paramId = -1);
  void bundleParam(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVParam* param);
  void bundleInput(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVPort* input);
  void bundleOutput(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVPort* output);
  void bundlePort(osc::OutboundPacketStream& bundle, VCVPort* port);
  void bundleDisplay(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVDisplay* display);
  void bundleModule(osc::OutboundPacketStream& bundle, VCVModule* module);

  void enqueueSyncModule(int64_t moduleId);
  void syncModule(VCVModule* module);
  rack::plugin::Model* findModel(std::string& pluginSlug, std::string& moduleSlug) const;
  void createModule(VCVModule& vcv_module);

  void enqueueSyncCable(int64_t cableId);
  void syncCable(VCVCable* cable);

  void registerLightReference(int64_t moduleId, VCVLight* light);

  void sendLightUpdates();
  void enqueueLightUpdates();
  void bundleLightUpdate(osc::OutboundPacketStream& bundle, int64_t moduleId, int lightId, NVGcolor color);

  void enqueueSyncLibrary();
  void syncLibrary();
  // dump library info to a json file and return the path
  std::string dumpLibraryJsonToFile();

  // context menus
  std::unordered_map<int64_t, ModuleMenuMap> ContextMenus;

  std::mutex menumutex;
  std::vector<VCVMenu> menusToSync;
  void processMenuRequests();
  void enqueueSyncMenu(int64_t moduleId, int menuId);
  void syncMenu(int64_t moduleId, int menuId);
  void printMenu(VCVMenu& menu);

  // UE callbacks
  void rxModule(int64_t outerId, int innerId, float value);
  void rxCable(int64_t outerId, int innerId, float value);

  void updateParam(int64_t outerId, int innerId, float value);

  void addCableToCreate(int64_t inputModuleId, int64_t outputModuleId, int inputPortId, int outputPortId);
  void addCableToDestroy(int64_t cableId);

  void addModuleToCreate(const std::string& pluginSlug, const std::string& moduleSlug, const int& returnId);
  void addModuleToDestroy(int64_t moduleId);
  void addModuleToDiff(int64_t moduleId);

  void setModuleFavorite(std::string pluginSlug, std::string moduleSlug, bool favorite);

  void addMenuToSync(VCVMenu menu);
  void clickMenuItem(int64_t moduleId, int menuId, int menuItemIndex);
  void updateMenuItemQuantity(int64_t moduleId, int menuId, int menuItemIndex, float value);

  bool readyToExit{false};
  void requestExit() { readyToExit = true; }
  void autosaveAndExit();

  void requestSave() { needsSave = true; }
  bool needsSave{false};
  void savePatch();

  std::string patchToLoadNext{""};
  void setPatchToLoadNext(std::string patchPath) {
    patchToLoadNext = patchPath;
  }
  void cleanupCurrentPatchAndPrepareNext();

  std::vector<std::tuple<int64_t, int64_t, bool>> modulesToArrange;
  void addModulesToArrange(int64_t leftModuleId, int64_t rightModuleId, bool attach);
  void arrangeModules(int64_t leftModuleId, int64_t rightModuleId, bool attach);
};
