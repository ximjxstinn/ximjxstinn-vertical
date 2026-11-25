/*
 * xImJxstinn Vertical – Anbindung an den MultiStream Hub
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#include "hub.hpp"
#include "vertical.hpp"

#include <curl/curl.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QProcess>
#include <QStandardPaths>

#include <chrono>
#include <mutex>

namespace Hub {

static std::atomic<bool> online{false};
static std::atomic<bool> watcherRunning{false};
static std::thread watcher;
static bool startedByUs = false;
static std::vector<std::pair<QPointer<QObject>, std::function<void(bool)>>> listeners;

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	static_cast<std::string *>(userdata)->append(ptr, size * nmemb);
	return size * nmemb;
}

static bool request(const char *path, bool post, long timeoutMs)
{
	std::string body;
	CURL *curl = curl_easy_init();
	if (!curl)
		return false;
	const std::string url = std::string(XV_HUB_URL) + path;
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, timeoutMs);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeoutMs * 2);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");
	struct curl_slist *headers = nullptr;
	if (post) {
		headers = curl_slist_append(headers, "Content-Type: application/json");
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{}");
	}
	const bool ok = curl_easy_perform(curl) == CURLE_OK;
	if (headers)
		curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return ok;
}

/* ---------- Ordner finden ---------- */

bool IsValidHubDir(const std::string &dir)
{
	if (dir.empty())
		return false;
	return QFileInfo::exists(QDir(QString::fromStdString(dir)).filePath(QStringLiteral("app.py")));
}

std::string FindHubDir()
{
	const std::string configured = Vertical::Get().settings.hubDir;
	if (IsValidHubDir(configured))
		return configured;

	QStringList bases;
	for (auto loc : {QStandardPaths::DesktopLocation, QStandardPaths::DownloadLocation,
			 QStandardPaths::DocumentsLocation, QStandardPaths::HomeLocation})
		bases << QStandardPaths::writableLocation(loc);

	for (const QString &base : bases) {
		if (base.isEmpty())
			continue;
		const QStringList candidates = {QDir(base).filePath(QStringLiteral("MultiStreamHub")),
						QDir(base).filePath(QStringLiteral("MultiStreamHub/MultiStreamHub"))};
		for (const QString &c : candidates) {
			if (IsValidHubDir(c.toStdString()))
				return QDir::toNativeSeparators(c).toStdString();
		}
	}
	return std::string();
}

/* ---------- Starten / Beenden ---------- */

bool Start(std::string &error)
{
	if (Online() || request("/api/state", false, 400)) {
		online = true;
		return true;
	}
	const std::string dir = FindHubDir();
	if (dir.empty()) {
		error = "MultiStream Hub nicht gefunden. Trag den Ordner in den Einstellungen (⚙) ein.";
		return false;
	}
	const QDir d(QString::fromStdString(dir));
	const QString pythonw = d.filePath(QStringLiteral(".venv/Scripts/pythonw.exe"));
	if (!QFileInfo::exists(pythonw)) {
		error = "Der MultiStream Hub ist noch nicht eingerichtet. Starte einmal start.bat im Hub-Ordner.";
		return false;
	}
	const bool ok = QProcess::startDetached(pythonw, {d.filePath(QStringLiteral("app.py")), QStringLiteral("--no-window")},
						d.absolutePath());
	if (!ok) {
		error = "Der MultiStream Hub konnte nicht gestartet werden.";
		return false;
	}
	startedByUs = true;
	blog(LOG_INFO, "[xImJxstinn Vertical] MultiStream Hub gestartet");
	return true;
}

void QuitIfStartedByUs()
{
	if (!startedByUs)
		return;
	request("/api/quit", true, 800);
	startedByUs = false;
}

/* ---------- Online-Überwachung ---------- */

bool Online()
{
	return online.load();
}

static void notify(bool state)
{
	QMetaObject::invokeMethod(
		QCoreApplication::instance(),
		[state] {
			for (auto &l : listeners) {
				if (l.first)
					l.second(state);
			}
		},
		Qt::QueuedConnection);
}

void StartWatcher()
{
	if (watcherRunning.exchange(true))
		return;
	watcher = std::thread([] {
		bool last = false;
		bool first = true;
		while (watcherRunning.load()) {
			const bool now = request("/api/state", false, 400);
			online = now;
			if (first || now != last)
				notify(now);
			last = now;
			first = false;
			for (int i = 0; i < 30 && watcherRunning.load(); i++)
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	});
}

void StopWatcher()
{
	if (!watcherRunning.exchange(false))
		return;
	if (watcher.joinable())
		watcher.join();
}

void AddListener(QObject *owner, std::function<void(bool)> cb)
{
	listeners.emplace_back(QPointer<QObject>(owner), std::move(cb));
}

/* ---------- Overlays ---------- */

struct OverlayInfo {
	const char *name;
	const char *path;
	int width;
	int height;
	bool audio;
};

static OverlayInfo overlay_info(Overlay kind, bool vertical)
{
	switch (kind) {
	case Overlay::Chat:
		return vertical ? OverlayInfo{"xImJxstinn Chat (Hochformat)", "/overlay?style=vertical", 1080, 900, false}
				: OverlayInfo{"xImJxstinn Chat", "/overlay", 450, 800, false};
	case Overlay::Alerts:
		return {"xImJxstinn Alerts", "/alerts", 800, 300, true};
	case Overlay::Viewers:
		return {"xImJxstinn Zuschauer", "/viewers", 600, 80, false};
	case Overlay::Highlight:
	default:
		return {"xImJxstinn Hervorgehoben", "/highlight", 900, 250, false};
	}
}

static void place(obs_sceneitem_t *item, float x, float y, float w, float h)
{
	obs_transform_info info;
	obs_sceneitem_get_info2(item, &info);
	info.rot = 0.0f;
	vec2_set(&info.pos, x, y);
	vec2_set(&info.scale, 1.0f, 1.0f);
	info.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
	info.bounds_type = OBS_BOUNDS_SCALE_INNER;
	info.bounds_alignment = OBS_ALIGN_CENTER;
	vec2_set(&info.bounds, w, h);
	info.crop_to_bounds = false;
	obs_sceneitem_set_info2(item, &info);
}

bool AddOverlay(obs_scene_t *scene, Overlay kind, bool vertical, uint32_t cxu, uint32_t cyu)
{
	if (!scene || !cxu || !cyu)
		return false;
	const OverlayInfo info = overlay_info(kind, vertical);

	obs_source_t *source = obs_get_source_by_name(info.name);
	if (!source) {
		obs_data_t *s = obs_data_create();
		obs_data_set_string(s, "url", (std::string(XV_HUB_URL) + info.path).c_str());
		obs_data_set_int(s, "width", info.width);
		obs_data_set_int(s, "height", info.height);
		obs_data_set_bool(s, "reroute_audio", info.audio);
		obs_data_set_bool(s, "shutdown", false);
		source = obs_source_create("browser_source", info.name, s, nullptr);
		obs_data_release(s);
	}
	if (!source)
		return false;

	obs_sceneitem_t *item = obs_scene_add(scene, source);
	obs_source_release(source);
	if (!item)
		return false;

	const float cx = (float)cxu;
	const float cy = (float)cyu;
	const float ratio = (float)info.height / (float)info.width;

	if (vertical) {
		switch (kind) {
		case Overlay::Chat:
			place(item, 0.0f, cy * 0.52f, cx, cy * 0.30f);
			break;
		case Overlay::Alerts: {
			const float w = cx * 0.9f;
			place(item, (cx - w) * 0.5f, cy * 0.36f, w, w * ratio);
			break;
		}
		case Overlay::Viewers: {
			const float w = cx * 0.6f;
			place(item, cx * 0.04f, cy * 0.015f, w, w * ratio);
			break;
		}
		case Overlay::Highlight: {
			const float w = cx * 0.92f;
			place(item, (cx - w) * 0.5f, cy * 0.84f, w, w * ratio);
			break;
		}
		}
	} else {
		switch (kind) {
		case Overlay::Chat: {
			const float h = cy * 0.72f;
			const float w = h / ratio;
			place(item, cx - w - cx * 0.012f, cy - h - cy * 0.02f, w, h);
			break;
		}
		case Overlay::Alerts: {
			const float w = cx * 0.42f;
			place(item, (cx - w) * 0.5f, cy * 0.04f, w, w * ratio);
			break;
		}
		case Overlay::Viewers: {
			const float w = cx * 0.25f;
			place(item, cx * 0.012f, cy * 0.02f, w, w * ratio);
			break;
		}
		case Overlay::Highlight: {
			const float w = cx * 0.46f;
			place(item, (cx - w) * 0.5f, cy - w * ratio - cy * 0.04f, w, w * ratio);
			break;
		}
		}
	}
	return true;
}

} // namespace Hub
