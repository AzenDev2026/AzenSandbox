#include "apps.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>

namespace {

QStringList desktopDirs() {
    QStringList dirs;
    dirs << "/usr/share/applications";
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    dirs << home + "/.local/share/applications";
    return dirs;
}

// Removes freedesktop field codes (%f %F %u %U %i %c %k ...) from one arg.
QString stripFieldCodes(QString arg) {
    static const QRegularExpression codes(R"(%[fFuUdDnNickvm])");
    arg.remove(codes);
    return arg;
}

} // namespace

QStringList DesktopApp::launchArgs() const {
    QStringList args = QProcess::splitCommand(exec);
    for (QString &a : args)
        a = stripFieldCodes(a);
    args.removeAll(QString());
    return args;
}

QIcon DesktopApp::themedIcon() const {
    QIcon ic = QIcon::fromTheme(icon);
    if (ic.isNull())
        ic = QIcon::fromTheme(QStringLiteral("application-x-executable"));
    return ic;
}

QVector<DesktopApp> AppList::load() {
    QVector<DesktopApp> out;
    for (const QString &dir : desktopDirs()) {
        const QDir d(dir);
        const QStringList files = d.entryList({QStringLiteral("*.desktop")}, QDir::Files);
        for (const QString &file : files) {
            const QString path = dir + "/" + file;
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            DesktopApp app;
            app.id = QFileInfo(file).completeBaseName();
            bool inEntry = false;
            while (!f.atEnd()) {
                const QString line = QString::fromUtf8(f.readLine()).trimmed();
                if (line.isEmpty() || line.startsWith('#'))
                    continue;
                if (line.startsWith('[')) {
                    inEntry = (line.startsWith("[Desktop Entry"));
                    continue;
                }
                if (!inEntry)
                    continue;
                const int eq = line.indexOf('=');
                if (eq <= 0)
                    continue;
                const QString key = line.left(eq).trimmed();
                const QString value = line.mid(eq + 1).trimmed();
                if (key == "Name" && app.name.isEmpty())
                    app.name = value;
                else if (key == "Exec")
                    app.exec = value;
                else if (key == "Icon")
                    app.icon = value;
                else if (key == "Comment")
                    app.comment = value;
                else if (key == "Terminal" && value.compare("true", Qt::CaseInsensitive) == 0)
                    app.terminal = true;
                else if ((key == "NoDisplay" || key == "Hidden") &&
                         value.compare("true", Qt::CaseInsensitive) == 0) {
                    app.name.clear(); // filtered out below
                }
            }
            f.close();
            if (app.name.isEmpty() || app.exec.isEmpty())
                continue;
            if (app.terminal) // keep it simple: no terminal wrapping in the toy
                continue;
            out.append(app);
        }
    }
    std::sort(out.begin(), out.end(),
              [](const DesktopApp &a, const DesktopApp &b) {
                  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
              });
    return out;
}

DesktopApp AppList::findByIconHint(const QString &hint) {
    const QVector<DesktopApp> all = load();
    for (const DesktopApp &a : all) {
        if (a.id == hint || a.name == hint)
            return a;
    }
    // fuzzy: the hint appears in the id
    for (const DesktopApp &a : all) {
        if (a.id.contains(hint, Qt::CaseInsensitive))
            return a;
    }
    return DesktopApp();
}
