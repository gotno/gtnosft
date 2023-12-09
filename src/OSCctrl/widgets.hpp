#include <rack.hpp>

namespace gtnosft {

enum LightSurrogateType {
  BOGVU,
}

struct LightSurrogate : rack::app::LightWidget {
  ParamWidget* target{nullptr};
  LightSurrogateType surrogateType{LightSurrogateType::BOGVU};

  void update() {
    switch (surrogateType) {
      case LightSurrogateType::BOGVU:
        break;
      default:
        break;
    }
  }
};

} // namespace gtnosft
