/*
 * xImJxstinn Stream Tools – Einstellungen
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#include "settings.hpp"
#include "hub.hpp"
#include "updater.hpp"

#include <obs-module.h>
#include <util/platform.h>

#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

XSettings &XSettings::Get()
{
	static XSettings s;
	return s;
}

void XSettings::Load()
{
	char *path = obs_module_config_path("settings.json");
	obs_data_t *d = obs_data_create_from_json_file_safe(path, "bak");
	bfree(path);
	if (!d)
		return;
	hubDir = obs_data_get_string(d, "hub_dir");
	if (obs_data_has_user_value(d, "hub_autostart"))
		hubAutostart = obs_data_get_bool(d, "hub_autostart");
	if (obs_data_has_user_value(d, "auto_update"))
		autoUpdate = obs_data_get_bool(d, "auto_update");
	obs_data_release(d);
}

void XSettings::Save()
{
	char *dir = obs_module_config_path("");
	if (dir) {
		os_mkdirs(dir);
		bfree(dir);
	}
	obs_data_t *d = obs_data_create();
	obs_data_set_string(d, "hub_dir", hubDir.c_str());
	obs_data_set_bool(d, "hub_autostart", hubAutostart);
	obs_data_set_bool(d, "auto_update", autoUpdate);
	char *path = obs_module_config_path("settings.json");
	obs_data_save_json_safe(d, path, "tmp", "bak");
	bfree(path);
	obs_data_release(d);
}

namespace XSettingsDialog {

void Run(QWidget *parent)
{
	XSettings &s = XSettings::Get();

	QDialog dlg(parent);
	dlg.setWindowTitle(QStringLiteral("xImJxstinn Stream Tools – Einstellungen"));
	dlg.setMinimumWidth(460);
	auto layout = new QVBoxLayout(&dlg);

	auto title = new QLabel(QString::fromUtf8(
		"<b>MultiStream Hub</b> – liefert Twitch- + TikTok-Chat und die Zuschauerzahlen."));
	title->setWordWrap(true);
	layout->addWidget(title);

	auto form = new QFormLayout();
	auto hubRow = new QHBoxLayout();
	auto hubDir = new QLineEdit(QString::fromStdString(s.hubDir.empty() ? Hub::FindHubDir() : s.hubDir));
	hubDir->setPlaceholderText(QString::fromUtf8("Ordner mit app.py und start.bat"));
	auto browse = new QPushButton(QString::fromUtf8("…"));
	browse->setMaximumWidth(36);
	QObject::connect(browse, &QPushButton::clicked, &dlg, [&dlg, hubDir] {
		const QString dir = QFileDialog::getExistingDirectory(&dlg, QString::fromUtf8("MultiStream-Hub-Ordner wählen"),
								      hubDir->text());
		if (!dir.isEmpty())
			hubDir->setText(QDir::toNativeSeparators(dir));
	});
	hubRow->addWidget(hubDir, 1);
	hubRow->addWidget(browse);
	form->addRow(QStringLiteral("Hub-Ordner"), hubRow);

	auto hubAuto = new QCheckBox(QString::fromUtf8("Hub automatisch mit OBS starten und beenden"));
	hubAuto->setChecked(s.hubAutostart);
	form->addRow(QString(), hubAuto);
	layout->addLayout(form);

	auto updateRow = new QHBoxLayout();
	auto autoUpdate = new QCheckBox(QString::fromUtf8("Automatisch nach Updates suchen (beim OBS-Start)"));
	autoUpdate->setChecked(s.autoUpdate);
	auto checkNow = new QPushButton(QString::fromUtf8("Jetzt suchen"));
	QObject::connect(checkNow, &QPushButton::clicked, &dlg, [&dlg] { Updater::Check(&dlg, true); });
	updateRow->addWidget(autoUpdate, 1);
	updateRow->addWidget(checkNow);
	layout->addLayout(updateRow);

	auto footer = new QLabel(QString::fromUtf8("<span style='color:#888'>xImJxstinn Stream Tools ") +
				 QString::fromUtf8(Updater::CurrentVersion()) +
				 QString::fromUtf8(" · <a href='https://www.twitch.tv/ximjxstinn'>twitch.tv/ximjxstinn</a></span>"));
	footer->setOpenExternalLinks(true);
	layout->addWidget(footer);

	auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	layout->addWidget(buttons);

	if (dlg.exec() != QDialog::Accepted)
		return;

	s.hubDir = hubDir->text().trimmed().toStdString();
	s.hubAutostart = hubAuto->isChecked();
	s.autoUpdate = autoUpdate->isChecked();
	if (!s.hubDir.empty() && !Hub::IsValidHubDir(s.hubDir))
		QMessageBox::warning(parent, QStringLiteral("MultiStream Hub"),
				     QString::fromUtf8("In diesem Ordner liegt keine app.py vom MultiStream Hub."));
	s.Save();
}

} // namespace XSettingsDialog
