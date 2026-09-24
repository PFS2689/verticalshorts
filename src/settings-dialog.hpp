#pragma once

#include "plugin-settings.hpp"
#include "stream-destination.hpp"
#include "vertical-outputs.hpp"

#include <QDialog>
#include <QHideEvent>
#include <QStringList>

class QComboBox;
class QSpinBox;
class QLineEdit;
class QLabel;
class QCheckBox;
class QListWidget;
class QStackedWidget;
class QDateEdit;
class QTimeEdit;
class QPushButton;
class QTextEdit;
class QGroupBox;
class QFrame;
class QWidget;
class QGraphicsOpacityEffect;
class QPropertyAnimation;
class PlatformSelector;

class SettingsDialog : public QDialog {
	Q_OBJECT
public:
	SettingsDialog(vsp::PluginSettings settings, VerticalOutputs *outputs, const QStringList &sceneNames,
		       const QStringList &sceneUuids, vsp::AutomationStatus automationStatus,
		       const QString &automationStatusText, QWidget *parent = nullptr);

	vsp::PluginSettings result() const { return settings; }
	bool WantsAutomationReset() const { return resetAutomation; }
	void FocusStreamingTab();

protected:
	void hideEvent(QHideEvent *event) override;

private slots:
	void OnCanvasPresetChanged(int index);
	void OnShortClipPresetChanged(int index);
	void OnLongClipPresetChanged(int index);
	void OnBrowsePath();
	void OnResetDefaults();
	void OnResetAutomation();
	void OnPlatformChanged(int index);
	void OnDestinationPickerChanged(int index);
	void OnToggleShowKey(bool checked);
	void OnTestDestination();
	void OnSaveDestination();
	void OnClearCredentials();
	void OnAddDestination();
	void OnRenameDestination();
	void OnDeleteDestination();
	void OnOpenPlatformHelp();
	void OnAccepted();

private:
	void BuildGeneralTab(QWidget *tab);
	void BuildCanvasTab(QWidget *tab);
	void BuildRecordingTab(QWidget *tab);
	void BuildClipsTab(QWidget *tab);
	void BuildAutomationTab(QWidget *tab);
	void BuildStreamingTab(QWidget *tab);
	void BuildAudioTab(QWidget *tab);
	void BuildAboutTab(QWidget *tab);
	void OnApply();
	int AddCategory(const char *localeKey, const char *fallback, QWidget *page);
	void SyncFieldsFromSettings();
	void SyncStreamingFields();
	void UpdateDestinationStatusCard();
	void UpdateProtocolIndicator();
	void UpdateSelectedPlatformHero();
	void AnimatePlatformPanel();
	void ApplyPlatformFieldVisibility();
	bool ValidateAndCommit(QString *error, QString *warning);
	void PersistActiveDestinationSecrets(bool applyPlatformFromCombo = true);
	void LoadSecretsIntoDestinations();
	vsp::StreamDestination CurrentUiDestination(bool applyPlatformFromCombo = true) const;
	void HighlightInvalidField(const QString &field);
	vsp::StreamPlatform SelectedPlatform() const;

	vsp::PluginSettings settings;
	VerticalOutputs *outputs = nullptr;
	QStringList sceneNames;
	QStringList sceneUuids;
	vsp::AutomationStatus automationStatus = vsp::AutomationStatus::Disabled;
	QString automationStatusText;
	bool resetAutomation = false;
	bool suppressPlatformPrompt = false;

	/* Canvas */
	QComboBox *presetCombo = nullptr;
	QSpinBox *widthSpin = nullptr;
	QSpinBox *heightSpin = nullptr;
	QLabel *aspectHint = nullptr;

	/* Recording */
	QLineEdit *pathEdit = nullptr;
	QLabel *encoderLabel = nullptr;
	QLabel *bitrateLabel = nullptr;
	QLabel *formatLabel = nullptr;
	QLabel *recordStatusLabel = nullptr;

	/* Clips */
	QComboBox *shortClipCombo = nullptr;
	QSpinBox *shortCustomSpin = nullptr;
	QComboBox *longClipCombo = nullptr;
	QLineEdit *longCustomEdit = nullptr;
	QSpinBox *longCustomMin = nullptr;
	QSpinBox *longCustomSec = nullptr;
	QLabel *longWarnLabel = nullptr;
	QCheckBox *clipBufferCheck = nullptr;
	QCheckBox *autoStartBufferCheck = nullptr;
	QCheckBox *stopIdleCheck = nullptr;
	QSpinBox *idleTimeoutSpin = nullptr;
	QCheckBox *saveAvailableCheck = nullptr;
	QCheckBox *bufferOnLiveCheck = nullptr;
	QCheckBox *bufferOnRecordCheck = nullptr;
	QLabel *bufferStatusInSettings = nullptr;

	/* Automation */
	QCheckBox *autoMaster = nullptr;
	QCheckBox *startMainStream = nullptr;
	QCheckBox *startScene = nullptr;
	QCheckBox *startObs = nullptr;
	QCheckBox *startSchedule = nullptr;
	QCheckBox *startCountdown = nullptr;
	QCheckBox *startVerticalLive = nullptr;
	QCheckBox *stopMainStream = nullptr;
	QCheckBox *stopScene = nullptr;
	QCheckBox *stopDuration = nullptr;
	QCheckBox *stopScheduleEnd = nullptr;
	QCheckBox *stopVerticalLive = nullptr;
	QCheckBox *stopObsShutdown = nullptr;
	QComboBox *sceneCombo = nullptr;
	QSpinBox *durationH = nullptr;
	QSpinBox *durationM = nullptr;
	QSpinBox *durationS = nullptr;
	QLabel *expectedStopLabel = nullptr;
	QSpinBox *countdownSpin = nullptr;
	QDateEdit *schedStartDate = nullptr;
	QTimeEdit *schedStartTime = nullptr;
	QDateEdit *schedEndDate = nullptr;
	QTimeEdit *schedEndTime = nullptr;
	QComboBox *schedRepeat = nullptr;
	QCheckBox *weekdayChecks[7] = {};
	QLabel *tzLabel = nullptr;
	QLabel *autoStatusLabel = nullptr;
	QCheckBox *confirmManualStop = nullptr;

	/* Category list (left) + content pages (right) */
	QStackedWidget *pages = nullptr;
	QListWidget *categories = nullptr;
	int streamingTabIndex = -1;
	QLabel *streamHelp = nullptr;
	QLabel *selectedPlatformHero = nullptr;
	QLabel *selectedPlatformTitle = nullptr;
	PlatformSelector *platformSelector = nullptr;
	QWidget *platformPanel = nullptr;
	QGraphicsOpacityEffect *platformPanelOpacity = nullptr;
	QPropertyAnimation *platformPanelAnim = nullptr;
	QComboBox *destinationPicker = nullptr;
	QFrame *statusCard = nullptr;
	QLabel *statusCardLabel = nullptr;
	QWidget *destNameRow = nullptr;
	QLineEdit *destNameEdit = nullptr;
	QWidget *serverRow = nullptr;
	QLineEdit *verticalServerEdit = nullptr;
	QWidget *protocolRow = nullptr;
	QLabel *protocolLabel = nullptr;
	QWidget *keyRowWidget = nullptr;
	QLineEdit *verticalKeyEdit = nullptr;
	QPushButton *showKeyBtn = nullptr;
	QWidget *usernameRow = nullptr;
	QLineEdit *usernameEdit = nullptr;
	QWidget *passwordRow = nullptr;
	QLineEdit *passwordEdit = nullptr;
	QWidget *twitchIngestRow = nullptr;
	QComboBox *twitchIngestCombo = nullptr;
	QLabel *platformNote = nullptr;
	QPushButton *platformHelpBtn = nullptr;
	QPushButton *testDestBtn = nullptr;
	QPushButton *saveDestBtn = nullptr;
	QPushButton *clearCredBtn = nullptr;
	QLabel *secureStoreLabel = nullptr;
	QLabel *liveStatusLabel = nullptr;
};
