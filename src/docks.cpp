/*
 * xImJxstinn Vertical – Docks: Vorschau, Szenen, Quellen
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#include "docks.hpp"
#include "hub.hpp"
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
#include <QToolButton>
#include <QAbstractItemModel>
#include <QMessageBox>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <string>
#include <vector>

static QPushButton *tool_button(const char *text, const char *tip, QWidget *parent)
{
	auto b = new QPushButton(QString::fromUtf8(text), parent);
	b->setToolTip(QString::fromUtf8(tip));
	b->setMinimumWidth(30);
	b->setMaximumWidth(44);
	return b;
}

/* ================================================================== */
/* Haupt-Dock: Vorschau + Steuerung                                    */
/* ================================================================== */

XVMainDock::XVMainDock(QWidget *parent) : QWidget(parent)
{
	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->setSpacing(4);

	preview = new XVPreview(this);
	layout->addWidget(preview, 1);

	auto row = new QHBoxLayout();
	row->setSpacing(4);

	streamButton = new QPushButton(this);
	streamButton->setMinimumHeight(30);
	connect(streamButton, &QPushButton::clicked, this, [this] {
		Vertical &v = Vertical::Get();
		if (v.IsStreaming()) {
			v.StopStream();
		} else {
			std::string error;
			if (!v.StartStream(error))
				QMessageBox::warning(this, QStringLiteral("xImJxstinn Vertical"), QString::fromStdString(error));
		}
	});
	row->addWidget(streamButton, 1);

	auto settingsButton = tool_button("\xE2\x9A\x99", "Einstellungen (TikTok-Key, Auflösung, Bitrate …)", this);
	connect(settingsButton, &QPushButton::clicked, this, [this] { XVSettingsDialog::Run(this); });
	row->addWidget(settingsButton);

	auto chatButton = tool_button("\xF0\x9F\x92\xAC", "MultiStream Hub öffnen (Twitch- + TikTok-Chat)", this);
	connect(chatButton, &QPushButton::clicked, this,
		[] { QDesktopServices::openUrl(QUrl(QStringLiteral("http://localhost:8770/"))); });
	row->addWidget(chatButton);

	auto overlayButton = tool_button("\xF0\x9F\x96\xBC", "Overlays einfügen (Chat, Alerts, Zuschauer, hervorgehobene Nachricht)", this);
	connect(overlayButton, &QPushButton::clicked, this, [this] { ShowOverlayMenu(); });
	row->addWidget(overlayButton);

	layout->addLayout(row);

	auto infoRow = new QHBoxLayout();
	status = new QLabel(this);
	infoRow->addWidget(status, 1);
	infoRow->addWidget(new ViewerCounter(this));
	layout->addLayout(infoRow);

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
	menu.addAction(QString::fromUtf8("MultiStream Hub im Browser öffnen"), this,
		       [] { QDesktopServices::openUrl(QUrl(QStringLiteral("http://localhost:8770/"))); });
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
	const bool live = v.IsStreaming();
	streamButton->setText(live ? QString::fromUtf8("\xE2\x96\xA0 TikTok stoppen")
				   : QString::fromUtf8("\xE2\x96\xB6 TikTok starten"));
	streamButton->setStyleSheet(live ? QStringLiteral("QPushButton{background:#e5253f;color:white;font-weight:bold;}")
					 : QString());
	const std::string scene = v.CurrentSceneName();
	QString text = live ? QString::fromUtf8("<b style='color:#ff4d5e'>\xE2\x97\x8F LIVE</b>")
			    : QString::fromUtf8("<span style='color:#888'>\xE2\x97\x8F Offline</span>");
	if (!scene.empty())
		text += QStringLiteral(" &nbsp;·&nbsp; ") + QString::fromStdString(scene).toHtmlEscaped();
	status->setText(text);
}

/* ================================================================== */
/* Szenen                                                              */
/* ================================================================== */

XVScenesDock::XVScenesDock(QWidget *parent) : QWidget(parent)
{
	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->setSpacing(4);

	list = new QListWidget(this);
	list->setSelectionMode(QAbstractItemView::SingleSelection);
	list->setDragDropMode(QAbstractItemView::InternalMove);
	list->setDefaultDropAction(Qt::MoveAction);
	layout->addWidget(list, 1);

	connect(list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *cur, QListWidgetItem *) {
		if (updating || !cur)
			return;
		Vertical::Get().SetCurrentScene(cur->text().toStdString());
	});
	/* Nach dem Ziehen die neue Reihenfolge fest speichern */
	connect(list->model(), &QAbstractItemModel::rowsMoved, this, [this] {
		if (updating)
			return;
		std::vector<std::string> order;
		for (int i = 0; i < list->count(); i++)
			order.push_back(list->item(i)->text().toStdString());
		Vertical::Get().SetSceneOrder(order);
	});
	connect(list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) { RenameScene(); });

	auto row = new QHBoxLayout();
	row->setSpacing(4);
	auto add = tool_button("+", "Neue Hochformat-Szene", this);
	auto remove = tool_button("\xE2\x88\x92", "Szene löschen", this);
	auto rename = tool_button("\xE2\x9C\x8E", "Szene umbenennen", this);
	auto sceneUp = tool_button("\xE2\x96\xB2", "Szene nach oben", this);
	auto sceneDown = tool_button("\xE2\x96\xBC", "Szene nach unten", this);
	auto moveScene = [this](int delta) {
		QListWidgetItem *cur = list->currentItem();
		if (cur)
			Vertical::Get().MoveScene(cur->text().toStdString(), delta);
	};
	connect(sceneUp, &QPushButton::clicked, this, [moveScene] { moveScene(-1); });
	connect(sceneDown, &QPushButton::clicked, this, [moveScene] { moveScene(1); });
	connect(add, &QPushButton::clicked, this, [this] { AddScene(); });
	connect(remove, &QPushButton::clicked, this, [this] { RemoveScene(); });
	connect(rename, &QPushButton::clicked, this, [this] { RenameScene(); });
	row->addWidget(add);
	row->addWidget(remove);
	row->addWidget(rename);
	row->addWidget(sceneUp);
	row->addWidget(sceneDown);
	row->addStretch(1);
	layout->addLayout(row);

	auto hint = new QLabel(
		QString::fromUtf8("Reihenfolge: ▲▼ oder per Maus ziehen – bleibt nach dem OBS-Neustart gleich. "
				  "Heißt eine Szene wie deine OBS-Szene + „ TT“, wechselt sie automatisch mit."),
		this);
	hint->setWordWrap(true);
	hint->setStyleSheet(QStringLiteral("color:#888;font-size:11px;"));
	layout->addWidget(hint);

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
	const QString name = item->text();
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
	const QString oldName = item->text();
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
	layout->setContentsMargins(4, 4, 4, 4);
	layout->setSpacing(4);

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

	auto row = new QHBoxLayout();
	row->setSpacing(4);
	auto add = tool_button("+", "Quelle hinzufügen", this);
	auto remove = tool_button("\xE2\x88\x92", "Quelle entfernen", this);
	auto up = tool_button("\xE2\x96\xB2", "Nach oben", this);
	auto down = tool_button("\xE2\x96\xBC", "Nach unten", this);
	auto props = tool_button("\xE2\x9A\x99", "Eigenschaften", this);
	auto fit = tool_button("\xE2\xA4\xA2", "Anpassen (Vollbild, obere/untere Hälfte …)", this);

	connect(add, &QPushButton::clicked, this, [this] { ShowAddMenu(); });
	connect(remove, &QPushButton::clicked, this, [this] {
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
	connect(up, &QPushButton::clicked, this, [move] { move(OBS_ORDER_MOVE_UP); });
	connect(down, &QPushButton::clicked, this, [move] { move(OBS_ORDER_MOVE_DOWN); });
	connect(props, &QPushButton::clicked, this, [this] {
		obs_source_t *src = Vertical::Get().CurrentSceneRef();
		if (obs_sceneitem_t *item = SelectedItem(obs_scene_from_source(src)))
			obs_frontend_open_source_properties(obs_sceneitem_get_source(item));
		obs_source_release(src);
	});
	connect(fit, &QPushButton::clicked, this, [this] { ShowFitMenu(); });

	for (auto b : {add, remove, up, down, props, fit})
		row->addWidget(b);
	row->addStretch(1);
	layout->addLayout(row);

	auto hint = new QLabel(QString::fromUtf8("In der Vorschau: klicken = auswählen, ziehen = verschieben, "
						 "Mausrad = Größe, Pfeiltasten = fein verschieben. 👁 = ein/aus, 🔒 = sperren."),
			       this);
	hint->setWordWrap(true);
	hint->setStyleSheet(QStringLiteral("color:#888;font-size:11px;"));
	layout->addWidget(hint);

	Vertical::Get().AddListener(this, [this] { Refresh(); });
	Refresh();
}

XVSourcesDock::~XVSourcesDock()
{
	DisconnectScene();
}

obs_sceneitem_t *XVSourcesDock::SelectedItem(obs_scene_t *scene)
{
	const int64_t id = Vertical::Get().selectedItem;
	if (!scene || !id)
		return nullptr;
	return obs_scene_find_sceneitem_by_id(scene, id);
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
};

void XVSourcesDock::Refresh()
{
	ConnectScene();
	updating = true;
	list->clear();

	obs_source_t *src = Vertical::Get().CurrentSceneRef();
	obs_scene_t *scene = obs_scene_from_source(src);
	std::vector<ItemRow> rows;
	if (scene) {
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
				auto out = static_cast<std::vector<ItemRow> *>(param);
				const char *n = obs_source_get_name(obs_sceneitem_get_source(item));
				out->push_back({obs_sceneitem_get_id(item), n ? n : "", obs_sceneitem_visible(item),
						obs_sceneitem_locked(item)});
				return true;
			},
			&rows);
	}
	obs_source_release(src);

	const int64_t selected = Vertical::Get().selectedItem;
	for (auto it = rows.rbegin(); it != rows.rend(); ++it) {
		const int64_t id = it->id;
		auto item = new QListWidgetItem(list);
		item->setData(Qt::UserRole, QVariant::fromValue<qlonglong>(id));

		auto rowWidget = new QWidget(list);
		auto rowLayout = new QHBoxLayout(rowWidget);
		rowLayout->setContentsMargins(4, 1, 4, 1);
		rowLayout->setSpacing(4);

		auto eye = new QToolButton(rowWidget);
		eye->setAutoRaise(true);
		eye->setText(it->visible ? QString::fromUtf8("\xF0\x9F\x91\x81") : QString::fromUtf8("\xE2\x80\x94"));
		eye->setToolTip(it->visible ? QString::fromUtf8("Sichtbar – klicken zum Ausblenden")
					    : QString::fromUtf8("Ausgeblendet – klicken zum Einblenden"));
		connect(eye, &QToolButton::clicked, this, [id] {
			obs_source_t *src = Vertical::Get().CurrentSceneRef();
			if (obs_sceneitem_t *si = obs_scene_find_sceneitem_by_id(obs_scene_from_source(src), id))
				obs_sceneitem_set_visible(si, !obs_sceneitem_visible(si));
			obs_source_release(src);
			Vertical::Get().Notify();
		});

		auto label = new QLabel(QString::fromStdString(it->name), rowWidget);
		if (!it->visible)
			label->setStyleSheet(QStringLiteral("color:#777;"));

		auto lock = new QToolButton(rowWidget);
		lock->setAutoRaise(true);
		lock->setText(it->locked ? QString::fromUtf8("\xF0\x9F\x94\x92") : QString::fromUtf8("\xF0\x9F\x94\x93"));
		lock->setToolTip(it->locked ? QString::fromUtf8("Gesperrt – kann in der Vorschau nicht verschoben werden")
					    : QString::fromUtf8("Entsperrt – klicken zum Sperren"));
		connect(lock, &QToolButton::clicked, this, [id] {
			obs_source_t *src = Vertical::Get().CurrentSceneRef();
			if (obs_sceneitem_t *si = obs_scene_find_sceneitem_by_id(obs_scene_from_source(src), id))
				obs_sceneitem_set_locked(si, !obs_sceneitem_locked(si));
			obs_source_release(src);
			Vertical::Get().Notify();
		});

		rowLayout->addWidget(eye);
		rowLayout->addWidget(label, 1);
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
	for (int i = 0; i < 7; i++)
		menu.addAction(QString::fromUtf8(labels[i]), this, [this, i] { ApplyFit(i); });
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
	}
	obs_source_release(src);
	Vertical::Get().Notify();
}
