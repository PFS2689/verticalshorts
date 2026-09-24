#include "plugin-settings.hpp"

#include <cassert>
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

int main()
{
	QString err;
	QString warn;
	expect(vsp::ValidateCanvasSize(1080, 1920, &err), "1080x1920 valid");
	expect(!vsp::ValidateCanvasSize(0, 1920, &err), "zero width invalid");
	expect(!vsp::ValidateCanvasSize(1080, 100, &err), "too small height invalid");
	expect(!vsp::ValidateCanvasSize(9000, 9000, &err), "too large invalid");

	expect(vsp::ValidateShortClipSeconds(30, &err), "30s short clip valid");
	expect(!vsp::ValidateShortClipSeconds(0, &err), "0s short clip invalid");
	expect(!vsp::ValidateShortClipSeconds(-5, &err), "negative short clip invalid");
	expect(!vsp::ValidateShortClipSeconds(vsp::kMaxClipBufferSeconds + 1, &err), "short clip over max invalid");

	expect(vsp::ValidateLongClipSeconds(120, &err, &warn), "2min long clip valid");
	expect(vsp::ValidateLongClipSeconds(300, &err, &warn), "5min long clip valid");
	expect(!warn.isEmpty(), "5min long clip warns about resources");
	expect(!vsp::ValidateLongClipSeconds(0, &err), "0s long clip invalid");
	expect(!vsp::ValidateLongClipSeconds(vsp::kMaxClipBufferSeconds + 1, &err), "long clip over max invalid");

	expect(vsp::ParseMmSs(QStringLiteral("2:30"), &err) == 150, "parse 2:30");
	expect(vsp::ParseMmSs(QStringLiteral("0:00"), &err) < 0, "reject 0:00");
	expect(vsp::ParseMmSs(QStringLiteral("ab:cd"), &err) < 0, "reject letters");
	expect(vsp::ParseMmSs(QStringLiteral("1:60"), &err) < 0, "reject bad seconds");
	expect(vsp::ParseMmSs(QStringLiteral("90"), &err) == 90, "parse total seconds");

	expect(vsp::ValidateAutoDurationSeconds(60, &err), "auto duration ok");
	expect(!vsp::ValidateAutoDurationSeconds(0, &err), "auto duration zero invalid");

	expect(vsp::IsPortrait(1080, 1920), "portrait");
	expect(!vsp::IsPortrait(1920, 1080), "landscape not portrait");

	uint32_t w = 0, h = 0;
	vsp::CanvasSizeForPreset(vsp::CanvasPreset::YouTubeVertical, 1, 1, w, h);
	expect(w == 1080 && h == 1920, "youtube preset size");
	vsp::CanvasSizeForPreset(vsp::CanvasPreset::Custom, 720, 1280, w, h);
	expect(w == 720 && h == 1280, "custom preset size");

	vsp::PluginSettings s;
	s.shortClipPreset = vsp::ShortClipPreset::Sec60;
	s.longClipPreset = vsp::LongClipPreset::Min3;
	expect(vsp::EffectiveShortClipSeconds(s) == 60, "short preset seconds");
	expect(vsp::EffectiveLongClipSeconds(s) == 180, "long preset seconds");
	expect(vsp::RequiredBufferSeconds(s) == 180, "buffer sized to long clip");

	s.shortClipPreset = vsp::ShortClipPreset::Custom;
	s.customShortClipSeconds = 42;
	s.longClipPreset = vsp::LongClipPreset::Custom;
	s.customLongClipSeconds = 150;
	expect(vsp::EffectiveShortClipSeconds(s) == 42, "custom short seconds");
	expect(vsp::EffectiveLongClipSeconds(s) == 150, "custom long seconds");
	expect(vsp::EffectiveShortClipSeconds(s) != vsp::EffectiveLongClipSeconds(s),
	       "short and long durations independent");

	expect(!s.automationEnabled, "automation disabled by default");
	expect(s.autoStartClipBuffer, "auto start buffer default on");
	expect(s.destinations.isEmpty(), "destinations empty until EnsureDefault");
	vsp::EnsureDefaultDestinations(s);
	expect(s.destinations.size() == 5, "five independent platform destinations");
	expect(!s.activeDestinationId.isEmpty(), "active destination set");
	expect(vsp::ActiveDestination(s).platform == vsp::StreamPlatform::YouTube, "default active youtube");
	/* Independent destination model — never inherits main OBS */
	expect(s.destinations[0].streamKey.isEmpty(), "keys not prefilled from main OBS");

	QString pathErr;
	/* recording path validation helper coverage via DefaultRecordingPath emptiness in test mode */
	expect(vsp::DefaultRecordingPath().isEmpty(), "test build has empty default path");
	expect(vsp::ValidateRecordingPath(QString(), &pathErr), "empty recording path ok");
	expect(vsp::ValidateRecordingPath(QStringLiteral("/tmp/vertical-recordings"), &pathErr), "normal path ok");
	expect(!vsp::ValidateRecordingPath(QStringLiteral("\\\\.\\PhysicalDrive0"), &pathErr),
	       "device path rejected");

	expect(vsp::kConfigSchemaVersion == 1, "config schema version is 1");
	expect(vsp::ClassifyConfigSchema(0) == vsp::ConfigSchemaAction::MigrateForward,
	       "schema 0 migrates forward");
	expect(vsp::ClassifyConfigSchema(1) == vsp::ConfigSchemaAction::None, "schema 1 is current");
	expect(vsp::ClassifyConfigSchema(2) == vsp::ConfigSchemaAction::RefuseDowngrade,
	       "newer schema refuses downgrade");
	expect(vsp::ClassifyConfigSchema(1, 2) == vsp::ConfigSchemaAction::MigrateForward,
	       "plugin schema bump migrates");

	if (failures) {
		std::cerr << failures << " test(s) failed\n";
		return 1;
	}
	std::cout << "all settings tests passed\n";
	return 0;
}
