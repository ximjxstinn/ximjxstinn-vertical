/*
 * xImJxstinn Vertical – Multichat-Dock (Twitch + TikTok) aus dem MultiStream Hub
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#pragma once

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

class QCefWidget;

class XVChatDock : public QWidget {
public:
	explicit XVChatDock(QWidget *parent = nullptr);
	void InitBrowser();

private:
	void SetOnline(bool online);

	QVBoxLayout *layout;
	QWidget *offlineBox;
	QLabel *offlineText;
	QPushButton *startButton;
	QCefWidget *browser = nullptr;
	bool wasOnline = false;
};
