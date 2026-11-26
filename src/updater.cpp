/*
 * xImJxstinn Stream Tools – automatische Updates über GitHub-Releases
 *
 * Ablauf: Beim OBS-Start wird das neueste Release von
 * github.com/ximjxstinn/ximjxstinn-vertical abgefragt. Ist es neuer, wird der
 * Installer heruntergeladen und – je nach Wahl – sofort (OBS startet neu) oder
 * beim nächsten Schließen von OBS still installiert.
 *
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#include "updater.hpp"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <curl/curl.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMainWindow>
#include <QMessageBox>
#include <QMetaObject>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextStream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <atomic>
#include <cstdio>
#include <string>
#include <thread>

#ifndef PLUGIN_VERSION
#define PLUGIN_VERSION "0.0.0"
#endif

#define XV_RELEASES_API "https://api.github.com/repos/ximjxstinn/ximjxstinn-vertical/releases/latest"

namespace Updater {

static std::atomic<bool> busy{false};
static bool installOnExit = false;
static QString installerPath;

const char *CurrentVersion()
{
	return PLUGIN_VERSION;
}

/* ---------- HTTP ---------- */

static size_t write_string(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	static_cast<std::string *>(userdata)->append(ptr, size * nmemb);
	return size * nmemb;
}

static size_t write_file(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	return fwrite(ptr, size, nmemb, static_cast<FILE *>(userdata)) * size;
}

static CURL *make_curl(const std::string &url)
{
	CURL *curl = curl_easy_init();
	if (!curl)
		return nullptr;
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "xImJxstinn-StreamTools-Updater");
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
	curl_easy_setopt(curl, CURLOPT_SSL_OPTIONS, (long)CURLSSLOPT_REVOKE_BEST_EFFORT);
	return curl;
}

static bool http_get(const std::string &url, std::string &out)
{
	CURL *curl = make_curl(url);
	if (!curl)
		return false;
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_string);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
	const bool ok = curl_easy_perform(curl) == CURLE_OK;
	curl_easy_cleanup(curl);
	return ok;
}

static bool http_download(const std::string &url, const QString &path)
{
	FILE *f = nullptr;
#ifdef _WIN32
	f = _wfopen(reinterpret_cast<const wchar_t *>(path.utf16()), L"wb");
#else
	f = fopen(path.toUtf8().constData(), "wb");
#endif
	if (!f)
		return false;
	CURL *curl = make_curl(url);
	if (!curl) {
		fclose(f);
		return false;
	}
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
	const bool ok = curl_easy_perform(curl) == CURLE_OK;
	curl_easy_cleanup(curl);
	fclose(f);
	return ok;
}

/* ---------- Versionen ---------- */

static bool is_newer(const QString &remote, const QString &local)
{
	QString r = remote;
	if (r.startsWith(QLatin1Char('v')) || r.startsWith(QLatin1Char('V')))
		r = r.mid(1);
	const QStringList a = r.split(QLatin1Char('.'));
	const QStringList b = local.split(QLatin1Char('.'));
	for (int i = 0; i < 3; i++) {
		const int x = i < a.size() ? a[i].toInt() : 0;
		const int y = i < b.size() ? b[i].toInt() : 0;
		if (x != y)
			return x > y;
	}
	return false;
}

/* ---------- Installation ---------- */

static void run_hidden(const QString &program, const QStringList &args)
{
	QProcess p;
	p.setProgram(program);
	p.setArguments(args);
#ifdef _WIN32
	p.setCreateProcessArgumentsModifier(
		[](QProcess::CreateProcessArguments *a) { a->flags |= CREATE_NO_WINDOW; });
#endif
	p.startDetached();
}

/* Schreibt ein kleines Skript, das wartet bis OBS zu ist, dann installiert
 * und OBS (optional) wieder startet. */
static void launch_installer(bool restartObs)
{
	if (installerPath.isEmpty() || !QFile::exists(installerPath))
		return;
	const QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
	const QString script = QDir(tmp).filePath(QStringLiteral("ximjxstinn-streamtools-update.cmd"));
	const QString obsExe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
	const QString obsDir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());

	QFile f(script);
	if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
		return;
	QTextStream out(&f);
	out << "@echo off\r\n";
	out << ":wait\r\n";
	out << "tasklist /FI \"IMAGENAME eq obs64.exe\" | find /I \"obs64.exe\" >nul && (timeout /t 1 /nobreak >nul & goto wait)\r\n";
	out << "\"" << QDir::toNativeSeparators(installerPath) << "\" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART\r\n";
	if (restartObs)
		out << "start \"\" /D \"" << obsDir << "\" \"" << obsExe << "\"\r\n";
	out << "del \"" << QDir::toNativeSeparators(installerPath) << "\" >nul 2>nul\r\n";
	f.close();

	run_hidden(QStringLiteral("cmd.exe"), {QStringLiteral("/c"), QDir::toNativeSeparators(script)});
	blog(LOG_INFO, "[xImJxstinn Stream Tools] Update wird installiert");
}

void OnExit()
{
	if (installOnExit) {
		installOnExit = false;
		launch_installer(false);
	}
}

/* ---------- Ablauf ---------- */

static void ask_user(QPointer<QWidget> parent, const QString &version, const QString &notes)
{
	QMessageBox box(parent);
	box.setWindowTitle(QStringLiteral("xImJxstinn Stream Tools – Update"));
	box.setIcon(QMessageBox::Information);
	QString text = QString::fromUtf8("<b>Neue Version %1 ist da</b> (du hast %2).<br><br>")
			       .arg(version.toHtmlEscaped(), QString::fromUtf8(PLUGIN_VERSION));
	if (!notes.trimmed().isEmpty())
		text += notes.left(600).toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")) +
			QStringLiteral("<br><br>");
	text += QString::fromUtf8("Das Update ist schon heruntergeladen. Windows fragt bei der Installation einmal nach Erlaubnis.");
	box.setText(text);
	auto now = box.addButton(QString::fromUtf8("Jetzt aktualisieren (OBS startet neu)"), QMessageBox::AcceptRole);
	auto onExit = box.addButton(QString::fromUtf8("Beim Schließen von OBS"), QMessageBox::ActionRole);
	box.addButton(QString::fromUtf8("Später"), QMessageBox::RejectRole);
	box.setDefaultButton(onExit);
	box.exec();

	if (box.clickedButton() == now) {
		launch_installer(true);
		auto main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
		if (main)
			QMetaObject::invokeMethod(main, "close", Qt::QueuedConnection);
	} else if (box.clickedButton() == onExit) {
		installOnExit = true;
	}
}

void Check(QWidget *parentWidget, bool manual)
{
	if (busy.exchange(true))
		return;
	QPointer<QWidget> parent(parentWidget);

	std::thread([parent, manual] {
		auto finish = [parent, manual](const QString &message, bool error) {
			busy = false;
			if (!manual)
				return;
			QMetaObject::invokeMethod(
				QCoreApplication::instance(),
				[parent, message, error] {
					if (error)
						QMessageBox::warning(parent, QStringLiteral("xImJxstinn Stream Tools"), message);
					else
						QMessageBox::information(parent, QStringLiteral("xImJxstinn Stream Tools"), message);
				},
				Qt::QueuedConnection);
		};

		std::string body;
		if (!http_get(XV_RELEASES_API, body)) {
			finish(QString::fromUtf8("Update-Suche fehlgeschlagen – keine Verbindung zu GitHub."), true);
			return;
		}
		const QJsonObject release = QJsonDocument::fromJson(QByteArray::fromStdString(body)).object();
		const QString tag = release.value(QStringLiteral("tag_name")).toString();
		if (tag.isEmpty() || !is_newer(tag, QString::fromUtf8(PLUGIN_VERSION))) {
			finish(QString::fromUtf8("Du hast schon die neueste Version (%1).").arg(QString::fromUtf8(PLUGIN_VERSION)),
			       false);
			return;
		}

		QString assetUrl;
		const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
		for (const QJsonValue &a : assets) {
			const QJsonObject o = a.toObject();
			const QString name = o.value(QStringLiteral("name")).toString();
			if (name.endsWith(QStringLiteral("-windows-installer.exe"), Qt::CaseInsensitive)) {
				assetUrl = o.value(QStringLiteral("browser_download_url")).toString();
				break;
			}
		}
		if (assetUrl.isEmpty()) {
			finish(QString::fromUtf8("Version %1 gefunden, aber ohne Installer.").arg(tag), true);
			return;
		}

		const QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
		const QString path = QDir(tmp).filePath(QStringLiteral("ximjxstinn-streamtools-%1-setup.exe").arg(tag));
		if (!http_download(assetUrl.toStdString(), path)) {
			finish(QString::fromUtf8("Download des Updates fehlgeschlagen."), true);
			return;
		}

		const QString notes = release.value(QStringLiteral("body")).toString();
		busy = false;
		QMetaObject::invokeMethod(
			QCoreApplication::instance(),
			[parent, tag, notes, path] {
				installerPath = path;
				ask_user(parent, tag, notes);
			},
			Qt::QueuedConnection);
	}).detach();
}

} // namespace Updater
