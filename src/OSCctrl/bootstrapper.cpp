#include "bootstrapper.hpp"
#include <jansson.h>

Bootstrapper::Bootstrapper() {
  getCtrlJson();

  /* addCtrl(); */
}

void Bootstrapper::addCtrl(const std::string& patchPath) {
  /* std::string jsonPath = tempDir + "/patch.json"; */

  /* std::fclose(file); */
  /* DEBUG("removeRecursively(tempDir) %d", removeRecursively(tempDir)); */
  /* DEBUG("remove(tempDir + '/modules') %d", remove(tempDir + "/modules")); */
  /* DEBUG("remove(tempDir + '/patch.json') %d", remove(tempDir + "/patch.json")); */
  /* DEBUG("remove(tempDir) %d", remove(tempDir)); */
}

void Bootstrapper::removeCtrl(const std::string& patchPath) {
}

std::string Bootstrapper::extractPatch(const std::string& patchPath) {
  using namespace rack::system;

  /* std::string tempDir = rack::asset::user(getStem(patchPath)); */
  std::string tempDir = getTempDirectory() + getStem(patchPath);
  if (!createDirectories(tempDir)) {
    DEBUG("bootstrapper extractPatch: unable to create tempdir: %s", tempDir.c_str());
    return std::string("");
  }

  DEBUG("extracting %s to %s", patchPath.c_str(), tempDir.c_str());

  unarchiveToDirectory(patchPath, tempDir);

  return tempDir;
}

void Bootstrapper::getCtrlJson() {
  using namespace rack::system;

  std::string bootstrapPatchPath =
    getWorkingDirectory() + "/oscctrl-bootstrap.vcv";

  std::string tempDir = extractPatch(bootstrapPatchPath);
  std::string jsonPath =  tempDir + "/patch.json";

  FILE* file = std::fopen(jsonPath.c_str(), "r");
  if (!file) {
    FATAL("could not open OSCctrl donor patch file %s", jsonPath.c_str());
    return;
  }

  json_error_t error;
  json_t* rootJ = json_loadf(file, 0, &error);
  if (!rootJ) {
    FATAL("failed to parse OSCctrl donor patch json at %s %d:%d %s", error.source, error.line, error.column, error.text);
    return;
  }
  DEFER({ json_decref(rootJ); });

  json_t* modulesJ = json_object_get(rootJ, "modules");
  DEFER({ json_decref(modulesJ); });

  size_t index;
  json_t *value;

  json_array_foreach(modulesJ, index, value) {
    std::string modelName(json_string_value(json_object_get(value, "model")));
    if (modelName == "OSCctrl") {
      ctrlJson = json_deep_copy(value);
      break;
    }
  }

  std::fclose(file);
  removeRecursively(tempDir);
}
