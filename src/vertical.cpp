/*
 * xImJxstinn Vertical – Kern: Leinwand, Szenen, TikTok-Stream
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#include "vertical.hpp"

#include <QCoreApplication>
#include <QMetaObject>

#include <util/platform.h>

#include <cstring>

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
	obs_canvas_enum_scenes(
		c,
		[](void *param, obs_source_t *src) {
			auto list = static_cast<std::vector<std::string> *>(param);
			const char *n = obs_source_get_name(src);
			if (n && !obs_source_is_group(src))
				list->emplace_back(n);
			return true;
		},
		&names);
	obs_canvas_release(c);
	return names;
}

obs_source_t *Vertical::CurrentSceneRef()
{
	obs_canvas_t *c = CanvasRef();
	if (!c)
		return nullptr;
	obs_source_t *s = obs_canvas_get_channel(c, 0);
	obs_canvas_release(c);
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
		obs_canvas_set_channel(c, 0, obs_scene_get_source(scene));
		obs_scene_release(scene);
		wantedScene = name;
		selectedItem = 0;
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
		if (wasCurrent)
			obs_canvas_set_channel(c, 0, nullptr);
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
	std::string mainName = obs_source_get_name(main) ? obs_source_get_name(main) : "";
	obs_source_release(main);
	if (mainName.empty())
		return;

	std::vector<std::string> names = SceneNames();
	const std::string candidates[] = {mainName + settings.suffix, mainName};
	for (const auto &cand : candidates) {
		for (const auto &n : names) {
			if (n == cand) {
				if (CurrentSceneName() != n)
					SetCurrentScene(n);
				return;
			}
		}
	}
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
	obs_data_set_obj(data, "ximjxstinn_vertical", obj);
	obs_data_release(obj);
}

void Vertical::LoadCollection(obs_data_t *data)
{
	obs_data_t *obj = obs_data_get_obj(data, "ximjxstinn_vertical");
	wantedScene.clear();
	if (obj) {
		wantedScene = obs_data_get_string(obj, "current_scene");
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
