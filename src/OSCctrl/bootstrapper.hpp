#include <rack.hpp>

class json_t;

struct Bootstrapper {
  Bootstrapper();

  std::string bootstrapPatchPath;

  json_t* ctrlJson();
  void addCtrl(const std::string& patchPath);
  void removeCtrl(const std::string& patchPath);
};
