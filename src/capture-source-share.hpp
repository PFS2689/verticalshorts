#pragma once

#include <obs.hpp>

#include <string>
#include <vector>

/* OBS multi-canvas helpers: discover Main OBS Video Capture Device sources by
 * registered type ids / stable device keys, then place the same obs_source_t on
 * the Vertical Shorts obs_canvas_t via obs_scene_add. One hardware session;
 * independent vertical scene-item transforms (Fill/Fit do not alter Main OBS). */

namespace vsp {

struct CaptureSourceInfo {
	OBSSource source;
	std::string typeId;
	std::string deviceKey;
	std::string displayName;
	uint32_t width = 0;
	uint32_t height = 0;
	bool active = false;
	bool showing = false;
};

bool IsVideoCaptureSourceId(const char *id);

/* Unversioned family: "dshow_input", "av_capture_input", "v4l2_input", … */
std::string CaptureSourceFamily(const char *id);

/* Stable device key: "family|device_id". Empty if unknown / unset. */
std::string GetCaptureDeviceKey(obs_source_t *source);
std::string GetCaptureDeviceKeyFromSettings(const char *typeId, obs_data_t *settings);

/* Strong-ref to an existing capture source with the same device key, excluding `exclude`. */
obs_source_t *FindExistingCaptureByDeviceKey(const std::string &deviceKey, obs_source_t *exclude = nullptr);

/* All Video Capture Device sources currently registered in OBS. */
std::vector<CaptureSourceInfo> EnumerateCaptureSources();

/* True if this source already has a scene item in the given vertical scene. */
bool VerticalSceneHasSource(obs_scene_t *scene, obs_source_t *source);

} // namespace vsp
