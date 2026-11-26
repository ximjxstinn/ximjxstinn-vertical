/*
 * xImJxstinn Stream Tools – eigene Vektor-Symbole (SVG, Strich-Stil)
 * Copyright (C) 2026 xImJxstinn – GPL-2.0-or-later
 */
#pragma once

#include <QPixmap>
#include <QString>
#include <QWidget>

namespace XIcons {

/* 24×24, Linien in currentColor */
inline constexpr const char *Users =
	"<path d='M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2'/><circle cx='9' cy='7' r='4'/>"
	"<path d='M22 21v-2a4 4 0 0 0-3-3.87'/><path d='M16 3.13a4 4 0 0 1 0 7.75'/>";
inline constexpr const char *TrendUp = "<polyline points='3 17 9 11 13 15 21 7'/><polyline points='15 7 21 7 21 13'/>";
inline constexpr const char *TrendDown = "<polyline points='3 7 9 13 13 9 21 17'/><polyline points='15 17 21 17 21 11'/>";
inline constexpr const char *Trophy =
	"<path d='M8 21h8M12 17v4M7 4h10v5a5 5 0 0 1-10 0V4z'/><path d='M17 5h3v2a3 3 0 0 1-3 3M7 5H4v2a3 3 0 0 0 3 3'/>";
inline constexpr const char *Screen =
	"<rect x='2' y='4' width='20' height='13' rx='2'/><path d='M10 8.5v5l4-2.5z'/><path d='M8 21h8M12 17v4'/>";
inline constexpr const char *Music =
	"<path d='M9 18V5l11-2v13'/><circle cx='6' cy='18' r='3'/><circle cx='17' cy='16' r='3'/>";
inline constexpr const char *Live =
	"<circle cx='12' cy='12' r='2'/><path d='M16.2 7.8a6 6 0 0 1 0 8.4M7.8 16.2a6 6 0 0 1 0-8.4"
	"M19.1 4.9a10 10 0 0 1 0 14.2M4.9 19.1a10 10 0 0 1 0-14.2'/>";
inline constexpr const char *Offline = "<circle cx='12' cy='12' r='9'/><path d='M8 12h8'/>";
inline constexpr const char *Chat = "<path d='M21 12a8 8 0 0 1-11.6 7.1L3 21l1.9-6.4A8 8 0 1 1 21 12z'/>";

/* Symbol in einer Farbe als Pixmap (scharf auf HiDPI) */
inline QPixmap Pixmap(const char *paths, const QString &color, int size, const QWidget *ref = nullptr)
{
	const qreal dpr = ref ? ref->devicePixelRatioF() : 1.0;
	const int px = (int)(size * dpr + 0.5);
	const QString svg = QStringLiteral("<svg xmlns='http://www.w3.org/2000/svg' width='%1' height='%1' viewBox='0 0 24 24' "
					   "fill='none' stroke='%2' stroke-width='2' stroke-linecap='round' "
					   "stroke-linejoin='round'>%3</svg>")
				    .arg(px)
				    .arg(color, QString::fromUtf8(paths));
	QPixmap pm;
	pm.loadFromData(svg.toUtf8(), "SVG");
	pm.setDevicePixelRatio(dpr);
	return pm;
}

} // namespace XIcons
