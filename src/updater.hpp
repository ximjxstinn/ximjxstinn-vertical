/*
 * xImJxstinn Stream Tools – automatische Updates über GitHub-Releases
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#pragma once

#include <QWidget>

namespace Updater {

/* Im Hintergrund nach Updates suchen. manual = true zeigt auch „Kein Update“ an. */
void Check(QWidget *parent, bool manual);

/* Beim Schließen von OBS aufrufen – installiert ein vorgemerktes Update */
void OnExit();

const char *CurrentVersion();

} // namespace Updater
