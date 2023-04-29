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

	enum OSCAction {
		None,
		Disable,
		Enable
	};
	OSCAction oscCurrentAction = OSCAction::Enable;

  OscRouter router;
  OscController controller;
  UdpListeningReceiveSocket* RxSocket = NULL;
	std::thread oscListenerThread;

  rack::dsp::ClockDivider fpsDivider;

	OSCctrl() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
	}

  void onRemove(const RemoveEvent& e) override {
    DEBUG("OSCctrl onRemove");
    cleanupListener();
	}

	/* void onDragStart(const DragStartEvent& e) override { */
	/* 	if (e.button != GLFW_MOUSE_BUTTON_LEFT) return; */

    /* syncRackModules(); */
	/* } */

  void startListener() {
    DEBUG("OSCctrl startListener");

    if (RxSocket != NULL) return;

    /* router.AddRoute("/module_received", UEModuleSynced); */

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
		switch(this->oscCurrentAction) {
      case OSCAction::Disable:
        DEBUG("disabling OSCctrl");
        cleanupListener();
        break;
      case OSCAction::Enable:
        DEBUG("enabling OSCctrl");
        DEBUG("starting Rx listener");
				startListener();

        router.SetController(&controller);
        router.AddRoute("/rx/*", &OscController::UERx);

        controller.init();

        break;
      case OSCAction::None:
        /* DEBUG("OSCctrl doing nothing"); */
        break;
      default:
        break;
		}
    this->oscCurrentAction = OSCAction::None;

    if (fpsDivider.getDivision() == 1) {
      fpsDivider.setDivision((uint32_t)(args.sampleRate / 30));
    }

    if (fpsDivider.process()) {
      controller.sendLightUpdates();
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
};

Model* modelOSCctrl = createModel<OSCctrl, OSCctrlWidget>("OSCctrl");
