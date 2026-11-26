/*
 * xImJxstinn Vertical – eigenes OBS-Plugin für den Hochformat-Stream (TikTok)
 * Copyright (C) 2026 xImJxstinn
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QMainWindow>

#include <string>

#include "chat-dock.hpp"
#include "docks.hpp"
#include "hub.hpp"
#include "updater.hpp"

#include <QTimer>
#include "vertical.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("xImJxstinn")

const char *obs_module_name(void)
{
	return "xImJxstinn Vertical";
}

const char *obs_module_description(void)
{
	return "Hochformat-Leinwand für TikTok mit Vorschau, Szenen, Quellen und eigenem Stream – von xImJxstinn";
}

static XVChatDock *chatDock = nullptr;

static void frontend_event(enum obs_frontend_event event, void *)
{
	Vertical &v = Vertical::Get();
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING: {
		v.AttachCanvas();
		Hub::StartWatcher();
		if (v.settings.hubAutostart) {
			std::string error;
			if (!Hub::Start(error))
				blog(LOG_INFO, "[xImJxstinn Vertical] Hub nicht gestartet: %s", error.c_str());
		}
		if (chatDock)
			chatDock->InitBrowser();
		if (v.settings.autoUpdate) {
			QTimer::singleShot(8000, [] {
				Updater::Check(static_cast<QWidget *>(obs_frontend_get_main_window()), false);
			});
		}
		break;
	}
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		v.AttachCanvas();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
		v.StopStream();
		v.DetachCanvas();
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		v.OnMainSceneChanged();
		break;
	case OBS_FRONTEND_EVENT_TRANSITION_CHANGED:
	case OBS_FRONTEND_EVENT_TRANSITION_LIST_CHANGED:
		if (v.settings.transitionMode == "obs")
			v.TransitionSettingsChanged();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		if (v.settings.startWithMain && !v.settings.key.empty()) {
			std::string error;
			if (!v.StartStream(error))
				blog(LOG_WARNING, "[xImJxstinn Vertical] %s", error.c_str());
		}
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		if (v.settings.startWithMain)
			v.StopStream();
		v.Notify();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		v.Notify();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		v.Shutdown();
		if (v.settings.hubAutostart)
			Hub::QuitIfStartedByUs();
		Hub::StopWatcher();
		Updater::OnExit();
		break;
	default:
		break;
	}
}

static void frontend_save(obs_data_t *data, bool saving, void *)
{
	if (saving)
		Vertical::Get().SaveCollection(data);
	else
		Vertical::Get().LoadCollection(data);
}

bool obs_module_load(void)
{
	Vertical::Get().LoadSettings();
	obs_frontend_add_event_callback(frontend_event, nullptr);
	obs_frontend_add_save_callback(frontend_save, nullptr);
	blog(LOG_INFO, "[xImJxstinn Vertical] geladen (Version %s)", Updater::CurrentVersion());
	return true;
}

void obs_module_post_load(void)
{
	auto mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	obs_frontend_add_dock_by_id("xImJxstinnVertical", "xImJxstinn Vertical", new XVMainDock(mainWindow));
	obs_frontend_add_dock_by_id("xImJxstinnVerticalScenes", "xImJxstinn Szenen", new XVScenesDock(mainWindow));
	obs_frontend_add_dock_by_id("xImJxstinnVerticalSources", "xImJxstinn Quellen", new XVSourcesDock(mainWindow));
	chatDock = new XVChatDock(mainWindow);
	obs_frontend_add_dock_by_id("xImJxstinnMultichat", "xImJxstinn Multichat", chatDock);
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	obs_frontend_remove_save_callback(frontend_save, nullptr);
	Hub::StopWatcher();
}
