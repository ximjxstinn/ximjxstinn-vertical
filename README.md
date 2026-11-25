# xImJxstinn Vertical

Eigenes OBS-Plugin von **xImJxstinn** für den TikTok-Hochformat-Stream (9:16).
Twitch läuft ganz normal über OBS, TikTok gleichzeitig über *xImJxstinn Vertical*.

## Funktionen
- **Hochformat-Leinwand** (1080×1920 oder 720×1280) direkt in OBS
- **Drei Docks:** Vorschau, Szenen, Quellen
- **Vorschau bearbeiten:** klicken = auswählen, ziehen = verschieben, Mausrad = Größe, Pfeiltasten = fein verschieben
- **Schnell-Layouts:** ganzes Bild, füllen, obere/untere Hälfte, Facecam-Drittel oben + Gameplay unten
- **Eigener TikTok-Stream** mit eigenem Key, Bitrate und Encoder (NVENC automatisch)
- **Startet automatisch mit „Stream starten“** in OBS
- **Szenen wechseln automatisch mit:** OBS-Szene „Gaming Facecam“ → Hochformat-Szene „Gaming Facecam TT“
- **Viewer Counter** (Twitch + TikTok + Gesamt) und **💬 Chat-Knopf** über den MultiStream Hub
- Übernimmt bestehende Hochformat-Szenen aus einer vorhandenen Aitum-Vertical-Leinwand automatisch

## Installieren
1. OBS schließen.
2. Unter **Actions** (oder **Releases**) den neuesten Build öffnen und den Installer
   `ximjxstinn-vertical-…-windows-installer.exe` herunterladen und ausführen.
3. OBS starten → **Docks** → „xImJxstinn Vertical“, „xImJxstinn Szenen“, „xImJxstinn Quellen“ einblenden.

Der Installer entfernt ein altes Aitum Vertical bzw. xImJxstinn Vertical 1.x automatisch.

## Bauen
Jeder Push auf `main` baut das Plugin über GitHub Actions (`.github/workflows/build-windows.yaml`).
Wird ein Release veröffentlicht, hängt GitHub den Installer automatisch an.

## Lizenz
GPL-2.0-or-later © 2026 xImJxstinn. Build-System basiert auf der offiziellen
[OBS-Plugin-Vorlage](https://github.com/obsproject/obs-plugintemplate).
