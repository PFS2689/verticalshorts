#include "shorts-dock.hpp"
#include "vertical-production-dock.hpp"
#include "plugin-support.h"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>

#include <QDockWidget>
#include <QMainWindow>

/*
 * OBS rejects modules whose obs_module_ver() major.minor is NEWER than the
 * running OBS (patch is ignored). We build against OBS 32.2.x headers/libs,
 * but every API this plugin imports has existed since OBS 32.0.0.
 *
 * Advertising LIBOBS_API_VER 32.2 caused OBS 32.0 / 32.1 to refuse to load
 * obs-shorts-vertical (Plugin Load Error dialog) even though LoadLibrary and
 * exports were fine. Pin the advertised module API to 32.0.0 so all OBS 32.x
 * hosts accept the module; OBS 32.2+ still loads older-advertised modules.
 */
#undef LIBOBS_API_VER
#define LIBOBS_API_VER MAKE_SEMANTIC_VERSION(32, 0, 0)

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-shorts-vertical", "en-US")
OBS_MODULE_AUTHOR("Vertical Shorts Plugin Contributors")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Professional Vertical Streaming Plugin for OBS Studio";
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return "Vertical Shorts Plugin";
}

static ShortsDock *g_workspace = nullptr;
static bool g_save_callback_registered = false;
static bool g_tools_menu_registered = false;
static bool g_docks_registered = false;

static const char *DOCK_CANVAS = "vertical_shorts_plugin_dock";
static const char *DOCK_PRODUCTION = "vertical_shorts_production_dock";
/* Legacy IDs — removed on unload so old scene collections do not leave ghost docks. */
static const char *DOCK_SCENES_LEGACY = "vertical_shorts_scenes_dock";
static const char *DOCK_SOURCES_LEGACY = "vertical_shorts_sources_dock";
static const char *DOCK_TRANSITIONS_LEGACY = "vertical_shorts_transitions_dock";

static void SaveCallback(obs_data_t *save_data, bool saving, void *)
{
	if (!g_workspace)
		return;

	if (saving) {
		OBSDataAutoRelease obj = obs_data_create();
		g_workspace->SaveSettings(obj);
		obs_data_set_obj(save_data, "obs-shorts-vertical", obj);
	} else {
		obs_data_t *obj = obs_data_get_obj(save_data, "obs-shorts-vertical");
		if (obj) {
			g_workspace->LoadSettings(obj);
			obs_data_release(obj);
		}
	}
}

static void ShowDockById(const char *id)
{
	QMainWindow *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main)
		return;

	QDockWidget *dock = main->findChild<QDockWidget *>(id);
	if (!dock)
		return;

	dock->setVisible(true);
	dock->raise();
	if (dock->toggleViewAction())
		dock->toggleViewAction()->setChecked(true);
}

static void OnToolsShowDock(void *)
{
	if (!g_workspace) {
		blog(LOG_WARNING, "[obs-shorts-vertical] Tools menu: docks not registered yet");
		return;
	}
	ShowDockById(DOCK_CANVAS);
	ShowDockById(DOCK_PRODUCTION);
}

static void RegisterToolsMenu()
{
	if (g_tools_menu_registered)
		return;

	const char *title = obs_module_text("ShortsDockMenu");
	if (!title || !*title)
		title = "Vertical Shorts";

	obs_frontend_add_tools_menu_item(title, OnToolsShowDock, nullptr);
	g_tools_menu_registered = true;
}

static bool AddDock(const char *id, const char *titleKey, const char *fallback, QWidget *widget)
{
	const char *title = obs_module_text(titleKey);
	if (!title || !*title)
		title = fallback;
	if (!obs_frontend_add_dock_by_id(id, title, widget)) {
		blog(LOG_ERROR, "[obs-shorts-vertical] Failed to add dock '%s'", id);
		delete widget;
		return false;
	}
	return true;
}

static void RegisterDocks()
{
	if (g_docks_registered)
		return;

	QMainWindow *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main) {
		blog(LOG_ERROR, "[obs-shorts-vertical] Main window not available; docks not registered");
		return;
	}

	/* Drop legacy three-dock layout if present from older builds. */
	obs_frontend_remove_dock(DOCK_TRANSITIONS_LEGACY);
	obs_frontend_remove_dock(DOCK_SOURCES_LEGACY);
	obs_frontend_remove_dock(DOCK_SCENES_LEGACY);

	auto *workspace = new ShortsDock(main);
	if (!AddDock(DOCK_CANVAS, "ShortsDock", "Vertical Shorts", workspace))
		return;
	g_workspace = workspace;

	if (!AddDock(DOCK_PRODUCTION, "VerticalProductionDock", "Vertical Production",
		     new VerticalProductionDock(workspace, main))) {
		blog(LOG_WARNING, "[obs-shorts-vertical] Vertical Production dock failed to register");
	}

	g_docks_registered = true;

	if (!g_save_callback_registered) {
		obs_frontend_add_save_callback(SaveCallback, nullptr);
		g_save_callback_registered = true;
	}

	RegisterToolsMenu();

	ShowDockById(DOCK_CANVAS);
	ShowDockById(DOCK_PRODUCTION);

	blog(LOG_INFO, "[obs-shorts-vertical] Native docks registered (Vertical Shorts + Vertical Production)");
}

static void FrontendEvent(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING)
		RegisterDocks();
}

bool obs_module_load(void)
{
	const uint32_t advertised = LIBOBS_API_VER;
	const uint32_t host = obs_get_version();

	blog(LOG_INFO,
	     "[obs-shorts-vertical] Loading Vertical Shorts Plugin %s (built %s, host libobs %s, "
	     "advertised module API %u.%u.%u, host API %u.%u.%u)",
	     PLUGIN_VERSION, PLUGIN_BUILD_TIMESTAMP, obs_get_version_string(), (advertised >> 24) & 0xFF,
	     (advertised >> 16) & 0xFF, advertised & 0xFFFF, (host >> 24) & 0xFF, (host >> 16) & 0xFF, host & 0xFFFF);

	const char *bin = obs_get_module_binary_path(obs_current_module());
	const char *data = obs_get_module_data_path(obs_current_module());
	blog(LOG_INFO, "[obs-shorts-vertical] Module binary: %s", bin ? bin : "(null)");
	blog(LOG_INFO, "[obs-shorts-vertical] Module data: %s", data ? data : "(null)");

	/* Major-only floor: Vertical Shorts requires OBS 32 canvas/frontend APIs. */
	if (((host >> 24) & 0xFF) < 32) {
		blog(LOG_ERROR,
		     "[obs-shorts-vertical] Refusing to load: OBS Studio 32.0 or newer is required "
		     "(detected %u.%u). Update OBS or remove this plugin.",
		     (host >> 24) & 0xFF, (host >> 16) & 0xFF);
		return false;
	}

	obs_frontend_add_event_callback(FrontendEvent, nullptr);

	/*
	 * Do NOT construct docks / canvas / camera UI during obs_module_load.
	 * The frontend may not be fully ready; defer to FINISHED_LOADING (and
	 * obs_module_post_load as a backup once the main window exists).
	 */
	return true;
}

void obs_module_post_load(void)
{
	blog(LOG_INFO, "[obs-shorts-vertical] obs_module_post_load");
	if (!g_docks_registered && obs_frontend_get_main_window())
		RegisterDocks();
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(FrontendEvent, nullptr);
	if (g_save_callback_registered) {
		obs_frontend_remove_save_callback(SaveCallback, nullptr);
		g_save_callback_registered = false;
	}

	if (g_docks_registered) {
		obs_frontend_remove_dock(DOCK_PRODUCTION);
		obs_frontend_remove_dock(DOCK_CANVAS);
		obs_frontend_remove_dock(DOCK_TRANSITIONS_LEGACY);
		obs_frontend_remove_dock(DOCK_SOURCES_LEGACY);
		obs_frontend_remove_dock(DOCK_SCENES_LEGACY);
		g_workspace = nullptr;
		g_docks_registered = false;
	}

	blog(LOG_INFO, "[obs-shorts-vertical] Plugin unloaded");
}
