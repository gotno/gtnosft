#include <rack.hpp>
#include "VCVStructure.hpp"

struct Collector {
  void collectParam(VCVModule& module, rack::app::ParamWidget* paramWidget) {
		rack::engine::ParamQuantity* pq = pw->getParamQuantity();

		module.Params[pq->paramId] = VCVParam(
			pq->paramId,
			pq->getLabel(),
			pq->getUnit(),
			pq->getDisplayValueString(),
			pq->getMinValue(),
			pq->getMaxValue(),
			pq->getDefaultValue(),
			pq->getValue()
		);
		module.Params[pq->paramId].visible = pw->isVisible();
  }

  void collectKnob(VCVParam& collectedParam, rack::app::Knob* knob) {
    collectedParam.type = ParamType::Knob;

    rack::math::Rect box = box2cm(knob->getBox());
    box.pos = ueCorrectPos(panelBox.size, box.pos, box.size);
    collectedParam.box = box;

    collectedParam.minAngle = knob->minAngle;
    collectedParam.maxAngle = knob->maxAngle;

    if (rack::app::SvgKnob* svgKnob = dynamic_cast<rack::app::SvgKnob*>(knob)) {
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
      collectedParam.svgPaths.push_back(foundBg);
      collectedParam.svgPaths.push_back(svgKnob->sw->svg->path);
      collectedParam.svgPaths.push_back(foundFg);
    }
  }
};
