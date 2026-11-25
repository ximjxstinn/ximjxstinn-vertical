#pragma once

// xImJxstinn Vertical – Viewer Counter
// Zeigt Twitch-, TikTok- und Gesamt-Zuschauer im Vertical-Dock an.
// Die Zahlen kommen vom MultiStream Hub (http://127.0.0.1:8770/api/state).

#include <QLabel>
#include <QTimer>
#include <atomic>
#include <thread>

class ViewerCounter : public QLabel {
public:
	explicit ViewerCounter(QWidget *parent = nullptr);
	~ViewerCounter() override;

protected:
	void mousePressEvent(QMouseEvent *event) override;

private:
	void Tick();
	void Apply(const QByteArray &json);

	QTimer timer;
	std::thread worker;
	std::atomic<bool> busy{false};
};
