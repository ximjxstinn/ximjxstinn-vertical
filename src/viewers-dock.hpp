/*
 * xImJxstinn Stream Tools – Viewer-Counter-Dock (Twitch + TikTok + Gesamt)
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#pragma once

#include <QFrame>
#include <QLabel>
#include <QTimer>
#include <QWidget>

#include <atomic>
#include <thread>

struct XViewerCard {
	QFrame *frame = nullptr;
	QLabel *value = nullptr;
	QLabel *badge = nullptr;
	QLabel *badgeIcon = nullptr;
};

class XViewersDock : public QWidget {
public:
	explicit XViewersDock(QWidget *parent = nullptr);
	~XViewersDock() override;

private:
	void Tick();
	void Apply(const QByteArray &json);
	void ShowAddMenu();
	void SetLive(XViewerCard &card, bool live, const QString &offlineText);

	QLabel *totalValue;
	QLabel *trend;
	QLabel *trendIcon;
	QLabel *peak;
	QLabel *peakIcon;
	XViewerCard twitch;
	XViewerCard tiktok;
	QLabel *state;

	qlonglong lastTotal = -1;
	qlonglong peakTotal = 0;

	QTimer timer;
	std::thread worker;
	std::atomic<bool> busy{false};
};
