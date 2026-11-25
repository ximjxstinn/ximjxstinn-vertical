/*
 * xImJxstinn Vertical – Vorschaubild der Hochformat-Leinwand
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#pragma once

#include <obs.h>
#include <graphics/vec2.h>

#include <QWidget>

class XVPreview : public QWidget {
public:
	explicit XVPreview(QWidget *parent = nullptr);
	~XVPreview() override;

	QPaintEngine *paintEngine() const override { return nullptr; }

protected:
	void paintEvent(QPaintEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void showEvent(QShowEvent *event) override;
	bool event(QEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;

private:
	void CreateDisplay();
	void DestroyDisplay();
	bool ToCanvas(const QPointF &widgetPos, vec2 &out);
	static void Draw(void *data, uint32_t cx, uint32_t cy);

	obs_display_t *display = nullptr;
	bool dragging = false;
	vec2 dragStart = {};
	vec2 itemStart = {};
};
