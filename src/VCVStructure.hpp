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
  int paramId = -1;
  rack::math::Rect box;
  LightShape shape;
  NVGcolor color;
  NVGcolor bgColor;

  rack::app::MultiLightWidget* widget;

  bool synced = false;
  bool hadFirstUpdate = false;

  VCVLight() {}
  VCVLight(int _id, int64_t _moduleId, rack::math::Rect _box, LightShape _shape, NVGcolor _color, NVGcolor _bgColor, rack::app::MultiLightWidget* _widget)
    : id(_id), moduleId(_moduleId), box(_box), shape(_shape), color(_color), bgColor(_bgColor), widget(_widget) {}
  VCVLight(int _id, int64_t _moduleId, int _paramId, rack::math::Rect _box, LightShape _shape, NVGcolor _color, NVGcolor _bgColor, rack::app::MultiLightWidget* _widget)
    : id(_id), moduleId(_moduleId), paramId(_paramId), box(_box), shape(_shape), color(_color), bgColor(_bgColor), widget(_widget) {}
};

enum ParamType {
  Knob, Slider, Button, Switch
};
struct VCVParam {
  int id;
  ParamType type;
  std::string name;
  std::string unit;
  std::string description;
  rack::math::Rect box;

  float minValue;
  float maxValue;
  float defaultValue;
  float value;

  bool snap;

  std::string svgPath;
  std::string handleSvgPath;
  std::string backgroundSvgPath;
  std::string foregroundSvgPath;

  // Knob
  float minAngle = 0.f;
  float maxAngle = 0.f;

  // Slider
  rack::math::Rect handleBox = rack::math::Rect(0.f, 0.f, 0.f, 0.f);
  rack::math::Vec minHandlePos = rack::math::Vec(0.f, 0.f);
  rack::math::Vec maxHandlePos = rack::math::Vec(0.f, 0.f);
  bool horizontal = false;
  float speed = 0.f;
  std::vector<std::string> sliderLabels;

  // Switch/Button
  // "button" will have one of latch or momentary, "switch" will have neither
  // (both are SvgSwitch under the hood)
  bool latch = false;
  bool momentary = false;
  std::vector<std::string> frames; // svg paths

  // internal operations, not useful?
  // float displayBase;
  // float displayMultiplier;
  // float displayOffset;

  bool synced = false;

  std::map<int, VCVLight> Lights;

  VCVParam() {}
  VCVParam(int paramId, std::string paramName, std::string paramUnit, std::string paramDescription, float paramMinValue, float paramMaxValue, float paramDefaultValue, float paramValue)
    : id(paramId), name(paramName), unit(paramUnit), description(paramDescription),
      minValue(paramMinValue), maxValue(paramMaxValue), defaultValue(paramDefaultValue),
      value(paramValue) {}
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

  bool synced = false;

  VCVPort() {}
  VCVPort(int portId, PortType portType, std::string portName, std::string portDescription, rack::math::Rect portBox)
    : id(portId), type(portType), name(portName), description(portDescription), box(portBox) {}
};

struct VCVDisplay {
  rack::math::Rect box;

  bool synced = false;

  VCVDisplay(rack::math::Rect displayBox) : box(displayBox) {}
};

struct VCVModule {
  int64_t id;
  std::string name;
  std::string description;
  rack::math::Rect box;
  std::string panelSvgPath;

  std::map<int, VCVParam> Params;
  std::map<int, VCVPort> Inputs;
  std::map<int, VCVPort> Outputs;
  std::map<int, VCVLight> Lights;
  std::map<int, VCVLight*> ParamLights;

  std::vector<VCVDisplay> Displays;

  bool synced = false;

  VCVModule() {}
  VCVModule(int64_t moduleId, std::string moduleName)
    : id(moduleId), name(moduleName) {}
  VCVModule(int64_t moduleId, std::string moduleName, std::string modelDescription, rack::math::Rect panelBox, std::string panelPath)
    : id(moduleId), name(moduleName), description(modelDescription), box(panelBox), panelSvgPath(panelPath) {}
};

struct VCVCable {
  int64_t id, inputModuleId, outputModuleId;
  int inputPortId, outputPortId;

  bool synced = false;

  VCVCable() {}
  VCVCable(int64_t _id, int64_t _inputModuleId, int64_t _outputModuleId, int _inputPortId, int _outputPortId)
    : id(_id), inputModuleId(_inputModuleId), outputModuleId(_outputModuleId), inputPortId(_inputPortId), outputPortId(_outputPortId) {}
};
