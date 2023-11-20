#include "collector.hpp"
#include <asset.hpp>

rack::math::Vec Collector::ueCorrectPos(const rack::math::Vec& parentSize, const rack::math::Rect& childBox) const {
  rack::math::Vec newPos;
  newPos.x = (childBox.pos.x - (parentSize.x * 0.5f)) + (childBox.size.x * 0.5f);
  newPos.y = (childBox.pos.y - (parentSize.y * 0.5f)) + (childBox.size.y * 0.5f);
  newPos.y = -newPos.y;
  return newPos;
}

rack::math::Vec Collector::ueCorrectPos(const rack::math::Vec& parentSize, const rack::math::Vec& childPos, const rack::math::Vec& childSize) const {
  rack::math::Rect childBox;
  childBox.pos = childPos;
  childBox.size = childSize;
  return ueCorrectPos(parentSize, childBox);
}

float Collector::px2cm(const float& px) const {
  float mm = px / (rack::window::SVG_DPI / rack::window::MM_PER_IN);
  return mm / 10.f;
}

rack::math::Vec Collector::vec2cm(const rack::math::Vec& pxVec) const {
  rack::math::Vec cmVec = pxVec;
  cmVec.x = px2cm(pxVec.x);
  cmVec.y = px2cm(pxVec.y);
  return cmVec;
}

rack::math::Rect Collector::box2cm(const rack::math::Rect& pxBox) const {
  rack::math::Rect cmBox = pxBox;

  cmBox.pos.x = px2cm(pxBox.pos.x);
  cmBox.pos.y = px2cm(pxBox.pos.y);
  cmBox.size.x = px2cm(pxBox.size.x);
  cmBox.size.y = px2cm(pxBox.size.y);

  return cmBox;
}

int Collector::randomId() {
  std::random_device dev;
  std::mt19937 rng(dev());
  std::uniform_int_distribution<std::mt19937::result_type> dist(10000, 30000);

  return dist(rng);
}

void Collector::collectModule(std::unordered_map<int64_t, VCVModule>& Modules, const int64_t& moduleId) {
  rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
  rack::engine::Module* mod = mw->getModule();

  if (mod->getModel()->name == "OSCctrl") return;

  rack::math::Rect panelBox;
  std::string panelSvgPath;
  if (!findModulePanel(mw, panelBox, panelSvgPath)) {
    WARN(
      "no panel found for %s:%s, abandoning collect.",
      mw->getModule()->getModel()->plugin->slug.c_str(),
      mw->getModule()->getModel()->name.c_str()
    );
    return;
  }

  panelBox.pos = mw->getPosition().minus(rack::app::RACK_OFFSET);
  panelBox = box2cm(panelBox);

  Modules[moduleId] = VCVModule(moduleId);
  VCVModule& vcv_module = Modules[moduleId];

  vcv_module.brand = mod->getModel()->plugin->brand;
  vcv_module.name = mod->getModel()->name;
  vcv_module.description = mod->getModel()->description;
  vcv_module.box = panelBox;
  vcv_module.panelSvgPath = panelSvgPath;

  rack::plugin::Model* model = mod->getModel();
  vcv_module.pluginSlug = model->plugin->slug;
  vcv_module.slug = model->slug;

  // Module LedDisplays and Lights
  for (rack::widget::Widget* widget : mw->children) {
    if (rack::app::LedDisplay* display = dynamic_cast<rack::app::LedDisplay*>(widget)) {
      collectDisplay(vcv_module, display);
    } else if (rack::app::LightWidget* lightWidget = dynamic_cast<rack::app::LightWidget*>(widget)) {
      collectModuleLight(vcv_module, lightWidget);
    }
  }

  // Params
  for (rack::app::ParamWidget* & paramWidget : mw->getParams()) {
    int paramId = paramWidget->getParamQuantity()->paramId;

    collectParam(vcv_module, paramWidget);

    for (rack::widget::Widget* & widget : paramWidget->children) {
      if (rack::app::LightWidget* lightWidget = dynamic_cast<rack::app::LightWidget*>(widget)) {
        collectParamLight(vcv_module, vcv_module.Params[paramId], lightWidget);
      }
    }

    // Knob
    if (rack::app::Knob* knob = dynamic_cast<rack::app::Knob*>(paramWidget)) {
      // avoid double-collecting sliders due to shared ancestors
      if (!dynamic_cast<rack::app::SliderKnob*>(paramWidget))
        collectKnob(vcv_module.Params[paramId], knob);
    }

    // Slider
    if (rack::app::SvgSlider* svgSlider = dynamic_cast<rack::app::SvgSlider*>(paramWidget)) {
      collectSlider(vcv_module.Params[paramId], svgSlider);
    }

    // Switch/Button
    if (rack::app::SvgSwitch* svgSwitch = dynamic_cast<rack::app::SvgSwitch*>(paramWidget)) {
      collectSwitch(vcv_module.Params[paramId], svgSwitch);
    }

    // Button (yet to see one in the wild)
    if (dynamic_cast<rack::app::SvgButton*>(paramWidget)) {
      WARN("found a button?! %s", vcv_module.name.c_str());
    }

    fillParamSvgPaths(vcv_module.Params[paramId]);
  }

  // Ports
  for (rack::app::PortWidget* portWidget : mw->getPorts()) {
    collectPort(vcv_module, portWidget);
  }
}

bool Collector::findModulePanel(rack::app::ModuleWidget* mw, rack::math::Rect& panelBox, std::string& panelSvgPath) {
  bool found{false};

  // special cases
  std::string& pluginSlug = mw->getModule()->getModel()->plugin->slug;
  std::string& moduleSlug = mw->getModule()->getModel()->slug;

  if (pluginSlug == "MockbaModular") {
    // TODO: handle mockba's two-layer ish? (base dark/light SvgPanel + SvgWidget overlay)
    doIfTypeRecursive<rack::widget::SvgWidget>(mw, [&](rack::widget::SvgWidget* svgWidget) {
      if (svgWidget->svg) {
        if (svgWidget->svg->path.find(moduleSlug) != std::string::npos) {
          panelBox = svgWidget->box;
          panelSvgPath = svgWidget->svg->path;
          found = true;
        }
      }
    });
  }
  if (found) return true;

  // generic thank-you-for-using-an-svg-panel case
  doIfTypeRecursive<rack::app::SvgPanel>(mw, [&](rack::app::SvgPanel* svgPanel) {
    if (svgPanel->svg) {
      panelBox = svgPanel->box;
      panelSvgPath = svgPanel->svg->path;
      found = true;
    }
  });

  return found;
}

void Collector::collectParam(VCVModule& vcv_module, rack::app::ParamWidget* paramWidget) {
  rack::engine::ParamQuantity* pq = paramWidget->getParamQuantity();

  vcv_module.Params[pq->paramId] = VCVParam(pq->paramId);
  VCVParam& param = vcv_module.Params[pq->paramId];

  param.name = pq->getLabel();
  param.unit = pq->getUnit();
  param.displayValue = pq->getDisplayValueString();
  param.minValue = pq->getMinValue();
  param.maxValue = pq->getMaxValue();
  param.defaultValue = pq->getDefaultValue();
  param.value = pq->getValue();
  param.visible = paramWidget->isVisible();
  param.snap = pq->snapEnabled;

  rack::math::Rect box = box2cm(paramWidget->getBox());
  box.pos = ueCorrectPos(vcv_module.box.size, box);
  param.box = box;
}

void Collector::fillParamSvgPaths(VCVParam& vcv_param) {
  int numPathsToFill = vcv_param.svgPaths.capacity() - vcv_param.svgPaths.size();
  for (int i = 0; i < numPathsToFill; i++) {
    vcv_param.svgPaths.push_back(std::string(""));
  }
}

void Collector::collectKnob(VCVParam& vcv_knob, rack::app::Knob* knob) {
  vcv_knob.type = ParamType::Knob;

  // more reasonable default min/max than Knob's -M_PI/M_PI,
  // specifically to account for Vult knobs' lack of defaults.
  // does this mess with endless encoders? probably?
  vcv_knob.minAngle =
    BasicallyEqual<float>(knob->minAngle, -M_PI) ? -0.75 * M_PI : knob->minAngle;
  vcv_knob.maxAngle =
    BasicallyEqual<float>(knob->maxAngle, M_PI) ? 0.75 * M_PI : knob->maxAngle;

  if (rack::app::SvgKnob* svgKnob = dynamic_cast<rack::app::SvgKnob*>(knob)) {
    try {
      // attempt to find foreground and background svgs by filename convention
      std::string foundBg, foundFg;
      for (rack::widget::Widget* & fb_child : svgKnob->fb->children) {
        if (rack::widget::SvgWidget* svg_widget = dynamic_cast<rack::widget::SvgWidget*>(fb_child)) {
          if (!svg_widget->svg) continue;
          std::string& path = svg_widget->svg->path;
          if (path.find("_bg") != std::string::npos || path.find("-bg") != std::string::npos) {
            foundBg = path;
          } else if (path.find("_fg") != std::string::npos || path.find("-fg") != std::string::npos) {
            foundFg = path;
          }
        }
      }
      vcv_knob.svgPaths.push_back(foundBg);
      vcv_knob.svgPaths.push_back(svgKnob->sw->svg->path);
      vcv_knob.svgPaths.push_back(foundFg);
    } catch (std::exception& e) {
      WARN("unable to find svgs for knob %s, using defaults (error: %s)", vcv_knob.name.c_str(), e.what());
      setDefaultKnobSvgs(vcv_knob);
    }
  } else {
    setDefaultKnobSvgs(vcv_knob);
  }
}

void Collector::setDefaultKnobSvgs(VCVParam& vcv_knob) {
  vcv_knob.svgPaths.clear();

  if (vcv_knob.box.size.x < 0.7f) {
    vcv_knob.svgPaths.push_back(
      rack::asset::system("res/ComponentLibrary/Trimpot_bg.svg")
    );
    vcv_knob.svgPaths.push_back(
      rack::asset::system("res/ComponentLibrary/Trimpot.svg")
    );
  } else if (vcv_knob.box.size.x < 1.2f) {
    vcv_knob.svgPaths.push_back(
      rack::asset::system("res/ComponentLibrary/RoundBlackKnob_bg.svg")
    );
    vcv_knob.svgPaths.push_back(
      rack::asset::system("res/ComponentLibrary/RoundBlackKnob.svg")
    );
  } else if (vcv_knob.box.size.x < 1.8f) {
    vcv_knob.svgPaths.push_back(
      rack::asset::system("res/ComponentLibrary/RoundLargeBlackKnob_bg.svg")
    );
    vcv_knob.svgPaths.push_back(
      rack::asset::system("res/ComponentLibrary/RoundLargeBlackKnob.svg")
    );
  } else {
    vcv_knob.svgPaths.push_back(
      rack::asset::system("res/ComponentLibrary/RoundHugeBlackKnob_bg.svg")
    );
    vcv_knob.svgPaths.push_back(
      rack::asset::system("res/ComponentLibrary/RoundHugeBlackKnob.svg")
    );
  }
}

void Collector::collectSwitch(VCVParam& vcv_switch, rack::app::SvgSwitch* svgSwitch) {
  vcv_switch.latch = svgSwitch->latch;
  vcv_switch.momentary = svgSwitch->momentary;

  // buttons have either momentary or latch, switches have neither
  if (svgSwitch->momentary || svgSwitch->latch) {
    vcv_switch.type = ParamType::Button;
  } else {
    vcv_switch.type = ParamType::Switch;
    vcv_switch.horizontal = vcv_switch.box.size.x > vcv_switch.box.size.y;
  }

  try {
		int index{-1};
		for (std::shared_ptr<rack::window::Svg> svg : svgSwitch->frames) {
			if (++index > 4) {
				WARN("%s switch has more than 5 frames", vcv_switch.name.c_str());
				break;
			}
			vcv_switch.svgPaths.push_back(svg->path);
		}
	} catch (std::exception& e) {
		WARN("unable to find svgs for switch %s, using defaults (error: %s)", vcv_switch.name.c_str(), e.what());
    setDefaultSwitchSvgs(vcv_switch, svgSwitch);
	}
}

void Collector::setDefaultSwitchSvgs(VCVParam& vcv_switch, rack::app::SvgSwitch* svgSwitch) {
  vcv_switch.svgPaths.clear();

  if (vcv_switch.type == ParamType::Button) {
    vcv_switch.svgPaths.push_back(
      rack::asset::system("res/ComponentLibrary/VCVButton_0.svg")
    );
    vcv_switch.svgPaths.push_back(
      rack::asset::system("res/ComponentLibrary/VCVButton_1.svg")
    );
  } else if (svgSwitch->frames.size() == 3) {
    if (vcv_switch.box.size.x == vcv_switch.box.size.y) {
      vcv_switch.svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/NKK_2.svg")
      );
      vcv_switch.svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/NKK_1.svg")
      );
      vcv_switch.svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/NKK_0.svg")
      );
    } else if (vcv_switch.horizontal) {
      vcv_switch.svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/CKSSThreeHorizontal_2.svg")
      );
      vcv_switch.svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/CKSSThreeHorizontal_1.svg")
      );
      vcv_switch.svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/CKSSThreeHorizontal_0.svg")
      );
    } else {
      vcv_switch.svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/CKSSThree_2.svg")
      );
      vcv_switch.svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/CKSSThree_1.svg")
      );
      vcv_switch.svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/CKSSThree_0.svg")
      );
    }
  } else if (svgSwitch->frames.size() == 2) {
    if (vcv_switch.box.size.x == vcv_switch.box.size.y) {
      vcv_switch.svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/NKK_2.svg")
      );
      vcv_switch.svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/NKK_0.svg")
      );
    } else {
      vcv_switch.svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/CKSS_1.svg")
      );
      vcv_switch.svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/CKSS_0.svg")
      );
    }
  } else {
    WARN("svgSwitch %s has frame strangeness (%lld frames)", vcv_switch.name.c_str(), svgSwitch->frames.size());
  }
}

void Collector::collectSlider(VCVParam& vcv_slider, rack::app::SvgSlider* svgSlider) {
  vcv_slider.type = ParamType::Slider;

  rack::math::Rect handleBox = box2cm(svgSlider->handle->getBox());
  handleBox.pos = ueCorrectPos(vcv_slider.box.size, handleBox);

  rack::math::Vec minHandlePos = vec2cm(svgSlider->minHandlePos);
  minHandlePos = ueCorrectPos(vcv_slider.box.size, minHandlePos, handleBox.size);

  rack::math::Vec maxHandlePos = vec2cm(svgSlider->maxHandlePos);
  maxHandlePos = ueCorrectPos(vcv_slider.box.size, maxHandlePos, handleBox.size);

  vcv_slider.horizontal = svgSlider->horizontal;
  vcv_slider.minHandlePos = minHandlePos;
  vcv_slider.maxHandlePos = maxHandlePos;
  vcv_slider.handleBox = handleBox;

  try {
    vcv_slider.svgPaths.push_back(svgSlider->background->svg->path);
    vcv_slider.svgPaths.push_back(svgSlider->handle->svg->path);
  } catch (std::exception& e) {
		WARN("unable to find svgs for slider %s, using defaults (error: %s)", vcv_slider.name.c_str(), e.what());
    setDefaultSliderSvgs(vcv_slider);
  }
}

void Collector::setDefaultSliderSvgs(VCVParam& vcv_slider) {
  vcv_slider.svgPaths.clear();

  if (vcv_slider.horizontal) {
    vcv_slider.svgPaths.push_back(
      rack::asset::system("res/ComponentLibrary/VCVSliderHorizontal.svg")
    );
  } else {
    vcv_slider.svgPaths.push_back(
      rack::asset::system("res/ComponentLibrary/VCVSlider.svg")
    );
  }

  vcv_slider.svgPaths.push_back(
    rack::asset::system("res/ComponentLibrary/VCVSliderHandle.svg")
  );
}

void Collector::collectModuleLight(VCVModule& vcv_module, rack::app::LightWidget* lightWidget) {
	int lightId = randomId();
  VCVLight vcv_light(lightId);
  vcv_light.moduleId = vcv_module.id;

	vcv_light.box = box2cm(lightWidget->getBox());
	vcv_light.box.pos = ueCorrectPos(vcv_module.box.size, vcv_light.box);

  vcv_light.shape = LightShape::Round;
  vcv_light.svgPath = rack::asset::system("res/ComponentLibrary/MediumLight.svg");

  vcv_light.widget = lightWidget;

  collectLight(vcv_light, lightWidget);

  vcv_module.Lights[lightId] = vcv_light;
}

void Collector::collectParamLight(VCVModule& vcv_module, VCVParam& vcv_param, rack::app::LightWidget* lightWidget) {
	int lightId = randomId();
  VCVLight vcv_light(lightId);
  vcv_light.moduleId = vcv_module.id;
  vcv_light.paramId = vcv_param.id;

	vcv_light.box = box2cm(lightWidget->getBox());
	vcv_light.box.pos = ueCorrectPos(vcv_param.box.size, vcv_light.box);

  vcv_light.shape =
    vcv_param.type == ParamType::Slider
      ? LightShape::Rectangle
      : LightShape::Round;

  vcv_light.svgPath =
    vcv_param.type == ParamType::Slider
      ? rack::asset::system("res/ComponentLibrary/VCVSliderLight.svg")
      : std::string("");

  vcv_light.widget = lightWidget;

  collectLight(vcv_light, lightWidget);

	vcv_param.Lights[lightId] = vcv_light;
  vcv_module.ParamLights[lightId] = &vcv_param.Lights[lightId];
}

void Collector::collectLight(VCVLight& vcv_light, rack::app::LightWidget* lightWidget) {
  vcv_light.visible = lightWidget->isVisible();
  vcv_light.color = lightWidget->color;
  vcv_light.bgColor = lightWidget->bgColor;
}

void Collector::collectDisplay(VCVModule& vcv_module, rack::app::LedDisplay* displayWidget) {
  rack::math::Rect box = box2cm(displayWidget->getBox());
  box.pos = ueCorrectPos(vcv_module.box.size, box);
  vcv_module.Displays.emplace_back(box);
}

void Collector::collectPort(VCVModule& vcv_module, rack::app::PortWidget* portWidget) {
  VCVPort* port;

  if (portWidget->type == rack::engine::Port::INPUT) {
		vcv_module.Inputs.emplace(portWidget->portId, portWidget->portId);
		port = &vcv_module.Inputs[portWidget->portId];
    port->type = PortType::Input;
  } else {
		vcv_module.Outputs.emplace(portWidget->portId, portWidget->portId);
		port = &vcv_module.Outputs[portWidget->portId];
    port->type = PortType::Output;
  }

	rack::math::Rect box = box2cm(portWidget->getBox());
	box.pos = ueCorrectPos(vcv_module.box.size, box);

  port->name = portWidget->getPortInfo()->name;
  port->box = box;
  port->description = portWidget->getPortInfo()->description;

  if (rack::app::SvgPort* svgPort = dynamic_cast<rack::app::SvgPort*>(portWidget)) {
    try {
      port->svgPath = svgPort->sw->svg->path;
    } catch (std::exception& e) {
      WARN("unable to find svg for port %s, using default (error: %s)", port->name.c_str(), e.what());
      setDefaultPortSvg(*port);
    }
  } else {
    setDefaultPortSvg(*port);
  }
}

void Collector::setDefaultPortSvg(VCVPort& vcv_port) {
  vcv_port.svgPath = rack::asset::system("res/ComponentLibrary/PJ301M.svg");
}
