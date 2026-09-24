#include "settings-dialog.hpp"
#include "credential-store.hpp"
#include "platform-logo.hpp"
#include "platform-selector.hpp"
#include "plugin-support.h"

#include <algorithm>
#include <QDesktopServices>
#include <QGraphicsOpacityEffect>
#include <QHideEvent>
#include <QIcon>
#include <QPropertyAnimation>
#include <QUrl>

#include <obs-module.h>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimeEdit>
#include <QTimeZone>
#include <QVBoxLayout>

namespace {

QWidget *WrapScroll(QWidget *inner)
{
	auto *scroll = new QScrollArea();
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidget(inner);
	return scroll;
}

} // namespace

int SettingsDialog::AddCategory(const char *localeKey, const char *fallback, QWidget *page)
{
	QString title = QString::fromUtf8(obs_module_text(localeKey));
	if (title.isEmpty() || title == QString::fromUtf8(localeKey))
		title = QString::fromUtf8(fallback);
	const int index = pages->addWidget(WrapScroll(page));
	categories->addItem(title);
	return index;
}

SettingsDialog::SettingsDialog(vsp::PluginSettings s, VerticalOutputs *outs, const QStringList &names,
			       const QStringList &uuids, vsp::AutomationStatus autoStatus, const QString &autoText,
			       QWidget *parent)
	: QDialog(parent),
	  settings(std::move(s)),
	  outputs(outs),
	  sceneNames(names),
	  sceneUuids(uuids),
	  automationStatus(autoStatus),
	  automationStatusText(autoText)
{
	setWindowTitle(QString::fromUtf8(obs_module_text("Settings")));
	setModal(true);
	setMinimumSize(720, 520);
	resize(900, 640);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(8, 8, 8, 8);
	root->setSpacing(8);

	/* Category list on the left, pages on the right (opposite of previous layout). */
	auto *body = new QHBoxLayout();
	body->setSpacing(8);

	pages = new QStackedWidget(this);
	categories = new QListWidget(this);
	categories->setFixedWidth(180);
	categories->setSelectionMode(QAbstractItemView::SingleSelection);
	connect(categories, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);

	auto *general = new QWidget();
	BuildGeneralTab(general);
	AddCategory("TabGeneral", "General", general);

	auto *canvas = new QWidget();
	BuildCanvasTab(canvas);
	AddCategory("TabVerticalCanvas", "Vertical Canvas", canvas);

	auto *recording = new QWidget();
	BuildRecordingTab(recording);
	AddCategory("TabVerticalRecording", "Vertical Recording", recording);

	auto *clips = new QWidget();
	BuildClipsTab(clips);
	AddCategory("TabVerticalClips", "Vertical Clips", clips);

	auto *automation = new QWidget();
	BuildAutomationTab(automation);
	AddCategory("TabVerticalRecordingAutomation", "Recording Automation", automation);

	auto *streaming = new QWidget();
	BuildStreamingTab(streaming);
	streamingTabIndex = AddCategory("TabVerticalStreaming", "Vertical Streaming", streaming);

	auto *audio = new QWidget();
	BuildAudioTab(audio);
	AddCategory("TabAudio", "Audio", audio);

	auto *about = new QWidget();
	BuildAboutTab(about);
	AddCategory("TabAbout", "About", about);

	body->addWidget(categories, 0);
	body->addWidget(pages, 1);
	root->addLayout(body, 1);

	if (categories->count() > 0)
		categories->setCurrentRow(0);

	auto *btnRow = new QHBoxLayout();
	auto *resetBtn = new QPushButton(QString::fromUtf8(obs_module_text("ResetDefaults")), this);
	connect(resetBtn, &QPushButton::clicked, this, &SettingsDialog::OnResetDefaults);
	btnRow->addWidget(resetBtn);
	btnRow->addStretch(1);
	buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply,
					 this);
	applyBtn = buttonBox->button(QDialogButtonBox::Apply);
	connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::OnAccepted);
	connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(applyBtn, &QPushButton::clicked, this, &SettingsDialog::OnApply);
	btnRow->addWidget(buttonBox);
	root->addLayout(btnRow);

	vsp::EnsureDefaultDestinations(settings);
	LoadSecretsIntoDestinations();
	baselineSettings = settings;
	SyncFieldsFromSettings();
	HookDirtyTracking();
	ClearDirty();
}

void SettingsDialog::BuildGeneralTab(QWidget *tab)
{
	auto *lay = new QVBoxLayout(tab);
	auto *help = new QLabel(QString::fromUtf8(obs_module_text("GeneralHelp")), tab);
	help->setWordWrap(true);
	lay->addWidget(help);
	lay->addStretch(1);
}

void SettingsDialog::BuildAudioTab(QWidget *tab)
{
	auto *lay = new QVBoxLayout(tab);
	auto *help = new QLabel(QString::fromUtf8(obs_module_text("AudioHelp")), tab);
	help->setWordWrap(true);
	lay->addWidget(help);
	lay->addStretch(1);
}

void SettingsDialog::BuildAboutTab(QWidget *tab)
{
	auto *lay = new QVBoxLayout(tab);
	auto *version = new QLabel(QString::fromUtf8(obs_module_text("PluginVersionLabel")), tab);
	if (version->text().isEmpty() || version->text() == QStringLiteral("PluginVersionLabel"))
		version->setText(QStringLiteral("Vertical Shorts Plugin %1").arg(QString::fromUtf8(PLUGIN_VERSION)));
	QFont vf = version->font();
	vf.setBold(true);
	version->setFont(vf);
	lay->addWidget(version);

	auto *built = new QLabel(tab);
	built->setText(QStringLiteral("Built: %1 UTC").arg(QString::fromUtf8(PLUGIN_BUILD_TIMESTAMP)));
	built->setWordWrap(true);
	lay->addWidget(built);

	auto *desc = new QLabel(QString::fromUtf8(obs_module_text("AboutHelp")), tab);
	desc->setWordWrap(true);
	lay->addWidget(desc);
	lay->addStretch(1);
}

void SettingsDialog::SetApplyEnabled(bool enabled)
{
	if (applyBtn)
		applyBtn->setEnabled(enabled);
}

void SettingsDialog::ClearDirty()
{
	dirty = false;
	baselineSettings = settings;
	SetApplyEnabled(false);
}

void SettingsDialog::MarkDirty()
{
	if (loadingFields)
		return;
	RecalcDirty();
}

void SettingsDialog::RecalcDirty()
{
	if (loadingFields)
		return;
	dirty = UiDiffersFromBaseline();
	SetApplyEnabled(dirty);
}

void SettingsDialog::HookDirtyTracking()
{
	auto hookCombo = [this](QComboBox *w) {
		if (!w)
			return;
		connect(w, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::MarkDirty);
	};
	auto hookSpin = [this](QSpinBox *w) {
		if (!w)
			return;
		connect(w, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingsDialog::MarkDirty);
	};
	auto hookEdit = [this](QLineEdit *w) {
		if (!w)
			return;
		connect(w, &QLineEdit::textChanged, this, &SettingsDialog::MarkDirty);
	};
	auto hookCheck = [this](QCheckBox *w) {
		if (!w)
			return;
		connect(w, &QCheckBox::toggled, this, &SettingsDialog::MarkDirty);
	};
	auto hookDate = [this](QDateEdit *w) {
		if (!w)
			return;
		connect(w, &QDateEdit::dateChanged, this, &SettingsDialog::MarkDirty);
	};
	auto hookTime = [this](QTimeEdit *w) {
		if (!w)
			return;
		connect(w, &QTimeEdit::timeChanged, this, &SettingsDialog::MarkDirty);
	};

	hookCombo(presetCombo);
	hookSpin(widthSpin);
	hookSpin(heightSpin);
	hookEdit(pathEdit);
	hookCombo(shortClipCombo);
	hookSpin(shortCustomSpin);
	hookCombo(longClipCombo);
	hookEdit(longCustomEdit);
	hookSpin(longCustomMin);
	hookSpin(longCustomSec);
	hookCheck(clipBufferCheck);
	hookCheck(autoStartBufferCheck);
	hookCheck(stopIdleCheck);
	hookSpin(idleTimeoutSpin);
	hookCheck(saveAvailableCheck);
	hookCheck(bufferOnLiveCheck);
	hookCheck(bufferOnRecordCheck);

	hookCheck(autoMaster);
	hookCheck(startMainStream);
	hookCheck(startScene);
	hookCheck(startObs);
	hookCheck(startSchedule);
	hookCheck(startCountdown);
	hookCheck(startVerticalLive);
	hookCheck(stopMainStream);
	hookCheck(stopScene);
	hookCheck(stopDuration);
	hookCheck(stopScheduleEnd);
	hookCheck(stopVerticalLive);
	hookCheck(stopObsShutdown);
	hookCheck(confirmManualStop);
	hookCombo(sceneCombo);
	hookSpin(durationH);
	hookSpin(durationM);
	hookSpin(durationS);
	hookSpin(countdownSpin);
	hookDate(schedStartDate);
	hookTime(schedStartTime);
	hookDate(schedEndDate);
	hookTime(schedEndTime);
	hookCombo(schedRepeat);
	for (QCheckBox *day : weekdayChecks)
		hookCheck(day);

	hookCombo(destinationPicker);
	hookEdit(destNameEdit);
	hookEdit(verticalServerEdit);
	hookEdit(verticalKeyEdit);
	hookEdit(usernameEdit);
	hookEdit(passwordEdit);
	hookCombo(twitchIngestCombo);
	if (platformSelector)
		connect(platformSelector, &PlatformSelector::platformChanged, this, &SettingsDialog::MarkDirty);
}

bool SettingsDialog::UiDiffersFromBaseline() const
{
	if (!presetCombo || !widthSpin || !heightSpin || !pathEdit)
		return dirty;

	if (static_cast<vsp::CanvasPreset>(presetCombo->currentData().toInt()) != baselineSettings.canvasPreset)
		return true;
	if ((uint32_t)widthSpin->value() != baselineSettings.customWidth)
		return true;
	if ((uint32_t)heightSpin->value() != baselineSettings.customHeight)
		return true;
	if (pathEdit->text().trimmed() != baselineSettings.recordingPath)
		return true;

	if (shortClipCombo &&
	    static_cast<vsp::ShortClipPreset>(shortClipCombo->currentData().toInt()) != baselineSettings.shortClipPreset)
		return true;
	if (shortCustomSpin && shortCustomSpin->value() != baselineSettings.customShortClipSeconds)
		return true;
	if (longClipCombo &&
	    static_cast<vsp::LongClipPreset>(longClipCombo->currentData().toInt()) != baselineSettings.longClipPreset)
		return true;
	if (longCustomMin && longCustomSec) {
		const int secs = longCustomMin->value() * 60 + longCustomSec->value();
		if (secs != baselineSettings.customLongClipSeconds)
			return true;
	}

	auto checkDiff = [&](QCheckBox *w, bool baseline) {
		return w && w->isChecked() != baseline;
	};
	if (checkDiff(clipBufferCheck, baselineSettings.clipBufferEnabled))
		return true;
	if (checkDiff(autoStartBufferCheck, baselineSettings.autoStartClipBuffer))
		return true;
	if (checkDiff(stopIdleCheck, baselineSettings.stopBufferWhenIdle))
		return true;
	if (idleTimeoutSpin && idleTimeoutSpin->value() != baselineSettings.bufferIdleTimeoutSeconds)
		return true;
	if (checkDiff(saveAvailableCheck, baselineSettings.saveAvailableWhenShort))
		return true;
	if (checkDiff(bufferOnLiveCheck, baselineSettings.bufferStartOnVerticalLive))
		return true;
	if (checkDiff(bufferOnRecordCheck, baselineSettings.bufferStartOnVerticalRecord))
		return true;

	if (checkDiff(autoMaster, baselineSettings.automationEnabled))
		return true;
	if (checkDiff(startMainStream, baselineSettings.autoStartOnMainStream))
		return true;
	if (checkDiff(startScene, baselineSettings.autoStartOnScene))
		return true;
	if (checkDiff(startObs, baselineSettings.autoStartOnObsStart))
		return true;
	if (checkDiff(startSchedule, baselineSettings.autoStartOnSchedule))
		return true;
	if (checkDiff(startCountdown, baselineSettings.autoStartOnCountdown))
		return true;
	if (checkDiff(startVerticalLive, baselineSettings.autoStartOnVerticalLive))
		return true;
	if (checkDiff(stopMainStream, baselineSettings.autoStopOnMainStreamStop))
		return true;
	if (checkDiff(stopScene, baselineSettings.autoStopOnSceneInactive))
		return true;
	if (checkDiff(stopDuration, baselineSettings.autoStopOnDuration))
		return true;
	if (checkDiff(stopScheduleEnd, baselineSettings.autoStopOnScheduleEnd))
		return true;
	if (checkDiff(stopVerticalLive, baselineSettings.autoStopOnVerticalLiveStop))
		return true;
	if (checkDiff(stopObsShutdown, baselineSettings.autoStopOnObsShutdown))
		return true;
	if (checkDiff(confirmManualStop, baselineSettings.confirmManualStopDuringAutomation))
		return true;
	if (sceneCombo && sceneCombo->currentData().toString() != baselineSettings.triggerSceneUuid)
		return true;
	if (durationH && durationM && durationS) {
		const int secs = durationH->value() * 3600 + durationM->value() * 60 + durationS->value();
		if (secs != baselineSettings.autoRecordDurationSeconds)
			return true;
	}
	if (countdownSpin && countdownSpin->value() != baselineSettings.countdownSeconds)
		return true;
	if (schedStartDate &&
	    schedStartDate->date().toString(QStringLiteral("yyyy-MM-dd")) != baselineSettings.scheduleStartDate)
		return true;
	if (schedStartTime &&
	    schedStartTime->time().toString(QStringLiteral("HH:mm")) != baselineSettings.scheduleStartTime)
		return true;
	if (schedEndDate &&
	    schedEndDate->date().toString(QStringLiteral("yyyy-MM-dd")) != baselineSettings.scheduleEndDate)
		return true;
	if (schedEndTime && schedEndTime->time().toString(QStringLiteral("HH:mm")) != baselineSettings.scheduleEndTime)
		return true;
	if (schedRepeat &&
	    static_cast<vsp::ScheduleRepeat>(schedRepeat->currentData().toInt()) != baselineSettings.scheduleRepeat)
		return true;
	int dayMask = 0;
	for (int d = 0; d < 7; ++d) {
		if (weekdayChecks[d] && weekdayChecks[d]->isChecked())
			dayMask |= (1 << d);
	}
	if (dayMask != baselineSettings.scheduleWeekdaysMask)
		return true;

	if (destinationPicker) {
		const QString id = destinationPicker->currentData().toString();
		if (id != baselineSettings.activeDestinationId)
			return true;
	}
	const vsp::StreamDestination baselineDest = vsp::ActiveDestination(baselineSettings);
	if (platformSelector && SelectedPlatform() != baselineDest.platform)
		return true;
	if (destNameEdit && destNameEdit->text().trimmed() != baselineDest.name)
		return true;
	if (verticalServerEdit && verticalServerEdit->text().trimmed() != baselineDest.server)
		return true;
	if (verticalKeyEdit && verticalKeyEdit->text() != baselineDest.streamKey)
		return true;
	if (usernameEdit && usernameEdit->text() != baselineDest.username)
		return true;
	if (passwordEdit && passwordEdit->text() != baselineDest.password)
		return true;

	if (resetAutomation)
		return true;

	return false;
}

void SettingsDialog::OnApply()
{
	QString error;
	QString warning;
	QString errorField;
	if (!ValidateAndCommit(&error, &warning, &errorField)) {
		NavigateToErrorField(errorField);
		HighlightInvalidField(errorField);
		if (!error.isEmpty())
			QMessageBox::warning(this, QString::fromUtf8(obs_module_text("Settings")), error);
		SetApplyEnabled(true);
		dirty = true;
		return;
	}
	HighlightInvalidField(QString());
	if (!warning.isEmpty())
		QMessageBox::information(this, QString::fromUtf8(obs_module_text("Settings")), warning);
	emit applied();
	/* Automation reset is consumed by the dock on applied(); clear so Apply stays off. */
	resetAutomation = false;
	ClearDirty();
}

void SettingsDialog::BuildCanvasTab(QWidget *tab)
{
	auto *form = new QFormLayout(tab);
	presetCombo = new QComboBox(tab);
	presetCombo->addItem(QString::fromUtf8(obs_module_text("PresetYouTube")),
			     (int)vsp::CanvasPreset::YouTubeVertical);
	presetCombo->addItem(QString::fromUtf8(obs_module_text("PresetTikTok")), (int)vsp::CanvasPreset::TikTokVertical);
	presetCombo->addItem(QString::fromUtf8(obs_module_text("PresetTwitch")), (int)vsp::CanvasPreset::TwitchVertical);
	presetCombo->addItem(QString::fromUtf8(obs_module_text("PresetInstagram")),
			     (int)vsp::CanvasPreset::InstagramVertical);
	presetCombo->addItem(QString::fromUtf8(obs_module_text("PresetCustom")), (int)vsp::CanvasPreset::Custom);
	connect(presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&SettingsDialog::OnCanvasPresetChanged);

	widthSpin = new QSpinBox(tab);
	widthSpin->setRange(160, 7680);
	heightSpin = new QSpinBox(tab);
	heightSpin->setRange(160, 7680);
	aspectHint = new QLabel(tab);
	aspectHint->setWordWrap(true);

	auto *help = new QLabel(QString::fromUtf8(obs_module_text("CanvasHelp")), tab);
	help->setWordWrap(true);

	form->addRow(help);
	form->addRow(QString::fromUtf8(obs_module_text("CanvasPreset")), presetCombo);
	form->addRow(QString::fromUtf8(obs_module_text("Width")), widthSpin);
	form->addRow(QString::fromUtf8(obs_module_text("Height")), heightSpin);
	form->addRow(QString(), aspectHint);
}

void SettingsDialog::BuildRecordingTab(QWidget *tab)
{
	auto *form = new QFormLayout(tab);
	pathEdit = new QLineEdit(tab);
	auto *browse = new QPushButton(QString::fromUtf8(obs_module_text("Browse")), tab);
	connect(browse, &QPushButton::clicked, this, &SettingsDialog::OnBrowsePath);
	auto *pathRow = new QHBoxLayout();
	pathRow->addWidget(pathEdit, 1);
	pathRow->addWidget(browse);

	encoderLabel = new QLabel(tab);
	bitrateLabel = new QLabel(tab);
	formatLabel = new QLabel(QString::fromUtf8(obs_module_text("FormatInherited")), tab);
	formatLabel->setWordWrap(true);
	recordStatusLabel = new QLabel(tab);
	recordStatusLabel->setWordWrap(true);

	form->addRow(QString::fromUtf8(obs_module_text("RecordingPath")), pathRow);
	form->addRow(QString::fromUtf8(obs_module_text("EncoderInherited")), encoderLabel);
	form->addRow(QString::fromUtf8(obs_module_text("BitrateInherited")), bitrateLabel);
	form->addRow(QString::fromUtf8(obs_module_text("FileFormat")), formatLabel);
	form->addRow(QString::fromUtf8(obs_module_text("RecordingStatus")), recordStatusLabel);
}

void SettingsDialog::BuildClipsTab(QWidget *tab)
{
	auto *root = new QVBoxLayout(tab);

	auto *shortBox = new QGroupBox(QString::fromUtf8(obs_module_text("ShortClip")), tab);
	auto *shortForm = new QFormLayout(shortBox);
	shortClipCombo = new QComboBox(shortBox);
	shortClipCombo->addItem(QString::fromUtf8(obs_module_text("Clip10")), (int)vsp::ShortClipPreset::Sec10);
	shortClipCombo->addItem(QString::fromUtf8(obs_module_text("Clip20")), (int)vsp::ShortClipPreset::Sec20);
	shortClipCombo->addItem(QString::fromUtf8(obs_module_text("Clip30")), (int)vsp::ShortClipPreset::Sec30);
	shortClipCombo->addItem(QString::fromUtf8(obs_module_text("Clip60")), (int)vsp::ShortClipPreset::Sec60);
	shortClipCombo->addItem(QString::fromUtf8(obs_module_text("ClipCustom")), (int)vsp::ShortClipPreset::Custom);
	connect(shortClipCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&SettingsDialog::OnShortClipPresetChanged);
	shortCustomSpin = new QSpinBox(shortBox);
	shortCustomSpin->setRange(1, vsp::kMaxClipBufferSeconds);
	shortCustomSpin->setSuffix(QStringLiteral(" s"));
	shortForm->addRow(QString::fromUtf8(obs_module_text("ShortClipLength")), shortClipCombo);
	shortForm->addRow(QString::fromUtf8(obs_module_text("CustomShortClipLength")), shortCustomSpin);
	root->addWidget(shortBox);

	auto *longBox = new QGroupBox(QString::fromUtf8(obs_module_text("LongClip")), tab);
	auto *longForm = new QFormLayout(longBox);
	longClipCombo = new QComboBox(longBox);
	longClipCombo->addItem(QString::fromUtf8(obs_module_text("LongClip2Min")), (int)vsp::LongClipPreset::Min2);
	longClipCombo->addItem(QString::fromUtf8(obs_module_text("LongClip3Min")), (int)vsp::LongClipPreset::Min3);
	longClipCombo->addItem(QString::fromUtf8(obs_module_text("LongClip4Min")), (int)vsp::LongClipPreset::Min4);
	longClipCombo->addItem(QString::fromUtf8(obs_module_text("LongClip5Min")), (int)vsp::LongClipPreset::Min5);
	longClipCombo->addItem(QString::fromUtf8(obs_module_text("LongClipCustom")), (int)vsp::LongClipPreset::Custom);
	connect(longClipCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&SettingsDialog::OnLongClipPresetChanged);

	auto *customRow = new QHBoxLayout();
	longCustomMin = new QSpinBox(longBox);
	longCustomMin->setRange(0, vsp::kMaxClipBufferSeconds / 60);
	longCustomMin->setSuffix(QStringLiteral(" min"));
	longCustomSec = new QSpinBox(longBox);
	longCustomSec->setRange(0, 59);
	longCustomSec->setSuffix(QStringLiteral(" s"));
	longCustomEdit = new QLineEdit(longBox);
	longCustomEdit->setPlaceholderText(QStringLiteral("MM:SS"));
	longCustomEdit->setMaximumWidth(80);
	customRow->addWidget(longCustomMin);
	customRow->addWidget(longCustomSec);
	customRow->addWidget(new QLabel(QStringLiteral("or"), longBox));
	customRow->addWidget(longCustomEdit);
	customRow->addStretch(1);

	longWarnLabel = new QLabel(longBox);
	longWarnLabel->setWordWrap(true);
	longWarnLabel->setStyleSheet(QStringLiteral("color: #a06000;"));

	longForm->addRow(QString::fromUtf8(obs_module_text("LongClipLength")), longClipCombo);
	longForm->addRow(QString::fromUtf8(obs_module_text("CustomLongClipLength")), customRow);
	longForm->addRow(QString(), longWarnLabel);
	root->addWidget(longBox);

	clipBufferCheck = new QCheckBox(QString::fromUtf8(obs_module_text("ClipBufferEnabled")), tab);
	root->addWidget(clipBufferCheck);
	autoStartBufferCheck = new QCheckBox(QString::fromUtf8(obs_module_text("AutoStartClipBuffer")), tab);
	root->addWidget(autoStartBufferCheck);
	stopIdleCheck = new QCheckBox(QString::fromUtf8(obs_module_text("StopBufferWhenIdle")), tab);
	root->addWidget(stopIdleCheck);
	idleTimeoutSpin = new QSpinBox(tab);
	idleTimeoutSpin->setRange(30, 7200);
	idleTimeoutSpin->setSuffix(QStringLiteral(" s"));
	auto *idleForm = new QFormLayout();
	idleForm->addRow(QString::fromUtf8(obs_module_text("BufferIdleTimeout")), idleTimeoutSpin);
	root->addLayout(idleForm);
	saveAvailableCheck = new QCheckBox(QString::fromUtf8(obs_module_text("SaveAvailableWhenShort")), tab);
	root->addWidget(saveAvailableCheck);
	bufferOnLiveCheck = new QCheckBox(QString::fromUtf8(obs_module_text("BufferStartOnLive")), tab);
	root->addWidget(bufferOnLiveCheck);
	bufferOnRecordCheck = new QCheckBox(QString::fromUtf8(obs_module_text("BufferStartOnRecord")), tab);
	root->addWidget(bufferOnRecordCheck);
	bufferStatusInSettings = new QLabel(tab);
	bufferStatusInSettings->setWordWrap(true);
	root->addWidget(bufferStatusInSettings);

	auto *note = new QLabel(QString::fromUtf8(obs_module_text("ClipsHelp")), tab);
	note->setWordWrap(true);
	root->addWidget(note);
	auto *preroll = new QLabel(QString::fromUtf8(obs_module_text("BufferPrerollNote")), tab);
	preroll->setWordWrap(true);
	root->addWidget(preroll);
	root->addStretch(1);
}

void SettingsDialog::BuildAutomationTab(QWidget *tab)
{
	auto *root = new QVBoxLayout(tab);

	autoMaster = new QCheckBox(QString::fromUtf8(obs_module_text("AutomationMaster")), tab);
	root->addWidget(autoMaster);

	auto *help = new QLabel(QString::fromUtf8(obs_module_text("AutomationHelp")), tab);
	help->setWordWrap(true);
	root->addWidget(help);

	autoStatusLabel = new QLabel(tab);
	autoStatusLabel->setWordWrap(true);
	root->addWidget(autoStatusLabel);

	tzLabel = new QLabel(QStringLiteral("Time zone: %1").arg(QString::fromUtf8(QTimeZone::systemTimeZoneId())),
			     tab);
	root->addWidget(tzLabel);

	auto *startBox = new QGroupBox(QString::fromUtf8(obs_module_text("AutoStartTriggers")), tab);
	auto *startLay = new QVBoxLayout(startBox);
	startMainStream = new QCheckBox(QString::fromUtf8(obs_module_text("StartOnMainStream")), startBox);
	startScene = new QCheckBox(QString::fromUtf8(obs_module_text("StartOnScene")), startBox);
	startObs = new QCheckBox(QString::fromUtf8(obs_module_text("StartOnObsStart")), startBox);
	startSchedule = new QCheckBox(QString::fromUtf8(obs_module_text("StartOnSchedule")), startBox);
	startCountdown = new QCheckBox(QString::fromUtf8(obs_module_text("StartOnCountdown")), startBox);
	startVerticalLive = new QCheckBox(QString::fromUtf8(obs_module_text("StartOnVerticalLive")), startBox);
	startLay->addWidget(startMainStream);
	startLay->addWidget(startScene);
	startLay->addWidget(startObs);
	startLay->addWidget(startSchedule);
	startLay->addWidget(startCountdown);
	startLay->addWidget(startVerticalLive);
	root->addWidget(startBox);

	auto *stopBox = new QGroupBox(QString::fromUtf8(obs_module_text("AutoStopTriggers")), tab);
	auto *stopLay = new QVBoxLayout(stopBox);
	stopMainStream = new QCheckBox(QString::fromUtf8(obs_module_text("StopOnMainStream")), stopBox);
	stopScene = new QCheckBox(QString::fromUtf8(obs_module_text("StopOnSceneInactive")), stopBox);
	stopDuration = new QCheckBox(QString::fromUtf8(obs_module_text("StopOnDuration")), stopBox);
	stopScheduleEnd = new QCheckBox(QString::fromUtf8(obs_module_text("StopOnScheduleEnd")), stopBox);
	stopVerticalLive = new QCheckBox(QString::fromUtf8(obs_module_text("StopOnVerticalLive")), stopBox);
	stopObsShutdown = new QCheckBox(QString::fromUtf8(obs_module_text("StopOnObsShutdown")), stopBox);
	stopLay->addWidget(stopMainStream);
	stopLay->addWidget(stopScene);
	stopLay->addWidget(stopDuration);
	stopLay->addWidget(stopScheduleEnd);
	stopLay->addWidget(stopVerticalLive);
	stopLay->addWidget(stopObsShutdown);
	root->addWidget(stopBox);

	auto *sceneForm = new QFormLayout();
	sceneCombo = new QComboBox(tab);
	sceneCombo->addItem(QString::fromUtf8(obs_module_text("NoSceneSelected")), QString());
	for (int i = 0; i < sceneNames.size(); ++i) {
		const QString uuid = i < sceneUuids.size() ? sceneUuids[i] : QString();
		sceneCombo->addItem(sceneNames[i], uuid);
	}
	sceneForm->addRow(QString::fromUtf8(obs_module_text("TriggerScene")), sceneCombo);
	root->addLayout(sceneForm);

	auto *durRow = new QHBoxLayout();
	durationH = new QSpinBox(tab);
	durationH->setRange(0, 24);
	durationH->setSuffix(QStringLiteral(" h"));
	durationM = new QSpinBox(tab);
	durationM->setRange(0, 59);
	durationM->setSuffix(QStringLiteral(" m"));
	durationS = new QSpinBox(tab);
	durationS->setRange(0, 59);
	durationS->setSuffix(QStringLiteral(" s"));
	durRow->addWidget(durationH);
	durRow->addWidget(durationM);
	durRow->addWidget(durationS);
	durRow->addStretch(1);
	expectedStopLabel = new QLabel(tab);
	auto *durForm = new QFormLayout();
	durForm->addRow(QString::fromUtf8(obs_module_text("AutoRecordDuration")), durRow);
	durForm->addRow(QString(), expectedStopLabel);
	countdownSpin = new QSpinBox(tab);
	countdownSpin->setRange(1, 3600);
	countdownSpin->setSuffix(QStringLiteral(" s"));
	durForm->addRow(QString::fromUtf8(obs_module_text("CountdownSeconds")), countdownSpin);
	root->addLayout(durForm);

	auto *schedBox = new QGroupBox(QString::fromUtf8(obs_module_text("Schedule")), tab);
	auto *schedForm = new QFormLayout(schedBox);
	schedStartDate = new QDateEdit(tab);
	schedStartDate->setCalendarPopup(true);
	schedStartDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
	schedStartTime = new QTimeEdit(tab);
	schedStartTime->setDisplayFormat(QStringLiteral("HH:mm"));
	schedEndDate = new QDateEdit(tab);
	schedEndDate->setCalendarPopup(true);
	schedEndDate->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
	schedEndTime = new QTimeEdit(tab);
	schedEndTime->setDisplayFormat(QStringLiteral("HH:mm"));
	schedRepeat = new QComboBox(tab);
	schedRepeat->addItem(QString::fromUtf8(obs_module_text("RepeatOnce")), (int)vsp::ScheduleRepeat::Once);
	schedRepeat->addItem(QString::fromUtf8(obs_module_text("RepeatDaily")), (int)vsp::ScheduleRepeat::Daily);
	schedRepeat->addItem(QString::fromUtf8(obs_module_text("RepeatWeekly")), (int)vsp::ScheduleRepeat::Weekly);
	schedRepeat->addItem(QString::fromUtf8(obs_module_text("RepeatByDay")), (int)vsp::ScheduleRepeat::Weekdays);
	schedForm->addRow(QString::fromUtf8(obs_module_text("ScheduleStartDate")), schedStartDate);
	schedForm->addRow(QString::fromUtf8(obs_module_text("ScheduleStartTime")), schedStartTime);
	schedForm->addRow(QString::fromUtf8(obs_module_text("ScheduleEndDate")), schedEndDate);
	schedForm->addRow(QString::fromUtf8(obs_module_text("ScheduleEndTime")), schedEndTime);
	schedForm->addRow(QString::fromUtf8(obs_module_text("ScheduleRepeat")), schedRepeat);

	auto *daysRow = new QHBoxLayout();
	static const char *kDayKeys[] = {"DayMon", "DayTue", "DayWed", "DayThu", "DayFri", "DaySat", "DaySun"};
	for (int d = 0; d < 7; ++d) {
		weekdayChecks[d] = new QCheckBox(QString::fromUtf8(obs_module_text(kDayKeys[d])), schedBox);
		daysRow->addWidget(weekdayChecks[d]);
	}
	daysRow->addStretch(1);
	schedForm->addRow(QString::fromUtf8(obs_module_text("RepeatDays")), daysRow);

	auto updateDaysEnabled = [this]() {
		const bool byDay = schedRepeat &&
				   schedRepeat->currentData().toInt() == (int)vsp::ScheduleRepeat::Weekdays;
		for (QCheckBox *cb : weekdayChecks) {
			if (cb)
				cb->setEnabled(byDay);
		}
	};
	connect(schedRepeat, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		[updateDaysEnabled](int) { updateDaysEnabled(); });
	updateDaysEnabled();

	auto *schedNote = new QLabel(QString::fromUtf8(obs_module_text("ScheduleNote")), schedBox);
	schedNote->setWordWrap(true);
	schedForm->addRow(schedNote);
	root->addWidget(schedBox);

	confirmManualStop = new QCheckBox(QString::fromUtf8(obs_module_text("ConfirmManualStop")), tab);
	root->addWidget(confirmManualStop);

	auto *resetAuto = new QPushButton(QString::fromUtf8(obs_module_text("ResetAutomation")), tab);
	connect(resetAuto, &QPushButton::clicked, this, &SettingsDialog::OnResetAutomation);
	root->addWidget(resetAuto);
	root->addStretch(1);
}


void SettingsDialog::BuildStreamingTab(QWidget *tab)
{
	auto *lay = new QVBoxLayout(tab);
	lay->setSpacing(14);
	lay->setContentsMargins(8, 8, 8, 8);

	auto *hero = new QFrame(tab);
	hero->setObjectName("vspStreamingHero");
	hero->setStyleSheet(QStringLiteral(
		"QFrame#vspStreamingHero {"
		"  background: palette(base);"
		"  border: 1px solid palette(mid);"
		"  border-radius: 12px;"
		"}"));
	auto *heroLay = new QHBoxLayout(hero);
	heroLay->setContentsMargins(16, 14, 16, 14);
	heroLay->setSpacing(14);

	selectedPlatformHero = new QLabel(hero);
	selectedPlatformHero->setFixedSize(40, 40);
	selectedPlatformHero->setAlignment(Qt::AlignCenter);

	auto *heroText = new QVBoxLayout();
	heroText->setSpacing(2);
	auto *title = new QLabel(QString::fromUtf8(obs_module_text("VerticalStreamingDestination")), hero);
	QFont tf = title->font();
	tf.setPointSize(tf.pointSize() + 2);
	tf.setBold(true);
	title->setFont(tf);
	selectedPlatformTitle = new QLabel(hero);
	selectedPlatformTitle->setStyleSheet(QStringLiteral("font-weight: 600;"));
	heroText->addWidget(title);
	heroText->addWidget(selectedPlatformTitle);
	heroLay->addWidget(selectedPlatformHero, 0, Qt::AlignVCenter);
	heroLay->addLayout(heroText, 1);
	lay->addWidget(hero);

	streamHelp = new QLabel(QString::fromUtf8(obs_module_text("StreamingHelpIndependent")), tab);
	streamHelp->setWordWrap(true);
	streamHelp->setStyleSheet(QStringLiteral("color: palette(window-text); opacity: 0.9;"));
	lay->addWidget(streamHelp);

	auto *destLabel = new QLabel(QString::fromUtf8(obs_module_text("DestinationPlatform")), tab);
	destLabel->setStyleSheet(QStringLiteral("font-weight: 600;"));
	lay->addWidget(destLabel);

	platformSelector = new PlatformSelector(tab);
	connect(platformSelector, &PlatformSelector::platformChanged, this, &SettingsDialog::OnPlatformChanged);
	lay->addWidget(platformSelector);

	secureStoreLabel = new QLabel(tab);
	secureStoreLabel->setWordWrap(true);
	lay->addWidget(secureStoreLabel);

	platformPanel = new QWidget(tab);
	platformPanelOpacity = new QGraphicsOpacityEffect(platformPanel);
	platformPanelOpacity->setOpacity(1.0);
	platformPanel->setGraphicsEffect(platformPanelOpacity);
	platformPanelAnim = new QPropertyAnimation(platformPanelOpacity, "opacity", this);
	platformPanelAnim->setDuration(180);
	auto *panelLay = new QVBoxLayout(platformPanel);
	panelLay->setContentsMargins(0, 4, 0, 0);
	panelLay->setSpacing(12);

	auto *form = new QFormLayout();
	form->setHorizontalSpacing(16);
	form->setVerticalSpacing(10);

	destinationPicker = new QComboBox(platformPanel);
	connect(destinationPicker, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&SettingsDialog::OnDestinationPickerChanged);
	form->addRow(QString::fromUtf8(obs_module_text("SavedDestinations")), destinationPicker);

	auto *destBtns = new QHBoxLayout();
	auto *addBtn = new QPushButton(QString::fromUtf8(obs_module_text("AddDestination")), platformPanel);
	auto *renBtn = new QPushButton(QString::fromUtf8(obs_module_text("RenameDestination")), platformPanel);
	auto *delBtn = new QPushButton(QString::fromUtf8(obs_module_text("DeleteDestination")), platformPanel);
	connect(addBtn, &QPushButton::clicked, this, &SettingsDialog::OnAddDestination);
	connect(renBtn, &QPushButton::clicked, this, &SettingsDialog::OnRenameDestination);
	connect(delBtn, &QPushButton::clicked, this, &SettingsDialog::OnDeleteDestination);
	destBtns->addWidget(addBtn);
	destBtns->addWidget(renBtn);
	destBtns->addWidget(delBtn);
	destBtns->addStretch(1);
	form->addRow(QString(), destBtns);

	statusCard = new QFrame(platformPanel);
	statusCard->setObjectName("vspStatusCard");
	statusCard->setFrameShape(QFrame::StyledPanel);
	statusCard->setStyleSheet(QStringLiteral(
		"QFrame#vspStatusCard { background: palette(base); border: 1px solid palette(mid); border-radius: 10px; padding: 10px; }"));
	auto *cardLay = new QVBoxLayout(statusCard);
	statusCardLabel = new QLabel(statusCard);
	statusCardLabel->setWordWrap(true);
	cardLay->addWidget(statusCardLabel);
	liveStatusLabel = new QLabel(statusCard);
	cardLay->addWidget(liveStatusLabel);
	form->addRow(QString::fromUtf8(obs_module_text("DestinationStatus")), statusCard);

	destNameEdit = new QLineEdit(platformPanel);
	form->addRow(QString::fromUtf8(obs_module_text("DestinationName")), destNameEdit);
	destNameRow = form->labelForField(destNameEdit);

	twitchIngestCombo = new QComboBox(platformPanel);
	for (const auto &ing : vsp::TwitchIngestOptions())
		twitchIngestCombo->addItem(ing.name, ing.id);
	form->addRow(QString::fromUtf8(obs_module_text("TwitchIngest")), twitchIngestCombo);
	twitchIngestRow = form->labelForField(twitchIngestCombo);

	verticalServerEdit = new QLineEdit(platformPanel);
	verticalServerEdit->setPlaceholderText(QStringLiteral("rtmps://…"));
	connect(verticalServerEdit, &QLineEdit::textChanged, this, [this](const QString &) {
		UpdateProtocolIndicator();
		UpdateDestinationStatusCard();
	});
	form->addRow(QString::fromUtf8(obs_module_text("VerticalStreamServer")), verticalServerEdit);
	serverRow = form->labelForField(verticalServerEdit);

	protocolLabel = new QLabel(platformPanel);
	form->addRow(QString::fromUtf8(obs_module_text("ProtocolIndicator")), protocolLabel);
	protocolRow = form->labelForField(protocolLabel);

	keyRowWidget = new QWidget(platformPanel);
	auto *keyRow = new QHBoxLayout(keyRowWidget);
	keyRow->setContentsMargins(0, 0, 0, 0);
	verticalKeyEdit = new QLineEdit(keyRowWidget);
	verticalKeyEdit->setEchoMode(QLineEdit::Password);
	verticalKeyEdit->setPlaceholderText(QString::fromUtf8(obs_module_text("StreamKeyPlaceholder")));
	verticalKeyEdit->setToolTip(QString::fromUtf8(obs_module_text("StreamKeyTooltip")));
	showKeyBtn = new QPushButton(QString::fromUtf8(obs_module_text("ShowKey")), keyRowWidget);
	showKeyBtn->setCheckable(true);
	connect(showKeyBtn, &QPushButton::toggled, this, &SettingsDialog::OnToggleShowKey);
	connect(verticalKeyEdit, &QLineEdit::textChanged, this, [this](const QString &) { UpdateDestinationStatusCard(); });
	keyRow->addWidget(verticalKeyEdit, 1);
	keyRow->addWidget(showKeyBtn);
	form->addRow(QString::fromUtf8(obs_module_text("VerticalStreamKey")), keyRowWidget);

	usernameEdit = new QLineEdit(platformPanel);
	passwordEdit = new QLineEdit(platformPanel);
	passwordEdit->setEchoMode(QLineEdit::Password);
	form->addRow(QString::fromUtf8(obs_module_text("OptionalUsername")), usernameEdit);
	form->addRow(QString::fromUtf8(obs_module_text("OptionalPassword")), passwordEdit);
	usernameRow = form->labelForField(usernameEdit);
	passwordRow = form->labelForField(passwordEdit);

	panelLay->addLayout(form);

	platformNote = new QLabel(platformPanel);
	platformNote->setWordWrap(true);
	platformNote->setStyleSheet(QStringLiteral("color: palette(mid);"));
	panelLay->addWidget(platformNote);

	platformHelpBtn = new QPushButton(platformPanel);
	connect(platformHelpBtn, &QPushButton::clicked, this, &SettingsDialog::OnOpenPlatformHelp);
	panelLay->addWidget(platformHelpBtn);

	auto *actionRow = new QHBoxLayout();
	testDestBtn = new QPushButton(QString::fromUtf8(obs_module_text("TestConfiguration")), platformPanel);
	saveDestBtn = new QPushButton(QString::fromUtf8(obs_module_text("SaveDestination")), platformPanel);
	clearCredBtn = new QPushButton(QString::fromUtf8(obs_module_text("ClearVerticalCredentials")), platformPanel);
	connect(testDestBtn, &QPushButton::clicked, this, &SettingsDialog::OnTestDestination);
	connect(saveDestBtn, &QPushButton::clicked, this, &SettingsDialog::OnSaveDestination);
	connect(clearCredBtn, &QPushButton::clicked, this, &SettingsDialog::OnClearCredentials);
	actionRow->addWidget(testDestBtn);
	actionRow->addWidget(saveDestBtn);
	actionRow->addWidget(clearCredBtn);
	actionRow->addStretch(1);
	panelLay->addLayout(actionRow);

	auto *warn = new QLabel(QString::fromUtf8(obs_module_text("IndependentStreamNote")), platformPanel);
	warn->setWordWrap(true);
	panelLay->addWidget(warn);

	lay->addWidget(platformPanel);
	lay->addStretch(1);
}

void SettingsDialog::SyncFieldsFromSettings()
{
	loadingFields = true;
	for (int i = 0; i < presetCombo->count(); ++i) {
		if (presetCombo->itemData(i).toInt() == (int)settings.canvasPreset) {
			presetCombo->setCurrentIndex(i);
			break;
		}
	}
	widthSpin->setValue((int)settings.customWidth);
	heightSpin->setValue((int)settings.customHeight);
	OnCanvasPresetChanged(presetCombo->currentIndex());

	for (int i = 0; i < shortClipCombo->count(); ++i) {
		if (shortClipCombo->itemData(i).toInt() == (int)settings.shortClipPreset) {
			shortClipCombo->setCurrentIndex(i);
			break;
		}
	}
	shortCustomSpin->setValue(settings.customShortClipSeconds);
	OnShortClipPresetChanged(shortClipCombo->currentIndex());

	for (int i = 0; i < longClipCombo->count(); ++i) {
		if (longClipCombo->itemData(i).toInt() == (int)settings.longClipPreset) {
			longClipCombo->setCurrentIndex(i);
			break;
		}
	}
	longCustomMin->setValue(settings.customLongClipSeconds / 60);
	longCustomSec->setValue(settings.customLongClipSeconds % 60);
	longCustomEdit->setText(QStringLiteral("%1:%2")
					.arg(settings.customLongClipSeconds / 60, 2, 10, QLatin1Char('0'))
					.arg(settings.customLongClipSeconds % 60, 2, 10, QLatin1Char('0')));
	OnLongClipPresetChanged(longClipCombo->currentIndex());

	pathEdit->setText(settings.recordingPath);
	clipBufferCheck->setChecked(settings.clipBufferEnabled);
	if (autoStartBufferCheck)
		autoStartBufferCheck->setChecked(settings.autoStartClipBuffer);
	if (stopIdleCheck)
		stopIdleCheck->setChecked(settings.stopBufferWhenIdle);
	if (idleTimeoutSpin)
		idleTimeoutSpin->setValue(settings.bufferIdleTimeoutSeconds);
	if (saveAvailableCheck)
		saveAvailableCheck->setChecked(settings.saveAvailableWhenShort);
	if (bufferOnLiveCheck)
		bufferOnLiveCheck->setChecked(settings.bufferStartOnVerticalLive);
	if (bufferOnRecordCheck)
		bufferOnRecordCheck->setChecked(settings.bufferStartOnVerticalRecord);
	if (bufferStatusInSettings && outputs)
		bufferStatusInSettings->setText(outputs->BufferStatusText());

	if (platformSelector)
		SyncStreamingFields();

	if (outputs) {
		encoderLabel->setText(outputs->ActiveEncoderSummary());
		bitrateLabel->setText(outputs->ActiveBitrateSummary());
		recordStatusLabel->setText(outputs->RecordingStatusSummary());
	}

	autoMaster->setChecked(settings.automationEnabled);
	startMainStream->setChecked(settings.autoStartOnMainStream);
	startScene->setChecked(settings.autoStartOnScene);
	startObs->setChecked(settings.autoStartOnObsStart);
	startSchedule->setChecked(settings.autoStartOnSchedule);
	startCountdown->setChecked(settings.autoStartOnCountdown);
	startVerticalLive->setChecked(settings.autoStartOnVerticalLive);
	stopMainStream->setChecked(settings.autoStopOnMainStreamStop);
	stopScene->setChecked(settings.autoStopOnSceneInactive);
	stopDuration->setChecked(settings.autoStopOnDuration);
	stopScheduleEnd->setChecked(settings.autoStopOnScheduleEnd);
	stopVerticalLive->setChecked(settings.autoStopOnVerticalLiveStop);
	stopObsShutdown->setChecked(settings.autoStopOnObsShutdown);
	confirmManualStop->setChecked(settings.confirmManualStopDuringAutomation);

	int sceneIdx = 0;
	for (int i = 0; i < sceneCombo->count(); ++i) {
		if (sceneCombo->itemData(i).toString() == settings.triggerSceneUuid ||
		    sceneCombo->itemText(i) == settings.triggerSceneName) {
			sceneIdx = i;
			break;
		}
	}
	sceneCombo->setCurrentIndex(sceneIdx);

	durationH->setValue(settings.autoRecordDurationSeconds / 3600);
	durationM->setValue((settings.autoRecordDurationSeconds % 3600) / 60);
	durationS->setValue(settings.autoRecordDurationSeconds % 60);
	countdownSpin->setValue(settings.countdownSeconds);

	const QDate sd = QDate::fromString(settings.scheduleStartDate, QStringLiteral("yyyy-MM-dd"));
	schedStartDate->setDate(sd.isValid() ? sd : QDate::currentDate());
	const QTime st = QTime::fromString(settings.scheduleStartTime, QStringLiteral("HH:mm"));
	schedStartTime->setTime(st.isValid() ? st : QTime::currentTime());
	const QDate ed = QDate::fromString(settings.scheduleEndDate, QStringLiteral("yyyy-MM-dd"));
	schedEndDate->setDate(ed.isValid() ? ed : QDate::currentDate());
	const QTime et = QTime::fromString(settings.scheduleEndTime, QStringLiteral("HH:mm"));
	schedEndTime->setTime(et.isValid() ? et : QTime::currentTime().addSecs(3600));

	for (int i = 0; i < schedRepeat->count(); ++i) {
		if (schedRepeat->itemData(i).toInt() == (int)settings.scheduleRepeat) {
			schedRepeat->setCurrentIndex(i);
			break;
		}
	}
	for (int d = 0; d < 7; ++d) {
		if (weekdayChecks[d])
			weekdayChecks[d]->setChecked((settings.scheduleWeekdaysMask & (1 << d)) != 0);
	}

	autoStatusLabel->setText(QStringLiteral("Status: %1").arg(
		automationStatusText.isEmpty() ? vsp::AutomationStatusLabel(automationStatus) : automationStatusText));
	loadingFields = false;
}

void SettingsDialog::OnCanvasPresetChanged(int)
{
	const auto preset = static_cast<vsp::CanvasPreset>(presetCombo->currentData().toInt());
	const bool custom = preset == vsp::CanvasPreset::Custom;
	widthSpin->setEnabled(custom);
	heightSpin->setEnabled(custom);
	if (!custom) {
		uint32_t w = 0, h = 0;
		vsp::CanvasSizeForPreset(preset, settings.customWidth, settings.customHeight, w, h);
		widthSpin->setValue((int)w);
		heightSpin->setValue((int)h);
	}
	aspectHint->setText(vsp::IsPortrait((uint32_t)widthSpin->value(), (uint32_t)heightSpin->value())
				    ? QString::fromUtf8(obs_module_text("PortraitHint"))
				    : QString::fromUtf8(obs_module_text("LandscapeHint")));
}

void SettingsDialog::OnShortClipPresetChanged(int)
{
	const auto preset = static_cast<vsp::ShortClipPreset>(shortClipCombo->currentData().toInt());
	shortCustomSpin->setEnabled(preset == vsp::ShortClipPreset::Custom);
}

void SettingsDialog::OnLongClipPresetChanged(int)
{
	const auto preset = static_cast<vsp::LongClipPreset>(longClipCombo->currentData().toInt());
	const bool custom = preset == vsp::LongClipPreset::Custom;
	longCustomMin->setEnabled(custom);
	longCustomSec->setEnabled(custom);
	longCustomEdit->setEnabled(custom);

	int secs = custom ? (longCustomMin->value() * 60 + longCustomSec->value()) : (int)preset;
	QString warn;
	vsp::ValidateLongClipSeconds(secs > 0 ? secs : 1, nullptr, &warn);
	longWarnLabel->setText(warn);
}

void SettingsDialog::OnBrowsePath()
{
	const QString dir = QFileDialog::getExistingDirectory(this, QString::fromUtf8(obs_module_text("RecordingPath")),
							      pathEdit->text());
	if (!dir.isEmpty())
		pathEdit->setText(dir);
}

void SettingsDialog::OnResetDefaults()
{
	settings = vsp::PluginSettings{};
	settings.recordingPath = vsp::DefaultRecordingPath();
	vsp::EnsureDefaultDestinations(settings);
	LoadSecretsIntoDestinations();
	SyncFieldsFromSettings();
	MarkDirty();
}

void SettingsDialog::OnResetAutomation()
{
	resetAutomation = true;
	settings.automationEnabled = false;
	settings.autoStartOnMainStream = false;
	settings.autoStartOnScene = false;
	settings.autoStartOnObsStart = false;
	settings.autoStartOnSchedule = false;
	settings.autoStartOnCountdown = false;
	settings.autoStartOnVerticalLive = false;
	settings.autoStopOnMainStreamStop = false;
	settings.autoStopOnSceneInactive = false;
	settings.autoStopOnDuration = false;
	settings.autoStopOnScheduleEnd = false;
	settings.autoStopOnVerticalLiveStop = false;
	settings.autoStopOnObsShutdown = true;
	settings.triggerSceneUuid.clear();
	settings.triggerSceneName.clear();
	settings.autoRecordDurationSeconds = 3600;
	settings.countdownSeconds = 60;
	settings.scheduleStartDate.clear();
	settings.scheduleStartTime.clear();
	settings.scheduleEndDate.clear();
	settings.scheduleEndTime.clear();
	settings.scheduleRepeat = vsp::ScheduleRepeat::Once;
	settings.scheduleWeekdaysMask = 0;
	vsp::EnsureDefaultDestinations(settings);
	LoadSecretsIntoDestinations();
	SyncFieldsFromSettings();
	MarkDirty();
}

bool SettingsDialog::ValidateAndCommit(QString *error, QString *warning, QString *errorField)
{
	auto fail = [&](const char *field) -> bool {
		if (errorField)
			*errorField = QString::fromUtf8(field);
		return false;
	};

	settings.canvasPreset = static_cast<vsp::CanvasPreset>(presetCombo->currentData().toInt());
	settings.customWidth = (uint32_t)widthSpin->value();
	settings.customHeight = (uint32_t)heightSpin->value();
	if (!vsp::ValidateCanvasSize(widthSpin->value(), heightSpin->value(), error))
		return fail("canvas");

	settings.shortClipPreset = static_cast<vsp::ShortClipPreset>(shortClipCombo->currentData().toInt());
	settings.customShortClipSeconds = shortCustomSpin->value();
	if (settings.shortClipPreset == vsp::ShortClipPreset::Custom &&
	    !vsp::ValidateShortClipSeconds(settings.customShortClipSeconds, error))
		return fail("shortclip");

	settings.longClipPreset = static_cast<vsp::LongClipPreset>(longClipCombo->currentData().toInt());
	if (settings.longClipPreset == vsp::LongClipPreset::Custom) {
		int secs = -1;
		const QString mmss = longCustomEdit->text().trimmed();
		if (!mmss.isEmpty() && mmss.contains(QLatin1Char(':'))) {
			secs = vsp::ParseMmSs(mmss, error);
			if (secs < 0)
				return fail("longclip");
		} else {
			secs = longCustomMin->value() * 60 + longCustomSec->value();
		}
		QString warn;
		if (!vsp::ValidateLongClipSeconds(secs, error, &warn))
			return fail("longclip");
		settings.customLongClipSeconds = secs;
		if (warning && !warn.isEmpty())
			*warning = warn;
	}

	settings.recordingPath = pathEdit->text().trimmed();
	if (!vsp::ValidateRecordingPath(settings.recordingPath, error))
		return fail("path");
	settings.clipBufferEnabled = clipBufferCheck->isChecked();
	if (autoStartBufferCheck)
		settings.autoStartClipBuffer = autoStartBufferCheck->isChecked();
	if (stopIdleCheck)
		settings.stopBufferWhenIdle = stopIdleCheck->isChecked();
	if (idleTimeoutSpin)
		settings.bufferIdleTimeoutSeconds = idleTimeoutSpin->value();
	if (saveAvailableCheck)
		settings.saveAvailableWhenShort = saveAvailableCheck->isChecked();
	if (bufferOnLiveCheck)
		settings.bufferStartOnVerticalLive = bufferOnLiveCheck->isChecked();
	if (bufferOnRecordCheck)
		settings.bufferStartOnVerticalRecord = bufferOnRecordCheck->isChecked();

	/* Persist destination metadata + secrets. Full destination validity is enforced on
	 * Go Live / Test Configuration so canvas/clip settings can still be saved. */
	if (platformSelector) {
		PersistActiveDestinationSecrets();
		HighlightInvalidField(QString());
	}

	settings.automationEnabled = autoMaster->isChecked();
	settings.autoStartOnMainStream = startMainStream->isChecked();
	settings.autoStartOnScene = startScene->isChecked();
	settings.autoStartOnObsStart = startObs->isChecked();
	settings.autoStartOnSchedule = startSchedule->isChecked();
	settings.autoStartOnCountdown = startCountdown->isChecked();
	settings.autoStartOnVerticalLive = startVerticalLive->isChecked();
	settings.autoStopOnMainStreamStop = stopMainStream->isChecked();
	settings.autoStopOnSceneInactive = stopScene->isChecked();
	settings.autoStopOnDuration = stopDuration->isChecked();
	settings.autoStopOnScheduleEnd = stopScheduleEnd->isChecked();
	settings.autoStopOnVerticalLiveStop = stopVerticalLive->isChecked();
	settings.autoStopOnObsShutdown = stopObsShutdown->isChecked();
	settings.confirmManualStopDuringAutomation = confirmManualStop->isChecked();

	settings.triggerSceneUuid = sceneCombo->currentData().toString();
	settings.triggerSceneName = sceneCombo->currentIndex() > 0 ? sceneCombo->currentText() : QString();

	settings.autoRecordDurationSeconds = durationH->value() * 3600 + durationM->value() * 60 + durationS->value();
	if (settings.autoStopOnDuration && !vsp::ValidateAutoDurationSeconds(settings.autoRecordDurationSeconds, error))
		return fail("duration");

	settings.countdownSeconds = countdownSpin->value();
	settings.scheduleStartDate = schedStartDate->date().toString(QStringLiteral("yyyy-MM-dd"));
	settings.scheduleStartTime = schedStartTime->time().toString(QStringLiteral("HH:mm"));
	settings.scheduleEndDate = schedEndDate->date().toString(QStringLiteral("yyyy-MM-dd"));
	settings.scheduleEndTime = schedEndTime->time().toString(QStringLiteral("HH:mm"));
	settings.scheduleRepeat = static_cast<vsp::ScheduleRepeat>(schedRepeat->currentData().toInt());
	int dayMask = 0;
	for (int d = 0; d < 7; ++d) {
		if (weekdayChecks[d] && weekdayChecks[d]->isChecked())
			dayMask |= (1 << d);
	}
	settings.scheduleWeekdaysMask = dayMask;
	if (settings.scheduleRepeat == vsp::ScheduleRepeat::Weekdays && dayMask == 0) {
		if (error)
			*error = QString::fromUtf8(obs_module_text("RepeatDaysRequired"));
		return fail("weekdays");
	}

	return true;
}

void SettingsDialog::OnAccepted()
{
	QString error;
	QString warning;
	QString errorField;
	if (!ValidateAndCommit(&error, &warning, &errorField)) {
		NavigateToErrorField(errorField);
		HighlightInvalidField(errorField);
		QMessageBox::warning(this, QString::fromUtf8(obs_module_text("Settings")), error);
		SetApplyEnabled(true);
		dirty = true;
		return;
	}
	HighlightInvalidField(QString());
	if (!warning.isEmpty()) {
		QMessageBox::information(this, QString::fromUtf8(obs_module_text("Settings")), warning);
	}
	emit applied();
	resetAutomation = false;
	ClearDirty();
	accept();
}


void SettingsDialog::hideEvent(QHideEvent *event)
{
	if (showKeyBtn)
		showKeyBtn->setChecked(false);
	if (verticalKeyEdit)
		verticalKeyEdit->setEchoMode(QLineEdit::Password);
	QDialog::hideEvent(event);
}

void SettingsDialog::LoadSecretsIntoDestinations()
{
	for (vsp::StreamDestination &d : settings.destinations) {
		QString key, pass;
		vsp::LoadSecret(vsp::KeyTarget(d.id), &key);
		vsp::LoadSecret(vsp::PasswordTarget(d.id), &pass);
		d.streamKey = key;
		d.password = pass;
	}
}

void SettingsDialog::PersistActiveDestinationSecrets(bool applyPlatformFromCombo)
{
	/* Always write into the currently active destination id (not a newly selected picker value). */
	vsp::StreamDestination d;
	d.id = settings.activeDestinationId;
	if (const auto *ex = vsp::FindDestination(settings, d.id))
		d = *ex;
	if (applyPlatformFromCombo && platformSelector)
		d.platform = platformSelector->platform();
	if (destNameEdit)
		d.name = destNameEdit->text().trimmed();
	if (verticalServerEdit)
		d.server = verticalServerEdit->text().trimmed();
	if (verticalKeyEdit)
		d.streamKey = verticalKeyEdit->text();
	if (usernameEdit)
		d.username = usernameEdit->text().trimmed();
	if (passwordEdit)
		d.password = passwordEdit->text();
	if (twitchIngestCombo) {
		d.twitchIngestId = twitchIngestCombo->currentData().toString();
		d.useRecommendedTwitchIngest = (d.twitchIngestId == QStringLiteral("auto"));
		if (d.platform == vsp::StreamPlatform::Twitch) {
			for (const auto &ing : vsp::TwitchIngestOptions()) {
				if (ing.id == d.twitchIngestId) {
					d.server = ing.url;
					break;
				}
			}
		}
	}

	auto *existing = vsp::FindDestination(settings, d.id);
	if (!existing)
		return;
	*existing = d;
	settings.activeDestinationId = d.id;
	auto keyRes = vsp::SaveSecret(vsp::KeyTarget(d.id), d.streamKey);
	if (!d.password.isEmpty())
		vsp::SaveSecret(vsp::PasswordTarget(d.id), d.password);
	else
		vsp::DeleteSecret(vsp::PasswordTarget(d.id));
	if (secureStoreLabel) {
		if (!keyRes.usedSecureStorage) {
			secureStoreLabel->setText(QString::fromUtf8(obs_module_text("InsecureCredentialWarning")));
		} else {
			secureStoreLabel->setText(QString::fromUtf8(obs_module_text("SecureCredentialInfo"))
							  .arg(vsp::SecureStorageDescription()));
		}
	}
}

vsp::StreamDestination SettingsDialog::CurrentUiDestination(bool applyPlatformFromCombo) const
{
	vsp::StreamDestination d;
	d.id = settings.activeDestinationId;
	if (const auto *ex = vsp::FindDestination(settings, d.id))
		d = *ex;
	if (applyPlatformFromCombo && platformSelector)
		d.platform = platformSelector->platform();
	if (destNameEdit)
		d.name = destNameEdit->text().trimmed();
	if (verticalServerEdit)
		d.server = verticalServerEdit->text().trimmed();
	if (verticalKeyEdit)
		d.streamKey = verticalKeyEdit->text();
	if (usernameEdit)
		d.username = usernameEdit->text().trimmed();
	if (passwordEdit)
		d.password = passwordEdit->text();
	if (twitchIngestCombo) {
		d.twitchIngestId = twitchIngestCombo->currentData().toString();
		d.useRecommendedTwitchIngest = (d.twitchIngestId == QStringLiteral("auto"));
		if (d.platform == vsp::StreamPlatform::Twitch) {
			for (const auto &ing : vsp::TwitchIngestOptions()) {
				if (ing.id == d.twitchIngestId) {
					d.server = ing.url;
					break;
				}
			}
		}
	}
	return d;
}

void SettingsDialog::SyncStreamingFields()
{
	suppressPlatformPrompt = true;
	vsp::EnsureDefaultDestinations(settings);
	LoadSecretsIntoDestinations();

	destinationPicker->blockSignals(true);
	destinationPicker->clear();
	for (const auto &d : settings.destinations) {
		destinationPicker->addItem(
			QStringLiteral("%1 — %2").arg(vsp::PlatformDisplayName(d.platform), d.name), d.id);
	}
	int destIdx = 0;
	for (int i = 0; i < destinationPicker->count(); ++i) {
		if (destinationPicker->itemData(i).toString() == settings.activeDestinationId) {
			destIdx = i;
			break;
		}
	}
	destinationPicker->setCurrentIndex(destIdx);
	destinationPicker->blockSignals(false);

	const vsp::StreamDestination d = vsp::ActiveDestination(settings);
	if (platformSelector)
		platformSelector->setPlatform(d.platform);
	UpdateSelectedPlatformHero();
	destNameEdit->setText(d.name);
	verticalServerEdit->setText(d.server);
	verticalKeyEdit->setText(d.streamKey);
	usernameEdit->setText(d.username);
	passwordEdit->setText(d.password);
	for (int i = 0; i < twitchIngestCombo->count(); ++i) {
		if (twitchIngestCombo->itemData(i).toString() == d.twitchIngestId) {
			twitchIngestCombo->setCurrentIndex(i);
			break;
		}
	}
	ApplyPlatformFieldVisibility();
	UpdateProtocolIndicator();
	UpdateDestinationStatusCard();
	if (outputs && liveStatusLabel)
		liveStatusLabel->setText(QStringLiteral("Vertical stream: %1").arg(outputs->LiveStatusText()));
	if (secureStoreLabel) {
		if (vsp::SecureStorageAvailable()) {
			secureStoreLabel->setText(QString::fromUtf8(obs_module_text("SecureCredentialInfo"))
							  .arg(vsp::SecureStorageDescription()));
		} else {
			secureStoreLabel->setText(QString::fromUtf8(obs_module_text("InsecureCredentialWarning")));
		}
	}
	suppressPlatformPrompt = false;
}

void SettingsDialog::UpdateDestinationStatusCard()
{
	if (!statusCardLabel)
		return;
	const vsp::StreamDestination d = CurrentUiDestination();
	QString err;
	const bool valid = vsp::ValidateDestination(d, &err);
	QString text = QStringLiteral("Platform: %1\nDestination: %2\nServer host: %3\nStream key configured: %4\n"
				      "Configuration valid: %5\nConnection status: not tested\nVertical stream: %6")
			       .arg(vsp::PlatformDisplayName(d.platform),
				    d.name.isEmpty() ? QStringLiteral("(unnamed)") : d.name, vsp::HostnameOnly(d.server),
				    d.streamKey.isEmpty() ? QStringLiteral("No") : QStringLiteral("Yes"),
				    valid ? QStringLiteral("Yes") : QStringLiteral("No"),
				    outputs ? outputs->LiveStatusText() : QStringLiteral("Offline"));
	if (!valid && !err.isEmpty())
		text += QStringLiteral("\n") + err;
	statusCardLabel->setText(text);
}

void SettingsDialog::UpdateProtocolIndicator()
{
	if (!protocolLabel || !verticalServerEdit)
		return;
	const QString s = verticalServerEdit->text().trimmed().toLower();
	if (s.startsWith(QStringLiteral("rtmps://")))
		protocolLabel->setText(QStringLiteral("RTMPS (preferred)"));
	else if (s.startsWith(QStringLiteral("rtmp://")))
		protocolLabel->setText(QStringLiteral("RTMP"));
	else if (s.isEmpty())
		protocolLabel->setText(QStringLiteral("—"));
	else
		protocolLabel->setText(QString::fromUtf8(obs_module_text("UnsupportedProtocol")));
}


vsp::StreamPlatform SettingsDialog::SelectedPlatform() const
{
	if (platformSelector)
		return platformSelector->platform();
	return vsp::ActiveDestination(settings).platform;
}

void SettingsDialog::UpdateSelectedPlatformHero()
{
	const auto p = SelectedPlatform();
	if (selectedPlatformHero)
		selectedPlatformHero->setPixmap(vsp::LoadPlatformLogoPixmap(p, QSize(40, 40)));
	if (selectedPlatformTitle)
		selectedPlatformTitle->setText(vsp::PlatformDisplayName(p));
}

void SettingsDialog::AnimatePlatformPanel()
{
	if (!platformPanelAnim || !platformPanelOpacity)
		return;
	platformPanelAnim->stop();
	platformPanelAnim->setStartValue(0.35);
	platformPanelAnim->setEndValue(1.0);
	platformPanelOpacity->setOpacity(0.35);
	platformPanelAnim->start();
}

void SettingsDialog::FocusStreamingTab()
{
	if (categories && pages && streamingTabIndex >= 0) {
		categories->setCurrentRow(streamingTabIndex);
		pages->setCurrentIndex(streamingTabIndex);
	}
}

void SettingsDialog::ApplyPlatformFieldVisibility()
{
	const auto p = SelectedPlatform();
	const bool twitch = p == vsp::StreamPlatform::Twitch;
	const bool custom = p == vsp::StreamPlatform::CustomRtmp;
	const bool showServer = !twitch; /* Twitch uses ingest picker */

	auto setRow = [](QWidget *field, QWidget *label, bool on) {
		if (field)
			field->setVisible(on);
		if (label)
			label->setVisible(on);
	};

	setRow(twitchIngestCombo, twitchIngestRow, twitch);
	setRow(verticalServerEdit, serverRow, showServer);
	setRow(protocolLabel, protocolRow, showServer || custom);
	setRow(usernameEdit, usernameRow, custom);
	setRow(passwordEdit, passwordRow, custom);
	setRow(destNameEdit, destNameRow, true);
	if (keyRowWidget)
		keyRowWidget->setVisible(true);

	QString note;
	QString helpLabel;
	switch (p) {
	case vsp::StreamPlatform::YouTube:
		note = QString::fromUtf8(obs_module_text("YouTubeNote"));
		helpLabel = QString::fromUtf8(obs_module_text("OpenYouTubeInstructions"));
		if (verticalServerEdit && verticalServerEdit->text().trimmed().isEmpty())
			verticalServerEdit->setText(vsp::SuggestedYouTubeServer());
		break;
	case vsp::StreamPlatform::Twitch:
		note = QString::fromUtf8(obs_module_text("TwitchNote"));
		helpLabel = QString::fromUtf8(obs_module_text("OpenTwitchInstructions"));
		break;
	case vsp::StreamPlatform::TikTok:
		note = QString::fromUtf8(obs_module_text("TikTokNote"));
		helpLabel = QString::fromUtf8(obs_module_text("OpenTikTokInstructions"));
		break;
	case vsp::StreamPlatform::Instagram:
		note = QString::fromUtf8(obs_module_text("InstagramNote"));
		helpLabel = QString::fromUtf8(obs_module_text("OpenInstagramInstructions"));
		break;
	case vsp::StreamPlatform::CustomRtmp:
		note = QString::fromUtf8(obs_module_text("CustomRtmpNote"));
		helpLabel = QString::fromUtf8(obs_module_text("CustomRtmpHelp"));
		break;
	}
	if (platformNote)
		platformNote->setText(note);
	if (platformHelpBtn) {
		platformHelpBtn->setText(helpLabel);
		platformHelpBtn->setVisible(!vsp::PlatformHelpUrl(p).isEmpty() || custom);
	}
	UpdateSelectedPlatformHero();
	UpdateProtocolIndicator();
	UpdateDestinationStatusCard();
}

void SettingsDialog::OnPlatformChanged(int)
{
	if (suppressPlatformPrompt)
		return;
	/* Persist the currently edited destination before switching platforms.
	 * Do not apply the new combo platform onto the previous destination. */
	PersistActiveDestinationSecrets(false);
	const auto p = SelectedPlatform();
	/* Prefer an existing destination for this platform */
	for (int i = 0; i < settings.destinations.size(); ++i) {
		if (settings.destinations[i].platform == p) {
			settings.activeDestinationId = settings.destinations[i].id;
			SyncStreamingFields();
			AnimatePlatformPanel();
			return;
		}
	}
	vsp::StreamDestination d = vsp::MakeDefaultDestination(p);
	settings.destinations.push_back(d);
	settings.activeDestinationId = d.id;
	SyncStreamingFields();
	AnimatePlatformPanel();
}

void SettingsDialog::OnDestinationPickerChanged(int)
{
	if (suppressPlatformPrompt || !destinationPicker)
		return;
	PersistActiveDestinationSecrets(false);
	settings.activeDestinationId = destinationPicker->currentData().toString();
	SyncStreamingFields();
}

void SettingsDialog::OnToggleShowKey(bool checked)
{
	verticalKeyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
	showKeyBtn->setText(QString::fromUtf8(obs_module_text(checked ? "HideKey" : "ShowKey")));
}

void SettingsDialog::OnOpenPlatformHelp()
{
	const auto p = SelectedPlatform();
	const QString url = vsp::PlatformHelpUrl(p);
	if (!url.isEmpty())
		QDesktopServices::openUrl(QUrl(url));
}

void SettingsDialog::OnSaveDestination()
{
	PersistActiveDestinationSecrets();
	UpdateDestinationStatusCard();
	QMessageBox::information(this, QString::fromUtf8(obs_module_text("SaveDestination")),
				 QString::fromUtf8(obs_module_text("DestinationSaved")));
	SyncStreamingFields();
	MarkDirty();
}

void SettingsDialog::OnClearCredentials()
{
	const auto reply = QMessageBox::question(this, QString::fromUtf8(obs_module_text("ClearVerticalCredentials")),
						 QString::fromUtf8(obs_module_text("ClearCredentialsConfirm")),
						 QMessageBox::Yes | QMessageBox::No);
	if (reply != QMessageBox::Yes)
		return;
	vsp::StreamDestination d = CurrentUiDestination();
	vsp::DeleteSecret(vsp::KeyTarget(d.id));
	vsp::DeleteSecret(vsp::PasswordTarget(d.id));
	if (auto *ex = vsp::FindDestination(settings, d.id)) {
		ex->streamKey.clear();
		ex->password.clear();
	}
	verticalKeyEdit->clear();
	passwordEdit->clear();
	UpdateDestinationStatusCard();
	MarkDirty();
}

void SettingsDialog::OnAddDestination()
{
	const auto p = SelectedPlatform();
	vsp::StreamDestination d = vsp::MakeDefaultDestination(p);
	d.name = QStringLiteral("%1 %2").arg(vsp::PlatformDisplayName(p)).arg(settings.destinations.size() + 1);
	settings.destinations.push_back(d);
	settings.activeDestinationId = d.id;
	SyncStreamingFields();
	MarkDirty();
}

void SettingsDialog::OnRenameDestination()
{
	bool ok = false;
	const QString name = QInputDialog::getText(this, QString::fromUtf8(obs_module_text("RenameDestination")),
						   QString::fromUtf8(obs_module_text("DestinationName")),
						   QLineEdit::Normal, destNameEdit->text(), &ok);
	if (!ok || name.trimmed().isEmpty())
		return;
	destNameEdit->setText(name.trimmed());
	PersistActiveDestinationSecrets();
	SyncStreamingFields();
	MarkDirty();
}

void SettingsDialog::OnDeleteDestination()
{
	if (settings.destinations.size() <= 1) {
		QMessageBox::warning(this, QString::fromUtf8(obs_module_text("DeleteDestination")),
				     QString::fromUtf8(obs_module_text("CannotDeleteLastDestination")));
		return;
	}
	const auto reply = QMessageBox::question(this, QString::fromUtf8(obs_module_text("DeleteDestination")),
						 QString::fromUtf8(obs_module_text("DeleteDestinationConfirm")),
						 QMessageBox::Yes | QMessageBox::No);
	if (reply != QMessageBox::Yes)
		return;
	const QString id = settings.activeDestinationId;
	vsp::DeleteSecret(vsp::KeyTarget(id));
	vsp::DeleteSecret(vsp::PasswordTarget(id));
	settings.destinations.erase(std::remove_if(settings.destinations.begin(), settings.destinations.end(),
						   [&](const vsp::StreamDestination &d) { return d.id == id; }),
				    settings.destinations.end());
	settings.activeDestinationId = settings.destinations.first().id;
	SyncStreamingFields();
	MarkDirty();
}

void SettingsDialog::NavigateToErrorField(const QString &field)
{
	if (!categories || field.isEmpty())
		return;
	int row = -1;
	if (field == QStringLiteral("canvas"))
		row = 1; /* Vertical Canvas */
	else if (field == QStringLiteral("path"))
		row = 2; /* Vertical Recording */
	else if (field == QStringLiteral("shortclip") || field == QStringLiteral("longclip"))
		row = 3; /* Vertical Clips */
	else if (field == QStringLiteral("duration") || field == QStringLiteral("weekdays"))
		row = 4; /* Recording Automation */
	else if (field == QStringLiteral("server") || field == QStringLiteral("key"))
		row = streamingTabIndex >= 0 ? streamingTabIndex : 5;
	if (row >= 0 && row < categories->count())
		categories->setCurrentRow(row);
}

void SettingsDialog::HighlightInvalidField(const QString &field)
{
	auto markEdit = [](QLineEdit *e, bool bad) {
		if (!e)
			return;
		e->setStyleSheet(bad ? QStringLiteral("QLineEdit { border: 1px solid #c62828; }") : QString());
	};
	auto markSpin = [](QSpinBox *e, bool bad) {
		if (!e)
			return;
		e->setStyleSheet(bad ? QStringLiteral("QSpinBox { border: 1px solid #c62828; }") : QString());
	};

	markSpin(widthSpin, field == QStringLiteral("canvas"));
	markSpin(heightSpin, field == QStringLiteral("canvas"));
	markEdit(pathEdit, field == QStringLiteral("path"));
	markSpin(shortCustomSpin, field == QStringLiteral("shortclip"));
	markEdit(longCustomEdit, field == QStringLiteral("longclip"));
	markSpin(longCustomMin, field == QStringLiteral("longclip"));
	markSpin(longCustomSec, field == QStringLiteral("longclip"));
	markSpin(durationH, field == QStringLiteral("duration"));
	markSpin(durationM, field == QStringLiteral("duration"));
	markSpin(durationS, field == QStringLiteral("duration"));
	for (QCheckBox *day : weekdayChecks) {
		if (!day)
			continue;
		day->setStyleSheet(field == QStringLiteral("weekdays")
					   ? QStringLiteral("QCheckBox { color: #c62828; }")
					   : QString());
	}
	markEdit(verticalServerEdit, field == QStringLiteral("server"));
	markEdit(verticalKeyEdit, field == QStringLiteral("key"));

	if (field == QStringLiteral("path") && pathEdit)
		pathEdit->setFocus(Qt::OtherFocusReason);
	else if (field == QStringLiteral("server") && verticalServerEdit)
		verticalServerEdit->setFocus(Qt::OtherFocusReason);
	else if (field == QStringLiteral("key") && verticalKeyEdit)
		verticalKeyEdit->setFocus(Qt::OtherFocusReason);
	else if (field == QStringLiteral("canvas") && widthSpin)
		widthSpin->setFocus(Qt::OtherFocusReason);
}

void SettingsDialog::OnTestDestination()
{
	PersistActiveDestinationSecrets();
	settings = settings; /* secrets already in settings.destinations */
	QString err, field;
	const auto d = CurrentUiDestination();
	if (!vsp::ValidateDestination(d, &err, &field)) {
		HighlightInvalidField(field);
		QMessageBox::warning(this, QString::fromUtf8(obs_module_text("TestConfiguration")), err);
		return;
	}
	HighlightInvalidField(QString());
	if (!outputs) {
		QMessageBox::information(this, QString::fromUtf8(obs_module_text("TestConfiguration")),
					 QStringLiteral("Local configuration valid.\nFull output test requires the dock."));
		return;
	}
	outputs->ApplySettings(settings);
	QString summary;
	if (!outputs->TestStreamDestination(&summary, &err)) {
		QMessageBox::warning(this, QString::fromUtf8(obs_module_text("TestConfiguration")), err);
		return;
	}
	QMessageBox::information(this, QString::fromUtf8(obs_module_text("TestConfiguration")), summary);
}

