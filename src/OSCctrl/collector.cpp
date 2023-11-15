#include "collector.hpp"

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
      return;
    } catch (std::exception& e) {
      WARN("unable to find svgs for knob %s, using defaults (error: %s)", vcv_knob.name.c_str(), e.what());
    }
  }
  setDefaultKnobSvgs(vcv_knob);
}

void Collector::setDefaultKnobSvgs(VCVParam& knob) {
  // check knob.box.size.x
  // use appropriate svg paths from system::asset
}
