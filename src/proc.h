// Runs a program in its own session/process group so the sandbox can kill
// the whole tree when it closes. Uses util-linux `setsid -w` as a wrapper.
#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>

class ProcessRunner : public QObject {
    Q_OBJECT
public:
    ProcessRunner(const QString &program, const QStringList &args, QObject *parent = nullptr);

    void start();
    qint64 pid() const { return pid_; }
    bool isRunning() const { return !finished_ && pid_ > 0; }
    //: SIGTERM the whole process group, SIGKILL after `msecs`.
    void terminate(int msecs = 2500);

signals:
    void started(qint64 pid);
    void exited(qint64 pid);

private:
    QProcess proc_;
    QString program_;
    QStringList args_;
    qint64 pid_ = -1;
    bool finished_ = false;
};
