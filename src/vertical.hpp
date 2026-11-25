/*
 * xImJxstinn Vertical – eigenes OBS-Plugin für den Hochformat-Stream (TikTok)
 * Copyright (C) 2026 xImJxstinn
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#pragma once

#include <obs.h>
#include <obs-frontend-api.h>

#include <QObject>
#include <QPointer>

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#define XV_CANVAS_NAME "xImJxstinn Vertical"
#define XV_LEGACY_CANVAS_NAME "Aitum Vertical"

struct XVSettings {
	std::string server;
	std::string key;
	int width = 1080;
	int height = 1920;
	int bitrate = 6000;
	std::string encoder = "auto";
	bool startWithMain = true;
	bool autoSwitch = true;
	std::string suffix = " TT";
	std::string hubDir;
	bool hubAutostart = true;
};

class Vertical {
public:
	static Vertical &Get();

	XVSettings settings;
	void LoadSettings();
	void SaveSettings();

	/* Leinwand */
	void AttachCanvas();
	void DetachCanvas();
	obs_canvas_t *CanvasRef(); /* Referenz oder nullptr – Aufrufer gibt frei */
	bool ApplyVideo(std::string &error);
	void CanvasSize(uint32_t &cx, uint32_t &cy);

	/* Szenen */
	std::vector<std::string> SceneNames();
	std::string CurrentSceneName();
	obs_source_t *CurrentSceneRef(); /* Referenz oder nullptr */
	void SetCurrentScene(const std::string &name);
	bool CreateScene(const std::string &name);
	bool RenameScene(const std::string &oldName, const std::string &newName);
	void RemoveScene(const std::string &name);
	void MoveScene(const std::string &name, int delta);
	void SetSceneOrder(const std::vector<std::string> &order);
	void OnMainSceneChanged();

	/* Auswahl im Vorschaubild */
	int64_t selectedItem = 0;

	/* TikTok-Stream */
	bool StartStream(std::string &error);
	void StopStream();
	bool IsStreaming();
	void Shutdown();

	/* Szenensammlung speichern/laden */
	void SaveCollection(obs_data_t *data);
	void LoadCollection(obs_data_t *data);

	/* UI-Benachrichtigung (läuft immer im UI-Thread) */
	void AddListener(QObject *owner, std::function<void()> cb);
	void Notify();

private:
	Vertical() = default;
	void MakeVideoInfo(obs_video_info &ovi);
	void ReleaseOutput();
	static void OutputStopped(void *data, calldata_t *cd);

	std::mutex mutex;
	obs_canvas_t *canvas = nullptr;
	std::string wantedScene;
	std::vector<std::string> sceneOrder;

	obs_output_t *output = nullptr;
	obs_encoder_t *videoEncoder = nullptr;
	obs_encoder_t *audioEncoder = nullptr;
	obs_service_t *service = nullptr;

	std::vector<std::pair<QPointer<QObject>, std::function<void()>>> listeners;
};
