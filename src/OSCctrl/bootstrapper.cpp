#include "bootstrapper.hpp"
#include <jansson.h>

Bootstrapper::Bootstrapper() {
  using namespace rack::system;

  bootstrapPatchPath = getWorkingDirectory() + "/oscctrl-bootstrap.vcv";
  /* addCtrl(bootstrapPatchPath); */
}

void Bootstrapper::addCtrl(const std::string& patchPath) {
  using namespace rack::system;

  /* std::string tempDir = rack::asset::user(getStem(patchPath)); */
  std::string tempDir = getTempDirectory() + getStem(patchPath);
  DEBUG("tempdir %s", tempDir.c_str());
  if (!createDirectories(tempDir)) {
    DEBUG("addCtrl fck");
    return;
  }

  DEBUG("extracting %s to %s", patchPath.c_str(), tempDir.c_str());

  unarchiveToDirectory(patchPath, tempDir);

  std::string jsonPath = tempDir + "/patch.json";
  FILE* file = std::fopen(jsonPath.c_str(), "r");
  if (!file)
    throw rack::Exception("could not open ctrl donor patch file %s", jsonPath.c_str());
  /* DEFER({std::fclose(file);}); */

  json_error_t error;
  json_t* rootJ = json_loadf(file, 0, &error);
  if (!rootJ)
    throw rack::Exception("failed to parse ctrl donor patch json at %s %d:%d %s", error.source, error.line, error.column, error.text);
  DEFER({json_decref(rootJ);});

  std::fclose(file);
  DEBUG("removeRecursively(tempDir) %d", removeRecursively(tempDir));
  /* DEBUG("remove(tempDir + '/modules') %d", remove(tempDir + "/modules")); */
  /* DEBUG("remove(tempDir + '/patch.json') %d", remove(tempDir + "/patch.json")); */
  /* DEBUG("remove(tempDir) %d", remove(tempDir)); */
}

void Bootstrapper::removeCtrl(const std::string& patchPath) {
}

json_t* ctrlJson() {
  return json_object();
}
