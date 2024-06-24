#pragma once

#include "math.hpp"

#include <string>
#include <map>
#include <unordered_map>
#include <vector>

template<typename T>
bool BasicallyEqual(T f1, T f2) {
  return (std::fabs(f1 - f2) <= std::numeric_limits<T>::epsilon() * std::fmax(std::fabs(f1), std::fabs(f2)));
}

enum LightShape {
  Round,
  Rectangle
};
struct VCVLight {
  int id;
  int64_t moduleId;
  int paramId{-1};
  rack::math::Rect box;
  LightShape shape;
  bool visible{true};
  NVGcolor color{nvgRGBA(0, 0, 0, 0)};
  NVGcolor bgColor{nvgRGBA(0, 0, 0, 0)};

  int overlapsParamId{-1};

  rack::app::LightWidget* widget;

  VCVLight() {}
  VCVLight(int _id) : id(_id) {}
};

enum ParamType {
  Knob, Slider, Button, Switch, Unknown
};
struct VCVParam {
  int id;
  ParamType type{ParamType::Unknown};
  std::string name;
  std::string unit;
  std::string displayValue;
  rack::math::Rect box;

  NVGcolor bodyColor;

  float minValue;
  float maxValue;
  float defaultValue;
  float value;

  bool visible{true};

  bool snap;

  // Knob
  float minAngle = 0.f;
  float maxAngle = 0.f;

  // Slider
  rack::math::Rect handleBox = rack::math::Rect(0.f, 0.f, 0.f, 0.f);
  rack::math::Vec minHandlePos = rack::math::Vec(0.f, 0.f);
  rack::math::Vec maxHandlePos = rack::math::Vec(0.f, 0.f);
  bool horizontal = false;
  float speed = 0.f;
  // TODO
  std::vector<std::string> sliderLabels;

  // Switch/Button
  bool momentary = false;

  // 0- knob bg, slider base, switch/button frame 0
  // 1- knob mg, slider handle, switch/button frame 1
  // 2- knob fg, switch/button frame 2
  // 3- switch/button frame 3
  // 4- switch/button frame 4
  std::vector<std::string> svgPaths;

  // internal operations, not useful?
  // float displayBase;
  // float displayMultiplier;
  // float displayOffset;

  std::map<int, VCVLight> Lights;

  VCVParam() {}
  VCVParam(int _id) : id(_id) {
    svgPaths.reserve(5);
  }

  void merge(const VCVParam& other) {
    value = other.value;
    visible = other.visible;
  }

  friend bool operator==(const VCVParam& a, const VCVParam& b) {
    return
      BasicallyEqual<float>(a.value, b.value)
        && a.visible == b.visible;
  }

  friend bool operator!=(const VCVParam& a, const VCVParam& b) {
    return !(a == b);
  }
};

enum PortType {
  Input, Output
};
struct VCVPort {
  int id;
  PortType type;
  std::string name;
  std::string description;
  rack::math::Rect box;
  std::string svgPath;

  bool visible{true};

  NVGcolor bodyColor;

  VCVPort() {}
  VCVPort(int _id) : id(_id) {}

  void merge(const VCVPort& other) {
    visible = other.visible;
  }

  friend bool operator==(const VCVPort& a, const VCVPort& b) {
    return a.visible == b.visible;
  }

  friend bool operator!=(const VCVPort& a, const VCVPort& b) {
    return !(a == b);
  }
};

struct VCVDisplay {
  rack::math::Rect box;

  VCVDisplay(rack::math::Rect displayBox) : box(displayBox) {}
};

struct VCVModule {
  int64_t id;
  std::string brand;
  std::string name;
  std::string description;
  rack::math::Rect box;
  std::string panelSvgPath;

  int64_t leftExpanderId{-1}, rightExpanderId{-1};

  NVGcolor bodyColor;

  std::string slug;
  std::string pluginSlug;

  int returnId;

  std::map<int, VCVParam> Params;
  std::map<int, VCVPort> Inputs;
  std::map<int, VCVPort> Outputs;
  std::map<int, VCVLight> Lights;
  std::map<int, VCVLight*> ParamLights;

  std::vector<VCVDisplay> Displays;

  bool synced{false};

  VCVModule() {}
  VCVModule(int64_t _id) : id(_id) {}
  VCVModule(std::string _moduleSlug, std::string _pluginSlug, int _returnId)
    : slug(_moduleSlug), pluginSlug(_pluginSlug), returnId(_returnId) {}

  std::map<int, VCVParam> getParams() const {
    return Params;
  }
  std::map<int, VCVPort> getInputs() const {
    return Inputs;
  }
  std::map<int, VCVPort> getOutputs() const {
    return Outputs;
  }
  friend bool operator==(const VCVModule& a, const VCVModule& b) {
    if (a.panelSvgPath != b.panelSvgPath) return false;

    auto aParams = a.getParams();
    auto bParams = b.getParams();
    for (auto& pair : aParams) {
      if (pair.second != bParams[pair.first]) return false;
    }

    auto aInputs = a.getInputs();
    auto bInputs = b.getInputs();
    for (auto& pair : aInputs) {
      if (pair.second != bInputs[pair.first]) return false;
    }

    auto aOutputs = a.getOutputs();
    auto bOutputs = b.getOutputs();
    for (auto& pair : aOutputs) {
      if (pair.second != bOutputs[pair.first]) return false;
    }

    return true;
  }
};

struct VCVCable {
  int64_t id = -1, inputModuleId, outputModuleId;
  int inputPortId, outputPortId;

  bool synced{false};

  NVGcolor color{nvgRGBA(0, 0, 0, 0)};

  VCVCable() {}
  VCVCable(int64_t _id, int64_t _inputModuleId, int64_t _outputModuleId, int _inputPortId, int _outputPortId, NVGcolor _color)
    : id(_id), inputModuleId(_inputModuleId), outputModuleId(_outputModuleId), inputPortId(_inputPortId), outputPortId(_outputPortId), color(_color) {}
  VCVCable(int64_t _inputModuleId, int64_t _outputModuleId, int _inputPortId, int _outputPortId, NVGcolor _color)
    : inputModuleId(_inputModuleId), outputModuleId(_outputModuleId), inputPortId(_inputPortId), outputPortId(_outputPortId), color(_color) {}
};

enum VCVMenuItemType {
  LABEL,
  ACTION,
  SUBMENU,
  RANGE,
  DIVIDER,
  UNKNOWN
};

struct VCVMenuItem {
  int index;
  VCVMenuItemType type{VCVMenuItemType::UNKNOWN};

  // menuItem->text or quantity->getString()
  std::string text;

  bool checked{false};
  bool disabled{false};

  std::string quantityLabel{""},
    quantityUnit{""};
  float quantityValue{0.f},
    quantityMinValue{0.f},
    quantityMaxValue{1.f},
    quantityDefaultValue{0.f};

  rack::Quantity* quantity{nullptr};

  VCVMenuItem(int _index) : index(_index) {}
};

struct VCVMenu {
  int64_t moduleId;
  int id, parentMenuId{-1}, parentItemIndex{-1};
  std::vector<VCVMenuItem> MenuItems;
};

typedef std::map<int, VCVMenu> ModuleMenuMap;
