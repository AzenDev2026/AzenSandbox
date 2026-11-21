// Desktop apps: parses .desktop files and turns them into launchable entries.
#pragma once

#include <QIcon>
#include <QString>
#include <QStringList>
#include <QVector>

struct DesktopApp {
    QString id;      // .desktop file base name
    QString name;
    QString exec;
    QString icon;
    QString comment;
    bool terminal = false;

    QStringList launchArgs() const;
    QIcon themedIcon() const;
};

namespace AppList {
QVector<DesktopApp> load();
DesktopApp findByIconHint(const QString &hint); // fallback lookup
} // namespace AppList
