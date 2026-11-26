/*
 * xImJxstinn Stream Tools – Viewer-Counter-Dock (Twitch + TikTok + Gesamt)
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#include "viewers-dock.hpp"
#include "chat-dock.hpp"
#include "hub.hpp"
#include "settings.hpp"

#include <obs-frontend-api.h>
#include <curl/curl.h>

#include <QCursor>
#include <QGridLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <string>

static size_t xvd_write(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	static_cast<std::string *>(userdata)->append(ptr, size * nmemb);
	return size * nmemb;
}

static std::string xvd_get_state()
{
	std::string body;
	CURL *curl = curl_easy_init();
	if (!curl)
		return body;
	curl_easy_setopt(curl, CURLOPT_URL, XV_HUB_URL "/api/state");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, xvd_write);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 800L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 2000L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl, CURLOPT_NOPROXY, "*");
	if (curl_easy_perform(curl) != CURLE_OK)
		body.clear();
	curl_easy_cleanup(curl);
	return body;
}

static QLabel *make_tile(QGridLayout *grid, int col, const QString &title, const QString &color, QWidget *parent)
{
	auto name = new QLabel(title, parent);
	name->setAlignment(Qt::AlignCenter);
	name->setStyleSheet(QStringLiteral("color:%1;font-weight:bold;").arg(color));
	auto value = new QLabel(QString::fromUtf8("\xE2\x80\x93"), parent);
	value->setAlignment(Qt::AlignCenter);
	value->setStyleSheet(QStringLiteral("font-size:26px;font-weight:800;"));
	grid->addWidget(value, 0, col);
	grid->addWidget(name, 1, col);
	return value;
}

static QToolButton *obs_tool_button(const char *cls, const char *tip, QWidget *parent)
{
	auto b = new QToolButton(parent);
	b->setProperty("class", cls);
	b->setProperty("themeID", cls);
	b->setToolTip(QString::fromUtf8(tip));
	b->setAutoRaise(true);
	b->setIconSize(QSize(16, 16));
	return b;
}

XViewersDock::XViewersDock(QWidget *parent) : QWidget(parent)
{
	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 8, 0, 0);
	layout->setSpacing(6);

	auto grid = new QGridLayout();
	grid->setHorizontalSpacing(12);
	twitchValue = make_tile(grid, 0, QStringLiteral("Twitch"), QStringLiteral("#9146ff"), this);
	tiktokValue = make_tile(grid, 1, QStringLiteral("TikTok"), QStringLiteral("#fe2c55"), this);
	totalValue = make_tile(grid, 2, QStringLiteral("Gesamt"), QStringLiteral("#dddddd"), this);
	layout->addLayout(grid);

	state = new QLabel(this);
	state->setAlignment(Qt::AlignCenter);
	state->setWordWrap(true);
	state->setStyleSheet(QStringLiteral("color:#888;"));
	layout->addWidget(state);
	layout->addStretch(1);

	auto toolbar = new QToolBar(this);
	toolbar->setIconSize(QSize(16, 16));
	toolbar->setMovable(false);
	toolbar->setFloatable(false);
	auto add = obs_tool_button("icon-plus", "Als Overlay in die aktuelle Szene einfügen", toolbar);
	auto settings = obs_tool_button("icon-gear", "Einstellungen", toolbar);
	auto more = obs_tool_button("icon-dots-vert", "Multichat öffnen, MultiStream Hub starten", toolbar);
	connect(add, &QAbstractButton::clicked, this, [this] { ShowAddMenu(); });
	connect(settings, &QAbstractButton::clicked, this, [this] { XSettingsDialog::Run(this); });
	connect(more, &QAbstractButton::clicked, this, [this] {
		QMenu menu(this);
		menu.addAction(QString::fromUtf8("Multichat in OBS öffnen"), this, [] { XVShowChatDock(); });
		if (!Hub::Online()) {
			menu.addAction(QString::fromUtf8("MultiStream Hub starten"), this, [this] {
				std::string error;
				if (!Hub::Start(error))
					QMessageBox::warning(this, QStringLiteral("MultiStream Hub"), QString::fromStdString(error));
			});
		}
		menu.exec(QCursor::pos());
	});
	toolbar->addWidget(add);
	toolbar->addWidget(settings);
	toolbar->addSeparator();
	toolbar->addWidget(more);
	layout->addWidget(toolbar);

	connect(&timer, &QTimer::timeout, this, [this] { Tick(); });
	timer.start(5000);
	QTimer::singleShot(1500, this, [this] { Tick(); });
}

XViewersDock::~XViewersDock()
{
	timer.stop();
	if (worker.joinable())
		worker.join();
}

void XViewersDock::Tick()
{
	if (busy.load())
		return;
	if (worker.joinable())
		worker.join();
	busy.store(true);
	worker = std::thread([this] {
		const std::string body = xvd_get_state();
		const QByteArray data(body.data(), static_cast<qsizetype>(body.size()));
		QMetaObject::invokeMethod(this, [this, data] { Apply(data); }, Qt::QueuedConnection);
		busy.store(false);
	});
}

void XViewersDock::Apply(const QByteArray &json)
{
	const QString dash = QString::fromUtf8("\xE2\x80\x93");
	if (json.isEmpty()) {
		twitchValue->setText(dash);
		tiktokValue->setText(dash);
		totalValue->setText(dash);
		state->setText(QString::fromUtf8("MultiStream Hub läuft nicht – über ⋮ starten."));
		return;
	}
	const QJsonObject st = QJsonDocument::fromJson(json).object().value(QStringLiteral("status")).toObject();
	const QJsonValue tw = st.value(QStringLiteral("twitch_viewers"));
	const QJsonValue tt = st.value(QStringLiteral("tiktok_viewers"));
	const QLocale de(QLocale::German);
	const bool hasTw = !tw.isNull() && !tw.isUndefined();
	const bool hasTt = !tt.isNull() && !tt.isUndefined();
	const qlonglong twN = hasTw ? (qlonglong)tw.toDouble() : 0;
	const qlonglong ttN = hasTt ? (qlonglong)tt.toDouble() : 0;
	twitchValue->setText(hasTw ? de.toString(twN) : dash);
	tiktokValue->setText(hasTt ? de.toString(ttN) : dash);
	totalValue->setText(hasTw || hasTt ? de.toString(twN + ttN) : dash);

	QStringList info;
	if (!hasTw)
		info << QString::fromUtf8("Twitch offline");
	if (!hasTt)
		info << QString::fromUtf8("TikTok nicht live");
	state->setText(info.join(QStringLiteral(" · ")));
}

void XViewersDock::ShowAddMenu()
{
	struct Entry {
		const char *label;
		Hub::Overlay kind;
	};
	const Entry entries[] = {{"Viewer Counter", Hub::Overlay::Viewers},
				 {"Chat (Twitch + TikTok)", Hub::Overlay::Chat},
				 {"Alerts", Hub::Overlay::Alerts},
				 {"Hervorgehobene Chat-Nachricht", Hub::Overlay::Highlight}};
	QMenu menu(this);
	for (const Entry &e : entries) {
		const Hub::Overlay kind = e.kind;
		menu.addAction(QString::fromUtf8(e.label), this, [this, kind] {
			obs_source_t *src = obs_frontend_get_current_scene();
			obs_video_info ovi;
			obs_get_video_info(&ovi);
			if (!Hub::AddOverlay(obs_scene_from_source(src), kind, false, ovi.base_width, ovi.base_height))
				QMessageBox::warning(this, QStringLiteral("Overlay"),
						     QStringLiteral("Overlay konnte nicht eingefügt werden."));
			obs_source_release(src);
		});
	}
	menu.exec(QCursor::pos());
}
