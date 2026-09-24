#include "recording-automation.hpp"

#include <obs-module.h>

#include <QTimeZone>

RecordingAutomation::RecordingAutomation(VerticalOutputs *outs, QObject *parent)
	: QObject(parent),
	  outputs(outs)
{
	pollTimer.setInterval(1000);
	connect(&pollTimer, &QTimer::timeout, this, &RecordingAutomation::OnPollTick);

	countdownTimer.setSingleShot(true);
	connect(&countdownTimer, &QTimer::timeout, this, &RecordingAutomation::OnCountdownTick);

	durationTimer.setSingleShot(true);
	connect(&durationTimer, &QTimer::timeout, this, &RecordingAutomation::OnDurationStop);
}

void RecordingAutomation::ApplySettings(const vsp::PluginSettings &s)
{
	settings = s;
	startFailCount = 0;

	if (!settings.automationEnabled) {
		countdownTimer.stop();
		durationTimer.stop();
		pollTimer.stop();
		countdownArmed = false;
		SetStatus(vsp::AutomationStatus::Disabled);
		return;
	}

	if (!pollTimer.isActive())
		pollTimer.start();

	if (settings.autoStartOnCountdown && !countdownArmed && obsReady && !outputs->IsRecording()) {
		countdownArmed = true;
		countdownTimer.start(settings.countdownSeconds * 1000);
		SetStatus(vsp::AutomationStatus::Waiting,
			  QStringLiteral("Countdown: %1 s").arg(settings.countdownSeconds));
	}

	if (settings.autoStartOnSchedule)
		SetStatus(vsp::AutomationStatus::Scheduled,
			  QStringLiteral("Local time zone: %1")
				  .arg(QString::fromUtf8(QTimeZone::systemTimeZoneId())));
	else if (status == vsp::AutomationStatus::Disabled || status == vsp::AutomationStatus::Completed)
		SetStatus(vsp::AutomationStatus::Waiting);
}

void RecordingAutomation::SetManualRecordingActive(bool manual)
{
	manualRecordingActive = manual;
	if (manual)
		automationOwnedRecording = false;
}

void RecordingAutomation::ResetRuntimeState()
{
	countdownTimer.stop();
	durationTimer.stop();
	pollTimer.stop();
	countdownArmed = false;
	obsStartFired = false;
	scheduleStartFiredToday = false;
	startFailCount = 0;
	automationOwnedRecording = false;
	lastError.clear();
	if (settings.automationEnabled && !pollTimer.isActive())
		pollTimer.start();
}

QString RecordingAutomation::StatusText() const
{
	QString base = vsp::AutomationStatusLabel(status);
	if (!lastError.isEmpty() && status == vsp::AutomationStatus::Error)
		base += QStringLiteral(" — %1").arg(lastError);
	return base;
}

void RecordingAutomation::SetStatus(vsp::AutomationStatus s, const QString &detail)
{
	status = s;
	QString text = vsp::AutomationStatusLabel(s);
	if (!detail.isEmpty())
		text += QStringLiteral(" — %1").arg(detail);
	emit statusChanged(s, text);
}

bool RecordingAutomation::TryAutoStart(const QString &reason)
{
	if (!settings.automationEnabled || !outputs)
		return false;
	if (outputs->IsRecording() || manualRecordingActive) {
		blog(LOG_INFO, "[obs-shorts-vertical] Auto-start skipped (%s): vertical recording already active",
		     reason.toUtf8().constData());
		return false;
	}
	if (startFailCount >= kMaxStartFails) {
		SetStatus(vsp::AutomationStatus::Error, QStringLiteral("Too many start failures — fix settings and re-enable."));
		return false;
	}

	SetStatus(vsp::AutomationStatus::Starting, reason);
	QString error;
	if (!outputs->StartRecording(&error)) {
		++startFailCount;
		lastError = error;
		SetStatus(vsp::AutomationStatus::Error, error);
		emit errorOccurred(error);
		emit notify(QStringLiteral("Vertical Recording Automation"),
			    QStringLiteral("Automatic start failed:\n%1").arg(error));
		blog(LOG_WARNING, "[obs-shorts-vertical] Auto-start failed (%s): %s", reason.toUtf8().constData(),
		     error.toUtf8().constData());
		return false;
	}

	automationOwnedRecording = true;
	startFailCount = 0;
	SetStatus(vsp::AutomationStatus::Recording, reason);
	emit notify(QStringLiteral("Vertical Recording Automation"),
		    QStringLiteral("Vertical recording started automatically (%1).\n%2")
			    .arg(reason, outputs->LastRecordingPath()));
	blog(LOG_INFO, "[obs-shorts-vertical] Auto-start succeeded (%s)", reason.toUtf8().constData());

	if (settings.autoStopOnDuration && settings.autoRecordDurationSeconds > 0) {
		durationTimer.start(settings.autoRecordDurationSeconds * 1000);
		const QDateTime stopAt = QDateTime::currentDateTime().addSecs(settings.autoRecordDurationSeconds);
		emit notify(QStringLiteral("Vertical Recording Automation"),
			    QStringLiteral("Will auto-stop at %1 (local time).")
				    .arg(stopAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
	}
	return true;
}

bool RecordingAutomation::TryAutoStop(const QString &reason)
{
	if (!settings.automationEnabled || !outputs)
		return false;
	if (!outputs->IsRecording())
		return false;

	SetStatus(vsp::AutomationStatus::Stopping, reason);
	outputs->StopRecording();
	durationTimer.stop();
	automationOwnedRecording = false;
	SetStatus(vsp::AutomationStatus::Completed, reason);
	emit notify(QStringLiteral("Vertical Recording Automation"),
		    QStringLiteral("Vertical recording stopped automatically (%1).").arg(reason));
	blog(LOG_INFO, "[obs-shorts-vertical] Auto-stop (%s)", reason.toUtf8().constData());
	return true;
}

void RecordingAutomation::OnObsFinishedLoading()
{
	obsReady = true;
	if (!settings.automationEnabled)
		return;
	if (!pollTimer.isActive())
		pollTimer.start();

	if (settings.autoStartOnObsStart && !obsStartFired) {
		obsStartFired = true;
		TryAutoStart(QStringLiteral("OBS started"));
	}

	if (settings.autoStartOnCountdown && !countdownArmed) {
		countdownArmed = true;
		countdownTimer.start(settings.countdownSeconds * 1000);
		SetStatus(vsp::AutomationStatus::Waiting,
			  QStringLiteral("Countdown: %1 s").arg(settings.countdownSeconds));
	}
}

void RecordingAutomation::OnMainStreamingStarted()
{
	if (settings.autoStartOnMainStream)
		TryAutoStart(QStringLiteral("Main OBS streaming started"));
}

void RecordingAutomation::OnMainStreamingStopped()
{
	if (settings.autoStopOnMainStreamStop)
		TryAutoStop(QStringLiteral("Main OBS streaming stopped"));
}

void RecordingAutomation::OnSceneChanged(const QString &sceneUuid, const QString &sceneName)
{
	if (!settings.automationEnabled)
		return;

	const bool matchUuid = !settings.triggerSceneUuid.isEmpty() && settings.triggerSceneUuid == sceneUuid;
	const bool matchName = !settings.triggerSceneName.isEmpty() && settings.triggerSceneName == sceneName;
	const bool isTrigger = matchUuid || matchName;
	const bool previouslyActive = !lastTriggerSceneUuid.isEmpty();

	if (settings.autoStartOnScene && isTrigger)
		TryAutoStart(QStringLiteral("Scene active: %1").arg(sceneName));

	if (settings.autoStopOnSceneInactive && previouslyActive && !isTrigger)
		TryAutoStop(QStringLiteral("Trigger scene inactive"));

	if (isTrigger) {
		if (!settings.triggerSceneUuid.isEmpty())
			lastTriggerSceneUuid = settings.triggerSceneUuid;
		else if (!sceneUuid.isEmpty())
			lastTriggerSceneUuid = sceneUuid;
		else
			lastTriggerSceneUuid = sceneName;
	} else {
		lastTriggerSceneUuid.clear();
	}
}

void RecordingAutomation::OnVerticalLiveStarted()
{
	if (settings.autoStartOnVerticalLive)
		TryAutoStart(QStringLiteral("Vertical Shorts live started"));
}

void RecordingAutomation::OnVerticalLiveStopped()
{
	if (settings.autoStopOnVerticalLiveStop)
		TryAutoStop(QStringLiteral("Vertical Shorts live stopped"));
}

void RecordingAutomation::OnObsShutdown()
{
	if (settings.autoStopOnObsShutdown && outputs && outputs->IsRecording()) {
		TryAutoStop(QStringLiteral("OBS shutting down"));
	}
	pollTimer.stop();
	countdownTimer.stop();
	durationTimer.stop();
}

void RecordingAutomation::OnVerticalRecordingChanged(bool active)
{
	if (!active) {
		durationTimer.stop();
		if (status == vsp::AutomationStatus::Recording || status == vsp::AutomationStatus::Stopping) {
			if (settings.automationEnabled)
				SetStatus(vsp::AutomationStatus::Waiting);
		}
		automationOwnedRecording = false;
	} else if (settings.automationEnabled && status != vsp::AutomationStatus::Recording) {
		SetStatus(vsp::AutomationStatus::Recording);
	}
}

void RecordingAutomation::OnCountdownTick()
{
	if (settings.autoStartOnCountdown)
		TryAutoStart(QStringLiteral("Countdown finished"));
}

void RecordingAutomation::OnDurationStop()
{
	if (settings.autoStopOnDuration)
		TryAutoStop(QStringLiteral("Configured recording duration reached"));
}

QDateTime RecordingAutomation::ParseLocalDateTime(const QString &date, const QString &time) const
{
	if (date.isEmpty() || time.isEmpty())
		return {};
	return QDateTime::fromString(date + QLatin1Char(' ') + time, QStringLiteral("yyyy-MM-dd HH:mm"));
}

bool RecordingAutomation::WeekdayAllowed(const QDate &date) const
{
	if (settings.scheduleRepeat != vsp::ScheduleRepeat::Weekdays &&
	    settings.scheduleRepeat != vsp::ScheduleRepeat::Weekly)
		return true;
	/* Qt: Mon=1 ... Sun=7 → bit0=Mon */
	const int bit = date.dayOfWeek() - 1;
	if (settings.scheduleWeekdaysMask == 0)
		return true;
	return (settings.scheduleWeekdaysMask & (1 << bit)) != 0;
}

bool RecordingAutomation::ScheduleMatchesNow(bool forEnd) const
{
	const QString date = forEnd ? settings.scheduleEndDate : settings.scheduleStartDate;
	const QString time = forEnd ? settings.scheduleEndTime : settings.scheduleStartTime;
	const QDateTime target = ParseLocalDateTime(date, time);
	if (!target.isValid())
		return false;

	const QDateTime now = QDateTime::currentDateTime();

	switch (settings.scheduleRepeat) {
	case vsp::ScheduleRepeat::Once:
		return now >= target && now < target.addSecs(60);
	case vsp::ScheduleRepeat::Daily:
	case vsp::ScheduleRepeat::Weekdays:
	case vsp::ScheduleRepeat::Weekly: {
		if (!WeekdayAllowed(now.date()))
			return false;
		const QTime t = target.time();
		return now.time().hour() == t.hour() && now.time().minute() == t.minute() && now.time().second() < 2;
	}
	}
	return false;
}

void RecordingAutomation::OnPollTick()
{
	if (!settings.automationEnabled)
		return;

	if (settings.autoStartOnSchedule && ScheduleMatchesNow(false)) {
		const QDate today = QDate::currentDate();
		if (lastScheduleFireDate != today || settings.scheduleRepeat == vsp::ScheduleRepeat::Once) {
			if (!(settings.scheduleRepeat == vsp::ScheduleRepeat::Once && scheduleStartFiredToday)) {
				lastScheduleFireDate = today;
				scheduleStartFiredToday = true;
				TryAutoStart(QStringLiteral("Scheduled start (%1)")
						     .arg(QString::fromUtf8(QTimeZone::systemTimeZoneId())));
			}
		}
	}

	if (settings.autoStopOnScheduleEnd && ScheduleMatchesNow(true))
		TryAutoStop(QStringLiteral("Scheduled end"));
}
