#include "bootstrapper.hpp"
#include <jansson.h>

Bootstrapper::Bootstrapper() {
}

void Bootstrapper::addCtrl(const std::string& patchArchivePath) {
  using namespace rack::system;

  std::string patchTempDir = extractPatch(patchArchivePath);
  std::string jsonPath =  patchTempDir + "/patch.json";

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

  std::string modelName;
  /* std::vector<int64_t> moduleIds; */

  size_t index;
  json_t *value;
  json_array_foreach(modulesJ, index, value) {
    /* moduleIds.push_back(json_integer_value(json_object_get(value, "id"))); */

    modelName =
      std::string(json_string_value(json_object_get(value, "model")));
    if (modelName == "OSCctrl") {
      std::fclose(file);
      removeRecursively(patchTempDir);
      DEBUG("OSCctrl already exists, aborting bootstrap");
      return;
    }
  }

  // TODO: address unlikely id collision, recursify?
  /* int64_t ctrlId = json_integer_value(json_object_get(getCtrlJson(), "id")); */
  /* for (int64_t& moduleId : moduleIds) { */
  /*   if (moduleId == ctrlId) ctrlId++; */
  /* } */
  json_array_append(modulesJ, getCtrlJson());
  json_dump_file(rootJ, jsonPath.c_str(), JSON_INDENT(2));

  archiveDirectory(patchArchivePath, patchTempDir, 1);

  std::fclose(file);
  removeRecursively(patchTempDir);
}

void Bootstrapper::removeCtrl(const std::string& patchArchivePath) {
  using namespace rack::system;

  std::string patchTempDir = extractPatch(patchArchivePath);
  std::string jsonPath =  patchTempDir + "/patch.json";

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

  std::string modelName;

  int64_t ctrlIndex{-1};
  size_t index;
  json_t *value;
  json_array_foreach(modulesJ, index, value) {
    modelName =
      std::string(json_string_value(json_object_get(value, "model")));
    if (modelName == "OSCctrl") {
      ctrlIndex = index;
      break;
    }
  }

  if (ctrlIndex != -1) {
    json_array_remove(modulesJ, ctrlIndex);
    json_dump_file(rootJ, jsonPath.c_str(), JSON_INDENT(2));
    archiveDirectory(patchArchivePath, patchTempDir, 1);
  }

  std::fclose(file);
  removeRecursively(patchTempDir);
}

std::string Bootstrapper::extractPatch(const std::string& patchArchivePath) {
  using namespace rack::system;

  /* std::string tempDir = rack::asset::user(getStem(patchArchivePath)); */
  std::string tempDir = getTempDirectory() + getStem(patchArchivePath);
  removeRecursively(tempDir);
  if (!createDirectories(tempDir)) {
    DEBUG("bootstrapper extractPatch: unable to create tempdir: %s", tempDir.c_str());
    return std::string("");
  }

  DEBUG("extracting %s to %s", patchArchivePath.c_str(), tempDir.c_str());

  unarchiveToDirectory(patchArchivePath, tempDir);

  return tempDir;
}

json_t* Bootstrapper::getCtrlJson() {
  if (ctrlJson) return ctrlJson;

  using namespace rack::system;

  std::string bootstrapPatchPath =
    getWorkingDirectory() + "/oscctrl-bootstrap.vcv";

  std::string tempDir = extractPatch(bootstrapPatchPath);
  std::string jsonPath =  tempDir + "/patch.json";

  FILE* file = std::fopen(jsonPath.c_str(), "r");
  if (!file) {
    FATAL("could not open OSCctrl donor patch file %s", jsonPath.c_str());
    return nullptr;
  }

  json_error_t error;
  json_t* rootJ = json_loadf(file, 0, &error);
  if (!rootJ) {
    FATAL("failed to parse OSCctrl donor patch json at %s %d:%d %s", error.source, error.line, error.column, error.text);
    return nullptr;
  }
  DEFER({ json_decref(rootJ); });

  json_t* modulesJ = json_object_get(rootJ, "modules");
  DEFER({ json_decref(modulesJ); });

  size_t index;
  json_t *value;
  std::string modelName;

  json_array_foreach(modulesJ, index, value) {
    modelName =
      std::string(json_string_value(json_object_get(value, "model")));
    if (modelName == "OSCctrl") {
      ctrlJson = json_deep_copy(value);
      break;
    }
  }

  std::fclose(file);
  removeRecursively(tempDir);

  return ctrlJson;
}
