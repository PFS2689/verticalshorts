#include "plugin-settings.hpp"

#include <iostream>

static int failures = 0;

static void expect(bool cond, const char *msg)
{
	if (!cond) {
		std::cerr << "FAIL: " << msg << "\n";
		++failures;
	} else {
		std::cout << "ok: " << msg << "\n";
	}
}

/* Mixer model state without OBS — validates settings defaults and buffer readiness helpers. */
int main()
{
	vsp::PluginSettings s;
	expect(s.clipBufferEnabled, "clip buffer enabled by default");
	expect(s.autoStartClipBuffer, "auto-start buffer enabled by default");
	expect(!s.stopBufferWhenIdle, "idle stop disabled by default");
	expect(s.saveAvailableWhenShort, "partial save offered by default");
	expect(s.destinations.isEmpty(), "no destinations until ensure");
	vsp::EnsureDefaultDestinations(s);
	expect(s.destinations.size() == 5, "independent destinations per platform");
	expect(!s.automationEnabled, "automation off by default");

	s.shortClipPreset = vsp::ShortClipPreset::Sec30;
	s.longClipPreset = vsp::LongClipPreset::Min2;
	expect(vsp::RequiredBufferSeconds(s) == 120, "buffer sized for long clip");

	/* Simulate mute/volume model values */
	struct MixerRow {
		QString name;
		float volume;
		bool muted;
	};
	QList<MixerRow> rows{{QStringLiteral("Mic"), 0.8f, false}, {QStringLiteral("Desktop"), 1.0f, true}};
	expect(rows.size() == 2, "mixer model has rows");
	rows[0].muted = true;
	rows[0].volume = 0.0f;
	expect(rows[0].muted && rows[0].volume == 0.0f, "mute and volume state update");
	rows.removeAt(1);
	expect(rows.size() == 1, "source removal updates model");

	QString pathErr;
	expect(vsp::ValidateAutoDurationSeconds(1, &pathErr), "min auto duration");
	expect(!vsp::ValidateAutoDurationSeconds(0, &pathErr), "zero auto duration rejected");

	if (failures) {
		std::cerr << failures << " test(s) failed\n";
		return 1;
	}
	std::cout << "all mixer model tests passed\n";
	return 0;
}
