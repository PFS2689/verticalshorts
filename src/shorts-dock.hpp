#pragma once

#include "plugin-settings.hpp"
#include "qt-display.hpp"
#include "recording-automation.hpp"
#include "vertical-outputs.hpp"

#include <graphics/matrix4.h>
#include <graphics/vec2.h>
#include <obs-frontend-api.h>
#include <obs-hotkey.h>
#include <obs.hpp>

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QMap>
#include <QMenu>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QString>
#include <QVBoxLayout>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#define ITEM_LEFT (1 << 0)
#define ITEM_RIGHT (1 << 1)
#define ITEM_TOP (1 << 2)
#define ITEM_BOTTOM (1 << 3)

enum class ItemHandle : uint32_t {
	None = 0,
	TopLeft = ITEM_TOP | ITEM_LEFT,
	TopCenter = ITEM_TOP,
	TopRight = ITEM_TOP | ITEM_RIGHT,
	CenterLeft = ITEM_LEFT,
	CenterRight = ITEM_RIGHT,
	BottomLeft = ITEM_BOTTOM | ITEM_LEFT,
	BottomCenter = ITEM_BOTTOM,
	BottomRight = ITEM_BOTTOM | ITEM_RIGHT,
};

/* Vertical scene-item fit modes (independent from main OBS transforms). */
enum class VerticalFitMode {
	Fill = 0,     /* cover canvas, preserve AR, crop excess (default for cameras) */
	FitInside = 1, /* entire source visible, preserve AR, may letterbox */
	Original = 2,  /* native source size, centered */
	Stretch = 3,   /* fill canvas, may distort AR (manual only) */
};

enum class VerticalFillPosition {
	Left = 0,
	Center = 1,
	Right = 2,
};

using EventFilterFunc = std::function<bool(QObject *, QEvent *)>;

class OBSEventFilter : public QObject {
public:
	explicit OBSEventFilter(EventFilterFunc filter_) : filter(std::move(filter_)) {}

protected:
	bool eventFilter(QObject *obj, QEvent *event) override { return filter(obj, event); }
	EventFilterFunc filter;
};

/* Shared vertical workspace controller + canvas dock.
 * Owns vertical scenes, preview/view, outputs, automation, settings, and hotkeys.
 * Companion docks call the public Request/Populate API. */
class ShortsDock : public QFrame {
	Q_OBJECT

public:
	explicit ShortsDock(QWidget *parent = nullptr);
	~ShortsDock() override;

	void SaveSettings(obs_data_t *data);
	void LoadSettings(obs_data_t *data);

	/* --- Public API for companion docks --- */
	void RequestAddScene();
	void RequestRemoveScene();
	void RequestDuplicateScene();
	void RequestRenameScene();
	void RequestSelectScene(const QString &uuid);
	void PopulateScenesList(QListWidget *list);

	void RequestAddSource();
	void ShowAddSourceMenu(QWidget *button);
	void RequestRemoveSource();
	void RequestToggleSourceVisible();
	void RequestToggleSourceLock();
	void RequestToggleSourceVisibleById(qint64 itemId);
	void RequestToggleSourceLockById(qint64 itemId);
	void RequestSourceProperties();
	void RequestSourceFilters();
	void RequestRenameSource();
	void RequestDuplicateSource();
	void RequestCopySource();
	void RequestPasteSource();
	void RequestCopyTransform();
	void RequestPasteTransform();
	void RequestSourceMoveUp();
	void RequestSourceMoveDown();
	void RequestSourceMoveTop();
	void RequestSourceMoveBottom();
	void RequestReorderSources(const QList<qint64> &topToBottomIds);
	void RequestSelectSource(qint64 itemId);
	void PopulateSourcesList(QListWidget *list);

	void RequestFitToScreen(); /* Fit Inside Vertical Canvas (legacy name) */
	void RequestFillVerticalCanvas();
	void RequestFitInsideVerticalCanvas();
	void RequestOriginalSize();
	void RequestStretchToScreen();
	void RequestSetFillPosition(VerticalFillPosition pos);
	void RequestCenterToScreen();
	void RequestCenterHorizontally();
	void RequestCenterVertically();
	void RequestRotateDegrees(float delta);
	void RequestFlipHorizontal();
	void RequestFlipVertical();
	void RequestResetTransform();
	void RequestEditTransform();
	void RequestCropDialog();
	void RequestTransformEdited(double x, double y, double w, double h, double rot);
	void PopulateTransformControls(QDoubleSpinBox *x, QDoubleSpinBox *y, QDoubleSpinBox *w, QDoubleSpinBox *h,
				       QDoubleSpinBox *rot);
	void AppendTransformFitMenu(QMenu *transformMenu);
	bool HasSelectedVerticalSource() const;
	bool HasSourceClipboard() const;
	bool HasTransformClipboard() const;
	VerticalFitMode SelectedFitMode() const;
	VerticalFillPosition SelectedFillPosition() const;
	uint32_t VerticalCanvasWidth() const { return verticalWidth; }
	uint32_t VerticalCanvasHeight() const { return verticalHeight; }

	void RequestSetTransition(const QString &name);
	void RequestSetTransitionDuration(int ms);
	void RequestPreviewTransition();
	void RequestTriggerTransition();
	void PopulateTransitions(QComboBox *combo, QSpinBox *duration);

	void CollectSceneLists(QStringList &names, QStringList &uuids) const;
	VerticalOutputs *Outputs() const { return outputs.get(); }
	const vsp::PluginSettings &Settings() const { return settings; }
	vsp::AutomationStatus CurrentAutomationStatus() const;
	QString CurrentAutomationStatusText() const;

protected:
	void showEvent(QShowEvent *event) override;

signals:
	void verticalScenesChanged();
	void verticalSourcesChanged();
	void verticalTransitionsChanged();
	void verticalTransformChanged();

public slots:
	void HotkeySaveShortClip();
	void HotkeySaveLongClip();
	void HotkeyStartRecording();
	void HotkeyStopRecording();
	void HotkeyToggleRecording();
	void HotkeyStartLive();
	void HotkeyStopLive();
	void HotkeyOpenSettings();

private slots:
	void OnGoLive();
	void OnRecord();
	void OnShortClip();
	void OnLongClip();
	void OnShortClipPresetChanged(int index);
	void OnLongClipPresetChanged(int index);
	void OnCanvasPresetChanged(int index);
	void OnSettings();
	void OpenSettingsStreaming(bool focusStreaming = true);
	void OnStreamingChanged(bool active);
	void OnRecordingChanged(bool active);
	void OnClipSaved(const QString &path, ClipKind kind);
	void OnAutomationStatus(vsp::AutomationStatus status, const QString &text);
	void OnAutomationNotify(const QString &title, const QString &message);
	void OnBufferStatus(BufferStatus status, const QString &text);
	void EnsureBufferIfConfigured();

private:
	void BuildUI();
	void EnsureDefaultVerticalScene();
	void RefreshVerticalWorkspace(bool force = false);
	void ApplyCanvasFromSettings();
	void CreateView();
	void DestroyView();
	void AttachScenesToCanvas();
	obs_scene_t *CreateVerticalScene(const char *name);
	void SetCanvasSize(uint32_t width, uint32_t height);
	void SetActiveScene(obs_scene_t *newScene, bool withTransition);
	/* Keep PROGRAM channel 0 bound to the active vertical scene (activation path). */
	void EnsureCanvasProgramChannel(bool forceRebind = false);
	void LogRenderPipeline(const char *reason);
	void FitSceneItemToCanvas(obs_sceneitem_t *item); /* default Fill */
	void ApplyVerticalFitMode(obs_sceneitem_t *item, VerticalFitMode mode, bool persist = true);
	void ReapplyStoredFitModes();
	void ScheduleDeferredFit(obs_sceneitem_t *item, int attemptsLeft);
	void ValidateItemTransform(obs_sceneitem_t *item);
	void EnsurePreviewSceneShowing(bool enable);
	static void StoreFitMode(obs_sceneitem_t *item, VerticalFitMode mode);
	static void StoreFillPosition(obs_sceneitem_t *item, VerticalFillPosition pos);
	static VerticalFitMode LoadFitMode(obs_sceneitem_t *item, VerticalFitMode fallback = VerticalFitMode::Fill);
	static VerticalFillPosition LoadFillPosition(obs_sceneitem_t *item);
	static bool SourceIsVisual(obs_source_t *source);
	static uint32_t FillBoundsAlignment(VerticalFillPosition pos);
	obs_scene_t *FindVerticalSceneByUuid(const QString &uuid) const;
	QString ActiveSceneUuid() const;
	void HandleClipSaveResult(const ClipSaveInfo &info, ClipKind kind);
	void PopulateClipPresetCombos();
	void SyncClipPresetControls();
	void ApplyClipPresetChange();
	void PopulateCanvasPresetCombo();
	void SyncCanvasPresetControl();
	void ApplyCanvasPresetChange();
	void RegisterHotkeys();
	void UnregisterHotkeys();
	void SaveHotkeys(obs_data_t *data) const;
	void LoadHotkeys(obs_data_t *data);
	void EmitSceneUiChanged();
	void EmitSourceUiChanged();
	obs_source_t *EnsureVerticalTransitionSource(const QString &name);
	obs_sceneitem_t *AddSourceToActiveScene(obs_source_t *source, bool fitIfSized);
	/* Camera-sharing: reuse an existing Video Capture Device (no second HW open). */
	void CreateOrShareCaptureSource(const std::string &typeId, const QString &label);
	void WatchCaptureSourceForShare(obs_source_t *created);
	void ResolveSharedCapture(OBSSource created);
	void RemoveVerticalItemsForSource(obs_source_t *source);
	bool TryShareCaptureFromSettings(const char *typeId, obs_data_t *settings, const char *logReason);
	void NotifySharedCameraFeed();
	void InstallCaptureSourceSignals();
	void UninstallCaptureSourceSignals();
	void OnObsCaptureSourceCreated(obs_source_t *source);
	void OnObsCaptureSourceRemoved(obs_source_t *source);
	static void OnSourceCreateSignal(void *data, calldata_t *cd);
	static void OnSourceRemoveSignal(void *data, calldata_t *cd);
	static void OnSourceDestroySignal(void *data, calldata_t *cd);

	std::unique_ptr<OBSEventFilter> BuildEventFilter();
	bool HandlePreviewEvent(QObject *obj, QEvent *event);
	vec2 GetMouseEventPos(QMouseEvent *event);
	OBSSceneItem GetItemAtPos(const vec2 &pos, bool selectBelow);
	bool SelectedAtPos(const vec2 &pos);
	void DoSelect(const vec2 &pos);
	void GetStretchHandleData(const vec2 &pos);
	void MoveItems(const vec2 &pos);
	void StretchItem(const vec2 &pos);
	static vec2 GetItemSize(obs_sceneitem_t *item);
	vec3 CalculateStretchPos(const vec3 &tl, const vec3 &br) const;
	void DrawPreview(uint32_t cx, uint32_t cy);
	void DrawSceneEditing();
	static bool DrawSelectedItem(obs_scene_t *scene, obs_sceneitem_t *item, void *param);
	void UpdateCursor(uint32_t flags);
	void UpdatePreviewScale(int cx, int cy);
	void ShowContextMenu(const QPoint &globalPos);

	static void DrawCallback(void *data, uint32_t cx, uint32_t cy);
	static void FrontendEvent(enum obs_frontend_event event, void *private_data);
	static void HotkeyThunk(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);

	/* Canvas + toolbar UI only */
	OBSQTDisplay *preview = nullptr;
	std::unique_ptr<OBSEventFilter> previewEventFilter;
	QWidget *controlsBar = nullptr;
	QWidget *presetBar = nullptr;
	QPushButton *goLiveBtn = nullptr;
	QPushButton *recordBtn = nullptr;
	QPushButton *shortClipBtn = nullptr;
	QComboBox *shortClipPresetCombo = nullptr;
	QPushButton *longClipBtn = nullptr;
	QComboBox *longClipPresetCombo = nullptr;
	QPushButton *settingsBtn = nullptr;
	QComboBox *canvasPresetCombo = nullptr;
	bool syncingClipPresets = false;
	bool syncingCanvasPreset = false;

	/* Shared state */
	vsp::PluginSettings settings;
	std::unique_ptr<VerticalOutputs> outputs;
	std::unique_ptr<RecordingAutomation> automation;
	bool recordingStartedManually = false;
	QString automationStatusText;
	vsp::AutomationStatus automationStatus = vsp::AutomationStatus::Disabled;

	/* OBS 32 PROGRAM canvas: MAIN_VIEW activation so cameras/capture devices start. */
	obs_canvas_t *canvas = nullptr;
	video_t *video = nullptr;
	obs_scene_t *scene = nullptr;
	QMap<QString, OBSScene> verticalScenes; /* uuid -> canvas-backed vertical scene */
	QStringList sceneOrder;                 /* ordered uuids */

	QString verticalTransitionName;
	int verticalTransitionDurationMs = 300;
	OBSSource verticalTransition; /* private transition source for vertical only */
	OBSSource transitionPreviewScene;

	uint32_t verticalWidth = 1080;
	uint32_t verticalHeight = 1920;
	float previewScale = 1.0f;
	int previewX = 0;
	int previewY = 0;

	bool locked = false;
	bool mouseDown = false;
	bool mouseMoved = false;
	bool mouseOverItems = false;
	bool updatingTransform = false;
	bool clearing = false;
	bool loadingSettings = false;
	bool shuttingDown = false;
	bool sharedCameraNoticeShown = false;
	bool captureSignalsInstalled = false;
	/* Config schema tracking (independent from PLUGIN_VERSION). */
	int loadedConfigSchema = 0;
	bool configSchemaTooNew = false;

	vec2 startPos{};
	vec2 mousePos{};
	vec2 lastMoveOffset{};
	vec2 startItemPos{};
	vec2 stretchItemSize{};
	obs_sceneitem_crop startCrop{};
	vec2 cropSize{};

	OBSSceneItem stretchItem;
	ItemHandle stretchHandle = ItemHandle::None;
	matrix4 screenToItem{};
	matrix4 itemToScreen{};

	gs_vertbuffer_t *rectFill = nullptr;
	gs_vertbuffer_t *box = nullptr; /* dark preview backdrop (OBS-style) */
	uint64_t drawCallbackCount = 0;
	uint64_t lastPipelineLogNs = 0;
	/* Extra show_ref on the active vertical scene while the dock preview is live
	 * (OBS secondary-display pattern). Paired with EnsurePreviewSceneShowing. */
	OBSSource previewShowingSource;
	bool previewShowingHeld = false;

	obs_hotkey_id hkShortClip = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hkLongClip = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hkStartRec = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hkStopRec = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hkToggleRec = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hkStartLive = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hkStopLive = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hkSettings = OBS_INVALID_HOTKEY_ID;
};
