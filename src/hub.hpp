/*
 * xImJxstinn Stream Tools – Anbindung an den MultiStream Hub
 * (Twitch- + TikTok-Chat, Alerts, Overlays, Zuschauerzahlen)
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#pragma once

#include <obs.h>

#include <QObject>
#include <QPointer>

#include <atomic>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#define XV_HUB_URL "http://127.0.0.1:8770"

namespace Hub {

/* Sucht den Hub-Ordner (mit app.py) an üblichen Orten */
std::string FindHubDir();
bool IsValidHubDir(const std::string &dir);

/* Startet den Hub im Hintergrund (ohne eigenes Fenster) */
bool Start(std::string &error);
/* Beendet den Hub, wenn das Plugin ihn gestartet hat */
void QuitIfStartedByUs();

/* Läuft der Hub gerade? (wird im Hintergrund alle 3 s geprüft) */
bool Online();
void StartWatcher();
void StopWatcher();
void AddListener(QObject *owner, std::function<void(bool online)> cb);

/* Overlays als Browserquellen einfügen */
enum class Overlay { Chat, Alerts, Viewers, Highlight };
bool AddOverlay(obs_scene_t *scene, Overlay kind, bool vertical, uint32_t cx, uint32_t cy);

} // namespace Hub
