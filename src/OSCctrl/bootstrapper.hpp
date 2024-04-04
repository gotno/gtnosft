#include <rack.hpp>

class json_t;

struct Bootstrapper {
  Bootstrapper();

  void addCtrl(const std::string& patchPath);
  void removeCtrl(const std::string& patchPath);

private:
  json_t* ctrlJson{nullptr};
  json_t* getCtrlJson();
  std::string extractPatch(const std::string& patchPath);
};
