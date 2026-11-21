// The toy start menu: search box, app grid, a Windows (Wine) section and a
// shut-down button. Modelled on the Plasma launcher look.
#pragma once

#include <QWidget>

class QGridLayout;
class QLineEdit;
class QLabel;

class StartMenu : public QWidget {
    Q_OBJECT
public:
    explicit StartMenu(QWidget *parent = nullptr);

    void popupAt(const QPoint &globalPos);
    void setWineAvailable(bool available, const QString &version);
    bool wineAvailable() const { return wineAvailable_; }

signals:
    void appLaunchRequested(const QString &desktopId);
    void wineAppRequested(const QString &exe);
    void wineExeFileRequested();
    void shutdownRequested();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void rebuild(const QString &filter);

    QLineEdit *search_ = nullptr;
    QWidget *scrollContent_ = nullptr;
    QGridLayout *grid_ = nullptr;
    QLabel *wineLabel_ = nullptr;
    bool wineAvailable_ = false;
    QString wineVersion_;
};
