#include "OscController.hpp"

#include "../dep/oscpack/ip/UdpSocket.h"

#include <plugin.hpp>
#include <tag.hpp>
#include <history.hpp>
#include <widget/event.hpp>

#include <chrono>
#include <cstring>
#include <random>
#include <sstream>

/* #include <BogAudioModules/src/widgets.hpp> */

OscController::OscController() {
  endpoint = IpEndpointName("127.0.0.1", 7001);
  queueWorker = std::thread(OscController::processQueue, this);
  DEBUG("USER D: %s", rack::asset::user().c_str());
}

OscController::~OscController() {
  queueWorkerRunning = false;

  // give the queue a reason to spin around one more time to exit
  Command command;
  command.first = CommandType::Noop;
  std::unique_lock<std::mutex> locker(qmutex);
  commandQueue.push(command);
  locker.unlock();
  queueLockCondition.notify_one();

  if (queueWorker.joinable()) queueWorker.join();

  delete[] oscBuffer;
}

float_time_point OscController::getCurrentTime() {
  return Time::now();
}

void OscController::collectAndSync() {
  std::unique_lock<std::mutex> qlocker(qmutex);
  while (!commandQueue.empty()) commandQueue.pop();
  qlocker.unlock();

  std::unique_lock<std::mutex> pulocker(pumutex);
  pendingParamUpdates.clear();
  pulocker.unlock();

  std::unique_lock<std::mutex> llocker(lmutex);
  LightReferences.clear();
  llocker.unlock();

  std::unique_lock<std::mutex> cablelocker(cablemutex);
  cablesToCreate.clear();
  cablesToDestroy.clear();
  cablelocker.unlock();

  std::unique_lock<std::mutex> modulelocker(modulemutex);
  modulesToCreate.clear();
  modulesToDestroy.clear();
  modulelocker.unlock();

  Modules.clear();
	collectModules(true);

  Cables.clear();
  collectCables(true);

  for (auto& pair : Modules) enqueueSyncModule(pair.first);
  for (auto& pair : Cables) enqueueSyncCable(pair.first);

  enqueueSyncLibrary();

  needsSync = false;
}

void OscController::enqueueCommand(Command command) {
  std::unique_lock<std::mutex> locker(qmutex);
  commandQueue.push(command);
  locker.unlock();
  queueLockCondition.notify_one();
}

void OscController::processQueue() {
  queueWorkerRunning = true;

  while (queueWorkerRunning) {
    std::unique_lock<std::mutex> locker(qmutex);
    queueLockCondition.wait(locker, [this](){ return !commandQueue.empty(); });

    Command command = commandQueue.front();
    commandQueue.pop();

    auto now = getCurrentTime();

    switch (command.first) {
      case CommandType::UpdateLights:
        sendLightUpdates();
        break;
      case CommandType::SyncModule:
        syncModule(&Modules[command.second.pid]);
        break;
      case CommandType::CheckModuleSync:
        // not time to check yet, requeue to check again later
        if ((now - command.second.lastCheck).count() <= command.second.wait) {
          commandQueue.push(command);
          continue;
        }

        // out of retries, abandon
        if (++command.second.retried >= command.second.retryLimit) {
          DEBUG("abandoning module sync %lld", command.second.pid);
          continue;
        }

        // check failed, requeue command
        if (!Modules[command.second.pid].synced) {
          command.second.lastCheck = now;
          commandQueue.push(command);
          continue;
        }

        // tx /module_sync_complete
        DEBUG("tx /module_sync_complete %lld", command.second.pid);
        sendModuleSyncComplete(command.second.pid);
        break;
      case CommandType::SyncCable:
        DEBUG("tx /cable/add %lld: %lld:%lld", command.second.pid, Cables[command.second.pid].inputModuleId, Cables[command.second.pid].outputModuleId);
        syncCable(&Cables[command.second.pid]);
        break;
      case CommandType::SyncLibrary:
        DEBUG("syncing library");
        syncLibrary();
        break;
      case CommandType::SyncParam:
        /* DEBUG("tx /param/sync"); */
        syncParam(command.second.pid, command.second.cid);
        break;
      case CommandType::SyncMenu:
        /* DEBUG("tx /menu/sync"); */
        syncMenu(command.second.pid, command.second.cid);
        break;
      case CommandType::Noop:
        DEBUG("Q:NOCOMMAND");
        break;
      default:
        break;
    }
  }
}
 
void OscController::collectModules(bool printResults) {
  DEBUG("collecting %lld modules", APP->engine->getModuleIds().size() - 1);
  for (int64_t& moduleId: APP->engine->getModuleIds()) {
    Collectr.collectModule(Modules, moduleId);
  }
  DEBUG("collected %lld modules", Modules.size());

  if (printResults) printModules();
}

void OscController::printModules() {
  for (std::pair<int64_t, VCVModule> module_pair : Modules) {
    // module id, name
    DEBUG("\n");
    if (module_pair.second.Displays.size() > 0) {
      DEBUG("%lld %s:%s (has %lld LED displays)", module_pair.first, module_pair.second.brand.c_str(), module_pair.second.name.c_str(), module_pair.second.Displays.size());
    } else {
      DEBUG("%lld %s:%s", module_pair.first, module_pair.second.brand.c_str(), module_pair.second.name.c_str());
    }
    if (module_pair.second.Lights.size() > 0) {
      DEBUG("        (has %lld lights)", module_pair.second.Lights.size());
    }
    DEBUG("        panel svg path: %s", module_pair.second.panelSvgPath.c_str());
    DEBUG("        body color: %fr %fg %fb", module_pair.second.bodyColor.r, module_pair.second.bodyColor.g, module_pair.second.bodyColor.b);
    DEBUG("  pos: %fx/%fy, size: %fx/%fy", module_pair.second.box.pos.x, module_pair.second.box.pos.y, module_pair.second.box.size.x, module_pair.second.box.size.y);

    if (module_pair.second.Params.size() > 0) {
      DEBUG("  Params:");

      for (std::pair<int, VCVParam> param_pair : module_pair.second.Params) {
        std::string type;
        switch (param_pair.second.type) {
          case (ParamType::Knob):
            type = "Knob";
            break;
          case (ParamType::Slider):
            type = "Slider";
            break;
          case (ParamType::Button):
            type = "Button";
            break;
          case (ParamType::Switch):
            type = "Switch";
            break;
          case (ParamType::Unknown):
          default:
            type = "Unknown";
            break;
        }

        // param id, type, name, unit
        DEBUG("    %d (%s): %s%s", param_pair.second.id, type.c_str(), param_pair.second.name.c_str(), param_pair.second.unit.c_str());
        DEBUG("    value: %f, min/default/max %f/%f/%f", param_pair.second.value, param_pair.second.minValue, param_pair.second.defaultValue, param_pair.second.maxValue);
        DEBUG("    box size %f/%f, pos %f/%f", param_pair.second.box.size.x, param_pair.second.box.size.y, param_pair.second.box.pos.x, param_pair.second.box.pos.y);
        if (param_pair.second.Lights.size() > 0) {
          DEBUG("      (has %lld lights)", param_pair.second.Lights.size());
        }

        if (type == "Knob") {
          DEBUG("      min/default/max %f/%f/%f (snap: %s)", param_pair.second.minValue, param_pair.second.defaultValue, param_pair.second.maxValue, param_pair.second.snap ? "true" : "false");
          DEBUG("      minAngle/maxAngle %f/%f", param_pair.second.minAngle, param_pair.second.maxAngle);
          for (std::string& path : param_pair.second.svgPaths) {
            DEBUG("        svg: %s", path.c_str());
          }
        }
        if (type == "Slider") {
          DEBUG("      speed %f (horizontal: %s, snap: %s)", param_pair.second.speed, param_pair.second.horizontal ? "true" : "false", param_pair.second.snap ? "true" : "false");
          DEBUG("      min/default/max %f/%f/%f", param_pair.second.minValue, param_pair.second.defaultValue, param_pair.second.maxValue);
          DEBUG("      box size %f/%f, pos %f/%f", param_pair.second.box.size.x, param_pair.second.box.size.y, param_pair.second.box.pos.x, param_pair.second.box.pos.y);
          DEBUG("      handleBox size %f/%f, pos %f/%f", param_pair.second.handleBox.size.x, param_pair.second.handleBox.size.y, param_pair.second.handleBox.pos.x, param_pair.second.handleBox.pos.y);
          DEBUG("      minHandlePos %f/%f, maxHandlePos %f/%f", param_pair.second.minHandlePos.x, param_pair.second.minHandlePos.y, param_pair.second.maxHandlePos.x, param_pair.second.maxHandlePos.y);
          for (std::string& label : param_pair.second.sliderLabels) {
            DEBUG("      label: %s", label.c_str());
          }
          for (std::string& path : param_pair.second.svgPaths) {
            DEBUG("        svg: %s", path.c_str());
          }
        }
        if (type == "Button") {
          DEBUG("      (momentary: %s)", param_pair.second.momentary ? "true" : "false");
          DEBUG("      min/default/max %f/%f/%f", param_pair.second.minValue, param_pair.second.defaultValue, param_pair.second.maxValue);
          DEBUG("      has %lld frames", param_pair.second.svgPaths.size());
          for (std::string& path : param_pair.second.svgPaths) {
            DEBUG("        frame svg: %s", path.c_str());
          }
        }
        if (type == "Switch") {
          DEBUG("      (horizontal: %s)", param_pair.second.horizontal ? "true" : "false");
          DEBUG("      min/default/max %f/%f/%f", param_pair.second.minValue, param_pair.second.defaultValue, param_pair.second.maxValue);
          DEBUG("      has %lld frames", param_pair.second.svgPaths.size());
          for (std::string& path : param_pair.second.svgPaths) {
            DEBUG("        frame svg: %s", path.c_str());
          }
        }
      }
    }
    if (module_pair.second.Inputs.size() > 0) {
      DEBUG("  Inputs:");

      for (std::pair<int, VCVPort> input_pair : module_pair.second.Inputs) {
        DEBUG("      %d %s", input_pair.second.id, input_pair.second.name.c_str());
        DEBUG("        %s", input_pair.second.description.c_str());
        DEBUG("        %s", input_pair.second.svgPath.c_str());
      }
    }
    if (module_pair.second.Outputs.size() > 0) {
      DEBUG("  Outputs:");

      for (std::pair<int, VCVPort> output_pair : module_pair.second.Outputs) {
        DEBUG("      %d %s %s", output_pair.second.id, output_pair.second.name.c_str(), output_pair.second.description.c_str());
        DEBUG("        %s", output_pair.second.svgPath.c_str());
      }
    }
  }
}

void OscController::collectCables(bool printResults) {
  DEBUG("collecting %lld cables", APP->engine->getCableIds().size());
	for (int64_t& cableId: APP->engine->getCableIds()) {
    Collectr.collectCable(Cables, cableId);
	}
  DEBUG("collected %lld cables", Cables.size());

  if (printResults) printCables();
}

void OscController::printCables() {
  for (std::pair<int64_t, VCVCable> cable_pair : Cables) {
    VCVModule* inputModule = &Modules[cable_pair.second.inputModuleId];
    VCVModule* outputModule = &Modules[cable_pair.second.outputModuleId];
    VCVCable* cable = &cable_pair.second;

    DEBUG("cable %lld connects %s:output %d to %s:input %d",
      cable->id,
      outputModule->name.c_str(),
      cable->outputPortId,
      inputModule->name.c_str(),
      cable->inputPortId
    );
  }
}

void OscController::bundleParam(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVParam* param) {
  bundle << osc::BeginMessage("/modules/param/add")
    << moduleId
    << param->id
    << param->type
    << param->name.c_str()
    << (param->displayValue.append(param->unit)).c_str()
    << param->box.pos.x
    << param->box.pos.y
    << param->box.size.x
    << param->box.size.y
    << param->minValue
    << param->maxValue
    << param->defaultValue
    << param->value
    << param->snap
    << param->minAngle
    << param->maxAngle
    << param->minHandlePos.x
    << param->minHandlePos.y
    << param->maxHandlePos.x
    << param->maxHandlePos.y
    << param->handleBox.pos.x
    << param->handleBox.pos.y
    << param->handleBox.size.x
    << param->handleBox.size.y
    << param->horizontal
    << param->speed
    << param->momentary
    << param->visible
    << param->svgPaths[0].c_str()
    << param->svgPaths[1].c_str()
    << param->svgPaths[2].c_str()
    << param->svgPaths[3].c_str()
    << param->svgPaths[4].c_str()
    << param->bodyColor.r
    << param->bodyColor.g
    << param->bodyColor.b
    << osc::EndMessage;
}

void OscController::bundleLight(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVLight* light, int paramId) {
  bundle << osc::BeginMessage("/modules/light/add")
    << moduleId
    << paramId
    << light->id
    << light->box.pos.x
    << light->box.pos.y
    << light->box.size.x
    << light->box.size.y
    << light->color.r
    << light->color.g
    << light->color.b
    << light->color.a
    << light->bgColor.r
    << light->bgColor.g
    << light->bgColor.b
    << light->bgColor.a
    << light->shape
    << light->visible
    << osc::EndMessage;
}

void OscController::bundleInput(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVPort* input) {
  bundle << osc::BeginMessage("/modules/input/add") << moduleId;
  bundlePort(bundle, input);
}

void OscController::bundleOutput(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVPort* output) {
  bundle << osc::BeginMessage("/modules/output/add") << moduleId;
  bundlePort(bundle, output);
}

void OscController::bundlePort(osc::OutboundPacketStream& bundle, VCVPort* port) {
  bundle << port->id
    << port->name.c_str()
    << port->description.c_str()
    << port->box.pos.x
    << port->box.pos.y
    << port->box.size.x
    << port->box.size.y
    << port->svgPath.c_str()
    << port->bodyColor.r
    << port->bodyColor.g
    << port->bodyColor.b
    << osc::EndMessage;
}

void OscController::bundleDisplay(osc::OutboundPacketStream& bundle, int64_t moduleId, VCVDisplay* display) {
  bundle << osc::BeginMessage("/modules/display/add")
    << moduleId
    << display->box.pos.x
    << display->box.pos.y
    << display->box.size.x
    << display->box.size.y
    << osc::EndMessage;
}

void OscController::bundleModule(osc::OutboundPacketStream& bundle, VCVModule* module) {
  bundle << osc::BeginBundleImmediate
    << osc::BeginMessage("/modules/add")
    << module->id
    << module->brand.c_str()
    << module->name.c_str()
    << module->description.c_str()
    << module->slug.c_str()
    << module->pluginSlug.c_str()
    << module->box.pos.x
    << module->box.pos.y
    << module->box.size.x
    << module->box.size.y
    << module->panelSvgPath.c_str()
    << module->bodyColor.r
    << module->bodyColor.g
    << module->bodyColor.b
    << osc::EndMessage;
}
 
void OscController::enqueueSyncModule(int64_t moduleId) {
  enqueueCommand(Command(CommandType::SyncModule, Payload(moduleId)));
  enqueueCommand(Command(
    CommandType::CheckModuleSync,
    Payload(moduleId, getCurrentTime(), 0.2f)
  ));
}

void OscController::syncModule(VCVModule* module) {
  osc::OutboundPacketStream bundle(oscBuffer, OSC_BUFFER_SIZE);

  bundle << osc::BeginBundleImmediate;
  bundleModule(bundle, module);

  for (std::pair<int, VCVParam> p_param : module->Params) {
    VCVParam* param = &p_param.second;

    if (param->type == ParamType::Unknown) {
      WARN("unknown param %d for %s, skipping bundle", param->id, module->name.c_str());
      continue;
    }

    bundleParam(bundle, module->id, param);

    for (std::pair<int, VCVLight> p_light : p_param.second.Lights) {
      VCVLight* light = &p_light.second;
      bundleLight(bundle, module->id, light, param->id);
    }
  }

  for (std::pair<int, VCVPort> p_port : module->Inputs) {
    bundleInput(bundle, module->id, &p_port.second);
  }

  for (std::pair<int, VCVPort> p_port : module->Outputs) {
    bundleOutput(bundle, module->id, &p_port.second);
  }

  for (std::pair<int, VCVLight> p_light : module->Lights) {
    VCVLight* light = &p_light.second;
    bundleLight(bundle, module->id, light);
  }

  // TODO? generate id like for Lights
  for (VCVDisplay& display : module->Displays) {
    bundleDisplay(bundle, module->id, &display);
  }

  bundle << osc::EndBundle;

  sendMessage(bundle);
}

rack::plugin::Model* OscController::findModel(std::string& pluginSlug, std::string& modelSlug) const {
  for (rack::plugin::Plugin* plugin : rack::plugin::plugins) {
    if (plugin->slug.compare(pluginSlug) == 0) {
      for (rack::plugin::Model* model : plugin->models) {
        if (model->slug.compare(modelSlug) == 0) return model;
      }
    }
  }
  return nullptr;
}

void OscController::createModule(std::string pluginSlug, std::string moduleSlug) {
  using namespace rack;

  plugin::Model* model = findModel(pluginSlug, moduleSlug);
  if (!model) return;

  // the following stolen from Browser.cpp
  // record usage
  settings::ModuleInfo& mi =
    settings::moduleInfos[model->plugin->slug][model->slug];
  mi.added++;
  mi.lastAdded = system::getUnixTime();

  /* INFO("Creating module %s", model->getFullName().c_str()); */
  engine::Module* module = model->createModule();
  APP->engine->addModule(module);

  /* INFO("Creating module widget %s", model->getFullName().c_str()); */
  ModuleWidget* moduleWidget = model->createModuleWidget(module);
  APP->scene->rack->setModulePosNearest(moduleWidget, rack::app::RACK_OFFSET);
  APP->scene->rack->addModule(moduleWidget);

  moduleWidget->loadTemplate();

  // history::ModuleAdd
  history::ModuleAdd* h = new history::ModuleAdd;
  h->setModule(moduleWidget);
  APP->history->push(h);

  if (moduleWidget) {
    Collectr.collectModule(Modules, module->id);
    enqueueSyncModule(module->id);
  }
}

void OscController::enqueueSyncCable(int64_t cableId) {
  enqueueCommand(Command(CommandType::SyncCable, Payload(cableId)));
}

void OscController::syncCable(VCVCable* cable) {
  /* DEBUG("syncing cable %lld", cable->id); */

  osc::OutboundPacketStream buffer(oscBuffer, OSC_BUFFER_SIZE);

  buffer << osc::BeginMessage("/cables/add")
    << cable->id
    << cable->inputModuleId
    << cable->outputModuleId
    << cable->inputPortId
    << cable->outputPortId
    << osc::EndMessage;

  sendMessage(buffer);
}

void OscController::enqueueLightUpdates() {
  enqueueCommand(Command(CommandType::UpdateLights, Payload()));
}

void OscController::sendLightUpdates() {
  /* DEBUG("calling send light updates"); */
  osc::OutboundPacketStream bundle(oscBuffer, OSC_BUFFER_SIZE);
  bundle << osc::BeginBundleImmediate;

  std::lock_guard<std::mutex> lock(lmutex);
  for (std::pair<int64_t, LightReferenceMap> module_pair : LightReferences) {
    int64_t& moduleId = module_pair.first;
    for (std::pair<int, VCVLight*> light_pair : module_pair.second) {
      int& lightId = light_pair.first;

      VCVLight* vcv_light = light_pair.second;
      if (!rack::color::isEqual(vcv_light->color, vcv_light->widget->color)) {
        vcv_light->color = vcv_light->widget->color;
        bundleLightUpdate(bundle, moduleId, lightId, vcv_light->color);
      }
    }
  }
  bundle << osc::EndBundle;

  // 16 bytes is the size of an *empty* bundle
  /* DEBUG("light update bundle size: %lld", bundle.Size()); */
  if(bundle.Size() > 16) sendMessage(bundle);
}

void OscController::bundleLightUpdate(osc::OutboundPacketStream& bundle, int64_t moduleId, int lightId, NVGcolor color) {
  bundle << osc::BeginMessage("/modules/light/update")
    << moduleId // 8
    << lightId // 4
    << color.r // 4
    << color.g // 4
    << color.b // 4
    << color.a // 4
    << osc::EndMessage; // 28
}

void OscController::registerLightReference(int64_t moduleId, VCVLight* light) {
  /* DEBUG("registering light %d", light->id); */
  std::lock_guard<std::mutex> lock(lmutex);
  LightReferences[moduleId][light->id] = light;
}

void OscController::sendModuleSyncComplete(int64_t moduleId) {
  osc::OutboundPacketStream message(oscBuffer, OSC_BUFFER_SIZE);

  message << osc::BeginMessage("/module_sync_complete")
    << moduleId
    << osc::EndMessage;

  sendMessage(message);
}

void OscController::sendMessage(osc::OutboundPacketStream packetStream) {
  // this _looks like_ it only sends the data present and not the whole buffer? good.
  /* DEBUG("data: %s, size: %lld", packetStream.Data(), packetStream.Size()); */
  UdpTransmitSocket(endpoint).Send(packetStream.Data(), packetStream.Size());
}

// UE callbacks
void OscController::rxModule(int64_t outerId, int innerId, float value) {
  for (auto& pair : Modules[outerId].Lights) {
    registerLightReference(outerId, &pair.second);
  }
  for (auto& pair : Modules[outerId].ParamLights) {
    registerLightReference(outerId, pair.second);
  }

  Modules[outerId].synced = true;
}

void OscController::rxCable(int64_t outerId, int innerId, float value) {
  Cables[outerId].synced = true;
}

void OscController::addCableToCreate(int64_t inputModuleId, int64_t outputModuleId, int inputPortId, int outputPortId) {
  std::lock_guard<std::mutex> lock(cablemutex);
  DEBUG("adding cable create to queue");
  cablesToCreate.push_back(VCVCable(inputModuleId, outputModuleId, inputPortId, outputPortId));
}

void OscController::addCableToDestroy(int64_t cableId) {
  std::lock_guard<std::mutex> lock(cablemutex);
  cablesToDestroy.push_back(cableId);
}

void OscController::addModuleToCreate(std::string pluginSlug, std::string moduleSlug) {
  std::lock_guard<std::mutex> lock(modulemutex);
  DEBUG("adding module create to queue");
  modulesToCreate.push_back(VCVModule(moduleSlug, pluginSlug));
}

void OscController::addModuleToDestroy(int64_t moduleId) {
  std::lock_guard<std::mutex> lock(modulemutex);
  modulesToDestroy.push_back(moduleId);
}

void OscController::addModuleToDiff(int64_t moduleId) {
  std::lock_guard<std::mutex> lock(modulediffmutex);
  pendingModuleDiffs.emplace(moduleId);
}

void OscController::processCableUpdates() {
  std::lock_guard<std::mutex> lock(cablemutex);
  for (VCVCable cable_model : cablesToCreate) {
    rack::engine::Cable* cable = new rack::engine::Cable;
    cable->id = cable_model.id;
    cable->inputModule = APP->engine->getModule(cable_model.inputModuleId);
    cable->inputId = cable_model.inputPortId;
    cable->outputModule = APP->engine->getModule(cable_model.outputModuleId);
    cable->outputId = cable_model.outputPortId;
    APP->engine->addCable(cable);

    rack::app::CableWidget* cableWidget = new rack::app::CableWidget;
		cableWidget->setCable(cable);
		cableWidget->color = rack::settings::cableColors[0];
		APP->scene->rack->addCable(cableWidget);

    Collectr.collectCable(Cables, cable->id);
    enqueueSyncCable(cable->id);
  }
  cablesToCreate.clear();

  for (int64_t cableId : cablesToDestroy) {
    APP->engine->removeCable(APP->engine->getCable(cableId));
    rack::app::CableWidget * cw = APP->scene->rack->getCable(cableId);
    APP->scene->rack->removeCable(cw);
    // TODO: does this need to delete objects?
    /* delete cw; */
    // crashes: Assertion failed: it != internal->cables.end(), file src/engine/Engine.cpp, line 1009
  }
  cablesToDestroy.clear();
}

void OscController::processMenuRequests() {
  std::lock_guard<std::mutex> lock(menumutex);
  for (VCVMenu& menu : menusToSync) {
    Collectr.collectMenu(ContextMenus, menu);
    enqueueSyncMenu(menu.moduleId, menu.id);
  }
  menusToSync.clear();
}

void OscController::enqueueSyncMenu(int64_t moduleId, int menuId) {
  enqueueCommand(Command(CommandType::SyncMenu, Payload(moduleId, menuId)));
}

void OscController::syncMenu(int64_t moduleId, int menuId) {
  if (ContextMenus.count(moduleId) == 0 || ContextMenus[moduleId].count(menuId) == 0) {
    WARN("no context menu to sync (%lld:%d)", moduleId, menuId);
    return;
  }

  VCVMenu& menu = ContextMenus.at(moduleId).at(menuId);

  /* printMenu(menu); */

  osc::OutboundPacketStream bundle(oscBuffer, OSC_BUFFER_SIZE);
  bundle << osc::BeginBundleImmediate;

  // sync plugin with modules and module tags, one plugin at a time
  for (VCVMenuItem& menuItem : menu.MenuItems) {
    if (menuItem.type == VCVMenuItemType::UNKNOWN) {
      WARN("unknown context menu item type %lld:%d-%d", moduleId, menuId, menuItem.index);
      continue;
    }

    bundle << osc::BeginMessage("/menu/item/add")
      << menu.moduleId
      << menu.id
      << menuItem.index
      << menuItem.type
      << menuItem.text.c_str()
      << menuItem.checked
      << menuItem.disabled
      << menuItem.quantityValue
      << menuItem.quantityMinValue
      << menuItem.quantityMaxValue
      << menuItem.quantityDefaultValue
      << menuItem.quantityLabel.c_str()
      << menuItem.quantityUnit.c_str()
      << osc::EndMessage;
  }

  bundle << osc::BeginMessage("/menu/synced")
    << menu.moduleId
    << menu.id
    << osc::EndMessage;

  sendMessage(bundle);
}

void OscController::printMenu(VCVMenu& menu) {
  DEBUG("\n\ncontext menu id:%d for %s:%s", menu.id, Modules[menu.moduleId].brand.c_str(), Modules[menu.moduleId].name.c_str());
  for (VCVMenuItem& item : menu.MenuItems) {
    switch (item.type) {
      case VCVMenuItemType::LABEL:
        DEBUG("[%s]", item.text.c_str());
        break;
      case VCVMenuItemType::ACTION:
        if (item.checked) {
          DEBUG("%s\t[x]", item.text.c_str());
        } else {
          DEBUG("%s", item.text.c_str());
        }
        break;
      case VCVMenuItemType::DIVIDER:
        DEBUG("--------------------");
        break;
      case VCVMenuItemType::SUBMENU:
        DEBUG("%s\t->", item.text.c_str());
        break;
      case VCVMenuItemType::RANGE:
        DEBUG("==%s--", item.text.c_str());
        break;
      default:
        DEBUG("???");
    }
  }
  DEBUG("\n");
}

void OscController::processModuleUpdates() {
  std::lock_guard<std::mutex> lock(cablemutex);
  for (VCVModule& module_model : modulesToCreate) {
    createModule(module_model.pluginSlug, module_model.slug);
  }
  modulesToCreate.clear();

  for (int64_t moduleId : modulesToDestroy) {
    std::unique_lock<std::mutex> locker(lmutex);
    LightReferences.erase(moduleId);
    locker.unlock();

    Modules.erase(moduleId);

    rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
    mw->removeAction();
  }
  modulesToDestroy.clear();
}

void OscController::updateParam(int64_t outerId, int innerId, float value) {
  std::lock_guard<std::mutex> lock(pumutex);
  Modules[outerId].Params[innerId].value = value;
  pendingParamUpdates.emplace(outerId, innerId);
}

void OscController::processParamUpdates() {
  if (pendingParamUpdates.empty()) return;

  std::set<std::pair<int64_t, int>> paramUpdates;
  std::unique_lock<std::mutex> locker(pumutex);
  paramUpdates.swap(pendingParamUpdates);
  locker.unlock();

  for (const std::pair<int64_t, int>& pair : paramUpdates) {
    const int64_t& moduleId = pair.first;
    const int& paramId = pair.second;
    const float& value = Modules[moduleId].Params[paramId].value;

    APP->engine->setParamValue(APP->engine->getModule(moduleId), paramId, value);
    rack::engine::ParamQuantity* pq =
      APP->scene->rack->getModule(moduleId)->getParam(paramId)->getParamQuantity();
    Modules[moduleId].Params[paramId].displayValue = pq->getDisplayValueString();
    enqueueSyncParam(moduleId, paramId);
  }
}

void OscController::processModuleDiffs() {
  if (pendingModuleDiffs.empty()) return;

  std::set<int64_t> moduleDiffs;
  std::unique_lock<std::mutex> locker(modulediffmutex);
  moduleDiffs.swap(pendingModuleDiffs);
  locker.unlock();

  for (const int64_t& moduleId : moduleDiffs) {
    VCVModule& moduleThen = Modules.at(moduleId);
    VCVModule moduleNow = Collectr.collectModule(moduleId);

    auto aParams = moduleThen.getParams();
    auto bParams = moduleNow.getParams();

    for (auto& pair : aParams) {
      int paramId = pair.first;
      if (pair.second != bParams[paramId]) {
        DEBUG("updating param %s from diff", pair.second.name.c_str());
        moduleThen.Params[paramId].merge(moduleNow.Params[paramId]);
        enqueueSyncParam(moduleId, paramId);
      }
    }
  }
}

void OscController::enqueueSyncParam(int64_t moduleId, int paramId) {
  enqueueCommand(Command(CommandType::SyncParam, Payload(moduleId, paramId)));
}

void OscController::syncParam(int64_t moduleId, int paramId) {
  osc::OutboundPacketStream buffer(oscBuffer, OSC_BUFFER_SIZE);

  VCVParam& param = Modules[moduleId].Params[paramId];

  buffer << osc::BeginMessage("/param/sync")
    << moduleId
    << paramId
    << param.displayValue.c_str()
    << param.value
    << param.visible
    << osc::EndMessage;

  sendMessage(buffer);
}

void OscController::enqueueSyncLibrary() {
  enqueueCommand(Command(CommandType::SyncLibrary, Payload()));
}

void OscController::syncLibrary() {
  // sync plugin with modules and module tags, one plugin at a time
  for (rack::plugin::Plugin* plugin : rack::plugin::plugins) {
    if (plugin->slug == "gtnosft") continue;

    osc::OutboundPacketStream bundle(oscBuffer, OSC_BUFFER_SIZE);
    bundle << osc::BeginBundleImmediate;

    bundle << osc::BeginMessage("/library/plugin/add")
      << plugin->brand.c_str()
      << plugin->slug.c_str()
      << osc::EndMessage;
    for (rack::plugin::Model* model : plugin->models) {
      // we can get the og mutable name from the slug, so why not
      std::string modelName = model->name;
      if (plugin->slug == "AudibleInstruments") {
        modelName.append(" (").append(model->slug).append(")");
      }

      // model->setFavorite();

      bundle << osc::BeginMessage("/library/module/add")
        << plugin->slug.c_str()
        << modelName.c_str()
        << model->slug.c_str()
        << model->description.c_str()
        << model->isFavorite()
        << osc::EndMessage;
      for (int& tagId : model->tagIds) {
        bundle << osc::BeginMessage("/library/module_tag/add")
          << plugin->slug.c_str()
          << model->slug.c_str()
          << osc::int32(tagId)
          << osc::EndMessage;
      }
    }

    bundle << osc::EndBundle;
    sendMessage(bundle);
  }

  // sync canonical tag aliases
  osc::OutboundPacketStream tagbundle(oscBuffer, OSC_BUFFER_SIZE);
  tagbundle << osc::BeginBundleImmediate;

  for (size_t i = 0; i < rack::tag::tagAliases.size(); i++) {
    tagbundle << osc::BeginMessage("/library/tag/add")
      << osc::int32(i)
      << rack::tag::tagAliases[i][0].c_str()
      << osc::EndMessage;
  }

  tagbundle << osc::EndBundle;
  sendMessage(tagbundle);
}

void OscController::setModuleFavorite(std::string pluginSlug, std::string moduleSlug, bool favorite) {
  for (rack::plugin::Plugin* plugin : rack::plugin::plugins) {
    if (plugin->slug != pluginSlug) continue;
    for (rack::plugin::Model* model : plugin->models) {
      if (model->slug != moduleSlug) continue;
      model->setFavorite(favorite);
    }
  }
}

void OscController::addMenuToSync(VCVMenu menu) {
  std::lock_guard<std::mutex> lock(menumutex);
  menusToSync.push_back(menu);
}

void OscController::clickMenuItem(int64_t moduleId, int menuId, int menuItemIndex) {
  std::lock_guard<std::mutex> lock(menuitemmutex);
  pendingMenuClicks.emplace(moduleId, menuId, menuItemIndex);
}

void OscController::updateMenuItemQuantity(int64_t moduleId, int menuId, int menuItemIndex, float value) {
  std::lock_guard<std::mutex> lock(menuquantitymutex);
  pendingMenuQuantityUpdates.emplace(moduleId, menuId, menuItemIndex, value);
}

void OscController::processMenuClicks() {
  if (pendingMenuClicks.empty()) return;

  std::set<std::tuple<int64_t, int, int>> menuClicks;
  std::unique_lock<std::mutex> locker(menuitemmutex);
  menuClicks.swap(pendingMenuClicks);
  locker.unlock();

  for (const std::tuple<int64_t, int, int>& tuple : menuClicks) {
    const int64_t& moduleId = std::get<0>(tuple);
    const int& menuId = std::get<1>(tuple);
    const int& menuItemIndex = std::get<2>(tuple);

    rack::ui::Menu* menu =
      Collectr.findContextMenu(ContextMenus, ContextMenus.at(moduleId).at(menuId));

    int index = -1;
    for (rack::widget::Widget* menu_child : menu->children) {
      if (++index != menuItemIndex) continue;

      rack::ui::MenuItem* menuItem =
        dynamic_cast<rack::ui::MenuItem*>(menu_child);

      if (!menuItem) {
        WARN("found menu selection is not a menu item");
        break;
      }

      menuItem->doAction(true);
      break;
    }

    // close menu
    rack::ui::MenuOverlay* overlay = menu->getAncestorOfType<rack::ui::MenuOverlay>();
    if (overlay) overlay->requestDelete();

    addMenuToSync(ContextMenus.at(moduleId).at(menuId));
  }
}

void OscController::processMenuQuantityUpdates() {
  if (pendingMenuQuantityUpdates.empty()) return;

  std::set<std::tuple<int64_t, int, int, float>> menuQuantityUpdates;
  std::unique_lock<std::mutex> locker(menuquantitymutex);
  menuQuantityUpdates.swap(pendingMenuQuantityUpdates);
  locker.unlock();

  for (const std::tuple<int64_t, int, int, float>& tuple : menuQuantityUpdates) {
    const int64_t& moduleId = std::get<0>(tuple);
    const int& menuId = std::get<1>(tuple);
    const int& menuItemIndex = std::get<2>(tuple);
    const float& value = std::get<3>(tuple);

    VCVMenu& vcv_menu = ContextMenus.at(moduleId).at(menuId);
    rack::ui::Menu* menu = Collectr.findContextMenu(ContextMenus, vcv_menu);

    int index = -1;
    for (rack::widget::Widget* menu_child : menu->children) {
      if (++index != menuItemIndex) continue;

      rack::Quantity* quantity{nullptr};

      if (rack::ui::Slider* slider = dynamic_cast<rack::ui::Slider*>(menu_child)) {
        quantity = slider->quantity;
      } else if (rack::ui::RadioButton* radioButton = dynamic_cast<rack::ui::RadioButton*>(menu_child)) {
        quantity = radioButton->quantity;
      } else if (rack::ui::Button* button = dynamic_cast<rack::ui::Button*>(menu_child)) {
        quantity = button->quantity;
      }

      if (!quantity) {
        WARN("found menu selection has no quantity");
        break;
      }

      quantity->setValue(value);
      break;
    }

    // close menu
    rack::ui::MenuOverlay* overlay = menu->getAncestorOfType<rack::ui::MenuOverlay>();
    if (overlay) overlay->requestDelete();

    addMenuToSync(vcv_menu);
  }
}
