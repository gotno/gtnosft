#include <rack.hpp>
#include "../VCVStructure.hpp"

struct Collector {
  void collectModule(std::unordered_map<int64_t, VCVModule>& Modules, const int64_t& moduleId);
  void collectCable(std::unordered_map<int64_t, VCVCable>& Cables, const int64_t& cableId);

  // shallow collect for diffing
  VCVModule collectModule(const int64_t& moduleId);

  void collectMenu(std::unordered_map<int64_t, ModuleMenuMap>& ContextMenus, VCVMenu& vcv_menu);
  rack::ui::Menu* findContextMenu(std::unordered_map<int64_t, ModuleMenuMap>& ContextMenus, VCVMenu& vcv_menu);

private:
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

  // special handling because some of you don't play by the rules and `getPanel`
  // doesn't work (looking at you, bogaudio and mockbamodular)
  bool findModulePanel(rack::app::ModuleWidget* mw, rack::math::Rect& panelBox, std::string& panelSvgPath);

  // attempt to parse out the most likely "main" color from an svg
  // by finding the largest visible, non-transparent bounding box
  void findMainSvgColor(std::string& svgPath, bool isPanelSvg);
  NVGcolor getSvgColor(std::string& svgPath, bool isPanelSvg = false);
  std::map<std::string, NVGcolor> svgColors;

  template<typename T>
  bool BasicallyEqual(T f1, T f2) {
    return (std::fabs(f1 - f2) <= std::numeric_limits<T>::epsilon() * std::fmax(std::fabs(f1), std::fabs(f2)));
  }

  template <class T, typename F>
  void doIfTypeRecursive(rack::widget::Widget* widget, F callback) {
    T* t = dynamic_cast<T*>(widget);
    if (t)
      callback(t);

    for (rack::widget::Widget* child : widget->children) {
      doIfTypeRecursive<T>(child, callback);
    }
  }

  bool endsWith(std::string str, std::string end) {
    return
      str.length() < end.length()
        ? false
        : str.compare(str.length() - end.length(), end.length(), end) == 0;
  }

  /* collect params */
  void collectParam(VCVModule& vcv_module, rack::app::ParamWidget* paramWidget);
  void fillParamSvgPaths(VCVParam& vcv_param);

  void collectKnob(VCVParam& vcv_knob, rack::app::Knob* knob);
  void setDefaultKnobSvgs(VCVParam& vcv_knob);

  void collectSwitch(VCVParam& vcv_switch, rack::app::ParamWidget* paramWidget);
  void setDefaultSwitchSvgs(VCVParam& vcv_switch, int numFrames, bool reverse = false);

  void collectSlider(VCVParam& vcv_slider, rack::app::SliderKnob* sliderKnob);
  void setDefaultSliderSvgs(VCVParam& vcv_slider);

  /* collect lights */
  void collectModuleLight(VCVModule& vcv_module, rack::app::LightWidget* lightWidget);
  void collectParamLight(VCVModule& vcv_module, VCVParam& vcv_param, rack::app::LightWidget* lightWidget);
  void collectLight(VCVLight& vcv_light, rack::app::LightWidget* lightWidget);

  /* collect display */
  void collectDisplay(VCVModule& vcv_module, rack::app::LedDisplay* displayWidget);

  /* collect ports */
  void collectPort(VCVModule& vcv_module, rack::app::PortWidget* port);
  void setDefaultPortSvg(VCVPort& vcv_port);

};
