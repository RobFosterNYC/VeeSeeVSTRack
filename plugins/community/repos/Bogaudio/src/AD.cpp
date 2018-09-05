
#include "AD.hpp"

void AD::onReset() {
	_trigger.reset();
	_eocPulseGen.process(10.0);
	_envelope.reset();
	_modulationStep = modulationSteps;
	_on = false;
}

void AD::onSampleRateChange() {
	float sr = engineGetSampleRate();
	_envelope.setSampleRate(sr);
	_attackSL.setParams(sr / (float)modulationSteps);
	_decaySL.setParams(sr / (float)modulationSteps);
	_modulationStep = modulationSteps;
}

void AD::step() {
	lights[LOOP_LIGHT].value = _loopMode = params[LOOP_PARAM].value > 0.5f;
	lights[LINEAR_LIGHT].value = _linearMode = params[LINEAR_PARAM].value > 0.5f;
	if (!(outputs[ENV_OUTPUT].active || outputs[EOC_OUTPUT].active || inputs[TRIGGER_INPUT].active)) {
		return;
	}

	++_modulationStep;
	if (_modulationStep >= modulationSteps) {
		_modulationStep = 0;

		float attack = powf(params[ATTACK_PARAM].value, 2.0f);
		if (inputs[ATTACK_INPUT].active) {
			attack *= clamp(inputs[ATTACK_INPUT].value / 10.0f, 0.0f, 1.0f);
		}
		_envelope.setAttack(_attackSL.next(attack * 10.f));

		float decay = powf(params[DECAY_PARAM].value, 2.0f);
		if (inputs[DECAY_INPUT].active) {
			decay *= clamp(inputs[DECAY_INPUT].value / 10.0f, 0.0f, 1.0f);
		}
		_envelope.setDecay(_decaySL.next(decay * 10.f));

		_envelope.setLinearShape(_linearMode);
	}

	_trigger.process(inputs[TRIGGER_INPUT].value);
	if (!_on && (_trigger.isHigh() || (_loopMode && _envelope.isStage(ADSR::STOPPED_STAGE)))) {
		_on = true;
	}
	_envelope.setGate(_on);
	outputs[ENV_OUTPUT].value = _envelope.next() * 10.0f;
	if (_on && _envelope.isStage(ADSR::SUSTAIN_STAGE)) {
		_envelope.reset();
		_on = false;
		_eocPulseGen.trigger(0.001f);
	}
	outputs[EOC_OUTPUT].value = _eocPulseGen.process(engineGetSampleTime()) ? 5.0f : 0.0f;

	lights[ATTACK_LIGHT].value = _envelope.isStage(ADSR::ATTACK_STAGE);
	lights[DECAY_LIGHT].value = _envelope.isStage(ADSR::DECAY_STAGE);
}

struct ADWidget : ModuleWidget {
	static constexpr int hp = 3;

	ADWidget(AD* module) : ModuleWidget(module) {
		box.size = Vec(RACK_GRID_WIDTH * hp, RACK_GRID_HEIGHT);

		{
			SVGPanel *panel = new SVGPanel();
			panel->box.size = box.size;
			panel->setBackground(SVG::load(assetPlugin(plugin, "res/AD.svg")));
			addChild(panel);
		}

		addChild(Widget::create<ScrewSilver>(Vec(0, 0)));
		addChild(Widget::create<ScrewSilver>(Vec(box.size.x - 15, 365)));

		// generated by svg_widgets.rb
		auto attackParamPosition = Vec(8.0, 33.0);
		auto decayParamPosition = Vec(8.0, 90.0);
		auto loopParamPosition = Vec(33.5, 131.2);
		auto linearParamPosition = Vec(33.5, 145.7);

		auto triggerInputPosition = Vec(10.5, 163.5);
		auto attackInputPosition = Vec(10.5, 198.5);
		auto decayInputPosition = Vec(10.5, 233.5);

		auto envOutputPosition = Vec(10.5, 271.5);
		auto eocOutputPosition = Vec(10.5, 306.5);

		auto attackLightPosition = Vec(20.8, 65.0);
		auto decayLightPosition = Vec(20.8, 122.0);
		auto loopLightPosition = Vec(2.5, 132.5);
		auto linearLightPosition = Vec(2.5, 147.0);
		// end generated by svg_widgets.rb

		addParam(ParamWidget::create<Knob29>(attackParamPosition, module, AD::ATTACK_PARAM, 0.0, 1.0, 0.12));
		addParam(ParamWidget::create<Knob29>(decayParamPosition, module, AD::DECAY_PARAM, 0.0, 1.0, 0.31623));
		addParam(ParamWidget::create<StatefulButton9>(loopParamPosition, module, AD::LOOP_PARAM, 0.0, 1.0, 0.0));
		addParam(ParamWidget::create<StatefulButton9>(linearParamPosition, module, AD::LINEAR_PARAM, 0.0, 1.0, 0.0));

		addInput(Port::create<Port24>(triggerInputPosition, Port::INPUT, module, AD::TRIGGER_INPUT));
		addInput(Port::create<Port24>(attackInputPosition, Port::INPUT, module, AD::ATTACK_INPUT));
		addInput(Port::create<Port24>(decayInputPosition, Port::INPUT, module, AD::DECAY_INPUT));

		addOutput(Port::create<Port24>(envOutputPosition, Port::OUTPUT, module, AD::ENV_OUTPUT));
		addOutput(Port::create<Port24>(eocOutputPosition, Port::OUTPUT, module, AD::EOC_OUTPUT));

		addChild(ModuleLightWidget::create<TinyLight<GreenLight>>(attackLightPosition, module, AD::ATTACK_LIGHT));
		addChild(ModuleLightWidget::create<TinyLight<GreenLight>>(decayLightPosition, module, AD::DECAY_LIGHT));
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(loopLightPosition, module, AD::LOOP_LIGHT));
		addChild(ModuleLightWidget::create<SmallLight<GreenLight>>(linearLightPosition, module, AD::LINEAR_LIGHT));
	}
};

RACK_PLUGIN_MODEL_INIT(Bogaudio, AD) {
   Model* modelAD = createModel<AD, ADWidget>("Bogaudio-AD", "AD", "utility envelope", ENVELOPE_GENERATOR_TAG);
   return modelAD;
}
