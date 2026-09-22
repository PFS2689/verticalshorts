#include "capture-source-share.hpp"

#include <obs-module.h>

#include <cstring>
#include <initializer_list>
#include <utility>

namespace vsp {
namespace {

bool ContainsInsensitive(const char *hay, const char *needle)
{
	if (!hay || !needle || !*needle)
		return false;
	const size_t n = strlen(needle);
	for (const char *p = hay; *p; ++p) {
		size_t i = 0;
		while (i < n) {
			char a = p[i];
			char b = needle[i];
			if (a >= 'A' && a <= 'Z')
				a = char(a - 'A' + 'a');
			if (b >= 'A' && b <= 'Z')
				b = char(b - 'A' + 'a');
			if (a != b)
				break;
			++i;
		}
		if (i == n)
			return true;
		if (!*(p + 1))
			break;
	}
	return false;
}

const char *FirstNonEmptySetting(obs_data_t *settings, std::initializer_list<const char *> keys)
{
	if (!settings)
		return nullptr;
	for (const char *key : keys) {
		const char *val = obs_data_get_string(settings, key);
		if (val && *val)
			return val;
	}
	return nullptr;
}

} // namespace

bool IsVideoCaptureSourceId(const char *id)
{
	if (!id || !*id)
		return false;
	/* Exact common IDs first, then substring fallback for versioned plugins. */
	if (strcmp(id, "dshow_input") == 0 || strcmp(id, "av_capture_input") == 0 ||
	    strcmp(id, "v4l2_input") == 0 || strcmp(id, "dshow") == 0 || strcmp(id, "av_capture") == 0 ||
	    strcmp(id, "v4l2") == 0)
		return true;
	return ContainsInsensitive(id, "dshow") || ContainsInsensitive(id, "av_capture") ||
	       ContainsInsensitive(id, "v4l2");
}

std::string CaptureSourceFamily(const char *id)
{
	if (!id || !*id)
		return {};
	if (ContainsInsensitive(id, "dshow"))
		return "dshow";
	if (ContainsInsensitive(id, "av_capture"))
		return "av_capture";
	if (ContainsInsensitive(id, "v4l2"))
		return "v4l2";
	return id ? id : "";
}

std::string GetCaptureDeviceKeyFromSettings(const char *typeId, obs_data_t *settings)
{
	const std::string family = CaptureSourceFamily(typeId);
	if (family.empty() || !settings)
		return {};

	const char *deviceId = nullptr;
	if (family == "dshow") {
		/* Windows DirectShow: path-based id is stable across renames. */
		deviceId = FirstNonEmptySetting(settings, {"video_device_id", "last_video_device_id", "video_device"});
	} else if (family == "av_capture") {
		/* macOS AVFoundation unique device string. */
		deviceId = FirstNonEmptySetting(settings, {"device", "device_uid", "uid", "video_device"});
	} else if (family == "v4l2") {
		/* Linux V4L2 node path, e.g. /dev/video0. */
		deviceId = FirstNonEmptySetting(settings, {"device_id", "device"});
	} else {
		deviceId = FirstNonEmptySetting(settings, {"video_device_id", "device_id", "device", "uid"});
	}

	if (!deviceId || !*deviceId)
		return {};

	return family + "|" + deviceId;
}

std::string GetCaptureDeviceKey(obs_source_t *source)
{
	if (!source)
		return {};
	const char *id = obs_source_get_id(source);
	if (!IsVideoCaptureSourceId(id))
		return {};
	OBSDataAutoRelease settings = obs_source_get_settings(source);
	return GetCaptureDeviceKeyFromSettings(id, settings);
}

obs_source_t *FindExistingCaptureByDeviceKey(const std::string &deviceKey, obs_source_t *exclude)
{
	if (deviceKey.empty())
		return nullptr;

	struct Ctx {
		const std::string *key;
		obs_source_t *exclude;
		obs_source_t *found;
	} ctx{&deviceKey, exclude, nullptr};

	obs_enum_sources(
		[](void *param, obs_source_t *source) -> bool {
			auto *c = static_cast<Ctx *>(param);
			if (!source || source == c->exclude || obs_source_removed(source))
				return true;
			if (!IsVideoCaptureSourceId(obs_source_get_id(source)))
				return true;
			const std::string key = GetCaptureDeviceKey(source);
			if (key.empty() || key != *c->key)
				return true;
			c->found = obs_source_get_ref(source);
			return false;
		},
		&ctx);

	return ctx.found;
}

std::vector<CaptureSourceInfo> EnumerateCaptureSources()
{
	std::vector<CaptureSourceInfo> out;
	obs_enum_sources(
		[](void *param, obs_source_t *source) -> bool {
			auto *list = static_cast<std::vector<CaptureSourceInfo> *>(param);
			if (!source || obs_source_removed(source))
				return true;
			if (!IsVideoCaptureSourceId(obs_source_get_id(source)))
				return true;
			CaptureSourceInfo info;
			info.source = OBSSource(source);
			info.typeId = obs_source_get_id(source) ? obs_source_get_id(source) : "";
			info.deviceKey = GetCaptureDeviceKey(source);
			const char *name = obs_source_get_name(source);
			info.displayName = name ? name : "";
			info.width = obs_source_get_width(source);
			info.height = obs_source_get_height(source);
			info.active = obs_source_active(source);
			info.showing = obs_source_showing(source);
			list->push_back(std::move(info));
			return true;
		},
		&out);
	return out;
}

bool VerticalSceneHasSource(obs_scene_t *scene, obs_source_t *source)
{
	if (!scene || !source)
		return false;
	struct Data {
		obs_source_t *source;
		bool found;
	} data{source, false};
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) -> bool {
			auto *d = static_cast<Data *>(param);
			if (obs_sceneitem_get_source(item) == d->source) {
				d->found = true;
				return false;
			}
			return true;
		},
		&data);
	return data.found;
}

} // namespace vsp
