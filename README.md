# xImJxstinn Vertical

Eigenes OBS-Plugin von **xImJxstinn** für den TikTok-Hochformat-Stream (9:16) –
mit eigener Vertikal-Vorschau, eigenen Szenen, Quellen und Übergängen direkt in OBS.
Twitch läuft ganz normal über OBS, TikTok über *xImJxstinn Vertical*.

## Was anders ist als beim Original
- Eigener Name und eigene Dock-Titel („xImJxstinn Vertical Szenen“, „… Quellen“)
- **Keine Werbung, kein Spenden-Knopf, keine Update-/Tracking-Abfrage** an externe Server
- **Viewer Counter** im Dock: Twitch- und TikTok-Zuschauer plus Gesamtzahl live (Daten vom MultiStream Hub)
- **💬 Chat-Knopf** im Dock → öffnet den MultiStream Hub (Twitch- + TikTok-Chat, Alerts, Overlays)
- Übernimmt bestehende Aitum-Vertical-Szenen und -Einstellungen automatisch (gleiche interne Kennungen)

## Installieren
1. **Aitum Vertical deinstallieren** (Windows → Apps → „Vertical“ / „vertical-canvas“). Beide gleichzeitig geht nicht.
2. Unter *Actions* (oder *Releases*) den neuesten Build öffnen und `…-windows-x64-Installer.exe` herunterladen.
3. Installer ausführen, OBS starten – fertig. Deine Szenen sind noch da.

## Selbst bauen
Jeder Push auf `main` baut das Plugin automatisch über GitHub Actions
(`.github/workflows/build-windows.yaml`). Mit einem Tag wie `1.0.1` entsteht ein Release mit Installer.

## Lizenz & Herkunft
Dieses Plugin basiert auf **[Aitum Vertical](https://github.com/Aitum/obs-vertical-canvas)** von Aitum
und steht wie das Original unter der **GNU General Public License v2.0** (siehe `LICENSE`).
Der Quellcode muss öffentlich bleiben. Die Original-README liegt in `README.aitum-original.md`.
