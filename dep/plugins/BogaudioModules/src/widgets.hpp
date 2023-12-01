#pragma once

#include "rack.hpp"
#include <functional>
#include <string>

using namespace rack;

extern Plugin *pluginInstance;

namespace bogaudio {

template <class BASE>
struct LightEmittingWidget : BASE {
	virtual bool isLit() = 0;

	void drawLayer(const typename BASE::DrawArgs& args, int layer) override {
		if (layer == 1 && isLit()) {
			drawLit(args);
		}
		BASE::drawLayer(args, layer);
	}

	virtual void drawLit(const typename BASE::DrawArgs& args) {}
};

struct StatefulButton : ParamWidget {
	std::vector<std::shared_ptr<Svg>> _frames;
	SvgWidget* _svgWidget; // deleted elsewhere.
	CircularShadow* shadow = NULL;

	StatefulButton(const char* offSvgPath, const char* onSvgPath);
	void onDragStart(const event::DragStart& e) override;
	void onDragEnd(const event::DragEnd& e) override;
	void onDoubleClick(const event::DoubleClick& e) override {}
};

struct VUSlider : LightEmittingWidget<SliderKnob> {
	const float slideHeight = 13.0f;
	float* _vuLevel = NULL;
	float* _stereoVuLevel = NULL;

	VUSlider(float height = 183.0f) {
		box.size = Vec(18.0f, height);
	}

	inline void setVULevel(float* level) {
		_vuLevel = level;
	}
	inline void setStereoVULevel(float* level) {
		_stereoVuLevel = level;
	}
	bool isLit() override;
	void draw(const DrawArgs& args) override;
	void drawLit(const DrawArgs& args) override;
	void drawTranslate(const DrawArgs& args);
};

} // namespace bogaudio
