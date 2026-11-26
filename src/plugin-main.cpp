/*
 * xImJxstinn Stream Tools – Viewer Counter + Multichat (Twitch & TikTok) in OBS
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
#include <QTimer>

#include <string>

#include "chat-dock.hpp"
#include "hub.hpp"
#include "settings.hpp"
#include "updater.hpp"
#include "viewers-dock.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("xImJxstinn")

const char *obs_module_name(void)
{
	return "xImJxstinn Stream Tools";
}

const char *obs_module_description(void)
{
	return "Viewer Counter und Multichat (Twitch + TikTok) direkt in OBS – von xImJxstinn";
}

static XVChatDock *chatDock = nullptr;

static void frontend_event(enum obs_frontend_event event, void *)
{
	XSettings &s = XSettings::Get();
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING: {
		Hub::StartWatcher();
		if (s.hubAutostart) {
			std::string error;
			if (!Hub::Start(error))
				blog(LOG_INFO, "[xImJxstinn Stream Tools] Hub nicht gestartet: %s", error.c_str());
		}
		if (chatDock)
			chatDock->InitBrowser();
		if (s.autoUpdate) {
			QTimer::singleShot(8000, [] {
				Updater::Check(static_cast<QWidget *>(obs_frontend_get_main_window()), false);
			});
		}
		break;
	}
	case OBS_FRONTEND_EVENT_EXIT:
		if (s.hubAutostart)
			Hub::QuitIfStartedByUs();
		Hub::StopWatcher();
		Updater::OnExit();
		break;
	default:
		break;
	}
}

bool obs_module_load(void)
{
	XSettings::Get().Load();
	obs_frontend_add_event_callback(frontend_event, nullptr);
	blog(LOG_INFO, "[xImJxstinn Stream Tools] geladen (Version %s)", Updater::CurrentVersion());
	return true;
}

void obs_module_post_load(void)
{
	auto mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	obs_frontend_add_dock_by_id("xImJxstinnViewers", "xImJxstinn Viewer Counter", new XViewersDock(mainWindow));
	chatDock = new XVChatDock(mainWindow);
	obs_frontend_add_dock_by_id("xImJxstinnMultichat", "xImJxstinn Multichat", chatDock);
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(frontend_event, nullptr);
	Hub::StopWatcher();
}
