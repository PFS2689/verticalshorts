#pragma once

#include "plugin-settings.hpp"
#include "vertical-outputs.hpp"

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>

class RecordingAutomation : public QObject {
	Q_OBJECT
public:
	explicit RecordingAutomation(VerticalOutputs *outputs, QObject *parent = nullptr);

	void ApplySettings(const vsp::PluginSettings &settings);
	void SetManualRecordingActive(bool manual);
	bool IsAutomationOwnedRecording() const { return automationOwnedRecording; }
	vsp::AutomationStatus Status() const { return status; }
	QString StatusText() const;
	QString LastError() const { return lastError; }

	void OnObsFinishedLoading();
	void OnMainStreamingStarted();
	void OnMainStreamingStopped();
	void OnSceneChanged(const QString &sceneUuid, const QString &sceneName);
	void OnVerticalLiveStarted();
	void OnVerticalLiveStopped();
	void OnObsShutdown();
	void OnVerticalRecordingChanged(bool active);

	void ResetRuntimeState();

signals:
	void statusChanged(vsp::AutomationStatus status, const QString &text);
	void notify(const QString &title, const QString &message);
	void errorOccurred(const QString &message);

private slots:
	void OnPollTick();
	void OnCountdownTick();
	void OnDurationStop();

private:
	bool TryAutoStart(const QString &reason);
	bool TryAutoStop(const QString &reason);
	void SetStatus(vsp::AutomationStatus s, const QString &detail = {});
	bool ScheduleMatchesNow(bool forEnd) const;
	QDateTime ParseLocalDateTime(const QString &date, const QString &time) const;
	bool WeekdayAllowed(const QDate &date) const;

	VerticalOutputs *outputs = nullptr;
	vsp::PluginSettings settings;
	vsp::AutomationStatus status = vsp::AutomationStatus::Disabled;

	QTimer pollTimer;
	QTimer countdownTimer;
	QTimer durationTimer;

	bool manualRecordingActive = false;
	bool automationOwnedRecording = false;
	bool obsReady = false;
	bool obsStartFired = false;
	bool countdownArmed = false;
	bool scheduleStartFiredToday = false;
	QDate lastScheduleFireDate;
	QString lastError;
	QString lastTriggerSceneUuid;
	int startFailCount = 0;
	static constexpr int kMaxStartFails = 3;
};
