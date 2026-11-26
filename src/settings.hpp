/*
 * xImJxstinn Stream Tools – Einstellungen
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#pragma once

#include <QWidget>

#include <string>

struct XSettings {
	std::string hubDir;
	bool hubAutostart = true;
	bool autoUpdate = true;

	static XSettings &Get();
	void Load();
	void Save();
};

namespace XSettingsDialog {
void Run(QWidget *parent);
}
