#include "vertical-outputs.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStorageInfo>

namespace {

struct EncoderPair {
	obs_encoder_t *video = nullptr;
	obs_encoder_t *audio = nullptr;
};

void ReleasePair(EncoderPair &p)
{
	if (p.video) {
		obs_encoder_release(p.video);
		p.video = nullptr;
	}
	if (p.audio) {
		obs_encoder_release(p.audio);
		p.audio = nullptr;
	}
}

QString SanitizeFilenamePart(QString s)
{
	const QString forbidden = QStringLiteral("<>:\"/\\|?*");
	for (QChar c : forbidden)
		s.replace(c, QLatin1Char('_'));
	s.replace(QLatin1Char('\n'), QLatin1Char('_'));
	s.replace(QLatin1Char('\r'), QLatin1Char('_'));
	s.replace(QChar('\0'), QLatin1Char('_'));
	/* Collapse path separators that could escape the recording directory. */
	s.replace(QLatin1Char('/'), QLatin1Char('_'));
	s.replace(QLatin1Char('\\'), QLatin1Char('_'));
	s = s.trimmed();
	if (s.isEmpty() || s == QStringLiteral(".") || s == QStringLiteral(".."))
		s = QStringLiteral("output");
	return s;
}

QString SanitizeOutputError(const char *err)
{
	if (!err || !*err)
		return QStringLiteral("Failed to start vertical stream.");
	return vsp::SanitizeUserFacingError(QString::fromUtf8(err));
}

EncoderPair MakeEncoders(video_t *video, bool forStreaming, QString *encoderName, int *vBitrate, int *aBitrate,
			 QString *error)
{
	EncoderPair pair;
	obs_encoder_t *templateV = nullptr;
	obs_encoder_t *templateA = nullptr;

	obs_output_t *mainOut = forStreaming ? obs_frontend_get_streaming_output() : obs_frontend_get_recording_output();
	if (mainOut) {
		templateV = obs_output_get_video_encoder(mainOut);
		templateA = obs_output_get_audio_encoder(mainOut, 0);
		obs_output_release(mainOut);
	}

	const char *vId = "obs_x264";
	const char *aId = "ffmpeg_aac";
	OBSDataAutoRelease vSettings = obs_data_create();
	OBSDataAutoRelease aSettings = obs_data_create();

	if (templateV) {
		const char *id = obs_encoder_get_id(templateV);
		if (id && *id)
			vId = id;
		OBSDataAutoRelease src = obs_encoder_get_settings(templateV);
		if (src)
			obs_data_apply(vSettings, src);
	} else {
		obs_data_set_string(vSettings, "rate_control", "CBR");
		obs_data_set_int(vSettings, "bitrate", forStreaming ? 4500 : 6000);
		obs_data_set_string(vSettings, "preset", "veryfast");
		obs_data_set_string(vSettings, "profile", "high");
		obs_data_set_int(vSettings, "keyint_sec", 2);
	}

	if (templateA) {
		const char *id = obs_encoder_get_id(templateA);
		if (id && *id)
			aId = id;
		OBSDataAutoRelease src = obs_encoder_get_settings(templateA);
		if (src)
			obs_data_apply(aSettings, src);
	} else {
		obs_data_set_int(aSettings, "bitrate", 160);
	}

	if (encoderName)
		*encoderName = QString::fromUtf8(vId);
	if (vBitrate)
		*vBitrate = (int)obs_data_get_int(vSettings, "bitrate");
	if (aBitrate)
		*aBitrate = (int)obs_data_get_int(aSettings, "bitrate");

	pair.video = obs_video_encoder_create(vId, "vertical_shorts_video", vSettings, nullptr);
	if (!pair.video) {
		vId = "obs_x264";
		if (encoderName)
			*encoderName = QStringLiteral("obs_x264");
		pair.video = obs_video_encoder_create(vId, "vertical_shorts_video", vSettings, nullptr);
	}
	if (!pair.video) {
		if (error)
			*error = QStringLiteral("Could not create a video encoder for the vertical canvas.");
		return pair;
	}

	pair.audio = obs_audio_encoder_create(aId, "vertical_shorts_audio", aSettings, 0, nullptr);
	if (!pair.audio)
		pair.audio = obs_audio_encoder_create("ffmpeg_aac", "vertical_shorts_audio", aSettings, 0, nullptr);
	if (!pair.audio) {
		ReleasePair(pair);
		if (error)
			*error = QStringLiteral("Could not create an audio encoder for the vertical canvas.");
		return pair;
	}

	obs_encoder_set_video(pair.video, video);
	obs_encoder_set_audio(pair.audio, obs_get_audio());
	return pair;
}

} // namespace

VerticalOutputs::VerticalOutputs(QObject *parent) : QObject(parent)
{
	statusTimer.setInterval(500);
	connect(&statusTimer, &QTimer::timeout, this, &VerticalOutputs::OnBufferStatusTick);
	idleTimer.setSingleShot(true);
	connect(&idleTimer, &QTimer::timeout, this, &VerticalOutputs::OnIdleTimeout);
}

VerticalOutputs::~VerticalOutputs()
{
	statusTimer.stop();
	idleTimer.stop();
	StopAll();
}

void VerticalOutputs::SetVideo(video_t *v)
{
	video = v;
}

void VerticalOutputs::ApplySettings(const vsp::PluginSettings &s, bool *bufferRestartRequired)
{
	const int oldBuf = configuredBufferSeconds;
	settings = s;
	configuredBufferSeconds = vsp::RequiredBufferSeconds(settings);
	if (bufferRestartRequired)
		*bufferRestartRequired = false;

	if (replayOutput && obs_output_active(replayOutput)) {
		if (configuredBufferSeconds != oldBuf) {
			if (bufferRestartRequired)
				*bufferRestartRequired = true;
		}
		OBSDataAutoRelease data = obs_data_create();
		obs_data_set_string(data, "directory", ResolveRecordingDirectory().toUtf8().constData());
		obs_data_set_string(data, "format", "VerticalShorts_%CCYY-%MM-%DD_%hh-%mm-%ss");
		obs_data_set_string(data, "extension", "mp4");
		obs_data_set_bool(data, "allow_spaces", false);
		obs_data_set_int(data, "max_time_sec", configuredBufferSeconds);
		obs_data_set_int(data, "max_size_mb", 0);
		obs_output_update(replayOutput, data);
	}

	if (settings.stopBufferWhenIdle && IsClipBufferActive()) {
		idleTimer.start(settings.bufferIdleTimeoutSeconds * 1000);
	} else {
		idleTimer.stop();
	}
}

QString VerticalOutputs::ResolveRecordingDirectory() const
{
	QString path = settings.recordingPath.trimmed();
	if (!vsp::ValidateRecordingPath(path))
		path.clear();
	if (path.isEmpty())
		path = vsp::DefaultRecordingPath();
	if (path.isEmpty())
		path = QDir::homePath();
	path = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
	QDir().mkpath(path);
	return path;
}

QString VerticalOutputs::MakeOutputFilename(const QString &outputType, int durationSeconds) const
{
	const QString dir = ResolveRecordingDirectory();
	const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
	QString base = QStringLiteral("%1_%2").arg(SanitizeFilenamePart(outputType), stamp);
	if (durationSeconds > 0)
		base += QStringLiteral("_%1s").arg(durationSeconds);

	QString path = QDir(dir).filePath(base + QStringLiteral(".mp4"));
	/* Ensure resolved file stays under the recording directory. */
	const QString canonicalDir = QDir(dir).canonicalPath();
	const QString absolute = QFileInfo(path).absoluteFilePath();
	if (!canonicalDir.isEmpty() && !absolute.startsWith(canonicalDir)) {
		path = QDir(dir).filePath(QStringLiteral("VerticalShorts_%1.mp4").arg(stamp));
	}
	int n = 1;
	while (QFileInfo::exists(path)) {
		path = QDir(dir).filePath(QStringLiteral("%1_%2.mp4").arg(base).arg(n++));
	}
	return path;
}

QString VerticalOutputs::ActiveEncoderSummary() const
{
	if (lastEncoderName.isEmpty())
		return QStringLiteral("(inherits OBS when output starts)");
	return lastEncoderName;
}

QString VerticalOutputs::ActiveBitrateSummary() const
{
	if (lastVideoBitrate <= 0)
		return QStringLiteral("(inherits OBS when output starts)");
	return QStringLiteral("%1 kbps video / %2 kbps audio").arg(lastVideoBitrate).arg(lastAudioBitrate);
}

QString VerticalOutputs::RecordingStatusSummary() const
{
	if (IsRecording())
		return QStringLiteral("Recording — %1").arg(lastRecordingPath);
	return QStringLiteral("Idle");
}

QString VerticalOutputs::StreamDestinationSummary() const
{
	const vsp::StreamDestination d = vsp::ActiveDestination(settings);
	return QStringLiteral("%1 — %2 (server %3, key %4)")
		.arg(vsp::PlatformDisplayName(d.platform), d.name.isEmpty() ? QStringLiteral("(unnamed)") : d.name,
		     vsp::HostnameOnly(d.server),
		     d.streamKey.isEmpty() ? QStringLiteral("not configured") : QStringLiteral("configured"));
}

QString VerticalOutputs::LiveStatusText() const
{
	return vsp::VerticalLiveStatusLabel(liveStatus);
}

void VerticalOutputs::SetLiveStatus(vsp::VerticalLiveStatus s)
{
	liveStatus = s;
	emit liveStatusChanged(s, LiveStatusText());
}

bool VerticalOutputs::ValidateActiveDestination(QString *error, QString *field) const
{
	return vsp::ValidateDestination(vsp::ActiveDestination(settings), error, field);
}

bool VerticalOutputs::TestStreamDestination(QString *summary, QString *error) const
{
	QString field;
	if (!ValidateActiveDestination(error, &field))
		return false;

	const vsp::StreamDestination d = vsp::ActiveDestination(settings);
	if (!video) {
		/* Encoder check is best-effort when video is ready */
	}

	obs_service_t *svc = CreateIndependentVerticalService(error);
	if (!svc)
		return false;
	obs_service_release(svc);

	QString warn;
	vsp::ValidateDestination(d, nullptr, nullptr, &warn);
	if (summary) {
		*summary = QStringLiteral(
			"Local configuration valid\n"
			"Platform: %1\n"
			"Destination: %2\n"
			"Server host: %3\n"
			"Stream key: configured\n"
			"Protocol: %4\n\n"
			"This test does not verify credentials with the platform and does not go live.\n"
			"Server reachable / credentials verified: not tested.")
			.arg(vsp::PlatformDisplayName(d.platform),
			     d.name.isEmpty() ? QStringLiteral("(unnamed)") : d.name, vsp::HostnameOnly(d.server),
			     d.server.startsWith(QStringLiteral("rtmps://"), Qt::CaseInsensitive)
				     ? QStringLiteral("RTMPS")
				     : QStringLiteral("RTMP"));
		if (!warn.isEmpty())
			*summary += QStringLiteral("\n\nWarning: ") + warn;
	}
	blog(LOG_INFO, "[obs-shorts-vertical] Test destination OK platform=%s host=%s key=configured",
	     vsp::PlatformDisplayName(d.platform).toUtf8().constData(),
	     vsp::HostnameOnly(d.server).toUtf8().constData());
	return true;
}

obs_service_t *VerticalOutputs::CreateIndependentVerticalService(QString *error) const
{
	const vsp::StreamDestination d = vsp::ActiveDestination(settings);
	QString field;
	if (!vsp::ValidateDestination(d, error, &field))
		return nullptr;

	OBSDataAutoRelease settingsData = obs_data_create();
	obs_data_set_string(settingsData, "server", d.server.trimmed().toUtf8().constData());
	obs_data_set_string(settingsData, "key", d.streamKey.toUtf8().constData());
	if (!d.username.isEmpty())
		obs_data_set_string(settingsData, "username", d.username.toUtf8().constData());
	if (!d.password.isEmpty())
		obs_data_set_string(settingsData, "password", d.password.toUtf8().constData());

	/* Always use an independent custom RTMP service — never the main OBS service object. */
	obs_service_t *svc =
		obs_service_create("rtmp_custom", "vertical_shorts_independent_service", settingsData, nullptr);
	if (!svc && error)
		*error = QStringLiteral("Could not create an independent vertical streaming service.");
	return svc;
}

bool VerticalOutputs::PathWritable(QString *error) const
{
	const QString dir = ResolveRecordingDirectory();
	QFileInfo fi(dir);
	if (!fi.exists()) {
		if (!QDir().mkpath(dir)) {
			if (error)
				*error = QStringLiteral("Recording path does not exist and could not be created:\n%1").arg(dir);
			return false;
		}
	}
	fi.refresh();
	if (!fi.isDir()) {
		if (error)
			*error = QStringLiteral("Recording path is not a directory:\n%1").arg(dir);
		return false;
	}
	if (!fi.isWritable()) {
		if (error)
			*error = QStringLiteral("Recording path is not writable:\n%1").arg(dir);
		return false;
	}

	QStorageInfo storage(dir);
	if (storage.isValid() && storage.bytesAvailable() >= 0 && storage.bytesAvailable() < 50LL * 1024 * 1024) {
		if (error)
			*error = QStringLiteral("Insufficient disk space on the recording path (less than 50 MB free).");
		return false;
	}
	return true;
}

bool VerticalOutputs::CanStartRecording(QString *error) const
{
	if (!video) {
		if (error)
			*error = QStringLiteral("Vertical video pipeline is not ready.");
		return false;
	}
	if (IsRecording()) {
		if (error)
			*error = QStringLiteral("Vertical recording is already active.");
		return false;
	}
	if (!PathWritable(error))
		return false;
	return true;
}

bool VerticalOutputs::StartStreaming(QString *error)
{
	if (!video) {
		if (error)
			*error = QStringLiteral("Vertical video pipeline is not ready.");
		return false;
	}
	if (IsStreaming())
		return true;

	const uint32_t cw = video_output_get_width(video);
	const uint32_t ch = video_output_get_height(video);
	if (!vsp::ValidateCanvasSize((int)cw, (int)ch, error)) {
		SetLiveStatus(vsp::VerticalLiveStatus::Error);
		return false;
	}

	QString field;
	if (!ValidateActiveDestination(error, &field)) {
		SetLiveStatus(vsp::VerticalLiveStatus::Error);
		return false;
	}

	if (ownedStreamService) {
		obs_service_release(ownedStreamService);
		ownedStreamService = nullptr;
	}

	SetLiveStatus(vsp::VerticalLiveStatus::Connecting);

	obs_service_t *service = CreateIndependentVerticalService(error);
	if (!service) {
		SetLiveStatus(vsp::VerticalLiveStatus::Error);
		return false;
	}
	ownedStreamService = service;

	EncoderPair pair = MakeEncoders(video, true, &lastEncoderName, &lastVideoBitrate, &lastAudioBitrate, error);
	if (!pair.video || !pair.audio) {
		obs_service_release(ownedStreamService);
		ownedStreamService = nullptr;
		ReleasePair(pair);
		SetLiveStatus(vsp::VerticalLiveStatus::Error);
		return false;
	}

	if (!streamOutput) {
		streamOutput = obs_output_create("rtmp_output", "vertical_shorts_stream", nullptr, nullptr);
		if (!streamOutput)
			streamOutput = obs_output_create("ffmpeg_mpegts_muxer", "vertical_shorts_stream", nullptr, nullptr);
		if (streamOutput) {
			signal_handler_t *sh = obs_output_get_signal_handler(streamOutput);
			signal_handler_connect(sh, "stop", OnStreamStop, this);
		}
	}
	if (!streamOutput) {
		ReleasePair(pair);
		obs_service_release(ownedStreamService);
		ownedStreamService = nullptr;
		if (error)
			*error = QStringLiteral("Could not create a vertical streaming output.");
		SetLiveStatus(vsp::VerticalLiveStatus::Error);
		return false;
	}

	/* Independent service + output only — never obs_frontend_get_streaming_service(). */
	obs_output_set_service(streamOutput, ownedStreamService);
	obs_output_set_media(streamOutput, video, obs_get_audio());
	obs_output_set_video_encoder(streamOutput, pair.video);
	obs_output_set_audio_encoder(streamOutput, pair.audio, 0);
	ReleasePair(pair);

	if (!obs_output_start(streamOutput)) {
		const char *err = obs_output_get_last_error(streamOutput);
		if (error)
			*error = SanitizeOutputError(err);
		blog(LOG_WARNING, "[obs-shorts-vertical] Vertical stream start failed (credentials never logged)");
		SetLiveStatus(vsp::VerticalLiveStatus::Error);
		return false;
	}

	emit streamingChanged(true);
	SetLiveStatus(vsp::VerticalLiveStatus::Live);
	const vsp::StreamDestination d = vsp::ActiveDestination(settings);
	blog(LOG_INFO, "[obs-shorts-vertical] Vertical live started platform=%s host=%s (main OBS untouched)",
	     vsp::PlatformDisplayName(d.platform).toUtf8().constData(),
	     vsp::HostnameOnly(d.server).toUtf8().constData());

	if (settings.bufferStartOnVerticalLive)
		EnsureClipBuffer(nullptr);
	NoteUserActivity();
	return true;
}

void VerticalOutputs::StopStreaming()
{
	if (streamOutput && obs_output_active(streamOutput)) {
		SetLiveStatus(vsp::VerticalLiveStatus::Stopping);
		obs_output_stop(streamOutput);
	}
	emit streamingChanged(false);
	SetLiveStatus(vsp::VerticalLiveStatus::Offline);
}

bool VerticalOutputs::IsStreaming() const
{
	return streamOutput && obs_output_active(streamOutput);
}

bool VerticalOutputs::StartRecording(QString *error)
{
	if (!CanStartRecording(error))
		return false;

	EncoderPair pair = MakeEncoders(video, false, &lastEncoderName, &lastVideoBitrate, &lastAudioBitrate, error);
	if (!pair.video || !pair.audio) {
		ReleasePair(pair);
		return false;
	}

	if (!recordOutput) {
		recordOutput = obs_output_create("ffmpeg_muxer", "vertical_shorts_record", nullptr, nullptr);
		if (!recordOutput)
			recordOutput = obs_output_create("ffmpeg_output", "vertical_shorts_record", nullptr, nullptr);
		if (recordOutput) {
			signal_handler_t *sh = obs_output_get_signal_handler(recordOutput);
			signal_handler_connect(sh, "stop", OnRecordStop, this);
		}
	}
	if (!recordOutput) {
		ReleasePair(pair);
		if (error)
			*error = QStringLiteral("Could not create a vertical recording output.");
		return false;
	}

	const QString path = MakeOutputFilename(QStringLiteral("VerticalRecording"));
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "path", path.toUtf8().constData());
	obs_data_set_string(data, "muxer_settings", "");
	obs_output_update(recordOutput, data);

	obs_output_set_media(recordOutput, video, obs_get_audio());
	obs_output_set_video_encoder(recordOutput, pair.video);
	obs_output_set_audio_encoder(recordOutput, pair.audio, 0);
	ReleasePair(pair);

	if (!obs_output_start(recordOutput)) {
		const char *err = obs_output_get_last_error(recordOutput);
		if (error)
			*error = err && *err ? QString::fromUtf8(err) : QStringLiteral("Failed to start vertical recording.");
		return false;
	}

	lastRecordingPath = path;
	emit recordingChanged(true);
	emit recordingStarted(path);
	if (settings.bufferStartOnVerticalRecord)
		EnsureClipBuffer(nullptr);
	NoteUserActivity();
	blog(LOG_INFO, "[obs-shorts-vertical] Vertical recording started: %s", path.toUtf8().constData());
	return true;
}

void VerticalOutputs::StopRecording()
{
	if (recordOutput && obs_output_active(recordOutput))
		obs_output_stop(recordOutput);
	emit recordingChanged(false);
	emit recordingStopped();
}

bool VerticalOutputs::IsRecording() const
{
	return recordOutput && obs_output_active(recordOutput);
}

int VerticalOutputs::ConfiguredBufferSeconds() const
{
	return configuredBufferSeconds > 0 ? configuredBufferSeconds : vsp::RequiredBufferSeconds(settings);
}

int VerticalOutputs::BufferedSecondsAvailable() const
{
	if (!IsClipBufferActive() || !clipBufferStartedAt.isValid())
		return 0;
	const qint64 elapsed = clipBufferStartedAt.secsTo(QDateTime::currentDateTime());
	if (elapsed < 0)
		return 0;
	const int cap = ConfiguredBufferSeconds();
	return elapsed >= cap ? cap : static_cast<int>(elapsed);
}

BufferStatus VerticalOutputs::GetBufferStatus() const
{
	return bufferStatus;
}

QString VerticalOutputs::BufferStatusText() const
{
	const int avail = BufferedSecondsAvailable();
	const int shortNeed = vsp::EffectiveShortClipSeconds(settings);
	const int longNeed = vsp::EffectiveLongClipSeconds(settings);
	QString text = BufferStatusLabel(bufferStatus);
	if (bufferStatus == BufferStatus::Error && !lastBufferError.isEmpty())
		text += QStringLiteral(" — %1").arg(lastBufferError);
	else if (bufferStatus != BufferStatus::Stopped)
		text += QStringLiteral(" — available %1s (short %2s / long %3s)")
				.arg(avail)
				.arg(shortNeed)
				.arg(longNeed);
	return text;
}

void VerticalOutputs::EmitBufferStatus()
{
	emit bufferStatusChanged(bufferStatus, BufferStatusText());
}

void VerticalOutputs::UpdateBufferStatus(bool emitSignal)
{
	BufferStatus next = BufferStatus::Stopped;
	if (!settings.clipBufferEnabled) {
		next = BufferStatus::Stopped;
	} else if (bufferStatus == BufferStatus::Error && !IsClipBufferActive()) {
		next = BufferStatus::Error;
	} else if (!IsClipBufferActive()) {
		next = (bufferStatus == BufferStatus::Starting) ? BufferStatus::Starting : BufferStatus::Stopped;
	} else {
		const int avail = BufferedSecondsAvailable();
		const int shortNeed = vsp::EffectiveShortClipSeconds(settings);
		const int longNeed = vsp::EffectiveLongClipSeconds(settings);
		if (avail >= longNeed)
			next = BufferStatus::ReadyLong;
		else if (avail >= shortNeed)
			next = BufferStatus::ReadyShort;
		else if (avail > 0)
			next = BufferStatus::Buffering;
		else
			next = BufferStatus::Starting;
	}

	if (next != bufferStatus || emitSignal) {
		bufferStatus = next;
		if (emitSignal)
			EmitBufferStatus();
	}
}

void VerticalOutputs::OnBufferStatusTick()
{
	UpdateBufferStatus(true);
}

void VerticalOutputs::OnIdleTimeout()
{
	if (!settings.stopBufferWhenIdle)
		return;
	if (IsStreaming() || IsRecording())
		return;
	blog(LOG_INFO, "[obs-shorts-vertical] Stopping idle clip buffer");
	StopClipBuffer();
}

void VerticalOutputs::NoteUserActivity()
{
	if (settings.stopBufferWhenIdle && IsClipBufferActive())
		idleTimer.start(settings.bufferIdleTimeoutSeconds * 1000);
}

bool VerticalOutputs::EnsureClipBuffer(QString *error)
{
	if (!settings.clipBufferEnabled) {
		if (error)
			*error = QStringLiteral("Clip buffer is disabled in Settings.");
		bufferStatus = BufferStatus::Stopped;
		EmitBufferStatus();
		return false;
	}
	if (!video) {
		if (error)
			*error = QStringLiteral("Vertical video pipeline is not ready.");
		lastBufferError = error ? *error : QString();
		bufferStatus = BufferStatus::Error;
		EmitBufferStatus();
		return false;
	}
	if (IsClipBufferActive()) {
		NoteUserActivity();
		UpdateBufferStatus(true);
		return true;
	}

	if (!PathWritable(error)) {
		lastBufferError = error ? *error : QString();
		bufferStatus = BufferStatus::Error;
		EmitBufferStatus();
		return false;
	}

	bufferStatus = BufferStatus::Starting;
	EmitBufferStatus();

	EncoderPair pair = MakeEncoders(video, false, &lastEncoderName, &lastVideoBitrate, &lastAudioBitrate, error);
	if (!pair.video || !pair.audio) {
		ReleasePair(pair);
		lastBufferError = error ? *error : QStringLiteral("Encoder unavailable");
		bufferStatus = BufferStatus::Error;
		EmitBufferStatus();
		return false;
	}

	if (!replayOutput) {
		replayOutput = obs_output_create("replay_buffer", "vertical_shorts_clip", nullptr, nullptr);
		if (replayOutput) {
			signal_handler_t *sh = obs_output_get_signal_handler(replayOutput);
			signal_handler_connect(sh, "saved", OnReplaySaved, this);
		}
	}
	if (!replayOutput) {
		ReleasePair(pair);
		if (error)
			*error = QStringLiteral("Could not create a vertical clip buffer (replay_buffer unavailable).");
		lastBufferError = *error;
		bufferStatus = BufferStatus::Error;
		EmitBufferStatus();
		return false;
	}

	configuredBufferSeconds = vsp::RequiredBufferSeconds(settings);

	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "directory", ResolveRecordingDirectory().toUtf8().constData());
	obs_data_set_string(data, "format", "VerticalShorts_%CCYY-%MM-%DD_%hh-%mm-%ss");
	obs_data_set_string(data, "extension", "mp4");
	obs_data_set_bool(data, "allow_spaces", false);
	obs_data_set_int(data, "max_time_sec", configuredBufferSeconds);
	obs_data_set_int(data, "max_size_mb", 0);
	obs_output_update(replayOutput, data);

	obs_output_set_media(replayOutput, video, obs_get_audio());
	obs_output_set_video_encoder(replayOutput, pair.video);
	obs_output_set_audio_encoder(replayOutput, pair.audio, 0);
	ReleasePair(pair);

	if (!obs_output_start(replayOutput)) {
		const char *err = obs_output_get_last_error(replayOutput);
		if (error)
			*error = err && *err ? QString::fromUtf8(err)
					     : QStringLiteral("Failed to start the vertical clip buffer.");
		lastBufferError = *error;
		bufferStatus = BufferStatus::Error;
		EmitBufferStatus();
		return false;
	}

	clipBufferStartedAt = QDateTime::currentDateTime();
	lastBufferError.clear();
	emit clipBufferChanged(true);
	if (!statusTimer.isActive())
		statusTimer.start();
	NoteUserActivity();
	UpdateBufferStatus(true);
	blog(LOG_INFO, "[obs-shorts-vertical] Vertical clip buffer started (%d sec capacity)",
	     configuredBufferSeconds);
	return true;
}

void VerticalOutputs::StopClipBuffer()
{
	statusTimer.stop();
	idleTimer.stop();
	if (replayOutput && obs_output_active(replayOutput))
		obs_output_stop(replayOutput);
	clipBufferStartedAt = QDateTime();
	bufferStatus = BufferStatus::Stopped;
	emit clipBufferChanged(false);
	EmitBufferStatus();
}

bool VerticalOutputs::IsClipBufferActive() const
{
	return replayOutput && obs_output_active(replayOutput);
}

ClipSaveInfo VerticalOutputs::SaveShortClip()
{
	return SaveClipOfDuration(vsp::EffectiveShortClipSeconds(settings), ClipKind::Short, false);
}

ClipSaveInfo VerticalOutputs::SaveLongClip()
{
	return SaveClipOfDuration(vsp::EffectiveLongClipSeconds(settings), ClipKind::Long, false);
}

ClipSaveInfo VerticalOutputs::SaveClipOfDuration(int seconds, ClipKind kind, bool allowPartial)
{
	return SaveClipInternal(seconds, kind, allowPartial);
}

ClipSaveInfo VerticalOutputs::SaveClipInternal(int seconds, ClipKind kind, bool allowPartial)
{
	ClipSaveInfo info;
	info.requestedSeconds = seconds;
	info.result = ClipSaveResult::Error;
	NoteUserActivity();

	if (seconds < 1) {
		info.message = QStringLiteral("Requested clip duration is invalid.");
		return info;
	}

	QString err;
	if (!IsClipBufferActive()) {
		if (!EnsureClipBuffer(&err)) {
			info.message = err;
			return info;
		}
		info.result = ClipSaveResult::BufferNotReady;
		info.availableSeconds = 0;
		info.message = QStringLiteral(
			"The vertical clip buffer just started.\n\n"
			"Requested: %1 seconds\n"
			"Available: 0 seconds\n\n"
			"Footage can only accumulate after the buffer starts. "
			"Allow the buffer to run longer, then try again.")
					   .arg(seconds);
		return info;
	}

	info.availableSeconds = BufferedSecondsAvailable();
	if (info.availableSeconds < seconds && !allowPartial) {
		info.result = info.availableSeconds > 0 ? ClipSaveResult::PartialAvailable : ClipSaveResult::BufferNotReady;
		info.message = QStringLiteral(
			"Not enough footage is buffered yet.\n\n"
			"Requested: %1 seconds\n"
			"Available: %2 seconds\n\n"
			"The buffer only retains footage captured after it started. "
			"%3")
					   .arg(seconds)
					   .arg(info.availableSeconds)
					   .arg(info.availableSeconds > 0
							? QStringLiteral(
								  "You can save the available portion, or wait for the buffer to fill.")
							: QStringLiteral(
								  "Allow the buffer to run longer before saving."));
		return info;
	}

	proc_handler_t *ph = obs_output_get_proc_handler(replayOutput);
	if (!ph) {
		info.message = QStringLiteral("Clip buffer does not support save.");
		return info;
	}

	pendingClipKind = kind;
	pendingClipSeconds = seconds;

	const char *typeName = kind == ClipKind::Long ? "LongClip" : "ShortClip";
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "directory", ResolveRecordingDirectory().toUtf8().constData());
	obs_data_set_string(data, "format",
			    QStringLiteral("%1_%2_%CCYY-%MM-%DD_%hh-%mm-%ss")
				    .arg(QString::fromUtf8(typeName))
				    .arg(seconds)
				    .toUtf8()
				    .constData());
	obs_data_set_string(data, "extension", "mp4");
	obs_data_set_bool(data, "allow_spaces", false);
	obs_data_set_int(data, "max_time_sec", ConfiguredBufferSeconds());
	obs_output_update(replayOutput, data);

	calldata_t cd;
	calldata_init(&cd);
	const bool ok = proc_handler_call(ph, "save", &cd);
	calldata_free(&cd);
	if (!ok) {
		info.message = QStringLiteral("Could not save clip — buffer may still be filling.");
		return info;
	}

	/* Keep buffer running — do not tear down after save */
	info.result = ClipSaveResult::Ok;
	info.message = QStringLiteral("Saving %1 clip (%2 s)…").arg(QString::fromUtf8(typeName)).arg(seconds);
	return info;
}

void VerticalOutputs::StopAll()
{
	statusTimer.stop();
	idleTimer.stop();
	StopStreaming();
	StopRecording();
	StopClipBuffer();

	if (streamOutput) {
		obs_output_release(streamOutput);
		streamOutput = nullptr;
	}
	if (recordOutput) {
		obs_output_release(recordOutput);
		recordOutput = nullptr;
	}
	if (replayOutput) {
		obs_output_release(replayOutput);
		replayOutput = nullptr;
	}
	if (ownedStreamService) {
		obs_service_release(ownedStreamService);
		ownedStreamService = nullptr;
	}
}

void VerticalOutputs::OnStreamStop(void *data, calldata_t *)
{
	auto *self = static_cast<VerticalOutputs *>(data);
	QMetaObject::invokeMethod(
		self,
		[self]() {
			emit self->streamingChanged(false);
			self->SetLiveStatus(vsp::VerticalLiveStatus::Offline);
		},
		Qt::QueuedConnection);
}

void VerticalOutputs::OnRecordStop(void *data, calldata_t *)
{
	auto *self = static_cast<VerticalOutputs *>(data);
	QMetaObject::invokeMethod(
		self,
		[self]() {
			emit self->recordingChanged(false);
			emit self->recordingStopped();
		},
		Qt::QueuedConnection);
}

void VerticalOutputs::OnReplaySaved(void *data, calldata_t *cd)
{
	auto *self = static_cast<VerticalOutputs *>(data);
	const char *path = calldata_string(cd, "path");
	QString qpath = path ? QString::fromUtf8(path) : QString();
	const ClipKind kind = self->pendingClipKind;
	QMetaObject::invokeMethod(
		self,
		[self, qpath, kind]() {
			if (!qpath.isEmpty()) {
				self->lastClipPath = qpath;
				emit self->clipSaved(qpath, kind);
			}
			self->NoteUserActivity();
		},
		Qt::QueuedConnection);
}
