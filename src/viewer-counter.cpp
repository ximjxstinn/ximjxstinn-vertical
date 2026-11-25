/* xImJxstinn Vertical – Viewer Counter. Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later */
#include "viewer-counter.hpp"

#include <curl/curl.h>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
#include <QMetaObject>
#include <QMouseEvent>
#include <QUrl>
#include <string>

static constexpr const char *HUB_STATE_URL = "http://127.0.0.1:8770/api/state";
static constexpr int POLL_INTERVAL_MS = 5000;

static size_t viewer_counter_write(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	const size_t total = size * nmemb;
	static_cast<std::string *>(userdata)->append(ptr, total);
	return total;
}

static std::string viewer_counter_get(const char *url)
{
	std::string body;
	CURL *curl = curl_easy_init();
	if (!curl)
		return body;
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, viewer_counter_write);
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

ViewerCounter::ViewerCounter(QWidget *parent) : QLabel(parent)
{
	setTextFormat(Qt::RichText);
	setCursor(Qt::PointingHandCursor);
	setStyleSheet(QString::fromUtf8("QLabel{padding:0 6px;font-weight:bold;}"));
	setText(QString::fromUtf8("\xF0\x9F\x91\x81 \xE2\x80\x93"));
	setToolTip(QString::fromUtf8("Zuschauer (Twitch · TikTok · Gesamt) – MultiStream Hub wird gesucht…"));

	QObject::connect(&timer, &QTimer::timeout, this, [this] { Tick(); });
	timer.start(POLL_INTERVAL_MS);
	QTimer::singleShot(1500, this, [this] { Tick(); });
}

ViewerCounter::~ViewerCounter()
{
	timer.stop();
	if (worker.joinable())
		worker.join();
}

void ViewerCounter::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton)
		QDesktopServices::openUrl(QUrl(QString::fromUtf8("http://localhost:8770/")));
	QLabel::mousePressEvent(event);
}

void ViewerCounter::Tick()
{
	if (busy.load())
		return;
	if (worker.joinable())
		worker.join();
	busy.store(true);
	worker = std::thread([this] {
		const std::string body = viewer_counter_get(HUB_STATE_URL);
		const QByteArray data(body.data(), static_cast<qsizetype>(body.size()));
		QMetaObject::invokeMethod(this, [this, data] { Apply(data); }, Qt::QueuedConnection);
		busy.store(false);
	});
}

static QString viewer_counter_fmt(const QJsonValue &v, bool &has)
{
	if (v.isNull() || v.isUndefined())
		return QString::fromUtf8("\xE2\x80\x93");
	has = true;
	return QLocale(QLocale::German).toString(static_cast<qlonglong>(v.toDouble()));
}

void ViewerCounter::Apply(const QByteArray &json)
{
	if (json.isEmpty()) {
		setText(QString::fromUtf8("\xF0\x9F\x91\x81 \xE2\x80\x93"));
		setToolTip(QString::fromUtf8(
			"MultiStream Hub läuft nicht – starte ihn (start.bat oder 💬 Chat), dann erscheinen hier die Zuschauer."));
		return;
	}
	const QJsonObject status = QJsonDocument::fromJson(json).object().value(QStringLiteral("status")).toObject();
	const QJsonValue tw = status.value(QStringLiteral("twitch_viewers"));
	const QJsonValue tt = status.value(QStringLiteral("tiktok_viewers"));

	bool hasTw = false, hasTt = false;
	const QString twText = viewer_counter_fmt(tw, hasTw);
	const QString ttText = viewer_counter_fmt(tt, hasTt);
	QString sumText = QString::fromUtf8("\xE2\x80\x93");
	if (hasTw || hasTt) {
		const qlonglong sum = (hasTw ? static_cast<qlonglong>(tw.toDouble()) : 0) +
				      (hasTt ? static_cast<qlonglong>(tt.toDouble()) : 0);
		sumText = QLocale(QLocale::German).toString(sum);
	}

	setText(QString::fromUtf8("<span style='color:#9146ff'>TW</span> %1 &nbsp;"
				  "<span style='color:#fe2c55'>TT</span> %2 &nbsp;"
				  "\xF0\x9F\x91\x81 %3")
			.arg(twText, ttText, sumText));
	setToolTip(QString::fromUtf8("Zuschauer: Twitch %1 · TikTok %2 · Gesamt %3\nKlicken öffnet den MultiStream Hub.")
			   .arg(twText, ttText, sumText));
}
