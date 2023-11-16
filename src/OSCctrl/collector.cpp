#include "collector.hpp"
#include <asset.hpp>

rack::math::Vec Collector::ueCorrectPos(const rack::math::Vec& parentSize, const rack::math::Rect& box) const {
  rack::math::Vec newPos;
  newPos.x = (box.pos.x - (parentSize.x * 0.5f)) + (box.size.x * 0.5f);
  newPos.y = (box.pos.y - (parentSize.y * 0.5f)) + (box.size.y * 0.5f);
  newPos.y = -newPos.y;
  return newPos;
}

float Collector::px2cm(const float& px) const {
  float mm = px / (rack::window::SVG_DPI / rack::window::MM_PER_IN);
  return mm / 10.f;
}

rack::math::Rect Collector::box2cm(const rack::math::Rect& pxBox) const {
  rack::math::Rect cmBox = pxBox;

  cmBox.pos.x = px2cm(pxBox.pos.x);
  cmBox.pos.y = px2cm(pxBox.pos.y);
  cmBox.size.x = px2cm(pxBox.size.x);
  cmBox.size.y = px2cm(pxBox.size.y);

  return cmBox;
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

  rack::math::Rect box = box2cm(paramWidget->getBox());
  box.pos = ueCorrectPos(vcv_module.box.size, box);
  param.box = box;
}

void Collector::collectKnob(VCVParam& vcv_knob, rack::app::Knob* knob) {
  vcv_knob.type = ParamType::Knob;
  vcv_knob.minAngle = knob->minAngle;
  vcv_knob.maxAngle = knob->maxAngle;

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
	}
}

void Collector::collectPort(VCVModule& vcv_module, rack::app::PortWidget* portWidget) {
	PortType type;
  VCVPort& port;

  if (portWidget->type == rack::engine::Port::INPUT) {
    type = PortType::Input
		vcv_module.Inputs[portWidget->portId] = VCVPort(portWidget->portId);
		port = &vcv_module.Inputs[portWidget->portId];
  } else {
    type = PortType::Output;
		vcv_module.Outputs[portWidget->portId] = VCVPort(portWidget->portId);
		port = &vcv_module.Outputs[portWidget->portId];
  }

	rack::math::Rect box = box2cm(portWidget->getBox());
	box.pos = ueCorrectPos(panelBox.size, box.pos, box.size);

  port.type = type;
  port.name = portWidget->getPortInfo()->name;
  // TODO: unneeded?
  port.description = portWidget->getPortInfo()->description;
  port.box = box;

  if (rack::app::SvgPort* svgPort = dynamic_cast<rack::app::SvgPort*>(portWidget)) {
    try {
      port.svgPath = svgPort->sw->svg->path;
    } catch (std::exception& e) {
      WARN("unable to find svg for port %s, using default (error: %s)", port.name.c_str(), e.what());
      setDefaultPortSvg(port);
    }
  } else {
    setDefaultPortSvg(port);
  }
}

void Collector::setDefaultPortSvg(VCVPort& vcv_port) {
  vcv_port.svgPath = rack::asset::system("res/ComponentLibrary/PJ301M.svg");
}
