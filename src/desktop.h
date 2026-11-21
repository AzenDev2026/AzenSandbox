// The sandbox desktop: the user's Plasma wallpaper plus a column of toy
// app icons (like a desktop folder view).
#pragma once

#include <QWidget>

class DesktopWidget : public QWidget {
    Q_OBJECT
public:
    explicit DesktopWidget(QWidget *parent = nullptr);

signals:
    void launchRequested(const QString &desktopId);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void buildIcons();
    QPixmap wallpaper_;
};
