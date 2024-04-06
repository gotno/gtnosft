#include "OscRouter.hpp"
#include "OscController.hpp"

#include <cstring>

void OscRouter::ProcessMessage(const osc::ReceivedMessage& message, const IpEndpointName& remoteEndpoint) {
  (void) remoteEndpoint; // suppress unused parameter warning

  if (!controller) {
    WARN("`OscController* controller` not set, cannot process message.");
    return;
  }

  std::string path(message.AddressPattern());
  if (path.compare(std::string("/set_unreal_server_port")) == 0) {
    osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();

    osc::uint32 unrealServerPort;
    unrealServerPort = (arg++)->AsInt32();
    controller->setUnrealServerPort(unrealServerPort);

    DEBUG("received /set_unreal_server_port %d", unrealServerPort);
    return;
  } else if (path.compare(std::string("/create/module")) == 0) {
    osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();

    std::string pluginSlug, moduleSlug;
    pluginSlug = (arg++)->AsString();
    moduleSlug = (arg++)->AsString();
    int returnId = (arg++)->AsInt32();

    controller->addModuleToCreate(pluginSlug, moduleSlug, returnId);
    DEBUG("received /create/module %s:%s", pluginSlug.c_str(), moduleSlug.c_str());
    return;
  } else if (path.compare(std::string("/destroy/module")) == 0) {
    osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();

    osc::uint64 moduleId;
    moduleId = (arg++)->AsInt64();

    controller->addModuleToDestroy(moduleId);
    DEBUG("received /destroy/module %lld", moduleId);
    return;
  } else if (path.compare(std::string("/create/cable")) == 0) {
    osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();

    osc::uint64 inputModuleId, outputModuleId;
    osc::uint32 inputPortId, outputPortId;
    inputModuleId = (arg++)->AsInt64();
    outputModuleId = (arg++)->AsInt64();
    inputPortId = (arg++)->AsInt32();
    outputPortId = (arg++)->AsInt32();

    controller->addCableToCreate(inputModuleId, outputModuleId, inputPortId, outputPortId);
    return;
  } else if (path.compare(std::string("/destroy/cable")) == 0) {
    osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();

    osc::uint64 cableId;
    cableId = (arg++)->AsInt64();

    controller->addCableToDestroy(cableId);
    return;
  } else if (path.compare(std::string("/diff/module")) == 0) {
    osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();

    osc::uint64 moduleId;
    moduleId = (arg++)->AsInt64();

    controller->addModuleToDiff(moduleId);
    DEBUG("received /diff/module %lld", moduleId);
    return;
  } else if (path.compare(std::string("/sync")) == 0) {
    controller->needsSync = true;
    return;
  } else if (path.compare(std::string("/get_menu")) == 0) {
    osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();

    VCVMenu menu;
    menu.moduleId = (arg++)->AsInt64();
    menu.id = (arg++)->AsInt32();
    menu.parentMenuId = (arg++)->AsInt32();
    menu.parentItemIndex = (arg++)->AsInt32();

    DEBUG("received /get_menu %lld", menu.moduleId);

    controller->addMenuToSync(menu);
    return;
  } else if (path.compare(std::string("/click_menu_item")) == 0) {
    osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();

    osc::uint64 moduleId;
    moduleId = (arg++)->AsInt64();

    osc::uint32 menuId, itemIndex;
    menuId = (arg++)->AsInt32();
    itemIndex = (arg++)->AsInt32();

    DEBUG("received /click_menu_item %lld", moduleId);

    controller->clickMenuItem(moduleId, menuId, itemIndex);
    return;
  } else if (path.compare(std::string("/update_menu_item_quantity")) == 0) {
    osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();

    osc::uint64 moduleId;
    moduleId = (arg++)->AsInt64();

    osc::uint32 menuId, itemIndex;
    menuId = (arg++)->AsInt32();
    itemIndex = (arg++)->AsInt32();

    float value;
    value = (arg++)->AsFloat();

    DEBUG("received /update_menu_item_quantity %lld", moduleId);

    controller->updateMenuItemQuantity(moduleId, menuId, itemIndex, value);
    return;
  } else if (path.compare(std::string("/favorite")) == 0) {
    osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();

    std::string pluginSlug, moduleSlug;
    bool favorite;

    pluginSlug = (arg++)->AsString();
    moduleSlug = (arg++)->AsString();
    favorite = (arg++)->AsBool();

    controller->setModuleFavorite(pluginSlug, moduleSlug, favorite);
    return;
  } else if (path.compare(std::string("/arrange_modules")) == 0) {
    DEBUG("received /arrange_modules");
    osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();

    osc::uint64 leftModuleId = (arg++)->AsInt64();
    osc::uint64 rightModuleId = (arg++)->AsInt64();
    bool attach = (arg++)->AsBool();

    controller->addModulesToArrange(leftModuleId, rightModuleId, attach);
  } else if (path.compare(std::string("/load_patch")) == 0) {
    osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();

    std::string patchPath = (arg++)->AsString();
    DEBUG("received /load_patch %s", patchPath.c_str());

    controller->setPatchToLoad(patchPath);
  } else if (path.compare(std::string("/autosave_and_exit")) == 0) {
    /* DEBUG("received /autosave_and_exit"); */
    controller->requestExit();
    return;
  } else if (routes.find(path) == routes.end()) {
    DEBUG("no route for %s", path.c_str());
    return;
  }

  try {
    osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();

    // potentially skip getting args if none are present
    // todo: implement basic route vs "full" route routemaps
    osc::uint64 outerId = -1;
    if (arg != message.ArgumentsEnd()) {
      outerId = (arg++)->AsInt64();
    }
    osc::uint32 innerId = -1;
    if (arg != message.ArgumentsEnd()) {
      innerId = (arg++)->AsInt32();
    }
    float value = -1.f;
    if (arg != message.ArgumentsEnd()) {
      value = (arg++)->AsFloat();
    }

    /* if (arg != message.ArgumentsEnd()) throw osc::ExcessArgumentException(); */

    RouteAction action = routes[path];
    (controller->*action)(outerId, innerId, value);
  } catch(osc::Exception& e) {
    DEBUG("Error parsing OSC message %s: %s", message.AddressPattern(), e.what());
  }
}

void OscRouter::SetController(OscController* controller) {
  this->controller = controller;
}

void OscRouter::AddRoute(std::string address, RouteAction action) {
  routes.insert(std::unordered_map<std::string, RouteAction>::value_type(address, action));
}
