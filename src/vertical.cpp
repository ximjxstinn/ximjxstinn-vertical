/*
 * xImJxstinn Vertical – Kern: Leinwand, Szenen, TikTok-Stream
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#include "vertical.hpp"

#include <obs-module.h>

#include <QCoreApplication>
#include <QMetaObject>

#include <util/platform.h>

#include <algorithm>
#include <cstring>

/* ------------------------------------------------------------------ */
/* Gruppen-Hilfen                                                      */
/* ------------------------------------------------------------------ */

struct FindData {
	int64_t id;
	obs_sceneitem_t *found;
};

static bool find_in_group(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	auto d = static_cast<FindData *>(param);
	if (obs_sceneitem_get_id(item) == d->id) {
		d->found = item;
		return false;
	}
	return true;
}

obs_sceneitem_t *XVFindItem(obs_scene_t *scene, int64_t id)
{
	if (!scene || !id)
		return nullptr;
	if (obs_sceneitem_t *item = obs_scene_find_sceneitem_by_id(scene, id))
		return item;
	struct Ctx {
		int64_t id;
		obs_sceneitem_t *found;
	} ctx = {id, nullptr};
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			auto c = static_cast<Ctx *>(param);
			if (!obs_sceneitem_is_group(item))
				return true;
			FindData d = {c->id, nullptr};
			obs_sceneitem_group_enum_items(item, find_in_group, &d);
			if (d.found) {
				c->found = d.found;
				return false;
			}
			return true;
		},
		&ctx);
	return ctx.found;
}

void XVItemBox(obs_scene_t *scene, obs_sceneitem_t *item, matrix4 &out)
{
	obs_sceneitem_get_box_transform(item, &out);
	obs_sceneitem_t *group = scene ? obs_sceneitem_get_group(scene, item) : nullptr;
	if (group) {
		matrix4 parent, combined;
		obs_sceneitem_get_draw_transform(group, &parent);
		matrix4_mul(&combined, &out, &parent);
		out = combined;
	}
}

void XVItemParentInverse(obs_scene_t *scene, obs_sceneitem_t *item, matrix4 &out)
{
	matrix4_identity(&out);
	obs_sceneitem_t *group = scene ? obs_sceneitem_get_group(scene, item) : nullptr;
	if (group) {
		matrix4 parent;
		obs_sceneitem_get_draw_transform(group, &parent);
		if (!matrix4_inv(&out, &parent))
			matrix4_identity(&out);
	}
}

Vertical &Vertical::Get()
{
	static Vertical instance;
	return instance;
}

/* ------------------------------------------------------------------ */
/* Einstellungen                                                       */
/* ------------------------------------------------------------------ */

void Vertical::LoadSettings()
{
	char *path = obs_module_config_path("settings.json");
	obs_data_t *d = obs_data_create_from_json_file_safe(path, "bak");
	bfree(path);
	if (!d)
		return;
	settings.server = obs_data_get_string(d, "server");
	settings.key = obs_data_get_string(d, "key");
	if (obs_data_has_user_value(d, "width"))
		settings.width = (int)obs_data_get_int(d, "width");
	if (obs_data_has_user_value(d, "height"))
		settings.height = (int)obs_data_get_int(d, "height");
	if (obs_data_has_user_value(d, "bitrate"))
		settings.bitrate = (int)obs_data_get_int(d, "bitrate");
	if (obs_data_has_user_value(d, "encoder"))
		settings.encoder = obs_data_get_string(d, "encoder");
	if (obs_data_has_user_value(d, "start_with_main"))
		settings.startWithMain = obs_data_get_bool(d, "start_with_main");
	if (obs_data_has_user_value(d, "auto_switch"))
		settings.autoSwitch = obs_data_get_bool(d, "auto_switch");
	if (obs_data_has_user_value(d, "suffix"))
		settings.suffix = obs_data_get_string(d, "suffix");
	settings.hubDir = obs_data_get_string(d, "hub_dir");
	if (obs_data_has_user_value(d, "hub_autostart"))
		settings.hubAutostart = obs_data_get_bool(d, "hub_autostart");
	if (obs_data_has_user_value(d, "auto_update"))
		settings.autoUpdate = obs_data_get_bool(d, "auto_update");
	if (obs_data_has_user_value(d, "transition_mode"))
		settings.transitionMode = obs_data_get_string(d, "transition_mode");
	if (obs_data_has_user_value(d, "transition_duration"))
		settings.transitionDuration = (int)obs_data_get_int(d, "transition_duration");
	obs_data_release(d);

	if (settings.width < 160 || settings.height < 160) {
		settings.width = 1080;
		settings.height = 1920;
	}
}

void Vertical::SaveSettings()
{
	char *dir = obs_module_config_path("");
	if (dir) {
		os_mkdirs(dir);
		bfree(dir);
	}
	obs_data_t *d = obs_data_create();
	obs_data_set_string(d, "server", settings.server.c_str());
	obs_data_set_string(d, "key", settings.key.c_str());
	obs_data_set_int(d, "width", settings.width);
	obs_data_set_int(d, "height", settings.height);
	obs_data_set_int(d, "bitrate", settings.bitrate);
	obs_data_set_string(d, "encoder", settings.encoder.c_str());
	obs_data_set_bool(d, "start_with_main", settings.startWithMain);
	obs_data_set_bool(d, "auto_switch", settings.autoSwitch);
	obs_data_set_string(d, "suffix", settings.suffix.c_str());
	obs_data_set_string(d, "hub_dir", settings.hubDir.c_str());
	obs_data_set_bool(d, "hub_autostart", settings.hubAutostart);
	obs_data_set_bool(d, "auto_update", settings.autoUpdate);
	obs_data_set_string(d, "transition_mode", settings.transitionMode.c_str());
	obs_data_set_int(d, "transition_duration", settings.transitionDuration);
	char *path = obs_module_config_path("settings.json");
	obs_data_save_json_safe(d, path, "tmp", "bak");
	bfree(path);
	obs_data_release(d);
}

/* ------------------------------------------------------------------ */
/* Leinwand                                                            */
/* ------------------------------------------------------------------ */

void Vertical::MakeVideoInfo(obs_video_info &ovi)
{
	obs_get_video_info(&ovi);
	ovi.base_width = (uint32_t)settings.width;
	ovi.base_height = (uint32_t)settings.height;
	ovi.output_width = (uint32_t)settings.width;
	ovi.output_height = (uint32_t)settings.height;
}

obs_canvas_t *Vertical::CanvasRef()
{
	std::lock_guard<std::mutex> lock(mutex);
	if (!canvas || obs_canvas_removed(canvas))
		return nullptr;
	return obs_canvas_get_ref(canvas);
}

void Vertical::CanvasSize(uint32_t &cx, uint32_t &cy)
{
	cx = (uint32_t)settings.width;
	cy = (uint32_t)settings.height;
	obs_canvas_t *c = CanvasRef();
	if (!c)
		return;
	obs_video_info ovi;
	if (obs_canvas_get_video_info(c, &ovi)) {
		cx = ovi.base_width;
		cy = ovi.base_height;
	}
	obs_canvas_release(c);
}

void Vertical::AttachCanvas()
{
	DetachCanvas();

	obs_canvas_t *found = nullptr;
	obs_canvas_t *legacy = nullptr;

	struct obs_frontend_canvas_list list = {};
	obs_frontend_get_canvases(&list);
	for (size_t i = 0; i < list.canvases.num; i++) {
		obs_canvas_t *c = list.canvases.array[i];
		const char *name = obs_canvas_get_name(c);
		if (!name)
			continue;
		if (strcmp(name, XV_CANVAS_NAME) == 0 && !found)
			found = obs_canvas_get_ref(c);
		else if (strcmp(name, XV_LEGACY_CANVAS_NAME) == 0 && !legacy)
			legacy = obs_canvas_get_ref(c);
	}
	obs_frontend_canvas_list_free(&list);

	/* Bestehende Hochformat-Szenen (z. B. aus Aitum) übernehmen */
	if (!found && legacy) {
		found = legacy;
		legacy = nullptr;
		obs_canvas_set_name(found, XV_CANVAS_NAME);
		blog(LOG_INFO, "[xImJxstinn Vertical] bestehende Hochformat-Leinwand übernommen");
	}
	if (legacy)
		obs_canvas_release(legacy);

	obs_video_info ovi;
	MakeVideoInfo(ovi);

	if (!found) {
		found = obs_frontend_add_canvas(XV_CANVAS_NAME, &ovi, PROGRAM);
		blog(LOG_INFO, "[xImJxstinn Vertical] neue Hochformat-Leinwand erstellt");
	} else if (!obs_canvas_has_video(found)) {
		obs_canvas_reset_video(found, &ovi);
	}

	if (!found) {
		blog(LOG_ERROR, "[xImJxstinn Vertical] Leinwand konnte nicht erstellt werden");
		return;
	}

	{
		std::lock_guard<std::mutex> lock(mutex);
		canvas = found;
	}

	/* Szene wählen */
	std::vector<std::string> names = SceneNames();
	if (names.empty()) {
		CreateScene("Szene 1");
		names = SceneNames();
	}
	std::string target = names.empty() ? std::string() : names.front();
	for (const auto &n : names) {
		if (n == wantedScene) {
			target = n;
			break;
		}
	}
	if (!target.empty())
		SetCurrentScene(target);

	Notify();
}

void Vertical::DetachCanvas()
{
	obs_canvas_t *old = nullptr;
	{
		std::lock_guard<std::mutex> lock(mutex);
		old = canvas;
		canvas = nullptr;
	}
	selectedItem = 0;
	ReleaseTransition();
	if (old)
		obs_canvas_release(old);
}

bool Vertical::ApplyVideo(std::string &error)
{
	if (IsStreaming()) {
		error = "Während des TikTok-Streams kann die Auflösung nicht geändert werden.";
		return false;
	}
	obs_canvas_t *c = CanvasRef();
	if (!c) {
		error = "Keine Hochformat-Leinwand vorhanden.";
		return false;
	}
	obs_video_info ovi;
	MakeVideoInfo(ovi);
	bool ok = obs_canvas_reset_video(c, &ovi);
	obs_canvas_release(c);
	if (!ok)
		error = "Die Auflösung konnte nicht übernommen werden.";
	return ok;
}

/* ------------------------------------------------------------------ */
/* Szenen                                                              */
/* ------------------------------------------------------------------ */

std::vector<std::string> Vertical::SceneNames()
{
	std::vector<std::string> names;
	obs_canvas_t *c = CanvasRef();
	if (!c)
		return names;
	std::vector<std::string> raw;
	obs_canvas_enum_scenes(
		c,
		[](void *param, obs_source_t *src) {
			auto list = static_cast<std::vector<std::string> *>(param);
			const char *n = obs_source_get_name(src);
			if (n && !obs_source_is_group(src))
				list->emplace_back(n);
			return true;
		},
		&raw);
	obs_canvas_release(c);

	/* Gespeicherte Reihenfolge zuerst, neue Szenen hinten anhängen */
	for (const auto &n : sceneOrder) {
		if (std::find(raw.begin(), raw.end(), n) != raw.end() &&
		    std::find(names.begin(), names.end(), n) == names.end())
			names.push_back(n);
	}
	for (const auto &n : raw) {
		if (std::find(names.begin(), names.end(), n) == names.end())
			names.push_back(n);
	}
	sceneOrder = names;
	return names;
}

void Vertical::MoveScene(const std::string &name, int delta)
{
	std::vector<std::string> names = SceneNames();
	auto it = std::find(names.begin(), names.end(), name);
	if (it == names.end())
		return;
	const long idx = (long)(it - names.begin());
	const long target = idx + delta;
	if (target < 0 || target >= (long)names.size())
		return;
	std::swap(names[(size_t)idx], names[(size_t)target]);
	SetSceneOrder(names);
}

void Vertical::SetSceneOrder(const std::vector<std::string> &order)
{
	sceneOrder = order;
	obs_frontend_save();
	Notify();
}

obs_source_t *Vertical::CurrentSceneRef()
{
	obs_canvas_t *c = CanvasRef();
	if (!c)
		return nullptr;
	obs_source_t *s = obs_canvas_get_channel(c, 0);
	obs_canvas_release(c);
	if (s && obs_source_get_type(s) == OBS_SOURCE_TYPE_TRANSITION) {
		obs_source_t *active = obs_transition_get_active_source(s);
		obs_source_release(s);
		s = active;
	}
	return s;
}

std::string Vertical::CurrentSceneName()
{
	std::string name;
	obs_source_t *s = CurrentSceneRef();
	if (s) {
		const char *n = obs_source_get_name(s);
		if (n)
			name = n;
		obs_source_release(s);
	}
	return name;
}

void Vertical::SetCurrentScene(const std::string &name)
{
	obs_canvas_t *c = CanvasRef();
	if (!c)
		return;
	obs_scene_t *scene = obs_canvas_get_scene_by_name(c, name.c_str());
	if (scene) {
		obs_source_t *target = obs_scene_get_source(scene);
		EnsureTransition(c);
		obs_source_t *cur = CurrentSceneRef();
		const bool same = cur == target;
		const bool first = cur == nullptr;
		obs_source_release(cur);
		if (transition) {
			const int ms = TransitionDurationMs();
			if (!same) {
				if (first || ms <= 0 || !obs_transition_start(transition, OBS_TRANSITION_MODE_AUTO, (uint32_t)ms, target))
					obs_transition_set(transition, target);
			}
		} else {
			obs_canvas_set_channel(c, 0, target);
		}
		obs_scene_release(scene);
		if (!same)
			selectedItem = 0;
		wantedScene = name;
	}
	obs_canvas_release(c);
	Notify();
}

bool Vertical::CreateScene(const std::string &name)
{
	if (name.empty())
		return false;
	obs_canvas_t *c = CanvasRef();
	if (!c)
		return false;
	bool ok = false;
	obs_source_t *existing = obs_canvas_get_source_by_name(c, name.c_str());
	if (existing) {
		obs_source_release(existing);
	} else {
		obs_scene_t *scene = obs_canvas_scene_create(c, name.c_str());
		if (scene) {
			obs_scene_release(scene); /* die Leinwand hält ihre eigene Referenz */
			ok = true;
		}
	}
	obs_canvas_release(c);
	if (ok)
		Notify();
	return ok;
}

bool Vertical::RenameScene(const std::string &oldName, const std::string &newName)
{
	if (newName.empty() || oldName == newName)
		return false;
	obs_canvas_t *c = CanvasRef();
	if (!c)
		return false;
	bool ok = false;
	obs_source_t *clash = obs_canvas_get_source_by_name(c, newName.c_str());
	if (clash) {
		obs_source_release(clash);
	} else {
		obs_source_t *src = obs_canvas_get_source_by_name(c, oldName.c_str());
		if (src) {
			obs_source_set_name(src, newName.c_str());
			obs_source_release(src);
			if (wantedScene == oldName)
				wantedScene = newName;
			for (auto &n : sceneOrder) {
				if (n == oldName)
					n = newName;
			}
			ok = true;
		}
	}
	obs_canvas_release(c);
	if (ok)
		Notify();
	return ok;
}

void Vertical::RemoveScene(const std::string &name)
{
	obs_canvas_t *c = CanvasRef();
	if (!c)
		return;
	const bool wasCurrent = CurrentSceneName() == name;
	obs_source_t *src = obs_canvas_get_source_by_name(c, name.c_str());
	if (src) {
		if (wasCurrent) {
			if (transition)
				obs_transition_set(transition, nullptr);
			else
				obs_canvas_set_channel(c, 0, nullptr);
		}
		obs_source_remove(src);
		obs_source_release(src);
	}
	obs_canvas_release(c);

	if (wasCurrent) {
		std::vector<std::string> names = SceneNames();
		if (!names.empty())
			SetCurrentScene(names.front());
	}
	Notify();
}

void Vertical::OnMainSceneChanged()
{
	if (!settings.autoSwitch)
		return;
	obs_source_t *main = obs_frontend_get_current_scene();
	if (!main)
		return;

	std::string target = LinkedVerticalFor(main);
	if (target.empty()) {
		/* Ohne feste Verbindung: gleicher Name (+ Endung) */
		const char *mn = obs_source_get_name(main);
		const std::string mainName = mn ? mn : "";
		const std::vector<std::string> names = SceneNames();
		for (const std::string &cand : {mainName + settings.suffix, mainName}) {
			if (std::find(names.begin(), names.end(), cand) != names.end()) {
				target = cand;
				break;
			}
		}
	}

	/* Übergangs-Überschreibung der OBS-Szene übernehmen */
	overrideTransition.clear();
	overrideMs = -1;
	obs_data_t *priv = obs_source_get_private_settings(main);
	if (priv) {
		const char *t = obs_data_get_string(priv, "transition");
		if (t && *t) {
			overrideTransition = t;
			overrideMs = (int)obs_data_get_int(priv, "transition_duration");
		}
		obs_data_release(priv);
	}
	obs_source_release(main);

	if (!target.empty() && CurrentSceneName() != target)
		SetCurrentScene(target);

	overrideTransition.clear();
	overrideMs = -1;
}

static std::string source_uuid(obs_source_t *s)
{
	const char *u = s ? obs_source_get_uuid(s) : nullptr;
	return u ? u : "";
}

void Vertical::SetLinks(const std::map<std::string, std::string> &newLinks)
{
	links = newLinks;
	obs_frontend_save();
	Notify();
}

std::string Vertical::LinkedVerticalFor(obs_source_t *mainScene)
{
	auto it = links.find(source_uuid(mainScene));
	if (it == links.end())
		return std::string();
	obs_source_t *v = obs_get_source_by_uuid(it->second.c_str());
	std::string name;
	if (v) {
		const char *n = obs_source_get_name(v);
		if (n)
			name = n;
		obs_source_release(v);
	}
	return name;
}

std::vector<std::string> Vertical::LinkedMainScenes(const std::string &verticalName)
{
	std::vector<std::string> result;
	obs_canvas_t *c = CanvasRef();
	if (!c)
		return result;
	obs_source_t *v = obs_canvas_get_source_by_name(c, verticalName.c_str());
	obs_canvas_release(c);
	const std::string vu = source_uuid(v);
	obs_source_release(v);
	if (vu.empty())
		return result;
	for (const auto &l : links) {
		if (l.second != vu)
			continue;
		obs_source_t *m = obs_get_source_by_uuid(l.first.c_str());
		if (m) {
			const char *n = obs_source_get_name(m);
			if (n)
				result.emplace_back(n);
			obs_source_release(m);
		}
	}
	return result;
}

/* ------------------------------------------------------------------ */
/* Szenenübergänge                                                     */
/* ------------------------------------------------------------------ */

int Vertical::TransitionDurationMs()
{
	if (settings.transitionMode == "cut")
		return 0;
	if (settings.transitionMode == "obs")
		return overrideMs > 0 ? overrideMs : obs_frontend_get_transition_duration();
	return settings.transitionDuration;
}

void Vertical::ReleaseTransition()
{
	if (!transition)
		return;
	obs_transition_clear(transition);
	obs_source_release(transition);
	transition = nullptr;
	transitionKey.clear();
}

void Vertical::EnsureTransition(obs_canvas_t *c)
{
	std::string key = settings.transitionMode;
	obs_source_t *mainTransition = nullptr;
	if (key == "obs") {
		if (!overrideTransition.empty()) {
			struct obs_frontend_source_list list = {};
			obs_frontend_get_transitions(&list);
			for (size_t i = 0; i < list.sources.num; i++) {
				const char *n = obs_source_get_name(list.sources.array[i]);
				if (n && overrideTransition == n) {
					mainTransition = obs_source_get_ref(list.sources.array[i]);
					break;
				}
			}
			obs_frontend_source_list_free(&list);
		}
		if (!mainTransition)
			mainTransition = obs_frontend_get_current_transition();
		const char *n = mainTransition ? obs_source_get_name(mainTransition) : nullptr;
		key += ":";
		key += n ? n : "";
	}

	if (transition && key == transitionKey) {
		obs_source_t *ch = obs_canvas_get_channel(c, 0);
		if (ch != transition)
			obs_canvas_set_channel(c, 0, transition);
		obs_source_release(ch);
		obs_source_release(mainTransition);
		return;
	}

	/* Aktuelle Szene merken, neuen Übergang bauen */
	obs_source_t *active = CurrentSceneRef();
	obs_source_t *created = nullptr;
	const char *name = "xImJxstinn Vertical Übergang";
	if (mainTransition)
		created = obs_source_duplicate(mainTransition, name, true);
	if (!created) {
		const char *id = "fade_transition";
		if (settings.transitionMode == "cut")
			id = "cut_transition";
		else if (settings.transitionMode == "slide")
			id = "slide_transition";
		else if (settings.transitionMode == "swipe")
			id = "swipe_transition";
		created = obs_source_create_private(id, name, nullptr);
	}
	obs_source_release(mainTransition);

	if (!created) {
		obs_source_release(active);
		return;
	}
	if (active && obs_source_get_type(active) != OBS_SOURCE_TYPE_TRANSITION)
		obs_transition_set(created, active);
	obs_canvas_set_channel(c, 0, created);
	obs_source_release(active);

	ReleaseTransition();
	transition = created;
	transitionKey = key;
}

void Vertical::TransitionSettingsChanged()
{
	obs_canvas_t *c = CanvasRef();
	if (!c)
		return;
	EnsureTransition(c);
	obs_canvas_release(c);
}

/* ------------------------------------------------------------------ */
/* TikTok-Stream                                                       */
/* ------------------------------------------------------------------ */

static bool encoder_available(const char *id)
{
	const char *type = nullptr;
	for (size_t i = 0; obs_enum_encoder_types(i, &type); i++) {
		if (type && strcmp(type, id) == 0)
			return true;
	}
	return false;
}

static std::string pick_encoder(const std::string &wanted)
{
	if (wanted == "nvenc") {
		if (encoder_available("obs_nvenc_h264_tex"))
			return "obs_nvenc_h264_tex";
		if (encoder_available("jim_nvenc"))
			return "jim_nvenc";
	} else if (wanted == "x264") {
		return "obs_x264";
	} else {
		const char *preferred[] = {"obs_nvenc_h264_tex", "jim_nvenc", "h264_texture_amf", "obs_qsv11_v2", "obs_x264"};
		for (const char *id : preferred) {
			if (encoder_available(id))
				return id;
		}
	}
	return "obs_x264";
}

bool Vertical::IsStreaming()
{
	return output && obs_output_active(output);
}

void Vertical::ReleaseOutput()
{
	if (output) {
		signal_handler_disconnect(obs_output_get_signal_handler(output), "stop", OutputStopped, this);
		if (obs_output_active(output))
			obs_output_force_stop(output);
		obs_output_release(output);
		output = nullptr;
	}
	if (videoEncoder) {
		obs_encoder_release(videoEncoder);
		videoEncoder = nullptr;
	}
	if (audioEncoder) {
		obs_encoder_release(audioEncoder);
		audioEncoder = nullptr;
	}
	if (service) {
		obs_service_release(service);
		service = nullptr;
	}
}

void Vertical::OutputStopped(void *data, calldata_t *cd)
{
	auto self = static_cast<Vertical *>(data);
	const long long code = calldata_int(cd, "code");
	if (code != OBS_OUTPUT_SUCCESS)
		blog(LOG_WARNING, "[xImJxstinn Vertical] TikTok-Stream beendet mit Code %lld", code);
	self->Notify();
}

bool Vertical::StartStream(std::string &error)
{
	if (IsStreaming())
		return true;
	if (settings.server.empty() || settings.key.empty()) {
		error = "Bitte zuerst Server-URL und Stream-Key von TikTok in den Einstellungen eintragen.";
		return false;
	}
	obs_canvas_t *c = CanvasRef();
	if (!c) {
		error = "Keine Hochformat-Leinwand vorhanden.";
		return false;
	}
	video_t *video = obs_canvas_get_video(c);
	obs_canvas_release(c);
	if (!video) {
		error = "Die Hochformat-Leinwand hat kein Videobild.";
		return false;
	}

	ReleaseOutput();

	obs_data_t *ss = obs_data_create();
	obs_data_set_string(ss, "server", settings.server.c_str());
	obs_data_set_string(ss, "key", settings.key.c_str());
	service = obs_service_create("rtmp_custom", "ximjxstinn_vertical_service", ss, nullptr);
	obs_data_release(ss);

	const std::string encId = pick_encoder(settings.encoder);
	obs_data_t *vs = obs_data_create();
	obs_data_set_string(vs, "rate_control", "CBR");
	obs_data_set_int(vs, "bitrate", settings.bitrate);
	obs_data_set_int(vs, "keyint_sec", 2);
	if (encId == "obs_x264")
		obs_data_set_string(vs, "preset", "veryfast");
	videoEncoder = obs_video_encoder_create(encId.c_str(), "ximjxstinn_vertical_video", vs, nullptr);
	obs_data_release(vs);

	obs_data_t *as = obs_data_create();
	obs_data_set_int(as, "bitrate", 160);
	audioEncoder = obs_audio_encoder_create("ffmpeg_aac", "ximjxstinn_vertical_audio", as, 0, nullptr);
	obs_data_release(as);

	output = obs_output_create("rtmp_output", "ximjxstinn_vertical_stream", nullptr, nullptr);

	if (!service || !videoEncoder || !audioEncoder || !output) {
		error = "Stream-Ausgabe konnte nicht erstellt werden.";
		ReleaseOutput();
		return false;
	}

	obs_encoder_set_video(videoEncoder, video);
	obs_encoder_set_audio(audioEncoder, obs_get_audio());
	obs_output_set_video_encoder(output, videoEncoder);
	obs_output_set_audio_encoder(output, audioEncoder, 0);
	obs_output_set_service(output, service);
	signal_handler_connect(obs_output_get_signal_handler(output), "stop", OutputStopped, this);

	if (!obs_output_start(output)) {
		const char *err = obs_output_get_last_error(output);
		error = std::string("TikTok-Stream konnte nicht starten") + (err && *err ? std::string(": ") + err : ".");
		ReleaseOutput();
		Notify();
		return false;
	}
	blog(LOG_INFO, "[xImJxstinn Vertical] TikTok-Stream gestartet (%s, %d kbps)", encId.c_str(), settings.bitrate);
	Notify();
	return true;
}

void Vertical::Shutdown()
{
	ReleaseOutput();
	DetachCanvas();
	ReleaseTransition();
}

void Vertical::StopStream()
{
	if (output && obs_output_active(output))
		obs_output_stop(output);
	Notify();
}

/* ------------------------------------------------------------------ */
/* Szenensammlung                                                      */
/* ------------------------------------------------------------------ */

void Vertical::SaveCollection(obs_data_t *data)
{
	obs_data_t *obj = obs_data_create();
	const std::string current = CurrentSceneName();
	obs_data_set_string(obj, "current_scene", current.empty() ? wantedScene.c_str() : current.c_str());
	obs_data_array_t *order = obs_data_array_create();
	for (const auto &n : sceneOrder) {
		obs_data_t *entry = obs_data_create();
		obs_data_set_string(entry, "name", n.c_str());
		obs_data_array_push_back(order, entry);
		obs_data_release(entry);
	}
	obs_data_set_array(obj, "scene_order", order);
	obs_data_array_release(order);
	obs_data_array_t *linkArray = obs_data_array_create();
	for (const auto &l : links) {
		obs_data_t *entry = obs_data_create();
		obs_data_set_string(entry, "main", l.first.c_str());
		obs_data_set_string(entry, "vertical", l.second.c_str());
		obs_data_array_push_back(linkArray, entry);
		obs_data_release(entry);
	}
	obs_data_set_array(obj, "links", linkArray);
	obs_data_array_release(linkArray);
	obs_data_set_obj(data, "ximjxstinn_vertical", obj);
	obs_data_release(obj);
}

void Vertical::LoadCollection(obs_data_t *data)
{
	obs_data_t *obj = obs_data_get_obj(data, "ximjxstinn_vertical");
	wantedScene.clear();
	sceneOrder.clear();
	links.clear();
	if (obj) {
		wantedScene = obs_data_get_string(obj, "current_scene");
		obs_data_array_t *order = obs_data_get_array(obj, "scene_order");
		const size_t count = obs_data_array_count(order);
		for (size_t i = 0; i < count; i++) {
			obs_data_t *entry = obs_data_array_item(order, i);
			const char *n = obs_data_get_string(entry, "name");
			if (n && *n)
				sceneOrder.emplace_back(n);
			obs_data_release(entry);
		}
		obs_data_array_release(order);
		obs_data_array_t *linkArray = obs_data_get_array(obj, "links");
		const size_t linkCount = obs_data_array_count(linkArray);
		for (size_t i = 0; i < linkCount; i++) {
			obs_data_t *entry = obs_data_array_item(linkArray, i);
			const char *m = obs_data_get_string(entry, "main");
			const char *v = obs_data_get_string(entry, "vertical");
			if (m && *m && v && *v)
				links[m] = v;
			obs_data_release(entry);
		}
		obs_data_array_release(linkArray);
		obs_data_release(obj);
	}
}

/* ------------------------------------------------------------------ */
/* UI-Benachrichtigung                                                 */
/* ------------------------------------------------------------------ */

void Vertical::AddListener(QObject *owner, std::function<void()> cb)
{
	listeners.emplace_back(QPointer<QObject>(owner), std::move(cb));
}

void Vertical::Notify()
{
	QMetaObject::invokeMethod(
		QCoreApplication::instance(),
		[this] {
			for (auto &l : listeners) {
				if (l.first)
					l.second();
			}
		},
		Qt::QueuedConnection);
}
