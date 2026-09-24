#pragma once

#include "plugin-settings.hpp"
#include "stream-destination.hpp"

#include <obs.hpp>

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>

enum class ClipKind { Short = 0, Long = 1 };

enum class ClipSaveResult {
	Ok = 0,
	BufferNotReady,
	PartialAvailable,
	Error,
};

enum class BufferStatus {
	Stopped = 0,
	Starting,
	Buffering,
	ReadyShort,
	ReadyLong,
	Error,
};

struct ClipSaveInfo {
	ClipSaveResult result = ClipSaveResult::Error;
	int requestedSeconds = 0;
	int availableSeconds = 0;
	QString path;
	QString message;
};

inline QString BufferStatusLabel(BufferStatus s)
{
	switch (s) {
	case BufferStatus::Stopped:
		return QStringLiteral("Buffer stopped");
	case BufferStatus::Starting:
		return QStringLiteral("Buffer starting");
	case BufferStatus::Buffering:
		return QStringLiteral("Buffering");
	case BufferStatus::ReadyShort:
		return QStringLiteral("Ready for short clip");
	case BufferStatus::ReadyLong:
		return QStringLiteral("Ready for long clip");
	case BufferStatus::Error:
		return QStringLiteral("Buffer error");
	}
	return QStringLiteral("Unknown");
}

class VerticalOutputs : public QObject {
	Q_OBJECT
public:
	explicit VerticalOutputs(QObject *parent = nullptr);
	~VerticalOutputs() override;

	void SetVideo(video_t *video);
	void ApplySettings(const vsp::PluginSettings &settings, bool *bufferRestartRequired = nullptr);

	bool StartStreaming(QString *error);
	void StopStreaming();
	bool IsStreaming() const;
	vsp::VerticalLiveStatus LiveStatus() const { return liveStatus; }
	QString LiveStatusText() const;

	bool TestStreamDestination(QString *summary, QString *error) const;
	QString StreamDestinationSummary() const;
	bool ValidateActiveDestination(QString *error, QString *field = nullptr) const;

	bool StartRecording(QString *error);
	void StopRecording();
	bool IsRecording() const;
	QString LastRecordingPath() const { return lastRecordingPath; }

	bool EnsureClipBuffer(QString *error);
	void StopClipBuffer();
	bool IsClipBufferActive() const;
	int BufferedSecondsAvailable() const;
	int ConfiguredBufferSeconds() const;
	BufferStatus GetBufferStatus() const;
	QString BufferStatusText() const;
	void NoteUserActivity();

	ClipSaveInfo SaveShortClip();
	ClipSaveInfo SaveLongClip();
	ClipSaveInfo SaveClipOfDuration(int seconds, ClipKind kind, bool allowPartial);
	QString LastClipPath() const { return lastClipPath; }

	void StopAll();

	QString ActiveEncoderSummary() const;
	QString ActiveBitrateSummary() const;
	QString RecordingStatusSummary() const;
	QString ResolveRecordingDirectory() const;

	bool CanStartRecording(QString *error) const;
	bool PathWritable(QString *error) const;

signals:
	void streamingChanged(bool active);
	void liveStatusChanged(vsp::VerticalLiveStatus status, const QString &text);
	void recordingChanged(bool active);
	void clipBufferChanged(bool active);
	void bufferStatusChanged(BufferStatus status, const QString &text);
	void clipSaved(const QString &path, ClipKind kind);
	void recordingStarted(const QString &path);
	void recordingStopped();

private slots:
	void OnBufferStatusTick();
	void OnIdleTimeout();

private:
	QString MakeOutputFilename(const QString &outputType, int durationSeconds = 0) const;
	ClipSaveInfo SaveClipInternal(int seconds, ClipKind kind, bool allowPartial);
	obs_service_t *CreateIndependentVerticalService(QString *error) const;
	void SetLiveStatus(vsp::VerticalLiveStatus s);
	void UpdateBufferStatus(bool emitSignal = true);
	void EmitBufferStatus();
	static void OnStreamStop(void *data, calldata_t *cd);
	static void OnRecordStop(void *data, calldata_t *cd);
	static void OnReplaySaved(void *data, calldata_t *cd);

	video_t *video = nullptr;
	vsp::PluginSettings settings;

	obs_output_t *streamOutput = nullptr;
	obs_output_t *recordOutput = nullptr;
	obs_output_t *replayOutput = nullptr;
	obs_service_t *ownedStreamService = nullptr;

	QString lastClipPath;
	QString lastRecordingPath;
	QString lastEncoderName;
	QString lastBufferError;
	int lastVideoBitrate = 0;
	int lastAudioBitrate = 0;
	int configuredBufferSeconds = 60;
	QDateTime clipBufferStartedAt;
	ClipKind pendingClipKind = ClipKind::Short;
	int pendingClipSeconds = 0;
	BufferStatus bufferStatus = BufferStatus::Stopped;
	vsp::VerticalLiveStatus liveStatus = vsp::VerticalLiveStatus::Offline;

	QTimer statusTimer;
	QTimer idleTimer;
};
