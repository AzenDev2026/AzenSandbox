#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QIcon>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>
#include <QUrl>

Theme gTheme;

namespace {

// Minimal INI reader: returns a map of "Section/Key" -> value.
QHash<QString, QString> readIni(const QString &path) {
    QHash<QString, QString> out;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return out;
    QString section;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(';'))
            continue;
        if (line.startsWith('[') && line.endsWith(']')) {
            section = line.mid(1, line.size() - 2);
            continue;
        }
        const int eq = line.indexOf('=');
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();
        out.insert(section + '/' + key, value);
    }
    return out;
}

QColor parseColor(const QString &csv, const QColor &fallback) {
    const QStringList parts = csv.split(',');
    if (parts.size() >= 3) {
        bool ok = true;
        const int r = parts[0].trimmed().toInt(&ok);
        const int g = parts[1].trimmed().toInt(&ok);
        const int b = parts[2].trimmed().toInt(&ok);
        if (ok)
            return QColor(r, g, b);
    }
    return fallback;
}

QString firstExistingDir(const QStringList &candidates) {
    for (const QString &c : candidates) {
        if (QFileInfo(c).isDir())
            return c;
    }
    return QString();
}

} // namespace

QFont Theme::font(int delta) const {
    QFont f(fontFamily);
    f.setPointSize(fontSize + delta);
    return f;
}

Theme Theme::load() {
    Theme t;

    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QHash<QString, QString> kdeglobals = readIni(home + "/.config/kdeglobals");

    t.windowBg = parseColor(kdeglobals.value("Colors:Window/BackgroundNormal"), t.windowBg);
    t.viewBg = parseColor(kdeglobals.value("Colors:View/BackgroundNormal"), t.viewBg);
    t.buttonBg = parseColor(kdeglobals.value("Colors:Button/BackgroundNormal"), t.buttonBg);
    t.text = parseColor(kdeglobals.value("Colors:View/ForegroundNormal"), t.text);
    t.textDim = parseColor(kdeglobals.value("Colors:Window/ForegroundInactive"), t.textDim);
    t.accent = parseColor(kdeglobals.value("Colors:Selection/BackgroundNormal"), t.accent);
    t.accentHover = parseColor(kdeglobals.value("Colors:Button/DecorationHover"), t.accentHover);
    t.panelBg = parseColor(kdeglobals.value("Colors:Window/BackgroundNormal"), t.panelBg);

    // Font: "Noto Sans CJK SC Medium,12,-1,5,500,..."
    const QString fontSpec = kdeglobals.value("General/font");
    if (!fontSpec.isEmpty()) {
        const QStringList parts = fontSpec.split(',');
        if (!parts.isEmpty() && !parts[0].trimmed().isEmpty()) {
            // Drop the trailing " Medium" weight word, keep the family.
            QString family = parts[0].trimmed();
            const QRegularExpression weightRx(R"(\s+(Medium|Regular|Bold|Light|Thin|SemiBold|DemiBold)$)",
                                             QRegularExpression::CaseInsensitiveOption);
            family.remove(weightRx);
            t.fontFamily = family;
        }
        if (parts.size() >= 2) {
            bool ok = false;
            const int size = parts[1].trimmed().toInt(&ok);
            if (ok && size > 0)
                t.fontSize = size;
        }
    }

    // Icon theme: try the KDE value, then the themes the user has installed.
    QStringList iconCandidates;
    const QString kdeIconTheme = kdeglobals.value("Icons/Theme");
    if (!kdeIconTheme.isEmpty())
        iconCandidates << kdeIconTheme;
    iconCandidates << QStringLiteral("WhiteSur") << QStringLiteral("Windows-Eleven")
                   << QStringLiteral("Fluent") << QStringLiteral("breeze-dark")
                   << QStringLiteral("breeze") << QStringLiteral("hicolor");
    const QStringList iconRoots = {home + "/.local/share/icons", "/usr/share/icons",
                                   home + "/.icons"};
    for (const QString &name : iconCandidates) {
        bool found = false;
        for (const QString &root : iconRoots) {
            if (QFileInfo(root + "/" + name).isDir()) {
                QIcon::setThemeName(name);
                found = true;
                break;
            }
        }
        if (found)
            break;
    }

    // Wallpaper: first Image= entry in the Plasma desktop applets config.
    const QHash<QString, QString> applets =
        readIni(home + "/.config/plasma-org.kde.plasma.desktop-appletsrc");
    for (auto it = applets.constBegin(); it != applets.constEnd(); ++it) {
        if (it.key().endsWith("/Image") && it.value().startsWith("file://")) {
            const QFileInfo info(QUrl(it.value()).toLocalFile());
            if (info.isFile())
                t.wallpaperPath = info.absoluteFilePath();
            break;
        }
    }

    return t;
}
