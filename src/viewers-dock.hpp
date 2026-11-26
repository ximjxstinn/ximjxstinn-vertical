/*
 * xImJxstinn Stream Tools – Viewer-Counter-Dock (Twitch + TikTok + Gesamt)
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#pragma once

#include <QLabel>
#include <QTimer>
#include <QWidget>

#include <atomic>
#include <thread>

class XViewersDock : public QWidget {
public:
	explicit XViewersDock(QWidget *parent = nullptr);
	~XViewersDock() override;

private:
	void Tick();
	void Apply(const QByteArray &json);
	void ShowAddMenu();

	QLabel *twitchValue;
	QLabel *tiktokValue;
	QLabel *totalValue;
	QLabel *state;

	QTimer timer;
	std::thread worker;
	std::atomic<bool> busy{false};
};
