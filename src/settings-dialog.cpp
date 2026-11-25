/*
 * xImJxstinn Vertical – Einstellungen
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#include "settings-dialog.hpp"
#include "vertical.hpp"
#include "hub.hpp"
#include "updater.hpp"

#include <QDir>
#include <QFileDialog>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSize>
#include <QSpinBox>
#include <QVBoxLayout>

namespace XVSettingsDialog {

void Run(QWidget *parent)
{
	Vertical &v = Vertical::Get();
	XVSettings &s = v.settings;

	QDialog dlg(parent);
	dlg.setWindowTitle(QStringLiteral("xImJxstinn Vertical – Einstellungen"));
	dlg.setMinimumWidth(460);
	auto layout = new QVBoxLayout(&dlg);

	auto form = new QFormLayout();

	auto title = new QLabel(QString::fromUtf8("<b>TikTok LIVE</b> – Server-URL und Stream-Key findest du in "
						  "TikTok LIVE Studio bzw. im LIVE Center."));
	title->setWordWrap(true);
	layout->addWidget(title);

	auto server = new QLineEdit(QString::fromStdString(s.server));
	server->setPlaceholderText(QStringLiteral("rtmp://push-rtmp-….tiktokcdn.com/game/"));
	form->addRow(QStringLiteral("Server-URL"), server);

	auto keyRow = new QHBoxLayout();
	auto key = new QLineEdit(QString::fromStdString(s.key));
	key->setEchoMode(QLineEdit::Password);
	key->setPlaceholderText(QStringLiteral("stream-…"));
	auto show = new QPushButton(QStringLiteral("Zeigen"));
	show->setCheckable(true);
	QObject::connect(show, &QPushButton::toggled, key, [key](bool on) {
		key->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
	});
	keyRow->addWidget(key, 1);
	keyRow->addWidget(show);
	form->addRow(QStringLiteral("Stream-Key"), keyRow);

	auto resolution = new QComboBox();
	resolution->addItem(QString::fromUtf8("1080 × 1920 (Full HD)"), QSize(1080, 1920));
	resolution->addItem(QString::fromUtf8("720 × 1280 (spart Leistung)"), QSize(720, 1280));
	resolution->setCurrentIndex(s.width == 720 ? 1 : 0);
	form->addRow(QStringLiteral("Auflösung"), resolution);

	auto bitrate = new QSpinBox();
	bitrate->setRange(1000, 20000);
	bitrate->setSingleStep(500);
	bitrate->setSuffix(QStringLiteral(" kbps"));
	bitrate->setValue(s.bitrate);
	form->addRow(QStringLiteral("Bitrate"), bitrate);

	auto encoder = new QComboBox();
	encoder->addItem(QStringLiteral("Automatisch (Grafikkarte, sonst CPU)"), QStringLiteral("auto"));
	encoder->addItem(QStringLiteral("NVIDIA NVENC"), QStringLiteral("nvenc"));
	encoder->addItem(QStringLiteral("CPU (x264)"), QStringLiteral("x264"));
	encoder->setCurrentIndex(s.encoder == "nvenc" ? 1 : s.encoder == "x264" ? 2 : 0);
	form->addRow(QStringLiteral("Encoder"), encoder);

	auto withMain = new QCheckBox(QString::fromUtf8("TikTok automatisch mit „Stream starten“ in OBS starten/stoppen"));
	withMain->setChecked(s.startWithMain);
	form->addRow(QString(), withMain);

	auto autoSwitch = new QCheckBox(QString::fromUtf8("Hochformat-Szene automatisch mitwechseln"));
	autoSwitch->setChecked(s.autoSwitch);
	form->addRow(QString(), autoSwitch);

	auto suffix = new QLineEdit(QString::fromStdString(s.suffix));
	suffix->setToolTip(QString::fromUtf8(
		"Beispiel: OBS-Szene „Gaming Facecam“ → Hochformat-Szene „Gaming Facecam TT“. "
		"Gibt es keine mit Endung, wird eine Szene mit genau gleichem Namen gesucht."));
	form->addRow(QStringLiteral("Endung der Szenen"), suffix);

	layout->addLayout(form);

	auto hubTitle = new QLabel(QString::fromUtf8("<b>MultiStream Hub</b> – Twitch- + TikTok-Chat, Alerts, Overlays und Zuschauerzahlen."));
	hubTitle->setWordWrap(true);
	layout->addWidget(hubTitle);
	auto hubForm = new QFormLayout();
	auto hubRow = new QHBoxLayout();
	auto hubDir = new QLineEdit(QString::fromStdString(s.hubDir.empty() ? Hub::FindHubDir() : s.hubDir));
	hubDir->setPlaceholderText(QString::fromUtf8("Ordner mit app.py und start.bat"));
	auto browse = new QPushButton(QString::fromUtf8("…"));
	browse->setMaximumWidth(36);
	QObject::connect(browse, &QPushButton::clicked, &dlg, [&dlg, hubDir] {
		const QString dir = QFileDialog::getExistingDirectory(&dlg, QString::fromUtf8("MultiStream-Hub-Ordner wählen"), hubDir->text());
		if (!dir.isEmpty())
			hubDir->setText(QDir::toNativeSeparators(dir));
	});
	hubRow->addWidget(hubDir, 1);
	hubRow->addWidget(browse);
	hubForm->addRow(QStringLiteral("Hub-Ordner"), hubRow);
	auto hubAuto = new QCheckBox(QString::fromUtf8("Hub automatisch mit OBS starten und beenden"));
	hubAuto->setChecked(s.hubAutostart);
	hubForm->addRow(QString(), hubAuto);
	layout->addLayout(hubForm);

	auto updateRow = new QHBoxLayout();
	auto autoUpdate = new QCheckBox(QString::fromUtf8("Automatisch nach Updates suchen (beim OBS-Start)"));
	autoUpdate->setChecked(s.autoUpdate);
	auto checkNow = new QPushButton(QString::fromUtf8("Jetzt suchen"));
	QObject::connect(checkNow, &QPushButton::clicked, &dlg, [&dlg] { Updater::Check(&dlg, true); });
	updateRow->addWidget(autoUpdate, 1);
	updateRow->addWidget(checkNow);
	layout->addLayout(updateRow);

	auto footer = new QLabel(QString::fromUtf8(
		"<span style='color:#888'>xImJxstinn Vertical " PLUGIN_VERSION " · "
		"<a href='https://www.twitch.tv/ximjxstinn'>twitch.tv/ximjxstinn</a></span>"));
	footer->setOpenExternalLinks(true);
	layout->addWidget(footer);

	auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	layout->addWidget(buttons);

	if (dlg.exec() != QDialog::Accepted)
		return;

	const QSize res = resolution->currentData().toSize();
	const bool resChanged = res.width() != s.width || res.height() != s.height;

	s.server = server->text().trimmed().toStdString();
	s.key = key->text().trimmed().toStdString();
	s.bitrate = bitrate->value();
	s.encoder = encoder->currentData().toString().toStdString();
	s.startWithMain = withMain->isChecked();
	s.autoSwitch = autoSwitch->isChecked();
	s.suffix = suffix->text().toStdString();
	s.hubDir = hubDir->text().trimmed().toStdString();
	s.hubAutostart = hubAuto->isChecked();
	s.autoUpdate = autoUpdate->isChecked();
	if (!s.hubDir.empty() && !Hub::IsValidHubDir(s.hubDir))
		QMessageBox::warning(parent, QStringLiteral("MultiStream Hub"),
				     QString::fromUtf8("In diesem Ordner liegt keine app.py vom MultiStream Hub."));

	if (resChanged) {
		const int oldW = s.width, oldH = s.height;
		s.width = res.width();
		s.height = res.height();
		std::string error;
		if (!v.ApplyVideo(error)) {
			s.width = oldW;
			s.height = oldH;
			QMessageBox::warning(parent, QStringLiteral("xImJxstinn Vertical"), QString::fromStdString(error));
		}
	}
	v.SaveSettings();
	v.Notify();
}

} // namespace XVSettingsDialog
