/*
 * xImJxstinn Stream Tools – Viewer-Counter-Dock (Twitch + TikTok + Gesamt)
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#include "viewers-dock.hpp"
#include "chat-dock.hpp"
#include "hub.hpp"
#include "settings.hpp"
#include "icons.hpp"

#include <obs-frontend-api.h>
#include <curl/curl.h>

#include <QCursor>
#include <QHBoxLayout>
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

static const char *CARD_STYLE =
	"QFrame#xvCard{background:rgba(255,255,255,0.045);border:1px solid rgba(255,255,255,0.09);"
	"border-radius:10px;}";

static QLabel *icon_label(const char *icon, const QString &color, int size, QWidget *parent)
{
	auto l = new QLabel(parent);
	l->setPixmap(XIcons::Pixmap(icon, color, size, parent));
	l->setFixedSize(size, size);
	return l;
}

static XViewerCard make_card(const QString &title, const QString &color, const char *icon, QWidget *parent)
{
	XViewerCard c;
	c.frame = new QFrame(parent);
	c.frame->setObjectName(QStringLiteral("xvCard"));
	c.frame->setStyleSheet(QString::fromUtf8(CARD_STYLE) +
			       QStringLiteral("QFrame#xvCard{border-left:4px solid %1;}").arg(color));
	auto v = new QVBoxLayout(c.frame);
	v->setContentsMargins(12, 8, 10, 10);
	v->setSpacing(2);

	auto head = new QHBoxLayout();
	head->setSpacing(6);
	head->addWidget(icon_label(icon, color, 16, c.frame));
	auto name = new QLabel(title, c.frame);
	name->setStyleSheet(QStringLiteral("color:%1;font-weight:700;font-size:12px;letter-spacing:0.5px;").arg(color));
	c.badge = new QLabel(c.frame);
	c.badgeIcon = new QLabel(c.frame);
	c.badgeIcon->setFixedSize(14, 14);
	head->addWidget(name);
	head->addStretch(1);
	head->addWidget(c.badgeIcon);
	head->addWidget(c.badge);
	v->addLayout(head);

	c.value = new QLabel(QString::fromUtf8("\xE2\x80\x93"), c.frame);
	c.value->setStyleSheet(QStringLiteral("font-size:24px;font-weight:800;"));
	v->addWidget(c.value);
	return c;
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
	layout->setContentsMargins(8, 8, 8, 0);
	layout->setSpacing(8);

	/* Große Gesamt-Karte */
	auto totalFrame = new QFrame(this);
	totalFrame->setObjectName(QStringLiteral("xvCard"));
	totalFrame->setStyleSheet(
		QStringLiteral("QFrame#xvCard{border-radius:12px;border:1px solid rgba(255,255,255,0.10);"
			       "background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 rgba(145,70,255,0.30),"
			       "stop:1 rgba(254,44,85,0.26));}"));
	auto tv = new QVBoxLayout(totalFrame);
	tv->setContentsMargins(14, 10, 14, 12);
	tv->setSpacing(0);
	auto titleRow = new QHBoxLayout();
	titleRow->setSpacing(6);
	titleRow->addWidget(icon_label(XIcons::Users, QStringLiteral("rgba(255,255,255,0.85)"), 16, totalFrame));
	auto totalTitle = new QLabel(QString::fromUtf8("ZUSCHAUER GESAMT"), totalFrame);
	totalTitle->setStyleSheet(QStringLiteral("font-size:11px;font-weight:700;letter-spacing:1px;color:rgba(255,255,255,0.8);"));
	titleRow->addWidget(totalTitle);
	titleRow->addStretch(1);
	tv->addLayout(titleRow);
	auto totalRow = new QHBoxLayout();
	totalValue = new QLabel(QString::fromUtf8("\xE2\x80\x93"), totalFrame);
	totalValue->setStyleSheet(QStringLiteral("font-size:38px;font-weight:900;color:white;"));
	trend = new QLabel(totalFrame);
	trend->setStyleSheet(QStringLiteral("font-size:13px;font-weight:700;"));
	trendIcon = new QLabel(totalFrame);
	trendIcon->setFixedSize(16, 16);
	totalRow->addWidget(totalValue);
	totalRow->addSpacing(6);
	totalRow->addWidget(trendIcon, 0, Qt::AlignBottom);
	totalRow->addWidget(trend, 0, Qt::AlignBottom);
	totalRow->addStretch(1);
	tv->addLayout(totalRow);
	auto peakRow = new QHBoxLayout();
	peakRow->setSpacing(5);
	peakIcon = icon_label(XIcons::Trophy, QStringLiteral("#ffd166"), 13, totalFrame);
	peakIcon->setVisible(false);
	peak = new QLabel(totalFrame);
	peak->setStyleSheet(QStringLiteral("font-size:11px;color:rgba(255,255,255,0.75);"));
	peakRow->addWidget(peakIcon);
	peakRow->addWidget(peak);
	peakRow->addStretch(1);
	tv->addLayout(peakRow);
	layout->addWidget(totalFrame);

	/* Twitch + TikTok nebeneinander */
	auto cards = new QHBoxLayout();
	cards->setSpacing(8);
	twitch = make_card(QStringLiteral("TWITCH"), QStringLiteral("#a970ff"), XIcons::Screen, this);
	tiktok = make_card(QStringLiteral("TIKTOK"), QStringLiteral("#fe2c55"), XIcons::Music, this);
	cards->addWidget(twitch.frame, 1);
	cards->addWidget(tiktok.frame, 1);
	layout->addLayout(cards);

	state = new QLabel(this);
	state->setAlignment(Qt::AlignCenter);
	state->setWordWrap(true);
	state->setStyleSheet(QStringLiteral("color:#8a8a99;font-size:11px;"));
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
		menu.addAction(QString::fromUtf8("Peak zurücksetzen"), this, [this] {
			peakTotal = lastTotal > 0 ? lastTotal : 0;
			peak->setText(QString::fromUtf8("Peak: ") + QLocale(QLocale::German).toString(peakTotal));
		});
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

void XViewersDock::SetLive(XViewerCard &card, bool live, const QString &offlineText)
{
	card.badgeIcon->setPixmap(XIcons::Pixmap(live ? XIcons::Live : XIcons::Offline,
						 live ? QStringLiteral("#ff4d64") : QStringLiteral("#8a8a99"), 14, card.frame));
	if (live) {
		card.badge->setText(QStringLiteral("LIVE"));
		card.badge->setStyleSheet(QStringLiteral(
			"color:white;background:#e91e3a;border-radius:7px;padding:1px 7px;font-size:10px;font-weight:800;"));
	} else {
		card.badge->setText(offlineText);
		card.badge->setStyleSheet(QStringLiteral(
			"color:#9a9aa8;background:rgba(255,255,255,0.07);border-radius:7px;padding:1px 7px;font-size:10px;"));
	}
}

void XViewersDock::Apply(const QByteArray &json)
{
	const QString dash = QString::fromUtf8("\xE2\x80\x93");
	const QLocale de(QLocale::German);
	if (json.isEmpty()) {
		twitch.value->setText(dash);
		tiktok.value->setText(dash);
		totalValue->setText(dash);
		trend->clear();
		trendIcon->clear();
		SetLive(twitch, false, QStringLiteral("offline"));
		SetLive(tiktok, false, QStringLiteral("offline"));
		state->setText(QString::fromUtf8("MultiStream Hub läuft nicht – über ⋮ starten."));
		return;
	}
	const QJsonObject st = QJsonDocument::fromJson(json).object().value(QStringLiteral("status")).toObject();
	const QJsonValue tw = st.value(QStringLiteral("twitch_viewers"));
	const QJsonValue tt = st.value(QStringLiteral("tiktok_viewers"));
	const bool hasTw = !tw.isNull() && !tw.isUndefined();
	const bool hasTt = !tt.isNull() && !tt.isUndefined();
	const qlonglong twN = hasTw ? (qlonglong)tw.toDouble() : 0;
	const qlonglong ttN = hasTt ? (qlonglong)tt.toDouble() : 0;

	twitch.value->setText(hasTw ? de.toString(twN) : dash);
	tiktok.value->setText(hasTt ? de.toString(ttN) : dash);
	SetLive(twitch, hasTw, QStringLiteral("offline"));
	SetLive(tiktok, hasTt, QStringLiteral("nicht live"));

	if (hasTw || hasTt) {
		const qlonglong total = twN + ttN;
		totalValue->setText(de.toString(total));
		if (lastTotal >= 0 && total != lastTotal) {
			const qlonglong diff = total - lastTotal;
			trendIcon->setPixmap(XIcons::Pixmap(diff > 0 ? XIcons::TrendUp : XIcons::TrendDown,
							    diff > 0 ? QStringLiteral("#3ddc84") : QStringLiteral("#ff6b81"), 16, this));
			trend->setText(de.toString(diff > 0 ? diff : -diff));
			trend->setStyleSheet(QStringLiteral("font-size:13px;font-weight:700;color:%1;")
						     .arg(diff > 0 ? QStringLiteral("#3ddc84") : QStringLiteral("#ff6b81")));
		}
		lastTotal = total;
		if (total > peakTotal)
			peakTotal = total;
		peak->setText(QString::fromUtf8("Peak heute: ") + de.toString(peakTotal));
		peakIcon->setVisible(true);
		state->clear();
	} else {
		totalValue->setText(dash);
		trend->clear();
		trendIcon->clear();
		peak->setText(peakTotal > 0 ? QString::fromUtf8("Peak heute: ") + de.toString(peakTotal) : QString());
		peakIcon->setVisible(peakTotal > 0);
		state->setText(QString::fromUtf8("Gerade offline – die Zahlen erscheinen, sobald du live bist."));
	}
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
