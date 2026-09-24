#include "stream-destination.hpp"
#include "plugin-settings.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
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
	/* Masking / logging safety */
	expect(vsp::MaskSecret(QStringLiteral("abcdefghij")) == QStringLiteral("****"), "default mask fully hides secret");
	expect(vsp::MaskSecret(QStringLiteral("abcdefghij"), 4) == QStringLiteral("****ghij"), "optional keepTail");
	expect(vsp::MaskSecret(QStringLiteral("ab")) == QStringLiteral("****"), "short secret fully masked");
	expect(vsp::MaskSecret(QString()).contains(QStringLiteral("empty")), "empty secret labeled");
	expect(!vsp::MaskSecret(QStringLiteral("super-secret-key-xyz")).contains(QStringLiteral("super-secret")),
	       "masked key never shows full secret");
	expect(!vsp::SanitizeUserFacingError(QStringLiteral("fail rtmps://user:pass@host/app key=abc"))
			.contains(QStringLiteral("pass")),
	       "user-facing errors strip credentials");

	const QString sanitized =
		vsp::SanitizeUrlForLog(QStringLiteral("rtmp://user:pass@ingest.example/app?token=1"));
	expect(!sanitized.contains(QStringLiteral("pass")), "password stripped from URL log");
	expect(!sanitized.contains(QStringLiteral("token=1")), "query stripped from URL log");
	expect(vsp::HostnameOnly(QStringLiteral("rtmps://a.rtmp.youtube.com/live2")) ==
		       QStringLiteral("a.rtmp.youtube.com"),
	       "hostname extracted");

	/* Platform presets */
	const auto platforms = {vsp::StreamPlatform::YouTube, vsp::StreamPlatform::Twitch, vsp::StreamPlatform::TikTok,
				vsp::StreamPlatform::Instagram, vsp::StreamPlatform::CustomRtmp};
	for (auto p : platforms) {
		expect(!vsp::PlatformDisplayName(p).isEmpty(), "platform display name present");
		expect(vsp::PlatformIconResource(p).startsWith(QStringLiteral(":/vsp/platforms/")),
		       "platform icon resource path");
		expect(vsp::PlatformIconResource(p).endsWith(QStringLiteral(".svg")), "platform icon is svg");
	}

	/* Bundled logo assets on disk (release package check helper).
	 * Resolve relative to common locations because ctest may run from the build dir. */
	QStringList iconRoots;
#ifdef VSP_SOURCE_DIR
	iconRoots << QDir(QStringLiteral(VSP_SOURCE_DIR)).filePath(QStringLiteral("data/icons/platforms"));
#endif
	iconRoots << QStringLiteral("data/icons/platforms") << QStringLiteral("../data/icons/platforms")
		  << QStringLiteral("../../data/icons/platforms");
	QString iconDir;
	for (const QString &root : iconRoots) {
		if (QFile::exists(QDir(root).filePath(QStringLiteral("youtube.svg")))) {
			iconDir = root;
			break;
		}
	}
	expect(!iconDir.isEmpty(), "found bundled platform SVG directory");
	const char *names[] = {"youtube.svg",         "twitch.svg",   "tiktok.svg",
			       "tiktok-dark.svg",     "instagram.svg", "instagram-dark.svg",
			       "custom-rtmp.svg",     "placeholder.svg"};
	for (const char *n : names) {
		const QString path = QDir(iconDir).filePath(QString::fromUtf8(n));
		expect(QFile::exists(path), qPrintable(QStringLiteral("icon exists: %1").arg(n)));
		if (QFile::exists(path))
			expect(QFileInfo(path).size() > 0, qPrintable(QStringLiteral("icon non-empty: %1").arg(n)));
	}

	/* Missing logo fallback path still returns custom icon */
	expect(vsp::PlatformIconResource(static_cast<vsp::StreamPlatform>(99))
		       .contains(QStringLiteral("placeholder")),
	       "unknown platform falls back to placeholder icon");

	/* URL / protocol validation */
	QString err, field;
	expect(vsp::ValidateServerUrl(QStringLiteral("rtmps://live.example/app"), &err), "rtmps valid");
	expect(vsp::ValidateServerUrl(QStringLiteral("rtmp://live.example/app"), &err), "rtmp valid");
	expect(!vsp::ValidateServerUrl(QStringLiteral("https://example.com"), &err), "https unsupported");
	expect(!vsp::ValidateServerUrl(QStringLiteral("not-a-url"), &err), "malformed rejected");
	expect(!vsp::ValidateServerUrl(QString(), &err), "empty server rejected");

	vsp::StreamDestination yt = vsp::MakeDefaultDestination(vsp::StreamPlatform::YouTube);
	expect(yt.server == vsp::SuggestedYouTubeServer(), "youtube suggested rtmps server");
	expect(!vsp::ValidateDestination(yt, &err, &field), "youtube without key invalid");
	expect(field == QStringLiteral("key"), "missing key field highlighted");
	yt.streamKey = QStringLiteral("vertical-only-key");
	expect(vsp::ValidateDestination(yt, &err, &field), "youtube with key valid");

	vsp::StreamDestination tw = vsp::MakeDefaultDestination(vsp::StreamPlatform::Twitch);
	expect(!tw.server.isEmpty(), "twitch has ingest preset");
	expect(!vsp::TwitchIngestOptions().isEmpty(), "twitch ingest list present");
	tw.streamKey = QStringLiteral("live_xxxxxxxx");
	expect(vsp::ValidateDestination(tw, &err), "twitch dest valid");

	vsp::StreamDestination tt = vsp::MakeDefaultDestination(vsp::StreamPlatform::TikTok);
	expect(tt.server.isEmpty(), "tiktok does not invent universal URL");
	tt.server = QStringLiteral("rtmps://user-provided.tiktok.example/live");
	tt.streamKey = QStringLiteral("user-key");
	expect(vsp::ValidateDestination(tt, &err), "tiktok with user URL valid");

	vsp::StreamDestination ig = vsp::MakeDefaultDestination(vsp::StreamPlatform::Instagram);
	expect(ig.server.isEmpty(), "instagram does not invent undocumented endpoint");
	ig.server = QStringLiteral("rtmps://user-provided.instagram.example/rtmp");
	ig.streamKey = QStringLiteral("user-key");
	expect(vsp::ValidateDestination(ig, &err), "instagram with user URL valid");

	vsp::StreamDestination custom = vsp::MakeDefaultDestination(vsp::StreamPlatform::CustomRtmp);
	custom.server = QStringLiteral("rtmps://private.example/live");
	custom.streamKey = QStringLiteral("k");
	custom.username = QStringLiteral("u");
	custom.password = QStringLiteral("p");
	expect(vsp::ValidateDestination(custom, &err), "custom rtmp valid");

	/* Separate credentials per platform / switching */
	vsp::PluginSettings s;
	vsp::EnsureDefaultDestinations(s);
	expect(s.destinations.size() == 5, "default one destination per platform");
	s.destinations[0].streamKey = QStringLiteral("yt-key");
	s.destinations[1].streamKey = QStringLiteral("tw-key");
	expect(s.destinations[0].streamKey != s.destinations[1].streamKey, "separate keys per platform");
	s.activeDestinationId = s.destinations[1].id;
	expect(vsp::ActiveDestination(s).streamKey == QStringLiteral("tw-key"), "switching loads platform key");
	s.activeDestinationId = s.destinations[0].id;
	expect(vsp::ActiveDestination(s).streamKey == QStringLiteral("yt-key"), "switch back restores youtube key");
	expect(vsp::ActiveDestination(s).streamKey != QStringLiteral("tw-key"), "keys not copied across platforms");

	/* Status helpers never expose full key */
	expect(vsp::VerticalLiveStatusLabel(vsp::VerticalLiveStatus::Connecting) == QStringLiteral("Connecting"),
	       "connecting label");
	expect(vsp::VerticalLiveStatusLabel(vsp::VerticalLiveStatus::Live) == QStringLiteral("Live"), "live label");
	expect(vsp::VerticalLiveStatusLabel(vsp::VerticalLiveStatus::Offline) == QStringLiteral("Offline"),
	       "offline label");

	if (failures) {
		std::cerr << failures << " test(s) failed\n";
		return 1;
	}
	std::cout << "all stream destination tests passed\n";
	return 0;
}
