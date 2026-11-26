/*
 * xImJxstinn Vertical – Docks: Vorschau, Szenen, Quellen
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#pragma once

#include <obs.h>

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QWidget>

#include <set>
#include <vector>

class XVPreview;

class XVMainDock : public QWidget {
public:
	explicit XVMainDock(QWidget *parent = nullptr);

private:
	void Refresh();
	void ShowOverlayMenu();
	XVPreview *preview;
	QPushButton *streamButton;
	QLabel *status;
};

class XVScenesDock : public QWidget {
public:
	explicit XVScenesDock(QWidget *parent = nullptr);

private:
	void Refresh();
	void AddScene();
	void RemoveScene();
	void RenameScene();
	void ShowLinkDialog();
	QListWidget *list;
	bool updating = false;
};

class XVSourcesDock : public QWidget {
public:
	explicit XVSourcesDock(QWidget *parent = nullptr);
	~XVSourcesDock() override;

private:
	void Refresh();
	void ConnectScene();
	void DisconnectScene();
	obs_sceneitem_t *SelectedItem(obs_scene_t *scene);
	void ShowAddMenu();
	void ShowFitMenu();
	void ShowContextMenu(const QPoint &pos);
	void ApplyFit(int mode);
	static void SceneSignal(void *data, calldata_t *cd);

	QListWidget *list;
	bool updating = false;
	obs_weak_source_t *connectedScene = nullptr;
	std::set<int64_t> collapsed;
	std::vector<int64_t> groupIds;
};
