#include <rack.hpp>
#include "../VCVStructure.hpp"

struct Collector {
private:
  /* box (size/position) utils */
  // convert rack's upper left origin to unreal's center origin
	rack::math::Vec ueCorrectPos(const rack::math::Vec& parentSize, const rack::math::Rect& box) const;
  // rack uses a mm->px formula: reverse it and divide by 10 for unreal centimeters
	float px2cm(const float& px) const;
	rack::math::Rect box2cm(const rack::math::Rect& pxBox) const;

  void collectParam(VCVModule& vcv_module, rack::app::ParamWidget* paramWidget);
  void collectKnob(VCVParam& vcv_knob, rack::app::Knob* knob);
  void setDefaultKnobSvgs(VCVParam& vcv_knob);

  void collectSwitch(VCVParam& vcv_switch, rack::app::SvgSwitch* svgSwitch);
  void setDefaultSwitchSvgs(VCVParam& vcv_switch, rack::app::SvgSwitch* svgSwitch);

  void collectPort(VCVModule& vcv_module, rack::app::PortWidget* port);
  void setDefaultPortSvg(VCVPort& vcv_port);
};
