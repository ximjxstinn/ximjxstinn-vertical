/*
 * xImJxstinn Vertical – Docks: Vorschau, Szenen, Quellen
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#include "docks.hpp"
#include "hub.hpp"
#include "chat-dock.hpp"
#include "preview.hpp"
#include "settings-dialog.hpp"
#include "vertical.hpp"
#include "viewer-counter.hpp"

#include <obs-frontend-api.h>

#include <QCursor>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>

#include <map>
#include <QToolButton>
#include <QToolBar>
#include <QCheckBox>
#include <QIcon>
#include <QMetaObject>
#include <QAbstractItemModel>
#include <QMessageBox>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <string>
#include <vector>

/* Knöpfe im OBS-Stil: Symbole kommen aus dem aktiven OBS-Theme */
static QToolButton *tool_button(const char *cls, const char *tip, QWidget *parent)
{
	auto b = new QToolButton(parent);
	b->setProperty("class", cls);
	b->setProperty("themeID", cls);
	b->setToolTip(QString::fromUtf8(tip));
	b->setAutoRaise(true);
	b->setIconSize(QSize(16, 16));
	return b;
}

static QToolBar *make_toolbar(QWidget *parent)
{
	auto tb = new QToolBar(parent);
	tb->setIconSize(QSize(16, 16));
	tb->setMovable(false);
	tb->setFloatable(false);
	tb->setToolButtonStyle(Qt::ToolButtonIconOnly);
	return tb;
}

/* Quellen-Symbol genau wie in OBS (aus dem OBS-Hauptfenster bzw. Theme) */
static QIcon obs_source_icon(obs_source_t *source)
{
	auto main = static_cast<QWidget *>(obs_frontend_get_main_window());
	if (!main || !source)
		return QIcon();
	const char *prop = "defaultIcon";
	if (obs_source_is_group(source)) {
		prop = "groupIcon";
	} else if (obs_source_is_scene(source)) {
		prop = "sceneIcon";
	} else {
		switch (obs_source_get_icon_type(obs_source_get_id(source))) {
		case OBS_ICON_TYPE_IMAGE:
			prop = "imageIcon";
			break;
		case OBS_ICON_TYPE_COLOR:
			prop = "colorIcon";
			break;
		case OBS_ICON_TYPE_SLIDESHOW:
			prop = "slideshowIcon";
			break;
		case OBS_ICON_TYPE_AUDIO_INPUT:
			prop = "audioInputIcon";
			break;
		case OBS_ICON_TYPE_AUDIO_OUTPUT:
			prop = "audioOutputIcon";
			break;
		case OBS_ICON_TYPE_DESKTOP_CAPTURE:
			prop = "desktopCapIcon";
			break;
		case OBS_ICON_TYPE_WINDOW_CAPTURE:
			prop = "windowCapIcon";
			break;
		case OBS_ICON_TYPE_GAME_CAPTURE:
			prop = "gameCapIcon";
			break;
		case OBS_ICON_TYPE_CAMERA:
			prop = "cameraIcon";
			break;
		case OBS_ICON_TYPE_TEXT:
			prop = "textIcon";
			break;
		case OBS_ICON_TYPE_MEDIA:
			prop = "mediaIcon";
			break;
		case OBS_ICON_TYPE_BROWSER:
			prop = "browserIcon";
			break;
		case OBS_ICON_TYPE_PROCESS_AUDIO_OUTPUT:
			prop = "audioProcessOutputIcon";
			break;
		default:
			break;
		}
	}
	return main->property(prop).value<QIcon>();
}

/* ================================================================== */
/* Haupt-Dock: Vorschau + Steuerung                                    */
/* ================================================================== */

XVMainDock::XVMainDock(QWidget *parent) : QWidget(parent)
{
	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(4);

	preview = new XVPreview(this);
	layout->addWidget(preview, 1);

	/* Großer Knopf: Twitch + TikTok gleichzeitig */
	streamButton = new QPushButton(this);
	streamButton->setMinimumHeight(34);
	connect(streamButton, &QAbstractButton::clicked, this, [this] {
		Vertical &v = Vertical::Get();
		const bool twitchLive = obs_frontend_streaming_active();
		const bool tiktokLive = v.IsStreaming();
		if (twitchLive || tiktokLive) {
			if (QMessageBox::question(this, QStringLiteral("Streams beenden"),
						  QString::fromUtf8("Twitch und TikTok wirklich beenden?")) != QMessageBox::Yes)
				return;
			if (tiktokLive)
				v.StopStream();
			if (twitchLive)
				obs_frontend_streaming_stop();
			return;
		}
		std::string error;
		if (!v.StartStream(error)) {
			if (QMessageBox::question(this, QStringLiteral("TikTok"),
						  QString::fromStdString(error) +
							  QString::fromUtf8("\n\nNur Twitch starten?")) != QMessageBox::Yes)
				return;
		}
		obs_frontend_streaming_start();
	});
	layout->addWidget(streamButton);

	auto row = new QHBoxLayout();
	row->setContentsMargins(0, 0, 0, 0);
	row->setSpacing(2);

	tiktokButton = new QPushButton(this);
	tiktokButton->setToolTip(QString::fromUtf8("Nur TikTok starten/stoppen"));
	connect(tiktokButton, &QAbstractButton::clicked, this, [this] {
		Vertical &v = Vertical::Get();
		if (v.IsStreaming()) {
			v.StopStream();
		} else {
			std::string error;
			if (!v.StartStream(error))
				QMessageBox::warning(this, QStringLiteral("xImJxstinn Vertical"), QString::fromStdString(error));
		}
	});
	row->addWidget(tiktokButton);

	status = new QLabel(this);
	row->addWidget(status, 1);
	row->addWidget(new ViewerCounter(this));

	auto toolbar = make_toolbar(this);
	auto settingsButton = tool_button("icon-gear", "Einstellungen (TikTok-Key, Auflösung, Übergang …)", toolbar);
	connect(settingsButton, &QAbstractButton::clicked, this, [this] { XVSettingsDialog::Run(this); });
	auto moreButton = tool_button("icon-dots-vert", "Multichat, Overlays, MultiStream Hub", toolbar);
	connect(moreButton, &QAbstractButton::clicked, this, [this] { ShowOverlayMenu(); });
	toolbar->addWidget(settingsButton);
	toolbar->addWidget(moreButton);
	row->addWidget(toolbar);

	layout->addLayout(row);

	Vertical::Get().AddListener(this, [this] { Refresh(); });
	Refresh();
}

void XVMainDock::ShowOverlayMenu()
{
	struct Entry {
		const char *label;
		Hub::Overlay kind;
	};
	const Entry entries[] = {{"Chat (Twitch + TikTok)", Hub::Overlay::Chat},
				 {"Alerts (Follows, Subs, Gifts …)", Hub::Overlay::Alerts},
				 {"Viewer Counter", Hub::Overlay::Viewers},
				 {"Hervorgehobene Nachricht", Hub::Overlay::Highlight}};

	QMenu menu(this);
	QMenu *tiktok = menu.addMenu(QString::fromUtf8("In TikTok-Szene (Hochformat)"));
	QMenu *main = menu.addMenu(QString::fromUtf8("In aktuelle OBS-Szene (Twitch)"));
	for (const Entry &e : entries) {
		const Hub::Overlay kind = e.kind;
		tiktok->addAction(QString::fromUtf8(e.label), this, [this, kind] {
			obs_source_t *src = Vertical::Get().CurrentSceneRef();
			uint32_t cx = 0, cy = 0;
			Vertical::Get().CanvasSize(cx, cy);
			if (!Hub::AddOverlay(obs_scene_from_source(src), kind, true, cx, cy))
				QMessageBox::warning(this, QStringLiteral("Overlay"),
						     QStringLiteral("Overlay konnte nicht eingefügt werden."));
			obs_source_release(src);
			Vertical::Get().Notify();
		});
		main->addAction(QString::fromUtf8(e.label), this, [this, kind] {
			obs_source_t *src = obs_frontend_get_current_scene();
			obs_video_info ovi;
			obs_get_video_info(&ovi);
			if (!Hub::AddOverlay(obs_scene_from_source(src), kind, false, ovi.base_width, ovi.base_height))
				QMessageBox::warning(this, QStringLiteral("Overlay"),
						     QStringLiteral("Overlay konnte nicht eingefügt werden."));
			obs_source_release(src);
		});
	}
	menu.addSeparator();
	menu.addAction(QString::fromUtf8("\xF0\x9F\x92\xAC Multichat in OBS öffnen"), this, [] { XVShowChatDock(); });
	if (!Hub::Online()) {
		menu.addAction(QString::fromUtf8("\xE2\x96\xB6 MultiStream Hub starten"), this, [this] {
			std::string error;
			if (!Hub::Start(error))
				QMessageBox::warning(this, QStringLiteral("MultiStream Hub"), QString::fromStdString(error));
		});
	}
	menu.exec(QCursor::pos());
}

void XVMainDock::Refresh()
{
	Vertical &v = Vertical::Get();
	const bool tiktokLive = v.IsStreaming();
	const bool twitchLive = obs_frontend_streaming_active();
	const bool anyLive = tiktokLive || twitchLive;

	streamButton->setText(anyLive ? QString::fromUtf8("\xE2\x96\xA0  Twitch + TikTok beenden")
				      : QString::fromUtf8("\xE2\x96\xB6  Twitch + TikTok starten"));
	streamButton->setStyleSheet(anyLive ? QStringLiteral("QPushButton{background:#d1203a;color:white;font-weight:bold;}")
					    : QStringLiteral("QPushButton{font-weight:bold;}"));
	tiktokButton->setText(tiktokLive ? QString::fromUtf8("TikTok stoppen") : QString::fromUtf8("Nur TikTok"));

	auto dot = [](bool live, const char *name) {
		return live ? QString::fromUtf8("<span style='color:#ff4d5e'>\xE2\x97\x8F</span> <b>%1</b>").arg(QString::fromUtf8(name))
			    : QString::fromUtf8("<span style='color:#777'>\xE2\x97\x8F %1</span>").arg(QString::fromUtf8(name));
	};
	status->setText(dot(twitchLive, "Twitch") + QStringLiteral("&nbsp;&nbsp;") + dot(tiktokLive, "TikTok"));
	const std::string scene = v.CurrentSceneName();
	status->setToolTip(scene.empty() ? QString() : QString::fromUtf8("Aktuelle Hochformat-Szene: ") + QString::fromStdString(scene));
}

/* ================================================================== */
/* Szenen                                                              */
/* ================================================================== */

static std::string SceneNameOf(QListWidgetItem *item)
{
	if (!item)
		return std::string();
	const QVariant v = item->data(Qt::UserRole);
	if (v.isValid() && !v.toString().isEmpty())
		return v.toString().toStdString();
	return item->text().toStdString();
}

void XVScenesDock::ShowLinkDialog()
{
	Vertical &v = Vertical::Get();
	const std::vector<std::string> verticalNames = v.SceneNames();

	QDialog dlg(this);
	dlg.setWindowTitle(QString::fromUtf8("Szenen verbinden"));
	dlg.setMinimumWidth(480);
	auto layout = new QVBoxLayout(&dlg);
	auto info = new QLabel(QString::fromUtf8(
		"Wähle für jede OBS-Szene die passende Hochformat-Szene. Wechselst du in OBS die Szene, "
		"wechselt TikTok automatisch mit – mit demselben Übergang wie in OBS."));
	info->setWordWrap(true);
	layout->addWidget(info);

	auto form = new QFormLayout();
	struct Row {
		std::string uuid;
		std::string name;
		QComboBox *combo;
	};
	std::vector<Row> rows;

	struct obs_frontend_source_list mainScenes = {};
	obs_frontend_get_scenes(&mainScenes);
	for (size_t i = 0; i < mainScenes.sources.num; i++) {
		obs_source_t *m = mainScenes.sources.array[i];
		const char *mn = obs_source_get_name(m);
		const char *mu = obs_source_get_uuid(m);
		if (!mn || !mu)
			continue;
		auto combo = new QComboBox();
		combo->addItem(QString::fromUtf8("— nicht verbunden —"), QString());
		const std::string linked = v.LinkedVerticalFor(m);
		for (const auto &vn : verticalNames) {
			combo->addItem(QString::fromStdString(vn), QString::fromStdString(vn));
			if (vn == linked)
				combo->setCurrentIndex(combo->count() - 1);
		}
		form->addRow(QString::fromUtf8(mn), combo);
		rows.push_back({mu, mn, combo});
	}
	obs_frontend_source_list_free(&mainScenes);
	layout->addLayout(form);

	auto autoButton = new QPushButton(QString::fromUtf8("Automatisch nach Namen verbinden („Szene“ → „Szene TT“)"));
	QObject::connect(autoButton, &QAbstractButton::clicked, &dlg, [&rows, &v] {
		for (auto &r : rows) {
			for (const std::string &cand : {r.name + v.settings.suffix, r.name}) {
				const int idx = r.combo->findData(QString::fromStdString(cand));
				if (idx > 0) {
					r.combo->setCurrentIndex(idx);
					break;
				}
			}
		}
	});
	layout->addWidget(autoButton);

	auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	layout->addWidget(buttons);

	if (dlg.exec() != QDialog::Accepted)
		return;

	std::map<std::string, std::string> newLinks;
	obs_canvas_t *c = v.CanvasRef();
	for (const auto &r : rows) {
		const QString vn = r.combo->currentData().toString();
		if (vn.isEmpty() || !c)
			continue;
		obs_source_t *vs = obs_canvas_get_source_by_name(c, vn.toUtf8().constData());
		if (vs) {
			const char *vu = obs_source_get_uuid(vs);
			if (vu)
				newLinks[r.uuid] = vu;
			obs_source_release(vs);
		}
	}
	if (c)
		obs_canvas_release(c);
	v.settings.autoSwitch = true;
	v.SaveSettings();
	v.SetLinks(newLinks);
	v.OnMainSceneChanged();
}

XVScenesDock::XVScenesDock(QWidget *parent) : QWidget(parent)
{
	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	list = new QListWidget(this);
	list->setSelectionMode(QAbstractItemView::SingleSelection);
	list->setDragDropMode(QAbstractItemView::InternalMove);
	list->setDefaultDropAction(Qt::MoveAction);
	layout->addWidget(list, 1);

	connect(list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *cur, QListWidgetItem *) {
		if (updating || !cur)
			return;
		Vertical::Get().SetCurrentScene(SceneNameOf(cur));
	});
	/* Nach dem Ziehen die neue Reihenfolge fest speichern */
	connect(list->model(), &QAbstractItemModel::rowsMoved, this, [this] {
		if (updating)
			return;
		std::vector<std::string> order;
		for (int i = 0; i < list->count(); i++)
			order.push_back(SceneNameOf(list->item(i)));
		Vertical::Get().SetSceneOrder(order);
	});
	connect(list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) { RenameScene(); });

	auto toolbar = make_toolbar(this);
	auto add = tool_button("icon-plus", "Neue Hochformat-Szene", toolbar);
	auto remove = tool_button("icon-trash", "Szene löschen", toolbar);
	auto more = tool_button("icon-dots-vert", "Umbenennen, OBS-Szenen verbinden", toolbar);
	auto sceneUp = tool_button("icon-up", "Szene nach oben", toolbar);
	auto sceneDown = tool_button("icon-down", "Szene nach unten", toolbar);
	auto moveScene = [this](int delta) {
		QListWidgetItem *cur = list->currentItem();
		if (cur)
			Vertical::Get().MoveScene(SceneNameOf(cur), delta);
	};
	connect(sceneUp, &QAbstractButton::clicked, this, [moveScene] { moveScene(-1); });
	connect(sceneDown, &QAbstractButton::clicked, this, [moveScene] { moveScene(1); });
	connect(add, &QAbstractButton::clicked, this, [this] { AddScene(); });
	connect(remove, &QAbstractButton::clicked, this, [this] { RemoveScene(); });
	connect(more, &QAbstractButton::clicked, this, [this] {
		QMenu menu(this);
		menu.addAction(QString::fromUtf8("Umbenennen"), this, [this] { RenameScene(); });
		menu.addAction(QString::fromUtf8("Mit OBS-Szenen verbinden …"), this, [this] { ShowLinkDialog(); });
		menu.exec(QCursor::pos());
	});
	toolbar->addWidget(add);
	toolbar->addWidget(remove);
	toolbar->addSeparator();
	toolbar->addWidget(more);
	toolbar->addSeparator();
	toolbar->addWidget(sceneUp);
	toolbar->addWidget(sceneDown);
	layout->addWidget(toolbar);

	list->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(list, &QListWidget::customContextMenuRequested, this, [this](const QPoint &) {
		QMenu menu(this);
		menu.addAction(QString::fromUtf8("Neue Szene"), this, [this] { AddScene(); });
		menu.addAction(QString::fromUtf8("Umbenennen"), this, [this] { RenameScene(); });
		menu.addAction(QString::fromUtf8("Löschen"), this, [this] { RemoveScene(); });
		menu.addSeparator();
		menu.addAction(QString::fromUtf8("Mit OBS-Szenen verbinden …"), this, [this] { ShowLinkDialog(); });
		menu.exec(QCursor::pos());
	});

	Vertical::Get().AddListener(this, [this] { Refresh(); });
	Refresh();
}

void XVScenesDock::Refresh()
{
	updating = true;
	Vertical &v = Vertical::Get();
	const std::vector<std::string> names = v.SceneNames();
	const std::string current = v.CurrentSceneName();
	list->clear();
	for (const auto &n : names) {
		auto item = new QListWidgetItem(QString::fromStdString(n), list);
		const std::vector<std::string> linked = v.LinkedMainScenes(n);
		if (!linked.empty()) {
			QStringList l;
			for (const auto &m : linked)
				l << QString::fromStdString(m);
			item->setToolTip(QString::fromUtf8("Verbunden mit OBS-Szene: ") + l.join(QStringLiteral(", ")));
			item->setData(Qt::UserRole, QString::fromStdString(n));
		}
		if (n == current) {
			QFont f = item->font();
			f.setBold(true);
			item->setFont(f);
			list->setCurrentItem(item);
		}
	}
	updating = false;
}

void XVScenesDock::AddScene()
{
	bool ok = false;
	const QString name = QInputDialog::getText(this, QStringLiteral("Neue Szene"), QStringLiteral("Name der Szene:"),
						   QLineEdit::Normal, QStringLiteral("Neue Szene TT"), &ok)
				     .trimmed();
	if (!ok || name.isEmpty())
		return;
	Vertical &v = Vertical::Get();
	if (!v.CreateScene(name.toStdString())) {
		QMessageBox::warning(this, QStringLiteral("Neue Szene"),
				     QStringLiteral("Eine Szene mit diesem Namen gibt es schon."));
		return;
	}
	v.SetCurrentScene(name.toStdString());
}

void XVScenesDock::RemoveScene()
{
	QListWidgetItem *item = list->currentItem();
	if (!item)
		return;
	if (list->count() <= 1) {
		QMessageBox::information(this, QStringLiteral("Szene löschen"),
					 QStringLiteral("Die letzte Szene kann nicht gelöscht werden."));
		return;
	}
	const QString name = QString::fromStdString(SceneNameOf(item));
	if (QMessageBox::question(this, QStringLiteral("Szene löschen"),
				  QStringLiteral("Szene „%1“ wirklich löschen?").arg(name)) != QMessageBox::Yes)
		return;
	Vertical::Get().RemoveScene(name.toStdString());
}

void XVScenesDock::RenameScene()
{
	QListWidgetItem *item = list->currentItem();
	if (!item)
		return;
	const QString oldName = QString::fromStdString(SceneNameOf(item));
	bool ok = false;
	const QString name = QInputDialog::getText(this, QStringLiteral("Szene umbenennen"), QStringLiteral("Neuer Name:"),
						   QLineEdit::Normal, oldName, &ok)
				     .trimmed();
	if (!ok || name.isEmpty() || name == oldName)
		return;
	if (!Vertical::Get().RenameScene(oldName.toStdString(), name.toStdString()))
		QMessageBox::warning(this, QStringLiteral("Szene umbenennen"),
				     QStringLiteral("Der Name ist schon vergeben."));
}

/* ================================================================== */
/* Quellen                                                             */
/* ================================================================== */

XVSourcesDock::XVSourcesDock(QWidget *parent) : QWidget(parent)
{
	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	list = new QListWidget(this);
	list->setSelectionMode(QAbstractItemView::SingleSelection);
	list->setContextMenuPolicy(Qt::CustomContextMenu);
	layout->addWidget(list, 1);

	connect(list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *cur, QListWidgetItem *) {
		if (updating || !cur)
			return;
		Vertical &v = Vertical::Get();
		v.selectedItem = cur->data(Qt::UserRole).toLongLong();
	});
	connect(list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
		obs_source_t *src = Vertical::Get().CurrentSceneRef();
		obs_sceneitem_t *item = SelectedItem(obs_scene_from_source(src));
		if (item)
			obs_frontend_open_source_properties(obs_sceneitem_get_source(item));
		obs_source_release(src);
	});
	connect(list, &QListWidget::customContextMenuRequested, this,
		[this](const QPoint &pos) { ShowContextMenu(pos); });

	auto toolbar = make_toolbar(this);
	auto add = tool_button("icon-plus", "Quelle hinzufügen", toolbar);
	auto remove = tool_button("icon-trash", "Quelle entfernen", toolbar);
	auto props = tool_button("icon-gear", "Eigenschaften", toolbar);
	auto fit = tool_button("icon-dots-vert", "Anpassen (Vollbild, Hälften …) und Filter", toolbar);
	auto up = tool_button("icon-up", "Nach oben", toolbar);
	auto down = tool_button("icon-down", "Nach unten", toolbar);

	connect(add, &QAbstractButton::clicked, this, [this] { ShowAddMenu(); });
	connect(remove, &QAbstractButton::clicked, this, [this] {
		obs_source_t *src = Vertical::Get().CurrentSceneRef();
		if (obs_sceneitem_t *item = SelectedItem(obs_scene_from_source(src))) {
			const char *name = obs_source_get_name(obs_sceneitem_get_source(item));
			if (QMessageBox::question(this, QStringLiteral("Quelle entfernen"),
						  QStringLiteral("„%1“ aus dieser Szene entfernen?")
							  .arg(QString::fromUtf8(name ? name : ""))) == QMessageBox::Yes) {
				obs_sceneitem_remove(item);
				Vertical::Get().selectedItem = 0;
			}
		}
		obs_source_release(src);
		Vertical::Get().Notify();
	});
	auto move = [this](obs_order_movement dir) {
		obs_source_t *src = Vertical::Get().CurrentSceneRef();
		if (obs_sceneitem_t *item = SelectedItem(obs_scene_from_source(src)))
			obs_sceneitem_set_order(item, dir);
		obs_source_release(src);
	};
	connect(up, &QAbstractButton::clicked, this, [move] { move(OBS_ORDER_MOVE_UP); });
	connect(down, &QAbstractButton::clicked, this, [move] { move(OBS_ORDER_MOVE_DOWN); });
	connect(props, &QAbstractButton::clicked, this, [this] {
		obs_source_t *src = Vertical::Get().CurrentSceneRef();
		if (obs_sceneitem_t *item = SelectedItem(obs_scene_from_source(src)))
			obs_frontend_open_source_properties(obs_sceneitem_get_source(item));
		obs_source_release(src);
	});
	connect(fit, &QAbstractButton::clicked, this, [this] { ShowFitMenu(); });

	toolbar->addWidget(add);
	toolbar->addWidget(remove);
	toolbar->addWidget(props);
	toolbar->addSeparator();
	toolbar->addWidget(fit);
	toolbar->addSeparator();
	toolbar->addWidget(up);
	toolbar->addWidget(down);
	layout->addWidget(toolbar);

	Vertical::Get().AddListener(this, [this] { Refresh(); });
	Refresh();
}

XVSourcesDock::~XVSourcesDock()
{
	DisconnectScene();
}

obs_sceneitem_t *XVSourcesDock::SelectedItem(obs_scene_t *scene)
{
	return XVFindItem(scene, Vertical::Get().selectedItem);
}

void XVSourcesDock::SceneSignal(void *, calldata_t *)
{
	Vertical::Get().Notify();
}

static const char *scene_signals[] = {"item_add", "item_remove", "reorder", "item_visible", "item_locked", "refresh"};

void XVSourcesDock::DisconnectScene()
{
	if (!connectedScene)
		return;
	obs_source_t *old = obs_weak_source_get_source(connectedScene);
	if (old) {
		signal_handler_t *sh = obs_source_get_signal_handler(old);
		for (const char *sig : scene_signals)
			signal_handler_disconnect(sh, sig, SceneSignal, this);
		obs_source_release(old);
	}
	obs_weak_source_release(connectedScene);
	connectedScene = nullptr;
}

void XVSourcesDock::ConnectScene()
{
	obs_source_t *current = Vertical::Get().CurrentSceneRef();
	if (connectedScene && current && obs_weak_source_references_source(connectedScene, current)) {
		obs_source_release(current);
		return;
	}
	DisconnectScene();
	if (current) {
		signal_handler_t *sh = obs_source_get_signal_handler(current);
		for (const char *sig : scene_signals)
			signal_handler_connect(sh, sig, SceneSignal, this);
		connectedScene = obs_source_get_weak_source(current);
		obs_source_release(current);
	}
}

struct ItemRow {
	int64_t id;
	std::string name;
	bool visible;
	bool locked;
	bool isGroup;
	int depth;
	QIcon icon;
};

static bool collect_items(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	auto out = static_cast<std::vector<ItemRow> *>(param);
	const char *n = obs_source_get_name(obs_sceneitem_get_source(item));
	out->push_back({obs_sceneitem_get_id(item), n ? n : "", obs_sceneitem_visible(item), obs_sceneitem_locked(item),
			obs_sceneitem_is_group(item), 0, obs_source_icon(obs_sceneitem_get_source(item))});
	return true;
}

void XVSourcesDock::Refresh()
{
	ConnectScene();
	updating = true;
	list->clear();

	obs_source_t *src = Vertical::Get().CurrentSceneRef();
	obs_scene_t *scene = obs_scene_from_source(src);
	std::vector<ItemRow> top;
	if (scene)
		obs_scene_enum_items(scene, collect_items, &top);

	/* Oben in der Liste = vorne im Bild; Gruppeninhalte eingerückt darunter */
	std::vector<ItemRow> rows;
	for (auto it = top.rbegin(); it != top.rend(); ++it) {
		rows.push_back(*it);
		if (!it->isGroup || collapsed.count(it->id))
			continue;
		obs_sceneitem_t *group = scene ? obs_scene_find_sceneitem_by_id(scene, it->id) : nullptr;
		if (!group)
			continue;
		std::vector<ItemRow> children;
		obs_sceneitem_group_enum_items(group, collect_items, &children);
		for (auto c = children.rbegin(); c != children.rend(); ++c) {
			ItemRow r = *c;
			r.depth = 1;
			rows.push_back(r);
		}
	}
	obs_source_release(src);

	const int64_t selected = Vertical::Get().selectedItem;
	for (const ItemRow &r : rows) {
		const int64_t id = r.id;
		auto item = new QListWidgetItem(list);
		item->setData(Qt::UserRole, QVariant::fromValue<qlonglong>(id));

		auto rowWidget = new QWidget(list);
		auto rowLayout = new QHBoxLayout(rowWidget);
		rowLayout->setContentsMargins(4 + r.depth * 22, 0, 4, 0);
		rowLayout->setSpacing(6);

		if (r.isGroup) {
			auto expand = new QCheckBox(rowWidget);
			expand->setProperty("class", "indicator-expand");
			expand->setChecked(collapsed.count(id) == 0);
			expand->setToolTip(QString::fromUtf8("Gruppe auf-/zuklappen"));
			connect(expand, &QCheckBox::toggled, this, [this, id](bool open) {
				if (open)
					collapsed.erase(id);
				else
					collapsed.insert(id);
				QMetaObject::invokeMethod(this, [this] { Refresh(); }, Qt::QueuedConnection);
			});
			rowLayout->addWidget(expand);
		}

		auto iconLabel = new QLabel(rowWidget);
		iconLabel->setPixmap(r.icon.pixmap(16, 16));
		iconLabel->setFixedSize(16, 16);
		rowLayout->addWidget(iconLabel);

		auto label = new QLabel(QString::fromStdString(r.name), rowWidget);
		label->setEnabled(r.visible);
		rowLayout->addWidget(label, 1);

		auto vis = new QCheckBox(rowWidget);
		vis->setProperty("class", "indicator-visibility");
		vis->setChecked(r.visible);
		vis->setToolTip(QString::fromUtf8("Sichtbar ein/aus"));
		connect(vis, &QCheckBox::toggled, this, [id](bool on) {
			obs_source_t *s = Vertical::Get().CurrentSceneRef();
			if (obs_sceneitem_t *si = XVFindItem(obs_scene_from_source(s), id))
				obs_sceneitem_set_visible(si, on);
			obs_source_release(s);
			Vertical::Get().Notify();
		});
		rowLayout->addWidget(vis);

		auto lock = new QCheckBox(rowWidget);
		lock->setProperty("class", "indicator-lock");
		lock->setChecked(r.locked);
		lock->setToolTip(QString::fromUtf8("Sperren (in der Vorschau nicht verschiebbar)"));
		connect(lock, &QCheckBox::toggled, this, [id](bool on) {
			obs_source_t *s = Vertical::Get().CurrentSceneRef();
			if (obs_sceneitem_t *si = XVFindItem(obs_scene_from_source(s), id))
				obs_sceneitem_set_locked(si, on);
			obs_source_release(s);
			Vertical::Get().Notify();
		});
		rowLayout->addWidget(lock);

		item->setSizeHint(rowWidget->sizeHint());
		list->setItemWidget(item, rowWidget);
		if (id == selected)
			list->setCurrentItem(item);
	}
	updating = false;
}

static std::string unique_source_name(const std::string &base)
{
	std::string name = base;
	for (int i = 2; i < 1000; i++) {
		obs_source_t *existing = obs_get_source_by_name(name.c_str());
		if (!existing)
			return name;
		obs_source_release(existing);
		name = base + " " + std::to_string(i);
	}
	return name;
}

void XVSourcesDock::ShowAddMenu()
{
	obs_source_t *sceneSrc = Vertical::Get().CurrentSceneRef();
	obs_scene_t *scene = obs_scene_from_source(sceneSrc);
	if (!scene) {
		obs_source_release(sceneSrc);
		return;
	}

	QMenu menu(this);
	auto addSource = [scene](obs_source_t *source) {
		obs_sceneitem_t *item = obs_scene_add(scene, source);
		if (item)
			Vertical::Get().selectedItem = obs_sceneitem_get_id(item);
	};

	/* Neue Quelle erstellen */
	QMenu *create = menu.addMenu(QString::fromUtf8("Neue Quelle erstellen"));
	std::vector<std::pair<QString, std::string>> types;
	const char *id = nullptr;
	for (size_t i = 0; obs_enum_input_types(i, &id); i++) {
		const uint32_t flags = obs_get_source_output_flags(id);
		if (flags & (OBS_SOURCE_CAP_DISABLED | OBS_SOURCE_DEPRECATED))
			continue;
		const char *display = obs_source_get_display_name(id);
		if (!display)
			continue;
		types.emplace_back(QString::fromUtf8(display), id);
	}
	std::sort(types.begin(), types.end(), [](const auto &a, const auto &b) {
		return a.first.localeAwareCompare(b.first) < 0;
	});
	for (const auto &t : types) {
		const std::string typeId = t.second;
		const QString display = t.first;
		create->addAction(display, this, [this, typeId, display, addSource] {
			bool ok = false;
			const QString name = QInputDialog::getText(this, QStringLiteral("Neue Quelle"),
								   QStringLiteral("Name der Quelle:"),
								   QLineEdit::Normal, display + QStringLiteral(" TT"), &ok)
						     .trimmed();
			if (!ok || name.isEmpty())
				return;
			const std::string unique = unique_source_name(name.toStdString());
			obs_source_t *source = obs_source_create(typeId.c_str(), unique.c_str(), nullptr, nullptr);
			if (!source)
				return;
			addSource(source);
			obs_frontend_open_source_properties(source);
			obs_source_release(source);
			Vertical::Get().Notify();
		});
	}

	/* Vorhandene Quelle aus OBS */
	QMenu *existing = menu.addMenu(QString::fromUtf8("Vorhandene Quelle (aus OBS)"));
	std::vector<std::string> names;
	obs_enum_sources(
		[](void *param, obs_source_t *source) {
			const char *n = obs_source_get_name(source);
			if (n && obs_source_get_type(source) == OBS_SOURCE_TYPE_INPUT)
				static_cast<std::vector<std::string> *>(param)->emplace_back(n);
			return true;
		},
		&names);
	std::sort(names.begin(), names.end());
	for (const auto &n : names) {
		existing->addAction(QString::fromStdString(n), this, [this, n, addSource] {
			obs_source_t *source = obs_get_source_by_name(n.c_str());
			if (!source)
				return;
			addSource(source);
			obs_source_release(source);
			ApplyFit(0);
		});
	}
	if (names.empty())
		existing->setEnabled(false);

	/* Ganze OBS-Szene als Quelle */
	QMenu *scenes = menu.addMenu(QString::fromUtf8("Ganze OBS-Szene einfügen"));
	struct obs_frontend_source_list mainScenes = {};
	obs_frontend_get_scenes(&mainScenes);
	for (size_t i = 0; i < mainScenes.sources.num; i++) {
		const char *n = obs_source_get_name(mainScenes.sources.array[i]);
		if (!n)
			continue;
		const std::string sceneName = n;
		scenes->addAction(QString::fromStdString(sceneName), this, [this, sceneName, addSource] {
			obs_source_t *source = obs_get_source_by_name(sceneName.c_str());
			if (!source)
				return;
			addSource(source);
			obs_source_release(source);
			ApplyFit(1);
		});
	}
	obs_frontend_source_list_free(&mainScenes);

	menu.addSeparator();
	menu.addAction(QString::fromUtf8("Neue Gruppe"), this, [this, scene] {
		bool ok = false;
		const QString name = QInputDialog::getText(this, QStringLiteral("Neue Gruppe"), QStringLiteral("Name der Gruppe:"),
							   QLineEdit::Normal, QStringLiteral("Gruppe TT"), &ok)
					     .trimmed();
		if (!ok || name.isEmpty())
			return;
		const std::string unique = unique_source_name(name.toStdString());
		if (obs_sceneitem_t *g = obs_scene_add_group2(scene, unique.c_str(), true))
			Vertical::Get().selectedItem = obs_sceneitem_get_id(g);
	});

	menu.exec(QCursor::pos());
	obs_source_release(sceneSrc);
	Vertical::Get().Notify();
}

void XVSourcesDock::ShowFitMenu()
{
	QMenu menu(this);
	const char *labels[] = {"Ganzes Bild einpassen", "Bild füllen (Ränder abschneiden)", "Obere Hälfte",
				"Untere Hälfte", "Oberes Drittel (z. B. Facecam)", "Untere zwei Drittel (z. B. Gameplay)",
				"Zurücksetzen"};
	QMenu *fitMenu = menu.addMenu(QString::fromUtf8("Anpassen"));
	for (int i = 0; i < 7; i++)
		fitMenu->addAction(QString::fromUtf8(labels[i]), this, [this, i] { ApplyFit(i); });
	menu.addAction(QString::fromUtf8("Filter"), this, [this] {
		obs_source_t *src = Vertical::Get().CurrentSceneRef();
		if (obs_sceneitem_t *item = SelectedItem(obs_scene_from_source(src)))
			obs_frontend_open_source_filters(obs_sceneitem_get_source(item));
		obs_source_release(src);
	});
	menu.exec(QCursor::pos());
}

void XVSourcesDock::ApplyFit(int mode)
{
	obs_source_t *src = Vertical::Get().CurrentSceneRef();
	obs_sceneitem_t *item = SelectedItem(obs_scene_from_source(src));
	if (!item) {
		obs_source_release(src);
		return;
	}
	uint32_t cxu = 0, cyu = 0;
	Vertical::Get().CanvasSize(cxu, cyu);
	const float cx = (float)cxu;
	const float cy = (float)cyu;

	obs_transform_info info;
	obs_sceneitem_get_info2(item, &info);
	info.rot = 0.0f;
	info.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
	info.bounds_alignment = OBS_ALIGN_CENTER;
	info.scale.x = 1.0f;
	info.scale.y = 1.0f;
	info.crop_to_bounds = true;
	info.bounds_type = OBS_BOUNDS_SCALE_OUTER;
	vec2_set(&info.pos, 0.0f, 0.0f);

	switch (mode) {
	case 0:
		info.bounds_type = OBS_BOUNDS_SCALE_INNER;
		info.crop_to_bounds = false;
		vec2_set(&info.bounds, cx, cy);
		break;
	case 1:
		vec2_set(&info.bounds, cx, cy);
		break;
	case 2:
		vec2_set(&info.bounds, cx, cy * 0.5f);
		break;
	case 3:
		vec2_set(&info.pos, 0.0f, cy * 0.5f);
		vec2_set(&info.bounds, cx, cy * 0.5f);
		break;
	case 4:
		vec2_set(&info.bounds, cx, std::round(cy / 3.0f));
		break;
	case 5:
		vec2_set(&info.pos, 0.0f, std::round(cy / 3.0f));
		vec2_set(&info.bounds, cx, cy - std::round(cy / 3.0f));
		break;
	default:
		info.bounds_type = OBS_BOUNDS_NONE;
		info.crop_to_bounds = false;
		vec2_set(&info.bounds, 0.0f, 0.0f);
		break;
	}
	obs_sceneitem_set_info2(item, &info);
	if (mode == 6) {
		obs_sceneitem_crop crop = {};
		obs_sceneitem_set_crop(item, &crop);
	}
	obs_source_release(src);
}

void XVSourcesDock::ShowContextMenu(const QPoint &pos)
{
	QListWidgetItem *row = list->itemAt(pos);
	if (!row)
		return;
	Vertical::Get().selectedItem = row->data(Qt::UserRole).toLongLong();

	obs_source_t *src = Vertical::Get().CurrentSceneRef();
	obs_sceneitem_t *item = SelectedItem(obs_scene_from_source(src));
	if (!item) {
		obs_source_release(src);
		return;
	}
	obs_source_t *source = obs_sceneitem_get_source(item);
	const bool locked = obs_sceneitem_locked(item);

	QMenu menu(this);
	QAction *props = menu.addAction(QString::fromUtf8("Eigenschaften"));
	QAction *filters = menu.addAction(QString::fromUtf8("Filter"));
	QAction *rename = menu.addAction(QString::fromUtf8("Umbenennen"));
	QAction *lock = menu.addAction(locked ? QString::fromUtf8("Entsperren") : QString::fromUtf8("Sperren"));
	QMenu *fitMenu = menu.addMenu(QString::fromUtf8("Anpassen"));
	const char *labels[] = {"Ganzes Bild einpassen", "Bild füllen", "Obere Hälfte", "Untere Hälfte",
				"Oberes Drittel", "Untere zwei Drittel", "Zurücksetzen"};
	for (int i = 0; i < 7; i++)
		fitMenu->addAction(QString::fromUtf8(labels[i]), this, [this, i] { ApplyFit(i); });
	obs_scene_t *ctxScene = obs_scene_from_source(src);
	obs_sceneitem_t *parentGroup = obs_sceneitem_get_group(ctxScene, item);
	QAction *ungroupItem = nullptr;
	if (parentGroup)
		ungroupItem = menu.addAction(QString::fromUtf8("Aus Gruppe nehmen"));
	std::vector<std::pair<QAction *, int64_t>> moveTargets;
	if (!obs_sceneitem_is_group(item)) {
		QMenu *toGroup = menu.addMenu(QString::fromUtf8("In Gruppe verschieben"));
		obs_scene_enum_items(
			ctxScene,
			[](obs_scene_t *, obs_sceneitem_t *si, void *param) {
				if (obs_sceneitem_is_group(si))
					static_cast<std::vector<int64_t> *>(param)->push_back(obs_sceneitem_get_id(si));
				return true;
			},
			&groupIds);
		for (int64_t gid : groupIds) {
			obs_sceneitem_t *g = obs_scene_find_sceneitem_by_id(ctxScene, gid);
			if (!g || g == parentGroup)
				continue;
			const char *gn = obs_source_get_name(obs_sceneitem_get_source(g));
			moveTargets.emplace_back(toGroup->addAction(QString::fromUtf8(gn ? gn : "Gruppe")), gid);
		}
		groupIds.clear();
		toGroup->setEnabled(!moveTargets.empty());
	}
	menu.addSeparator();
	QAction *removeAction = menu.addAction(QString::fromUtf8("Entfernen"));

	QAction *chosen = menu.exec(list->viewport()->mapToGlobal(pos));
	if (chosen == props) {
		obs_frontend_open_source_properties(source);
	} else if (chosen == filters) {
		obs_frontend_open_source_filters(source);
	} else if (chosen == rename) {
		bool ok = false;
		const char *old = obs_source_get_name(source);
		const QString name = QInputDialog::getText(this, QStringLiteral("Umbenennen"), QStringLiteral("Neuer Name:"),
							   QLineEdit::Normal, QString::fromUtf8(old ? old : ""), &ok)
					     .trimmed();
		if (ok && !name.isEmpty() && name != QString::fromUtf8(old ? old : ""))
			obs_source_set_name(source, unique_source_name(name.toStdString()).c_str());
	} else if (chosen == lock) {
		obs_sceneitem_set_locked(item, !locked);
	} else if (chosen == removeAction) {
		obs_sceneitem_remove(item);
		Vertical::Get().selectedItem = 0;
	} else if (chosen && chosen == ungroupItem) {
		obs_sceneitem_group_remove_item(parentGroup, item);
	} else if (chosen) {
		for (const auto &t : moveTargets) {
			if (t.first == chosen) {
				if (obs_sceneitem_t *g = obs_scene_find_sceneitem_by_id(ctxScene, t.second))
					obs_sceneitem_group_add_item(g, item);
				break;
			}
		}
	}
	obs_source_release(src);
	Vertical::Get().Notify();
}
