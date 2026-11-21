// Azen Sandbox — a tiny toy desktop inside your real one.
// Theme: reads the user's KDE Plasma look (colors, font, wallpaper) and
// falls back to a built-in dark palette when the files are missing.
#pragma once

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QString>

struct Theme {
    QColor windowBg{30, 30, 30};     // [Colors:Window] BackgroundNormal
    QColor viewBg{31, 31, 31};       // [Colors:View] BackgroundNormal
    QColor buttonBg{68, 68, 68};     // [Colors:Button] BackgroundNormal
    QColor text{250, 250, 250};      // ForegroundNormal
    QColor textDim{136, 136, 136};   // ForegroundInactive
    QColor accent{91, 156, 188};     // Selection / accent
    QColor accentHover{75, 200, 255};
    QColor panelBg{32, 32, 32};      // Plasma panel colorizer (#202020)
    QColor border{15, 15, 15};
    QColor closeHover{218, 68, 83};
    QString fontFamily = QStringLiteral("Noto Sans");
    int fontSize = 11;
    QString wallpaperPath;

    static Theme load();
    QFont font(int delta = 0) const;
};

//: The single theme instance, filled by Theme::load() in main().
extern Theme gTheme;
