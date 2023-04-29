#include "OscController.hpp"

#include "../dep/oscpack/ip/UdpSocket.h"

#include <chrono>
#include <cstring>
#include <random>

OscController::OscController() {
  endpoint = IpEndpointName("127.0.0.1", 7001);
}

OscController::~OscController() {
  if (syncworker.joinable()) syncworker.join();
}

void OscController::init() {
	collectRackModules();
  collectCables();
	printRackModules();
  printCables();
  syncAll();
}

rack::math::Rect OscController::box2cm(rack::math::Rect pxBox) {
  rack::math::Rect cmBox = pxBox;

  cmBox.pos.x = rack::window::px2cm(pxBox.pos.x);
  cmBox.pos.y = rack::window::px2cm(pxBox.pos.y);
  cmBox.size.x = rack::window::px2cm(pxBox.size.x);
  cmBox.size.y = rack::window::px2cm(pxBox.size.y);

  return cmBox;
}

rack::math::Vec OscController::vec2cm(rack::math::Vec pxVec) {
  rack::math::Vec cmVec = pxVec;

  cmVec.x = rack::window::px2cm(pxVec.x);
  cmVec.y = rack::window::px2cm(pxVec.y);

  return cmVec;
}

int OscController::randomId() {
	std::random_device dev;
	std::mt19937 rng(dev());
	std::uniform_int_distribution<std::mt19937::result_type> dist(10000, 30000);

  return dist(rng);
}

bool OscController::isRectangleLight(rack::app::MultiLightWidget* light) {
  using namespace rack::componentlibrary;

  if (dynamic_cast<VCVSliderLight<GrayModuleLightWidget>*>(light) ||
    dynamic_cast<VCVSliderLight<WhiteLight>*>(light) ||
    dynamic_cast<VCVSliderLight<RedLight>*>(light) ||
    dynamic_cast<VCVSliderLight<GreenLight>*>(light) ||
    dynamic_cast<VCVSliderLight<BlueLight>*>(light) ||
    dynamic_cast<VCVSliderLight<YellowLight>*>(light) ||
    dynamic_cast<VCVSliderLight<GreenRedLight>*>(light) ||
    dynamic_cast<VCVSliderLight<RedGreenBlueLight>*>(light)) return true;

  return false;
}

void OscController::collectRackModules() {
  DEBUG("collecting modules");
  for (int64_t& moduleId: APP->engine->getModuleIds()) {
    rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
    rack::engine::Module* mod = mw->getModule();

    if (mod->getModel()->name == "OSCctrl") continue;

    RackModules[moduleId] = VCVModule(
      moduleId,
      mod->getModel()->name,
      mod->getModel()->description,
      box2cm(mw->getPanel()->getBox())
    );

    for (rack::widget::Widget* mw_child : mw->children) {
      if (rack::app::LedDisplay* display = dynamic_cast<rack::app::LedDisplay*>(mw_child)) {
        RackModules[moduleId].Displays.emplace_back(box2cm(display->getBox()));
      } else if (rack::app::MultiLightWidget* light = dynamic_cast<rack::app::MultiLightWidget*>(mw_child)) {
        int lightId = randomId();

        RackModules[moduleId].Lights[lightId] = VCVLight(
          lightId,
          moduleId,
          box2cm(light->getBox()),
          LightShape::Round, // fixme
          light->color,
          light->bgColor,
          light
        );
      }
    }

    for (rack::app::ParamWidget* & pw : mw->getParams()) {
      rack::engine::ParamQuantity* pq = pw->getParamQuantity();

      RackModules[moduleId].Params[pq->paramId] = VCVParam(
        pq->paramId,
        pq->getLabel(),
        pq->getUnit(),
        pq->getDescription(),
        pq->getMinValue(),
        pq->getMaxValue(),
        pq->getDefaultValue(),
        pq->getValue()
      );

			for (rack::widget::Widget* & pw_child : pw->children) {
				if (rack::app::MultiLightWidget* light = dynamic_cast<rack::app::MultiLightWidget*>(pw_child)) {
          int lightId = randomId();
          LightShape lightShape = isRectangleLight(light) ? LightShape::Rectangle : LightShape::Round;

          RackModules[moduleId].Params[pq->paramId].Lights[lightId] = VCVLight(
            lightId,
            moduleId,
            pq->paramId,
            box2cm(light->getBox()),
            lightShape,
            light->color,
            light->bgColor,
            light
          );

          RackModules[moduleId].ParamLights[lightId] = 
            &RackModules[moduleId].Params[pq->paramId].Lights[lightId];
				}
			}

      RackModules[moduleId].Params[pq->paramId].snap = pq->snapEnabled;

      // Knob
      if (rack::app::SvgKnob* p_knob = dynamic_cast<rack::app::SvgKnob*>(pw)) {
        RackModules[moduleId].Params[pq->paramId].type = ParamType::Knob;
        RackModules[moduleId].Params[pq->paramId].box = box2cm(p_knob->getBox());
        RackModules[moduleId].Params[pq->paramId].minAngle = p_knob->minAngle;
        RackModules[moduleId].Params[pq->paramId].maxAngle = p_knob->maxAngle;
      }

      // Slider
      if (rack::app::SvgSlider* p_slider = dynamic_cast<rack::app::SvgSlider*>(pw)) {
        RackModules[moduleId].Params[pq->paramId].type = ParamType::Slider;
        RackModules[moduleId].Params[pq->paramId].box = box2cm(p_slider->getBox());
        RackModules[moduleId].Params[pq->paramId].horizontal = p_slider->horizontal;
        RackModules[moduleId].Params[pq->paramId].speed = p_slider->speed;
        RackModules[moduleId].Params[pq->paramId].minHandlePos = vec2cm(p_slider->minHandlePos);
        RackModules[moduleId].Params[pq->paramId].maxHandlePos = vec2cm(p_slider->maxHandlePos);
        RackModules[moduleId].Params[pq->paramId].handleBox = box2cm(p_slider->handle->getBox());
      }

      // Switch
      // ?? 1-4/4-1, 3-position switch
      // ag addFrame
      if (rack::app::SvgSwitch* p_switch = dynamic_cast<rack::app::SvgSwitch*>(pw)) {
        RackModules[moduleId].Params[pq->paramId].type = ParamType::Switch;
        RackModules[moduleId].Params[pq->paramId].box = box2cm(p_switch->getBox());
        RackModules[moduleId].Params[pq->paramId].latch = p_switch->latch;
        RackModules[moduleId].Params[pq->paramId].momentary = p_switch->momentary;
      }

      // Button
      if (rack::app::SvgButton* p_button = dynamic_cast<rack::app::SvgButton*>(pw)) {
        RackModules[moduleId].Params[pq->paramId].type = ParamType::Button;
        RackModules[moduleId].Params[pq->paramId].box = box2cm(p_button->getBox());
      }
    }

    for (rack::app::PortWidget* portWidget : mw->getPorts()) {
      PortType type = portWidget->type == rack::engine::Port::INPUT ? PortType::Input : PortType::Output;

      if (type == PortType::Input) {
        RackModules[moduleId].Inputs[portWidget->portId] = VCVPort(
          portWidget->portId,
          type,
          portWidget->getPortInfo()->name,
          portWidget->getPortInfo()->description,
          box2cm(portWidget->getBox())
        );
      } else {
        RackModules[moduleId].Outputs[portWidget->portId] = VCVPort(
          portWidget->portId,
          type,
          portWidget->getPortInfo()->name,
          portWidget->getPortInfo()->description,
          box2cm(portWidget->getBox())
        );
      }
    }
  }
  DEBUG("collected %lld modules", RackModules.size());
}

void OscController::printRackModules() {
  for (std::pair<int64_t, VCVModule> module_pair : RackModules) {
    // module id, name
    if (module_pair.second.Displays.size() > 0) {
      DEBUG("\n %lld %s (has %lld LED displays)", module_pair.first, module_pair.second.name.c_str(), module_pair.second.Displays.size());
    } else {
      DEBUG("\n %lld %s", module_pair.first, module_pair.second.name.c_str());
    }
    if (module_pair.second.Lights.size() > 0) {
      DEBUG("        (has %lld lights)", module_pair.second.Lights.size());
    }
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
          default:
            break;
        }

        // param id, type, name, unit
        DEBUG("    %d (%s): %s%s", param_pair.second.id, type.c_str(), param_pair.second.name.c_str(), param_pair.second.unit.c_str());
        if (param_pair.second.Lights.size() > 0) {
          DEBUG("      (has %lld lights)", param_pair.second.Lights.size());
        }

        if (type == "Knob") {
          DEBUG("      min/default/max %f/%f/%f (snap: %s)", param_pair.second.minValue, param_pair.second.defaultValue, param_pair.second.maxValue, param_pair.second.snap ? "true" : "false");
          DEBUG("      minAngle/maxAngle %f/%f", param_pair.second.minAngle, param_pair.second.maxAngle);
        }
        if (type == "Slider") {
          DEBUG("      speed %f (horizontal: %s)", param_pair.second.speed, param_pair.second.horizontal ? "true" : "false");
        }
        if (type == "Button") {
          DEBUG("      (momentary: %s)", param_pair.second.momentary ? "true" : "false");
        }
        if (type == "Switch") {
          DEBUG("      (latch: %s)", param_pair.second.latch ? "true" : "false");
          DEBUG("      (momentary: %s)", param_pair.second.momentary ? "true" : "false");
        }
      }
    }
    if (module_pair.second.Inputs.size() > 0) {
      DEBUG("  Inputs:");

      for (std::pair<int, VCVPort> input_pair : module_pair.second.Inputs) {
        DEBUG("      %d %s %s", input_pair.second.id, input_pair.second.name.c_str(), input_pair.second.description.c_str());
      }
    }
    if (module_pair.second.Outputs.size() > 0) {
      DEBUG("  Outputs:");

      for (std::pair<int, VCVPort> output_pair : module_pair.second.Outputs) {
        DEBUG("      %d %s %s", output_pair.second.id, output_pair.second.name.c_str(), output_pair.second.description.c_str());
      }
    }
  }
}

void OscController::collectCables() {
	for (int64_t& cableId: APP->engine->getCableIds()) {
		rack::engine::Cable* cable = APP->engine->getCable(cableId);

    Cables[cableId] = VCVCable(
      cable->id,
      cable->inputModule->getId(),
      cable->outputModule->getId(),
      cable->inputId,
      cable->outputId
		);
	}
}

void OscController::printCables() {
  for (std::pair<int64_t, VCVCable> cable_pair : Cables) {
    VCVModule* inputModule = &RackModules[cable_pair.second.inputModuleId];
    VCVModule* outputModule = &RackModules[cable_pair.second.outputModuleId];
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

void OscController::syncModuleParam(int64_t moduleId, VCVParam* param) {
  if (param->synced) return;

  char buffer[512];
  osc::OutboundPacketStream PacketStream(buffer, 512);

  PacketStream << osc::BeginMessage("/modules/param/add")
    << moduleId
    << param->id
    << param->type
    << param->name.c_str()
    << param->unit.c_str()
    << param->description.c_str()
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
    << param->latch
    << param->momentary
    << osc::EndMessage;

  sendMessage(PacketStream);
}
void OscController::syncModuleParams(int64_t moduleId) {
  for (std::pair<int, VCVParam> param_pair : RackModules[moduleId].Params) {
    syncModuleParam(moduleId, &param_pair.second);
  }
}

void OscController::syncModuleLight(int64_t moduleId, VCVLight* light, int paramId) {
  if (light->synced) return;

  /* if (paramId != -1) { */
  /*   DEBUG("syncing light (param) %lld:%d:%d", moduleId, paramId, light->id); */
  /* } else { */
  /*   DEBUG("syncing light %lld:%d", moduleId, light->id); */
  /* } */

  char buffer[512];
  osc::OutboundPacketStream PacketStream(buffer, 512);

  PacketStream << osc::BeginMessage("/modules/light/add")
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
    << osc::EndMessage;

  sendMessage(PacketStream);
}
void OscController::syncModuleLights(int64_t moduleId) {
  for (std::pair<int, VCVLight> light_pair : RackModules[moduleId].Lights) {
    syncModuleLight(moduleId, &light_pair.second);
  }
}
void OscController::syncParamLights(int64_t moduleId, VCVParam* param) {
  for (std::pair<int, VCVLight> light_pair : param->Lights) {
    syncModuleLight(moduleId, &light_pair.second, param->id);
  }
}

void OscController::syncModuleInput(int64_t moduleId, VCVPort* input) {
  if (input->synced) return;

  char buffer[512];
  osc::OutboundPacketStream PacketStream(buffer, 512);

  PacketStream << osc::BeginMessage("/modules/input/add")
    << moduleId
    << input->id
    << input->name.c_str()
    << input->description.c_str()
    << input->box.pos.x
    << input->box.pos.y
    << input->box.size.x
    << input->box.size.y
    << osc::EndMessage;

  sendMessage(PacketStream);
}
void OscController::syncModuleInputs(int64_t moduleId) {
  for (std::pair<int, VCVPort> pair : RackModules[moduleId].Inputs) {
    syncModuleInput(moduleId, &pair.second);
  }
}

void OscController::syncModuleOutput(int64_t moduleId, VCVPort* output) {
  if (output->synced) return;

  char buffer[512];
  osc::OutboundPacketStream PacketStream(buffer, 512);

  PacketStream << osc::BeginMessage("/modules/output/add")
    << moduleId
    << output->id
    << output->name.c_str()
    << output->description.c_str()
    << output->box.pos.x
    << output->box.pos.y
    << output->box.size.x
    << output->box.size.y
    << osc::EndMessage;

  sendMessage(PacketStream);
}
void OscController::syncModuleOutputs(int64_t moduleId) {
  for (std::pair<int, VCVPort> pair : RackModules[moduleId].Outputs) {
    syncModuleOutput(moduleId, &pair.second);
  }
}

void OscController::syncModuleDisplay(int64_t moduleId, VCVDisplay* display) {
  if (display->synced) return;

  char buffer[512];
  osc::OutboundPacketStream PacketStream(buffer, 512);

  PacketStream << osc::BeginMessage("/modules/display/add")
    << moduleId
    << display->box.pos.x
    << display->box.pos.y
    << display->box.size.x
    << display->box.size.y
    << osc::EndMessage;

  sendMessage(PacketStream);
}
void OscController::syncModuleDisplays(int64_t moduleId) {
  for (VCVDisplay display : RackModules[moduleId].Displays) {
    syncModuleDisplay(moduleId, &display);
  }
}

void OscController::syncRackModule(VCVModule* module) {
  if (module->synced) return;

  /* DEBUG("syncing module %lld\t %s", module->id, module->name.c_str()); */

  char buffer[512];
  osc::OutboundPacketStream PacketStream(buffer, 512);

  PacketStream << osc::BeginMessage("/modules/add")
    << module->id
    << module->name.c_str()
    << module->description.c_str()
    << module->box.pos.x
    << module->box.pos.y
    << module->box.size.x
    << module->box.size.y
    << osc::EndMessage;

  sendMessage(PacketStream);
}

void OscController::syncAll() {
  syncRackModules();
  syncCables();

  syncworker = std::thread(OscController::ensureSynced, this);
}

void OscController::syncRackModules() {
  for (std::pair<int64_t, VCVModule> pair : RackModules) {
    syncRackModule(&pair.second);
	}
}

void OscController::ensureSynced() {
  while(syncCheckCount < 5 && !isSynced()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    syncCheckCount++;
    DEBUG("not synced yet");
  }

  if (!isSynced()) {
    DEBUG("couldn't sync");
  } else {
    DEBUG("initial sync complete");
    sendInitialSyncComplete();
  }
}

bool OscController::isSynced() {
  for (std::pair<int64_t, VCVModule> p_module : RackModules) {
    if (!p_module.second.synced) return false;

    for (std::pair<int, VCVParam> p_param : p_module.second.Params) {
      if (!p_param.second.synced) return false;

      for (std::pair<int, VCVLight> p_light : p_param.second.Lights) {
        if (!p_light.second.synced) return false;
      }
    }

    for (std::pair<int, VCVPort> p_input : p_module.second.Inputs) {
      if (!p_input.second.synced) return false;
    }

    for (std::pair<int, VCVPort> p_output : p_module.second.Outputs) {
      if (!p_output.second.synced) return false;
    }

    for (std::pair<int, VCVLight> p_light : p_module.second.Lights) {
      if (!p_light.second.synced) return false;
    }

    // how, for multiple?
    // generate id like for Lights
    for (VCVDisplay& display : p_module.second.Displays) {
      if (!display.synced) return false;
    }
	}

  return true;
}

void OscController::syncRackModuleComponents(int64_t moduleId) {
  /* DEBUG("syncing components for %lld", moduleId); */
  syncModuleParams(moduleId);
  syncModuleLights(moduleId);
  syncModuleInputs(moduleId);
  syncModuleOutputs(moduleId);
  syncModuleDisplays(moduleId);
}

void OscController::syncCable(VCVCable* cable) {
  if (cable->synced) return;

  /* DEBUG("syncing cable %lld", cable->id); */

  char buffer[512];
  osc::OutboundPacketStream PacketStream(buffer, 512);

  PacketStream << osc::BeginMessage("/cables/add")
    << cable->id
    << cable->inputModuleId
    << cable->outputModuleId
    << cable->inputPortId
    << cable->outputPortId
    << osc::EndMessage;

  sendMessage(PacketStream);
}

void OscController::syncCables() {
  for (std::pair<int64_t, VCVCable> pair : Cables) {
    syncCable(&pair.second);
	}
}

void OscController::sendLightUpdates() {
  /* DEBUG("calling send light updates"); */
  for (std::pair<int64_t, LightReferenceMap> module_pair : LightReferences) {
    for (std::pair<int, VCVLight*> light_pair : module_pair.second) {
      VCVLight* light = light_pair.second;
      
      // unreal emissive colors ignore alpha and obscure the background color,
      // so screen the light color over the background color and
      // apply the alpha to the rgb values before sending.
      NVGcolor lightColor = rack::color::screen(light->widget->bgColor, light->widget->color);
      lightColor = rack::color::mult(light->widget->color, light->widget->color.a);

      // TODO: this doesn't work for the initial update of static lights
      /* if (!rack::color::isEqual(light->color, lightColor)) { */
      /*   light->color = lightColor; */
        sendLightUpdate(module_pair.first, light_pair.first, lightColor);
      /* } */
    }
  }
}
void OscController::sendLightUpdate(int64_t moduleId, int lightId, NVGcolor color) {
  /* if (RackModules[moduleId].Lights.count(lightId) > 0) { */
  /*   DEBUG("sending module light update for %s:%d", RackModules[moduleId].name.c_str(), lightId); */ 
  /* } else { */
  /*   DEBUG("sending param light update for %s:%d", RackModules[moduleId].name.c_str(), lightId); */ 
  /* } */

  char buffer[512]; // 32?
  osc::OutboundPacketStream PacketStream(buffer, 512);

  PacketStream << osc::BeginMessage("/modules/light/update")
    << moduleId // 8
    << lightId // 4
    << color.r // 4
    << color.g // 4
    << color.b // 4
    << color.a // 4
    << osc::EndMessage; // 28

  sendMessage(PacketStream);
}

void OscController::registerLightReference(int64_t moduleId, VCVLight* light) {
  /* DEBUG("registering light %d", light->id); */
  LightReferences[moduleId][light->id] = light;
}

void OscController::sendInitialSyncComplete() {
  char buffer[512];
  osc::OutboundPacketStream PacketStream(buffer, 512);

  PacketStream << osc::BeginMessage("/initial_sync_complete") << osc::EndMessage;
  sendMessage(PacketStream);
}

void OscController::sendMessage(osc::OutboundPacketStream packetStream) {
  UdpTransmitSocket(endpoint).Send(packetStream.Data(), packetStream.Size());
}

// UE callbacks

void OscController::UERx(const char* path, int64_t outerId, int innerId) {
  if (std::strcmp(path, "/rx/module") == 0) {
    /* DEBUG("/rx/module %lld", outerId); */
    RackModules[outerId].synced = true;
    syncRackModuleComponents(outerId);
  } else if (std::strcmp(path, "/rx/param") == 0) {
    /* DEBUG("/rx/param %lld:%d", outerId, innerId); */
    VCVParam* param = &RackModules[outerId].Params[innerId];
    param->synced = true;
    syncParamLights(outerId, param);
  } else if (std::strcmp(path, "/rx/input") == 0) {
    /* DEBUG("/rx/input %lld:%d", outerId, innerId); */
    RackModules[outerId].Inputs[innerId].synced = true;
  } else if (std::strcmp(path, "/rx/output") == 0) {
    /* DEBUG("/rx/output %lld:%d", outerId, innerId); */
    RackModules[outerId].Outputs[innerId].synced = true;
  } else if (std::strcmp(path, "/rx/module_light") == 0) {
    VCVLight* light = nullptr;

    if (RackModules[outerId].Lights.count(innerId) > 0) {
      /* DEBUG("/rx/module_light %lld:%d", outerId, innerId); */
      light = &RackModules[outerId].Lights[innerId];
    } else if (RackModules[outerId].ParamLights.count(innerId) > 0) {
      /* DEBUG("/rx/module_light (param) %lld:%d", outerId, innerId); */
      light = RackModules[outerId].ParamLights[innerId];
    }

    if (!light) {
      DEBUG("no module or param light found on /rx/module_light");
      return;
    }

    light->synced = true;
    registerLightReference(outerId, light);
  } else if (std::strcmp(path, "/rx/display") == 0) {
    /* DEBUG("/rx/display %lld:%d", outerId, innerId); */
    // how, for multiple?
    // generate id like for Lights
    RackModules[outerId].Displays[0].synced = true;
  } else if (std::strcmp(path, "/rx/cable") == 0) {
    /* DEBUG("/rx/cable %lld", outerId); */
    Cables[outerId].synced = true;
  } else {
    DEBUG("no known /rx/* path for %s", path);
  }
}
