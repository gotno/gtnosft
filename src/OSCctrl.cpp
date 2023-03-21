#include "OSC.hpp"
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

  OscDebugPacketListener listener;
  UdpListeningReceiveSocket* RxSocket = NULL;
	std::thread oscListenerThread;

	OSCctrl() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
	}

  void onRemove(const RemoveEvent& e) override {
    DEBUG("OSCctrl onRemove");
    cleanupListener();
	}

  void startListener() {
    DEBUG("OSCctrl startListener");
    if (RxSocket != NULL) return;

		RxSocket = new UdpListeningReceiveSocket(IpEndpointName(IpEndpointName::ANY_ADDRESS, 7000), &listener);
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
				startListener();
        break;
      case OSCAction::None:
        /* DEBUG("OSCctrl doing nothing"); */
      default:
        break;
		}
    this->oscCurrentAction = OSCAction::None;
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
