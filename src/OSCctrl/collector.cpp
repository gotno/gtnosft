#include "collector.hpp"
#include <asset.hpp>
#include <regex>

#include <BogaudioModules/src/widgets.hpp>

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

void Collector::collectMenu(std::unordered_map<int64_t, ModuleMenuMap>& ContextMenus, VCVMenu& vcv_menu) {
  rack::ui::Menu* menu = findContextMenu(ContextMenus, vcv_menu);

  if (!menu) {
    WARN("no menu found for %lld:%d", vcv_menu.moduleId, vcv_menu.id);
    return;
  }

  using namespace rack::widget;

  int index = -1;
  for (Widget* menu_child : menu->children) {
    VCVMenuItem vcv_menu_item(++index);

    if (rack::ui::MenuLabel* label = dynamic_cast<rack::ui::MenuLabel*>(menu_child)) {
      vcv_menu_item.type = VCVMenuItemType::LABEL;
      vcv_menu_item.text = label->text;
    } else if (rack::ui::MenuItem* menuItem = dynamic_cast<rack::ui::MenuItem*>(menu_child)) {
      // step to handle rightText generation for some menu item types
      menuItem->step();


      if (endsWith(menuItem->rightText, std::string(RIGHT_ARROW))) {
        // submenu
        vcv_menu_item.type = VCVMenuItemType::SUBMENU;
      } else {
        // generic action
        vcv_menu_item.type = VCVMenuItemType::ACTION;
      }

      vcv_menu_item.text = menuItem->text;
      vcv_menu_item.disabled = menuItem->disabled;
      if (endsWith(menuItem->rightText, std::string(CHECKMARK_STRING)))
        vcv_menu_item.checked = true;
    } else if (rack::ui::Slider* slider = dynamic_cast<rack::ui::Slider*>(menu_child)) {
      vcv_menu_item.type = VCVMenuItemType::RANGE;
      if (slider->quantity) {
        vcv_menu_item.quantity = slider->quantity;

        vcv_menu_item.text = slider->quantity->getString();

        vcv_menu_item.quantityLabel = slider->quantity->getLabel();
        vcv_menu_item.quantityUnit = slider->quantity->getUnit();

        vcv_menu_item.quantityValue = slider->quantity->getValue();
        vcv_menu_item.quantityMinValue = slider->quantity->getMinValue();
        vcv_menu_item.quantityMaxValue = slider->quantity->getMaxValue();
        vcv_menu_item.quantityDefaultValue = slider->quantity->getDefaultValue();
      }
    } else if (dynamic_cast<rack::ui::MenuSeparator*>(menu_child)) {
      vcv_menu_item.type = VCVMenuItemType::DIVIDER;
    } else {
      vcv_menu_item.type = VCVMenuItemType::UNKNOWN;
    }

    vcv_menu.MenuItems.push_back(vcv_menu_item);
  }

  ContextMenus[vcv_menu.moduleId][vcv_menu.id] = vcv_menu;

  // close menu
  rack::ui::MenuOverlay* overlay = menu->getAncestorOfType<rack::ui::MenuOverlay>();
  if (overlay) overlay->requestDelete();
}

rack::ui::Menu* Collector::findContextMenu(std::unordered_map<int64_t, ModuleMenuMap>& ContextMenus, VCVMenu& vcv_menu) {
  using namespace rack::widget;

  std::vector<int> selections;
  int parentMenuId = vcv_menu.parentMenuId;

  if (parentMenuId != -1) {
    selections.push_back(vcv_menu.parentItemIndex);
    while (parentMenuId != 0) {
      VCVMenu& parentMenu = ContextMenus.at(vcv_menu.moduleId).at(parentMenuId);
      parentMenuId = parentMenu.parentMenuId;
      selections.push_back(parentMenu.parentItemIndex);
    }
    std::reverse(selections.begin(), selections.end());
  }

  // open module context menu
  rack::app::ModuleWidget* moduleWidget = APP->scene->rack->getModule(vcv_menu.moduleId);
  moduleWidget->createContextMenu();

  // grab base menu
  rack::ui::Menu* foundMenu{nullptr};
  for (Widget* scene_child : APP->scene->children) {
    if (dynamic_cast<rack::ui::MenuOverlay*>(scene_child)) {
      for (Widget* overlay_child : scene_child->children) {
        if (rack::ui::Menu* menu = dynamic_cast<rack::ui::Menu*>(overlay_child)) {
          foundMenu = menu;
          break;
        }
      }
      if (foundMenu) break;
    }
  }

  // step through selections if any, opening submenus
  for (int& selection : selections) {
    int index = -1;
    for (Widget* menu_child : foundMenu->children) {
      if (++index != selection) continue;

      rack::ui::MenuItem* menuItem =
        dynamic_cast<rack::ui::MenuItem*>(menu_child);

      if (!menuItem) {
        WARN("found menu selection is not a menu item");
        return nullptr;
      }

      // Dispatch EnterEvent
      EventContext cEnter;
      cEnter.target = menuItem;
      Widget::EnterEvent eEnter;
      eEnter.context = &cEnter;
      menuItem->onEnter(eEnter);

      if (foundMenu->childMenu) {
        foundMenu = foundMenu->childMenu;
        break;
      }
    }
  }

  return foundMenu;
}

void Collector::collectCable(std::unordered_map<int64_t, VCVCable>& Cables, const int64_t& cableId) {
  rack::engine::Cable* cable = APP->engine->getCable(cableId);

  Cables[cableId] = VCVCable(
    cable->id,
    cable->inputModule->getId(),
    cable->outputModule->getId(),
    cable->inputId,
    cable->outputId
  );
}

VCVModule Collector::collectModule(const int64_t& moduleId) {
  VCVModule vcv_module(moduleId);

  rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);

  rack::math::Rect _panelBox;
  std::string panelSvgPath;
  findModulePanel(mw, _panelBox, panelSvgPath);

  vcv_module.panelSvgPath = panelSvgPath;

  for (rack::app::ParamWidget* & paramWidget : mw->getParams()) {
    rack::engine::ParamQuantity* pq = paramWidget->getParamQuantity();

    vcv_module.Params[pq->paramId] = VCVParam(pq->paramId);
    VCVParam& param = vcv_module.Params[pq->paramId];

    param.value = pq->getValue();
    param.visible = paramWidget->isVisible();
  }

  return vcv_module;
}

void Collector::collectModule(std::unordered_map<int64_t, VCVModule>& Modules, const int64_t& moduleId, int returnId) {
  rack::app::ModuleWidget* mw = APP->scene->rack->getModule(moduleId);
  rack::engine::Module* mod = mw->getModule();

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

  vcv_module.returnId = returnId;
  vcv_module.brand = mod->getModel()->plugin->brand;
  vcv_module.name = mod->getModel()->name;
  vcv_module.description = mod->getModel()->description;
  vcv_module.box = panelBox;
  vcv_module.panelSvgPath = panelSvgPath;
  vcv_module.bodyColor = getSvgColor(panelSvgPath);

  if (mod->leftExpander.moduleId > 0)
    vcv_module.leftExpanderId = mod->leftExpander.moduleId;
  if (mod->rightExpander.moduleId > 0)
    vcv_module.rightExpanderId = mod->rightExpander.moduleId;

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
    VCVParam& vcv_param = vcv_module.Params[paramId];

    // Param lights
    for (rack::widget::Widget* & widget : paramWidget->children) {
      if (rack::app::LightWidget* lightWidget = dynamic_cast<rack::app::LightWidget*>(widget)) {
        collectParamLight(vcv_module, vcv_param, lightWidget);
      }
    }

    // Knob
    if (rack::app::Knob* knob = dynamic_cast<rack::app::Knob*>(paramWidget)) {
      // avoid double-collecting sliders due to shared ancestors
      if (!dynamic_cast<rack::app::SliderKnob*>(paramWidget))
        collectKnob(vcv_param, knob);
    }

    // Slider
    if (rack::app::SliderKnob* sliderKnob = dynamic_cast<rack::app::SliderKnob*>(paramWidget)) {
      collectSlider(vcv_param, sliderKnob);
    }

    // Switch/Button
    bool isSwitch = dynamic_cast<rack::app::SvgSwitch*>(paramWidget)
      || dynamic_cast<rack::app::Switch*>(paramWidget)
      || dynamic_cast<bogaudio::StatefulButton*>(paramWidget);
    if (isSwitch) {
      collectSwitch(vcv_param, paramWidget);
    }

    // Button (yet to see one in the wild)
    if (dynamic_cast<rack::app::SvgButton*>(paramWidget)) {
      WARN("found a button?! %s", vcv_module.name.c_str());
    }

    for (std::string& path : vcv_param.svgPaths) {
      if (path.empty()) continue;
      vcv_param.bodyColor = getSvgColor(path);
      break;
    }

    fillParamSvgPaths(vcv_param);
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

  if (pluginSlug == "SlimeChild-Substation") {
    // TODO: handle slimechild's two-layer ish? (base png panel + svg text overlay)
    //       (replace "_Text.svg" with "_400z.png" in path)
    std::smatch match;
    std::string moduleName;

    std::map<std::string, std::string> actualFilenames;
    actualFilenames.emplace("Envelopes", "Envelope");
    actualFilenames.emplace("Filter-Expander", "FilterPlus");

    if (std::regex_search(moduleSlug, match, std::regex("SlimeChild-Substation-(.+)"))) {
      moduleName = match.str(1);
      if (actualFilenames.count(moduleName) > 0)
        moduleName = actualFilenames.at(moduleName);
    }
    if (moduleName.empty()) return false;

    doIfTypeRecursive<rack::widget::SvgWidget>(mw, [&](rack::widget::SvgWidget* svgWidget) {
      if (svgWidget->svg) {
        if (svgWidget->svg->path.find(moduleName) != std::string::npos) {
          panelBox = svgWidget->box;
          panelSvgPath = svgWidget->svg->path;
          // std::regex_search(panelSvgPath, match, std::regex("(.*)_Text\.svg"))) {
          // panelPngPath = match.str(1);
          // panelPngPath.append("_400z.png");
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

void Collector::findMainSvgColor(std::string& svgPath, bool isPanelSvg) {
  // educated guess as to what a reasonable minimum area for a panel svg
  // should be, based on testing a bunch of different modules
  float largestArea{isPanelSvg ? 11000.f : 0.f};

  // default in case we can't find anything reasonable
  NVGcolor svgColor =
    isPanelSvg ? nvgRGBA(255, 255, 255, 255) : nvgRGBA(0, 0, 0, 255);

  NSVGimage* handle = nullptr;
  handle = nsvgParseFromFile(svgPath.c_str(), "px", SVG_DPI);

  if (!handle) {
    WARN("defaulting svg color for %s", svgPath.c_str());
    svgColors[svgPath] = svgColor;
  }

  for (NSVGshape* shape = handle->shapes; shape; shape = shape->next) {
    // skip invisible shapes
    if (!(shape->flags & NSVG_FLAGS_VISIBLE)) continue;

    // "Tight bounding box of the shape [minx,miny,maxx,maxy]."
    float area =
      (shape->bounds[2] - shape->bounds[0]) * (shape->bounds[3] - shape->bounds[1]);

    // skip if this shape is smaller than the largest so far
    if (area < largestArea) continue;

    unsigned int color;
    switch (shape->fill.type) {
      case NSVG_PAINT_COLOR:
        color = shape->fill.color;
        break;
      case NSVG_PAINT_LINEAR_GRADIENT:
      case NSVG_PAINT_RADIAL_GRADIENT:
        // the final stop tends to be the darker color
        color = shape->fill.gradient->stops[shape->fill.gradient->nstops - 1].color;
        break;
      default:
        // skip unknown fill types
        continue;
    }

    // skip transparent colors
    if (((color >> 24) & 0xff) < 255) continue;

    largestArea = area;
    svgColor = nvgRGBA(
      (color >> 0) & 0xff,
      (color >> 8) & 0xff,
      (color >> 16) & 0xff,
      (color >> 24) & 0xff
    );
  }

  svgColors[svgPath] = svgColor;
  delete handle;
}

NVGcolor Collector::getSvgColor(std::string& svgPath, bool isPanelSvg) {
  if (svgColors.count(svgPath) == 0) findMainSvgColor(svgPath, isPanelSvg);
  return svgColors.at(svgPath);
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

void Collector::collectSwitch(VCVParam& vcv_switch, rack::app::ParamWidget* paramWidget) {
  rack::app::SvgSwitch* svgSwitch = dynamic_cast<rack::app::SvgSwitch*>(paramWidget);
  rack::app::SvgSwitch defaultSwitch;
  if (!svgSwitch) svgSwitch = &defaultSwitch;

  bogaudio::StatefulButton* BSButton = dynamic_cast<bogaudio::StatefulButton*>(paramWidget);
  if (BSButton) svgSwitch->frames = BSButton->_frames;


  // we only use momentary right now (latch has something to do with
  // internal svg handling? unclear)
  vcv_switch.momentary = svgSwitch->momentary;

  // buttons have either momentary or latch, switches have neither.
  if (svgSwitch->momentary || svgSwitch->latch) {
    vcv_switch.type = ParamType::Button;
  } else {
    vcv_switch.type = ParamType::Switch;
    vcv_switch.horizontal = vcv_switch.box.size.x > vcv_switch.box.size.y;
  }

  // we're only set up to handle 5 frames max. this is 2 more than i've ever
  // seen, but just in case, call it out if we see it.
  if (svgSwitch->frames.size() > 5) {
    WARN("%s switch has more than 5 frames", vcv_switch.name.c_str());
  }

  // we might be looking at a custom switch (slimechild, for instance)
  // TODO: ok so this only runs for slimechild afaik, so we're defaulting to 
  //       reversed frame order here which is probably not going to be right
  //       for everyone else
  if (svgSwitch->frames.size() == 0) {
    WARN("no frames found in SvgSwitch %s, defaulting (and reversing)", vcv_switch.name.c_str());
    setDefaultSwitchSvgs(vcv_switch, vcv_switch.maxValue + 1, true);
    return;
  }

  // sometimes grabbing the svg path errors out (vult, for instance)
  try {
		for (std::shared_ptr<rack::window::Svg> svg : svgSwitch->frames) {
			vcv_switch.svgPaths.push_back(svg->path);
		}
	} catch (std::exception& e) {
		WARN("errored finding svgs for switch %s, using defaults (error: %s)", vcv_switch.name.c_str(), e.what());
    setDefaultSwitchSvgs(vcv_switch, svgSwitch->frames.size());
    return;
	}

  if (vcv_switch.svgPaths.size() > 0) return;
  WARN("switch %s has no frames, setting type to Unknown", vcv_switch.name.c_str());
  vcv_switch.type = ParamType::Unknown;
}

void Collector::setDefaultSwitchSvgs(VCVParam& vcv_switch, int numFrames, bool reverse) {
  vcv_switch.svgPaths.clear();
  std::vector<std::string> svgPaths;

  if (vcv_switch.type == ParamType::Button) {
    vcv_switch.svgPaths.push_back(
      rack::asset::system("res/ComponentLibrary/VCVButton_0.svg")
    );
    vcv_switch.svgPaths.push_back(
      rack::asset::system("res/ComponentLibrary/VCVButton_1.svg")
    );
    return;
  }

  if (numFrames == 3) {
    if (vcv_switch.box.size.x == vcv_switch.box.size.y) {
      svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/NKK_2.svg")
      );
      svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/NKK_1.svg")
      );
      svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/NKK_0.svg")
      );
    } else if (vcv_switch.horizontal) {
      svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/CKSSThreeHorizontal_2.svg")
      );
      svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/CKSSThreeHorizontal_1.svg")
      );
      svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/CKSSThreeHorizontal_0.svg")
      );
    } else {
      svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/CKSSThree_2.svg")
      );
      svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/CKSSThree_1.svg")
      );
      svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/CKSSThree_0.svg")
      );
    }
  } else if (numFrames == 2) {
    if (vcv_switch.box.size.x == vcv_switch.box.size.y) {
      svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/NKK_2.svg")
      );
      svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/NKK_0.svg")
      );
    } else {
      svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/CKSS_1.svg")
      );
      svgPaths.push_back(
        rack::asset::system("res/ComponentLibrary/CKSS_0.svg")
      );
    }
  } else {
    WARN("svgSwitch %s has frame strangeness (%d frames)", vcv_switch.name.c_str(), numFrames);
  }

  if (reverse) std::reverse(svgPaths.begin(), svgPaths.end());
  for (auto& path : svgPaths) {
    vcv_switch.svgPaths.push_back(path);
  }
}

void Collector::collectSlider(VCVParam& vcv_slider, rack::app::SliderKnob* sliderKnob) {
  vcv_slider.type = ParamType::Slider;

  rack::app::SvgSlider* svgSlider = dynamic_cast<rack::app::SvgSlider*>(sliderKnob);

  rack::componentlibrary::VCVSlider defaultSlider;
  if (!svgSlider) {
    svgSlider = dynamic_cast<rack::app::SvgSlider*>(&defaultSlider);
  }

  rack::math::Rect handleBox = box2cm(svgSlider->handle->getBox());
  handleBox.pos = ueCorrectPos(vcv_slider.box.size, handleBox);
  vcv_slider.handleBox = handleBox;

  rack::math::Vec minHandlePos = vec2cm(svgSlider->minHandlePos);
  minHandlePos = ueCorrectPos(vcv_slider.box.size, minHandlePos, handleBox.size);
  vcv_slider.minHandlePos = minHandlePos;

  rack::math::Vec maxHandlePos = vec2cm(svgSlider->maxHandlePos);
  maxHandlePos = ueCorrectPos(vcv_slider.box.size, maxHandlePos, handleBox.size);
  vcv_slider.maxHandlePos = maxHandlePos;

  vcv_slider.horizontal = svgSlider->horizontal;

  if (bogaudio::VUSlider* bogSlider = dynamic_cast<bogaudio::VUSlider*>(sliderKnob)) {
    rack::math::Vec factor{
      bogSlider->box.size.x / svgSlider->box.size.x,
      bogSlider->box.size.y / svgSlider->box.size.y,
    };

    rack::math::Rect handleBox = box2cm(svgSlider->handle->getBox());
    handleBox.size = handleBox.size.mult(factor);
    handleBox.pos = handleBox.pos.mult(factor);
    handleBox.pos = ueCorrectPos(vcv_slider.box.size, handleBox);
    vcv_slider.handleBox = handleBox;

    rack::math::Vec minHandlePos = vec2cm(svgSlider->minHandlePos);
    minHandlePos = minHandlePos.mult(factor);
    minHandlePos = ueCorrectPos(vcv_slider.box.size, minHandlePos, handleBox.size);
    vcv_slider.minHandlePos = minHandlePos;

    rack::math::Vec maxHandlePos = vec2cm(svgSlider->maxHandlePos);
    maxHandlePos = maxHandlePos.mult(factor);
    maxHandlePos = ueCorrectPos(vcv_slider.box.size, maxHandlePos, handleBox.size);
    vcv_slider.maxHandlePos = maxHandlePos;
  }

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

  vcv_light.shape =
    vcv_light.box.size.x != vcv_light.box.size.y
      ? LightShape::Rectangle
      : LightShape::Round;

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
    vcv_param.type == ParamType::Slider || vcv_light.box.size.x != vcv_light.box.size.y
      ? LightShape::Rectangle
      : LightShape::Round;

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

  port->bodyColor = getSvgColor(port->svgPath);
}

void Collector::setDefaultPortSvg(VCVPort& vcv_port) {
  vcv_port.svgPath = rack::asset::system("res/ComponentLibrary/PJ301M.svg");
}
