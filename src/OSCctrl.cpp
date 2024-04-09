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
    startListener();

    controller.setModuleId(id);
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
    if (RxSocket != NULL) return;

    DEBUG("starting Rx listener");

    int retries{20}, listenPort{7000};

    do {
      try {
        DEBUG("OSCctrl trying startListener on port %d", listenPort);

        --retries;

        RxSocket = new UdpListeningReceiveSocket(IpEndpointName("127.0.0.1", listenPort), &router);
        oscListenerThread = std::thread(&UdpListeningReceiveSocket::Run, RxSocket);

        DEBUG("OSCctrl startListener success on port %d", listenPort);
        break;
      } catch (std::runtime_error& err) {
        if (RxSocket != NULL) cleanupListener();
        if (retries < 0) throw err;
        ++listenPort;
      }
    } while (retries >= 0);

    controller.setListenPort(listenPort);
	}

  void cleanupListener() {
    DEBUG("OSCctrl cleanupListener");
    if (RxSocket == NULL) return;

    RxSocket->AsynchronousBreak();

    if (oscListenerThread.joinable()) {
      oscListenerThread.join();
    }

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
      controller.processMenuQuantityUpdates();
      controller.processModuleDiffs();
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

    if (ctrl.readyToExit) {
      ctrl.autosaveAndExit();
      return;
    }

    if (ctrl.needsSync) ctrl.collectAndSync();
    ctrl.processCableUpdates();
    ctrl.processModuleUpdates();
    ctrl.processMenuRequests();
    ctrl.processMenuClicks();
  }
};

Model* modelOSCctrl = createModel<OSCctrl, OSCctrlWidget>("OSCctrl");
