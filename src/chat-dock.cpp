/*
 * xImJxstinn Vertical – Multichat-Dock (Twitch + TikTok) aus dem MultiStream Hub
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#include "chat-dock.hpp"
#include "browser-panel.hpp"
#include "hub.hpp"

#include <QMessageBox>

static QCef *xv_cef = nullptr;

XVChatDock::XVChatDock(QWidget *parent) : QWidget(parent)
{
	layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	offlineBox = new QWidget(this);
	auto box = new QVBoxLayout(offlineBox);
	box->setContentsMargins(12, 12, 12, 12);
	offlineText = new QLabel(QString::fromUtf8("MultiStream Hub wird gesucht …"), offlineBox);
	offlineText->setWordWrap(true);
	offlineText->setAlignment(Qt::AlignCenter);
	startButton = new QPushButton(QString::fromUtf8("\xE2\x96\xB6 MultiStream Hub starten"), offlineBox);
	connect(startButton, &QPushButton::clicked, this, [this] {
		std::string error;
		if (!Hub::Start(error))
			QMessageBox::warning(this, QStringLiteral("MultiStream Hub"), QString::fromStdString(error));
		else
			offlineText->setText(QString::fromUtf8("Hub startet …"));
	});
	box->addStretch(1);
	box->addWidget(offlineText);
	box->addWidget(startButton, 0, Qt::AlignCenter);
	box->addStretch(1);
	layout->addWidget(offlineBox, 1);

	Hub::AddListener(this, [this](bool online) { SetOnline(online); });
}

void XVChatDock::InitBrowser()
{
	if (browser)
		return;
	if (!xv_cef) {
		xv_cef = obs_browser_init_panel();
		if (!xv_cef) {
			offlineText->setText(QString::fromUtf8("Der OBS-Browser ist nicht verfügbar – öffne den Chat über 💬."));
			return;
		}
		xv_cef->init_browser();
		xv_cef->wait_for_browser_init();
	}
	browser = xv_cef->create_widget(this, std::string(XV_HUB_URL) + "/dock", nullptr);
	if (!browser)
		return;
	layout->addWidget(browser, 1);
	SetOnline(Hub::Online());
}

void XVChatDock::SetOnline(bool online)
{
	if (online) {
		offlineBox->setVisible(browser == nullptr);
		if (browser) {
			browser->setVisible(true);
			if (!wasOnline)
				browser->reloadPage();
		}
	} else {
		offlineText->setText(QString::fromUtf8("MultiStream Hub läuft nicht.\n"
						      "Starte ihn hier – dann erscheinen Twitch- und TikTok-Chat."));
		offlineBox->setVisible(true);
		if (browser)
			browser->setVisible(false);
	}
	wasOnline = online;
}
