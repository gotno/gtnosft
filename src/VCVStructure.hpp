#pragma once

#include "math.hpp"

#include <string>
#include <map>
#include <unordered_map>
#include <vector>

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
  std::string svgPath;
  bool visible{true};
  NVGcolor color;
  NVGcolor bgColor;

  rack::app::LightWidget* widget;

  bool hadFirstUpdate{false};

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
  // "button" will have one of latch or momentary, "switch" will have neither
  // (both are SvgSwitch under the hood)
  bool latch = false;
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

  VCVPort() {}
  VCVPort(int _id) : id(_id) {}
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

  std::string slug;
  std::string pluginSlug;

  std::map<int, VCVParam> Params;
  std::map<int, VCVPort> Inputs;
  std::map<int, VCVPort> Outputs;
  std::map<int, VCVLight> Lights;
  std::map<int, VCVLight*> ParamLights;

  std::vector<VCVDisplay> Displays;

  bool synced{false};

  VCVModule() {}
  VCVModule(int64_t _id) : id(_id) {}
  VCVModule(std::string _moduleSlug, std::string _pluginSlug)
    : slug(_moduleSlug), pluginSlug(_pluginSlug) {}
};

struct VCVCable {
  int64_t id = -1, inputModuleId, outputModuleId;
  int inputPortId, outputPortId;

  bool synced{false};

  VCVCable() {}
  VCVCable(int64_t _id, int64_t _inputModuleId, int64_t _outputModuleId, int _inputPortId, int _outputPortId)
    : id(_id), inputModuleId(_inputModuleId), outputModuleId(_outputModuleId), inputPortId(_inputPortId), outputPortId(_outputPortId) {}
  VCVCable(int64_t _inputModuleId, int64_t _outputModuleId, int _inputPortId, int _outputPortId)
    : inputModuleId(_inputModuleId), outputModuleId(_outputModuleId), inputPortId(_inputPortId), outputPortId(_outputPortId) {}
};
