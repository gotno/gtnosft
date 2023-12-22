#include "plugin.hpp"

#include "OscRouter.hpp"
#include "OscController.hpp"

#include "../dep/oscpack/ip/UdpSocket.h"

#include <thread>

struct OSCctrl : Module {
	enum ParamId {
		PARAMS_LEN
	};
	enum InputId {
		INPUTS_LEN
	};
	enum OutputId {
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};


  OscRouter router;
  OscController controller;
  UdpListeningReceiveSocket* RxSocket = NULL;
	std::thread oscListenerThread;

  rack::dsp::ClockDivider fpsDivider;

	OSCctrl() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
	}

  void onAdd(const AddEvent& e) override {
    DEBUG("enabling OSCctrl");
    DEBUG("starting Rx listener");
    startListener();

    router.SetController(&controller);

    router.AddRoute("/rx/module", &OscController::rxModule);
    router.AddRoute("/rx/cable", &OscController::rxCable);
    router.AddRoute("/update/param", &OscController::updateParam);
  }

  void onRemove(const RemoveEvent& e) override {
    DEBUG("OSCctrl onRemove");
    cleanupListener();
	}

  void startListener() {
    DEBUG("OSCctrl startListener");
    if (RxSocket != NULL) return;

		RxSocket = new UdpListeningReceiveSocket(IpEndpointName(IpEndpointName::ANY_ADDRESS, 7000), &router);
		oscListenerThread = std::thread(&UdpListeningReceiveSocket::Run, RxSocket);
	}

  void cleanupListener() {
    DEBUG("OSCctrl cleanupListener");
    if (RxSocket == NULL) return;

		RxSocket->AsynchronousBreak();
		oscListenerThread.join();
		delete RxSocket;
		RxSocket = NULL;
	}

	void process(const ProcessArgs& args) override {
    if (fpsDivider.getDivision() == 1) {
      fpsDivider.setDivision((uint32_t)(args.sampleRate / 60));
    }

    if (fpsDivider.process() && !controller.needsSync) {
      controller.enqueueLightUpdates();
      controller.processParamUpdates();
    }
	}
};


struct OSCctrlWidget : ModuleWidget {
	OSCctrlWidget(OSCctrl* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/OSCctrl.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
	}

  void step() override {
    ModuleWidget::step();
    if (!getModule()) return;

    OscController& ctrl = dynamic_cast<OSCctrl*>(getModule())->controller;

    if (ctrl.needsSync) ctrl.collectAndSync();
    ctrl.processCableUpdates();
    ctrl.processModuleUpdates();
    ctrl.processMenuRequests();
    ctrl.processMenuClicks();
  }
};

Model* modelOSCctrl = createModel<OSCctrl, OSCctrlWidget>("OSCctrl");
