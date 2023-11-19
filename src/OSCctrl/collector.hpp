#include <rack.hpp>
#include "../VCVStructure.hpp"

struct Collector {
  void collectModule(std::unordered_map<int64_t, VCVModule>& Modules, const int64_t& moduleId);
/* private: */
  /* utils */
  // convert rack's upper left origin to unreal's center origin
  rack::math::Vec ueCorrectPos(const rack::math::Vec& parentSize, const rack::math::Rect& childBox) const;
  rack::math::Vec ueCorrectPos(const rack::math::Vec& parentSize, const rack::math::Vec& childPos, const rack::math::Vec& childSize) const;
  // rack uses a mm->px formula: reverse it and divide by 10 for unreal centimeters
  float px2cm(const float& px) const;
  rack::math::Vec vec2cm(const rack::math::Vec& pxVec) const;
  rack::math::Rect box2cm(const rack::math::Rect& pxBox) const;
  // generate a random id to keep track of lights
	int randomId();
  // some modules (looking at you, BogAudio) define their own module widget
  // with no `getPanel`, so we gotta dig through the children.
  rack::app::SvgPanel* findModulePanel(const rack::app::ModuleWidget* mw) const;

  /* collect params */
  void collectParam(VCVModule& vcv_module, rack::app::ParamWidget* paramWidget);

  void collectKnob(VCVParam& vcv_knob, rack::app::Knob* knob);
  void setDefaultKnobSvgs(VCVParam& vcv_knob);

  void collectSwitch(VCVParam& vcv_switch, rack::app::SvgSwitch* svgSwitch);
  void setDefaultSwitchSvgs(VCVParam& vcv_switch, rack::app::SvgSwitch* svgSwitch);

  void collectSlider(VCVParam& vcv_slider, rack::app::SvgSlider* svgSlider);
  void setDefaultSliderSvgs(VCVParam& vcv_slider);

  /* collect ports */
  void collectPort(VCVModule& vcv_module, rack::app::PortWidget* port);
  void setDefaultPortSvg(VCVPort& vcv_port);

  template<typename T>
  bool BasicallyEqual(T f1, T f2) {
    return (std::fabs(f1 - f2) <= std::numeric_limits<T>::epsilon() * std::fmax(std::fabs(f1), std::fabs(f2)));
  }
};
