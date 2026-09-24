#pragma once

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <cstdint>

#include "stream-destination.hpp"

#ifdef VSP_SETTINGS_TEST
/* Test build: avoid linking OBS */
#else
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>
#include <util/config-file.h>
#endif

namespace vsp {

/* Safety ceiling for the shared vertical clip buffer (15 minutes). */
constexpr int kMaxClipBufferSeconds = 900;
constexpr int kMemoryWarnClipSeconds = 300; /* warn when custom exceeds 5 minutes */

/*
 * Configuration schema version — independent from the plugin product version.
 *
 * Example: plugin 1.0.5 may ship schema 1; a future 2.0.0 may require schema 2.
 * Bump kConfigSchemaVersion only when the persisted settings shape changes and
 * a forward migration is required. Never auto-downgrade a newer schema.
 */
constexpr int kConfigSchemaVersion = 1;

enum class ConfigSchemaAction {
	None = 0,
	MigrateForward,
	RefuseDowngrade,
};

inline ConfigSchemaAction ClassifyConfigSchema(int storedSchema,
					       int pluginSchema = kConfigSchemaVersion)
{
	if (storedSchema < 0)
		storedSchema = 0;
	if (storedSchema < pluginSchema)
		return ConfigSchemaAction::MigrateForward;
	if (storedSchema > pluginSchema)
		return ConfigSchemaAction::RefuseDowngrade;
	return ConfigSchemaAction::None;
}

#ifdef VSP_SETTINGS_TEST
inline int ReadConfigSchema(void *)
{
	return 0;
}
#else
inline int ReadConfigSchema(obs_data_t *data)
{
	if (!data)
		return 0;
	if (!obs_data_has_user_value(data, "config_schema"))
		return 0; /* pre-schema installs */
	return (int)obs_data_get_int(data, "config_schema");
}
#endif

enum class CanvasPreset {
	YouTubeVertical = 0,
	TikTokVertical = 1,
	TwitchVertical = 2,
	InstagramVertical = 3,
	Custom = 4,
};

enum class ShortClipPreset { Sec10 = 10, Sec20 = 20, Sec30 = 30, Sec60 = 60, Custom = 0 };
enum class LongClipPreset { Min2 = 120, Min3 = 180, Min4 = 240, Min5 = 300, Custom = 0 };

enum class ScheduleRepeat { Once = 0, Daily = 1, Weekly = 2, Weekdays = 3 };

enum class AutomationStatus {
	Disabled = 0,
	Waiting,
	Scheduled,
	Starting,
	Recording,
	Stopping,
	Completed,
	Error,
};

inline QString AutomationStatusLabel(AutomationStatus s)
{
	switch (s) {
	case AutomationStatus::Disabled:
		return QStringLiteral("Automation disabled");
	case AutomationStatus::Waiting:
		return QStringLiteral("Waiting for trigger");
	case AutomationStatus::Scheduled:
		return QStringLiteral("Scheduled");
	case AutomationStatus::Starting:
		return QStringLiteral("Starting");
	case AutomationStatus::Recording:
		return QStringLiteral("Recording");
	case AutomationStatus::Stopping:
		return QStringLiteral("Stopping");
	case AutomationStatus::Completed:
		return QStringLiteral("Completed");
	case AutomationStatus::Error:
		return QStringLiteral("Error");
	}
	return QStringLiteral("Unknown");
}

struct PluginSettings {
	CanvasPreset canvasPreset = CanvasPreset::YouTubeVertical;
	uint32_t customWidth = 1080;
	uint32_t customHeight = 1920;

	/* Short clip (📸) — independent from long clip */
	ShortClipPreset shortClipPreset = ShortClipPreset::Sec30;
	int customShortClipSeconds = 45;

	/* Long clip (📷) — independent from short clip */
	LongClipPreset longClipPreset = LongClipPreset::Min2;
	int customLongClipSeconds = 150; /* 2:30 default custom */

	QString recordingPath; /* empty => use OBS default when available */
	bool clipBufferEnabled = true;

	/* Clip buffer readiness / lifecycle */
	bool autoStartClipBuffer = true;      /* start when dock ready / OBS loaded */
	bool stopBufferWhenIdle = false;      /* optional idle stop */
	int bufferIdleTimeoutSeconds = 300;   /* 5 minutes */
	bool saveAvailableWhenShort = true;   /* offer partial save (UI default) */
	bool bufferStartOnVerticalLive = true;
	bool bufferStartOnVerticalRecord = true;

	/* Vertical Streaming — completely independent destination (never inherits main OBS). */
	QString activeDestinationId;
	QList<StreamDestination> destinations; /* secrets loaded into memory separately */

	/* Vertical Recording Automation (disabled by default) */
	bool automationEnabled = false;

	bool autoStartOnMainStream = false;
	bool autoStartOnScene = false;
	bool autoStartOnObsStart = false;
	bool autoStartOnSchedule = false;
	bool autoStartOnCountdown = false;
	bool autoStartOnVerticalLive = false;

	bool autoStopOnMainStreamStop = false;
	bool autoStopOnSceneInactive = false;
	bool autoStopOnDuration = false;
	bool autoStopOnScheduleEnd = false;
	bool autoStopOnVerticalLiveStop = false;
	bool autoStopOnObsShutdown = true;

	QString triggerSceneUuid;
	QString triggerSceneName;

	int autoRecordDurationSeconds = 3600; /* 1 hour default when duration stop enabled */
	int countdownSeconds = 60;

	/* Schedule stored as local ISO date/time strings */
	QString scheduleStartDate; /* yyyy-MM-dd */
	QString scheduleStartTime; /* HH:mm */
	QString scheduleEndDate;
	QString scheduleEndTime;
	ScheduleRepeat scheduleRepeat = ScheduleRepeat::Once;
	int scheduleWeekdaysMask = 0; /* bit0=Mon ... bit6=Sun */

	bool confirmManualStopDuringAutomation = true;
};

inline void CanvasSizeForPreset(CanvasPreset preset, uint32_t customW, uint32_t customH, uint32_t &outW,
				uint32_t &outH)
{
	switch (preset) {
	case CanvasPreset::YouTubeVertical:
	case CanvasPreset::TikTokVertical:
	case CanvasPreset::TwitchVertical:
	case CanvasPreset::InstagramVertical:
		outW = 1080;
		outH = 1920;
		break;
	case CanvasPreset::Custom:
	default:
		outW = customW;
		outH = customH;
		break;
	}
}

inline int EffectiveShortClipSeconds(const PluginSettings &s)
{
	if (s.shortClipPreset == ShortClipPreset::Custom)
		return s.customShortClipSeconds;
	return static_cast<int>(s.shortClipPreset);
}

inline int EffectiveLongClipSeconds(const PluginSettings &s)
{
	if (s.longClipPreset == LongClipPreset::Custom)
		return s.customLongClipSeconds;
	return static_cast<int>(s.longClipPreset);
}

inline int RequiredBufferSeconds(const PluginSettings &s)
{
	const int shortSec = EffectiveShortClipSeconds(s);
	const int longSec = EffectiveLongClipSeconds(s);
	int needed = shortSec > longSec ? shortSec : longSec;
	if (needed < 60)
		needed = 60;
	if (needed > kMaxClipBufferSeconds)
		needed = kMaxClipBufferSeconds;
	return needed;
}

inline bool ValidateCanvasSize(int w, int h, QString *error)
{
	if (w < 160 || h < 160) {
		if (error)
			*error = QStringLiteral("Minimum resolution is 160×160.");
		return false;
	}
	if (w > 7680 || h > 7680) {
		if (error)
			*error = QStringLiteral("Maximum resolution is 7680×7680.");
		return false;
	}
	return true;
}

inline bool ValidateShortClipSeconds(int seconds, QString *error)
{
	if (seconds < 1) {
		if (error)
			*error = QStringLiteral("Short clip length must be at least 1 second.");
		return false;
	}
	if (seconds > kMaxClipBufferSeconds) {
		if (error)
			*error = QStringLiteral("Short clip length cannot exceed %1 seconds (%2 minutes).")
					 .arg(kMaxClipBufferSeconds)
					 .arg(kMaxClipBufferSeconds / 60);
		return false;
	}
	return true;
}

inline bool ValidateLongClipSeconds(int seconds, QString *error, QString *warning = nullptr)
{
	if (seconds < 1) {
		if (error)
			*error = QStringLiteral("Long clip length must be at least 1 second.");
		return false;
	}
	if (seconds > kMaxClipBufferSeconds) {
		if (error)
			*error = QStringLiteral(
					 "Long clip length cannot exceed the safe buffer limit of %1 seconds (%2 minutes).")
					 .arg(kMaxClipBufferSeconds)
					 .arg(kMaxClipBufferSeconds / 60);
		return false;
	}
	if (warning && seconds >= kMemoryWarnClipSeconds) {
		*warning = QStringLiteral(
			"Durations of %1 seconds or more may require substantial memory, disk activity, "
			"and encoder resources for the vertical clip buffer.")
				   .arg(kMemoryWarnClipSeconds);
	}
	return true;
}

/* Backward-compatible alias used by older tests */
inline bool ValidateClipSeconds(int seconds, QString *error)
{
	return ValidateShortClipSeconds(seconds, error);
}

inline int ParseMmSs(const QString &text, QString *error)
{
	const QString t = text.trimmed();
	if (t.isEmpty()) {
		if (error)
			*error = QStringLiteral("Duration cannot be empty.");
		return -1;
	}
	/* Accept MM:SS or total seconds as digits */
	if (t.contains(QLatin1Char(':'))) {
		const QStringList parts = t.split(QLatin1Char(':'));
		if (parts.size() != 2) {
			if (error)
				*error = QStringLiteral("Use MM:SS format (for example 2:30).");
			return -1;
		}
		bool okM = false;
		bool okS = false;
		const int m = parts[0].trimmed().toInt(&okM);
		const int s = parts[1].trimmed().toInt(&okS);
		if (!okM || !okS || m < 0 || s < 0 || s > 59) {
			if (error)
				*error = QStringLiteral("Invalid time value. Use minutes and seconds (MM:SS).");
			return -1;
		}
		const int total = m * 60 + s;
		if (total < 1) {
			if (error)
				*error = QStringLiteral("Duration must be greater than zero.");
			return -1;
		}
		return total;
	}
	for (QChar c : t) {
		if (!c.isDigit()) {
			if (error)
				*error = QStringLiteral("Enter a duration as MM:SS or whole seconds (digits only).");
			return -1;
		}
	}
	bool ok = false;
	const int sec = t.toInt(&ok);
	if (!ok) {
		if (error)
			*error = QStringLiteral("Enter a duration as MM:SS or whole seconds (digits only).");
		return -1;
	}
	if (sec < 1) {
		if (error)
			*error = QStringLiteral("Duration must be greater than zero.");
		return -1;
	}
	return sec;
}

inline bool ValidateAutoDurationSeconds(int seconds, QString *error)
{
	if (seconds < 1) {
		if (error)
			*error = QStringLiteral("Recording duration must be at least 1 second.");
		return false;
	}
	if (seconds > 24 * 3600) {
		if (error)
			*error = QStringLiteral("Recording duration cannot exceed 24 hours.");
		return false;
	}
	return true;
}

/* Reject empty / null / clearly unsafe recording folder inputs. Absolute user paths are allowed. */
inline bool ValidateRecordingPath(const QString &path, QString *error = nullptr)
{
	const QString t = path.trimmed();
	if (t.isEmpty())
		return true; /* empty => OBS default */
	if (t.contains(QChar('\0'))) {
		if (error)
			*error = QStringLiteral("Recording path contains invalid characters.");
		return false;
	}
	/* Disallow device paths / alternate data stream style tricks on Windows. */
	if (t.contains(QStringLiteral("\\\\.\\")) || t.contains(QStringLiteral("\\\\?\\"))) {
		if (error)
			*error = QStringLiteral("Recording path is not allowed.");
		return false;
	}
	const QFileInfo fi(t);
	if (fi.fileName() == QStringLiteral(".") || fi.fileName() == QStringLiteral("..")) {
		if (error)
			*error = QStringLiteral("Recording path is not valid.");
		return false;
	}
	return true;
}

inline bool IsPortrait(uint32_t w, uint32_t h)
{
	return h > w;
}

inline StreamDestination *FindDestination(PluginSettings &s, const QString &id)
{
	for (StreamDestination &d : s.destinations) {
		if (d.id == id)
			return &d;
	}
	return nullptr;
}

inline const StreamDestination *FindDestination(const PluginSettings &s, const QString &id)
{
	for (const StreamDestination &d : s.destinations) {
		if (d.id == id)
			return &d;
	}
	return nullptr;
}

inline StreamDestination ActiveDestination(const PluginSettings &s)
{
	if (const StreamDestination *d = FindDestination(s, s.activeDestinationId))
		return *d;
	if (!s.destinations.isEmpty())
		return s.destinations.first();
	return MakeDefaultDestination(StreamPlatform::YouTube);
}

inline void EnsureDefaultDestinations(PluginSettings &s)
{
	if (!s.destinations.isEmpty())
		return;
	/* One empty slot per platform so switching presets preserves separate configs */
	for (int i = 0; i <= (int)StreamPlatform::CustomRtmp; ++i) {
		StreamDestination d = MakeDefaultDestination(static_cast<StreamPlatform>(i));
		s.destinations.push_back(d);
	}
	s.activeDestinationId = s.destinations.first().id;
}


inline QString DefaultRecordingPath()
{
#ifdef VSP_SETTINGS_TEST
	return {};
#else
	char *path = obs_frontend_get_current_record_output_path();
	QString result;
	if (path) {
		result = QString::fromUtf8(path);
		bfree(path);
	}
	if (result.isEmpty()) {
		config_t *cfg = obs_frontend_get_profile_config();
		if (cfg) {
			const char *simple = config_get_string(cfg, "SimpleOutput", "FilePath");
			if (simple && *simple)
				result = QString::fromUtf8(simple);
			if (result.isEmpty()) {
				const char *adv = config_get_string(cfg, "AdvOut", "RecFilePath");
				if (adv && *adv)
					result = QString::fromUtf8(adv);
			}
		}
	}
	return result;
#endif
}

#ifdef VSP_SETTINGS_TEST
inline void SaveSettingsToData(void *, const PluginSettings &, uint32_t, uint32_t) {}
inline PluginSettings LoadSettingsFromData(void *, uint32_t &canvasW, uint32_t &canvasH)
{
	PluginSettings s;
	canvasW = 1080;
	canvasH = 1920;
	return s;
}
inline bool BackupConfigBlob(void *, const char *)
{
	return true;
}
inline void MigrateConfigSchema(void *, int fromSchema, int toSchema)
{
	(void)fromSchema;
	(void)toSchema;
}
#else
/*
 * Lightweight JSON backup under plugin_config before a schema migration.
 * Never deletes the original blob; caller keeps using `data` in-place.
 */
inline bool BackupConfigBlob(obs_data_t *data, const char *reason)
{
	if (!data)
		return false;

	char *dir = obs_module_get_config_path(obs_current_module(), "backups");
	if (!dir)
		return false;

	QString backupDir = QString::fromUtf8(dir);
	bfree(dir);
	QDir().mkpath(backupDir);

	const QString stamp =
		QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_hhmmss"));
	const QString path =
		backupDir + QStringLiteral("/config_schema_") + stamp + QStringLiteral(".json");

	const char *json = obs_data_get_json(data);
	if (!json)
		return false;

	QFile f(path);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return false;
	f.write(json);
	if (reason && *reason) {
		f.write("\n/* reason: ");
		f.write(reason);
		f.write(" */\n");
	}
	f.close();
	blog(LOG_INFO, "[obs-shorts-vertical] Config backup written: %s", path.toUtf8().constData());
	return true;
}

/* Forward-migrate persisted settings. Keep older keys until startup validates. */
inline void MigrateConfigSchema(obs_data_t *data, int fromSchema, int toSchema)
{
	if (!data || fromSchema >= toSchema)
		return;

	/*
	 * Schema 0 → 1: introduce config_schema. Field-level legacy key migrations
	 * (clip_preset → short_clip_preset, etc.) already run inside LoadSettingsFromData.
	 * Future bumps add staged transforms here; never delete the pre-migration backup
	 * until a successful plugin startup has rewritten settings.
	 */
	obs_data_set_int(data, "config_schema", toSchema);
	blog(LOG_INFO, "[obs-shorts-vertical] Migrated config schema %d → %d", fromSchema, toSchema);
}

inline void SaveSettingsToData(obs_data_t *data, const PluginSettings &s, uint32_t canvasW, uint32_t canvasH)
{
	obs_data_set_int(data, "config_schema", kConfigSchemaVersion);
	obs_data_set_int(data, "canvas_preset", static_cast<int>(s.canvasPreset));
	obs_data_set_int(data, "custom_width", s.customWidth);
	obs_data_set_int(data, "custom_height", s.customHeight);

	obs_data_set_int(data, "short_clip_preset", static_cast<int>(s.shortClipPreset));
	obs_data_set_int(data, "custom_short_clip_seconds", s.customShortClipSeconds);
	obs_data_set_int(data, "long_clip_preset", static_cast<int>(s.longClipPreset));
	obs_data_set_int(data, "custom_long_clip_seconds", s.customLongClipSeconds);

	/* Keep legacy keys in sync for older readers */
	obs_data_set_int(data, "clip_preset", static_cast<int>(s.shortClipPreset));
	obs_data_set_int(data, "custom_clip_seconds", s.customShortClipSeconds);

	obs_data_set_string(data, "recording_path", s.recordingPath.toUtf8().constData());
	obs_data_set_bool(data, "clip_buffer_enabled", s.clipBufferEnabled);
	obs_data_set_bool(data, "auto_start_clip_buffer", s.autoStartClipBuffer);
	obs_data_set_bool(data, "stop_buffer_when_idle", s.stopBufferWhenIdle);
	obs_data_set_int(data, "buffer_idle_timeout_seconds", s.bufferIdleTimeoutSeconds);
	obs_data_set_bool(data, "save_available_when_short", s.saveAvailableWhenShort);
	obs_data_set_bool(data, "buffer_start_on_vertical_live", s.bufferStartOnVerticalLive);
	obs_data_set_bool(data, "buffer_start_on_vertical_record", s.bufferStartOnVerticalRecord);

	/* Destination metadata only — never persist stream keys / passwords here */
	obs_data_set_string(data, "active_destination_id", s.activeDestinationId.toUtf8().constData());
	OBSDataArrayAutoRelease destArr = obs_data_array_create();
	for (const StreamDestination &d : s.destinations) {
		OBSDataAutoRelease obj = obs_data_create();
		obs_data_set_string(obj, "id", d.id.toUtf8().constData());
		obs_data_set_int(obj, "platform", static_cast<int>(d.platform));
		obs_data_set_string(obj, "name", d.name.toUtf8().constData());
		obs_data_set_string(obj, "server", d.server.toUtf8().constData());
		obs_data_set_string(obj, "username", d.username.toUtf8().constData());
		obs_data_set_string(obj, "twitch_ingest_id", d.twitchIngestId.toUtf8().constData());
		obs_data_set_bool(obj, "twitch_recommended", d.useRecommendedTwitchIngest);
		obs_data_set_bool(obj, "has_key", !d.streamKey.isEmpty());
		obs_data_array_push_back(destArr, obj);
	}
	obs_data_set_array(data, "stream_destinations", destArr);

	/* Clear legacy insecure keys if present from older versions */
	obs_data_erase(data, "vertical_stream_key");
	obs_data_erase(data, "stream_dest_mode");

	obs_data_set_bool(data, "automation_enabled", s.automationEnabled);
	obs_data_set_bool(data, "auto_start_main_stream", s.autoStartOnMainStream);
	obs_data_set_bool(data, "auto_start_scene", s.autoStartOnScene);
	obs_data_set_bool(data, "auto_start_obs_start", s.autoStartOnObsStart);
	obs_data_set_bool(data, "auto_start_schedule", s.autoStartOnSchedule);
	obs_data_set_bool(data, "auto_start_countdown", s.autoStartOnCountdown);
	obs_data_set_bool(data, "auto_start_vertical_live", s.autoStartOnVerticalLive);

	obs_data_set_bool(data, "auto_stop_main_stream", s.autoStopOnMainStreamStop);
	obs_data_set_bool(data, "auto_stop_scene_inactive", s.autoStopOnSceneInactive);
	obs_data_set_bool(data, "auto_stop_duration", s.autoStopOnDuration);
	obs_data_set_bool(data, "auto_stop_schedule_end", s.autoStopOnScheduleEnd);
	obs_data_set_bool(data, "auto_stop_vertical_live", s.autoStopOnVerticalLiveStop);
	obs_data_set_bool(data, "auto_stop_obs_shutdown", s.autoStopOnObsShutdown);

	obs_data_set_string(data, "trigger_scene_uuid", s.triggerSceneUuid.toUtf8().constData());
	obs_data_set_string(data, "trigger_scene_name", s.triggerSceneName.toUtf8().constData());
	obs_data_set_int(data, "auto_record_duration_seconds", s.autoRecordDurationSeconds);
	obs_data_set_int(data, "countdown_seconds", s.countdownSeconds);

	obs_data_set_string(data, "schedule_start_date", s.scheduleStartDate.toUtf8().constData());
	obs_data_set_string(data, "schedule_start_time", s.scheduleStartTime.toUtf8().constData());
	obs_data_set_string(data, "schedule_end_date", s.scheduleEndDate.toUtf8().constData());
	obs_data_set_string(data, "schedule_end_time", s.scheduleEndTime.toUtf8().constData());
	obs_data_set_int(data, "schedule_repeat", static_cast<int>(s.scheduleRepeat));
	obs_data_set_int(data, "schedule_weekdays_mask", s.scheduleWeekdaysMask);
	obs_data_set_bool(data, "confirm_manual_stop_automation", s.confirmManualStopDuringAutomation);

	obs_data_set_int(data, "width", canvasW);
	obs_data_set_int(data, "height", canvasH);
}

inline PluginSettings LoadSettingsFromData(obs_data_t *data, uint32_t &canvasW, uint32_t &canvasH)
{
	PluginSettings s;
	s.canvasPreset = static_cast<CanvasPreset>(obs_data_get_int(data, "canvas_preset"));
	s.customWidth = (uint32_t)obs_data_get_int(data, "custom_width");
	s.customHeight = (uint32_t)obs_data_get_int(data, "custom_height");

	if (obs_data_has_user_value(data, "short_clip_preset")) {
		s.shortClipPreset = static_cast<ShortClipPreset>(obs_data_get_int(data, "short_clip_preset"));
		s.customShortClipSeconds = (int)obs_data_get_int(data, "custom_short_clip_seconds");
	} else {
		/* Migrate legacy clip_* keys */
		s.shortClipPreset = static_cast<ShortClipPreset>(obs_data_get_int(data, "clip_preset"));
		s.customShortClipSeconds = (int)obs_data_get_int(data, "custom_clip_seconds");
	}

	if (obs_data_has_user_value(data, "long_clip_preset")) {
		s.longClipPreset = static_cast<LongClipPreset>(obs_data_get_int(data, "long_clip_preset"));
		s.customLongClipSeconds = (int)obs_data_get_int(data, "custom_long_clip_seconds");
	}

	s.recordingPath = QString::fromUtf8(obs_data_get_string(data, "recording_path"));
	s.clipBufferEnabled = obs_data_has_user_value(data, "clip_buffer_enabled")
				      ? obs_data_get_bool(data, "clip_buffer_enabled")
				      : true;
	s.autoStartClipBuffer = obs_data_has_user_value(data, "auto_start_clip_buffer")
					? obs_data_get_bool(data, "auto_start_clip_buffer")
					: true;
	s.stopBufferWhenIdle = obs_data_get_bool(data, "stop_buffer_when_idle");
	s.bufferIdleTimeoutSeconds = (int)obs_data_get_int(data, "buffer_idle_timeout_seconds");
	s.saveAvailableWhenShort = obs_data_has_user_value(data, "save_available_when_short")
					   ? obs_data_get_bool(data, "save_available_when_short")
					   : true;
	s.bufferStartOnVerticalLive = obs_data_has_user_value(data, "buffer_start_on_vertical_live")
					      ? obs_data_get_bool(data, "buffer_start_on_vertical_live")
					      : true;
	s.bufferStartOnVerticalRecord = obs_data_has_user_value(data, "buffer_start_on_vertical_record")
						? obs_data_get_bool(data, "buffer_start_on_vertical_record")
						: true;

	s.activeDestinationId = QString::fromUtf8(obs_data_get_string(data, "active_destination_id"));
	s.destinations.clear();
	obs_data_array_t *destArr = obs_data_get_array(data, "stream_destinations");
	if (destArr) {
		const size_t n = obs_data_array_count(destArr);
		for (size_t i = 0; i < n; ++i) {
			OBSDataAutoRelease obj = obs_data_array_item(destArr, i);
			StreamDestination d;
			d.id = QString::fromUtf8(obs_data_get_string(obj, "id"));
			d.platform = static_cast<StreamPlatform>(obs_data_get_int(obj, "platform"));
			d.name = QString::fromUtf8(obs_data_get_string(obj, "name"));
			d.server = QString::fromUtf8(obs_data_get_string(obj, "server"));
			d.username = QString::fromUtf8(obs_data_get_string(obj, "username"));
			d.twitchIngestId = QString::fromUtf8(obs_data_get_string(obj, "twitch_ingest_id"));
			d.useRecommendedTwitchIngest = obs_data_has_user_value(obj, "twitch_recommended")
							       ? obs_data_get_bool(obj, "twitch_recommended")
							       : true;
			if (d.id.isEmpty())
				d.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
			s.destinations.push_back(d);
		}
		obs_data_array_release(destArr);
	}

	/* Migrate legacy plain-text vertical key into first custom destination memory only;
	 * do not keep it in the settings blob. Caller should move it into secure storage. */
	if (s.destinations.isEmpty()) {
		const char *legacyServer = obs_data_get_string(data, "vertical_stream_server");
		const char *legacyKey = obs_data_get_string(data, "vertical_stream_key");
		EnsureDefaultDestinations(s);
		if (legacyServer && *legacyServer) {
			for (StreamDestination &d : s.destinations) {
				if (d.platform == StreamPlatform::CustomRtmp) {
					d.server = QString::fromUtf8(legacyServer);
					if (legacyKey && *legacyKey)
						d.streamKey = QString::fromUtf8(legacyKey);
					s.activeDestinationId = d.id;
					break;
				}
			}
		}
	} else if (s.activeDestinationId.isEmpty()) {
		s.activeDestinationId = s.destinations.first().id;
	}

	/* Scrub legacy insecure keys from the live settings object immediately on load. */
	obs_data_erase(data, "vertical_stream_key");
	obs_data_erase(data, "stream_dest_mode");
	obs_data_erase(data, "vertical_stream_server");

	s.automationEnabled = obs_data_get_bool(data, "automation_enabled");
	s.autoStartOnMainStream = obs_data_get_bool(data, "auto_start_main_stream");
	s.autoStartOnScene = obs_data_get_bool(data, "auto_start_scene");
	s.autoStartOnObsStart = obs_data_get_bool(data, "auto_start_obs_start");
	s.autoStartOnSchedule = obs_data_get_bool(data, "auto_start_schedule");
	s.autoStartOnCountdown = obs_data_get_bool(data, "auto_start_countdown");
	s.autoStartOnVerticalLive = obs_data_get_bool(data, "auto_start_vertical_live");

	s.autoStopOnMainStreamStop = obs_data_get_bool(data, "auto_stop_main_stream");
	s.autoStopOnSceneInactive = obs_data_get_bool(data, "auto_stop_scene_inactive");
	s.autoStopOnDuration = obs_data_get_bool(data, "auto_stop_duration");
	s.autoStopOnScheduleEnd = obs_data_get_bool(data, "auto_stop_schedule_end");
	s.autoStopOnVerticalLiveStop = obs_data_get_bool(data, "auto_stop_vertical_live");
	s.autoStopOnObsShutdown = obs_data_has_user_value(data, "auto_stop_obs_shutdown")
					  ? obs_data_get_bool(data, "auto_stop_obs_shutdown")
					  : true;

	s.triggerSceneUuid = QString::fromUtf8(obs_data_get_string(data, "trigger_scene_uuid"));
	s.triggerSceneName = QString::fromUtf8(obs_data_get_string(data, "trigger_scene_name"));
	s.autoRecordDurationSeconds = (int)obs_data_get_int(data, "auto_record_duration_seconds");
	s.countdownSeconds = (int)obs_data_get_int(data, "countdown_seconds");

	s.scheduleStartDate = QString::fromUtf8(obs_data_get_string(data, "schedule_start_date"));
	s.scheduleStartTime = QString::fromUtf8(obs_data_get_string(data, "schedule_start_time"));
	s.scheduleEndDate = QString::fromUtf8(obs_data_get_string(data, "schedule_end_date"));
	s.scheduleEndTime = QString::fromUtf8(obs_data_get_string(data, "schedule_end_time"));
	s.scheduleRepeat = static_cast<ScheduleRepeat>(obs_data_get_int(data, "schedule_repeat"));
	s.scheduleWeekdaysMask = (int)obs_data_get_int(data, "schedule_weekdays_mask");
	s.confirmManualStopDuringAutomation = obs_data_has_user_value(data, "confirm_manual_stop_automation")
						      ? obs_data_get_bool(data, "confirm_manual_stop_automation")
						      : true;

	if (s.customWidth == 0)
		s.customWidth = 1080;
	if (s.customHeight == 0)
		s.customHeight = 1920;
	if (s.customShortClipSeconds <= 0)
		s.customShortClipSeconds = 45;
	if (s.customLongClipSeconds <= 0)
		s.customLongClipSeconds = 150;
	if (s.autoRecordDurationSeconds <= 0)
		s.autoRecordDurationSeconds = 3600;
	if (s.countdownSeconds <= 0)
		s.countdownSeconds = 60;
	if (s.bufferIdleTimeoutSeconds <= 0)
		s.bufferIdleTimeoutSeconds = 300;
	EnsureDefaultDestinations(s);
	if (s.activeDestinationId.isEmpty() && !s.destinations.isEmpty())
		s.activeDestinationId = s.destinations.first().id;

	auto validShort = [](ShortClipPreset p) {
		return p == ShortClipPreset::Custom || p == ShortClipPreset::Sec10 || p == ShortClipPreset::Sec20 ||
		       p == ShortClipPreset::Sec30 || p == ShortClipPreset::Sec60;
	};
	auto validLong = [](LongClipPreset p) {
		return p == LongClipPreset::Custom || p == LongClipPreset::Min2 || p == LongClipPreset::Min3 ||
		       p == LongClipPreset::Min4 || p == LongClipPreset::Min5;
	};
	if (!validShort(s.shortClipPreset))
		s.shortClipPreset = ShortClipPreset::Sec30;
	if (!validLong(s.longClipPreset))
		s.longClipPreset = LongClipPreset::Min2;

	canvasW = (uint32_t)obs_data_get_int(data, "width");
	canvasH = (uint32_t)obs_data_get_int(data, "height");
	if (canvasW == 0 || canvasH == 0)
		CanvasSizeForPreset(s.canvasPreset, s.customWidth, s.customHeight, canvasW, canvasH);

	if (s.recordingPath.isEmpty())
		s.recordingPath = DefaultRecordingPath();

	return s;
}
#endif

} // namespace vsp
