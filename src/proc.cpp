#include "proc.h"

#include <QGuiApplication>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTimer>

#include <csignal>
#include <unistd.h>

ProcessRunner::ProcessRunner(const QString &program, const QStringList &args, QObject *parent)
    : QObject(parent), program_(program), args_(args) {
    proc_.setProcessChannelMode(QProcess::SeparateChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Foreign windows can only be embedded on X11; force spawned Qt apps to
    // use the xcb backend so their windows land on the X display we watch.
    if (QGuiApplication::platformName() == QLatin1String("xcb")) {
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("xcb"));
        env.remove(QStringLiteral("WAYLAND_DISPLAY"));
    }
    proc_.setProcessEnvironment(env);
    connect(&proc_, &QProcess::started, this, [this]() {
        pid_ = proc_.processId();
        emit started(pid_);
    });
    connect(&proc_, &QProcess::finished, this, [this](int, QProcess::ExitStatus) {
        finished_ = true;
        emit exited(pid_);
    });
}

void ProcessRunner::start() {
    const QString setsid = QStandardPaths::findExecutable(QStringLiteral("setsid"));
    if (!setsid.isEmpty()) {
        proc_.setProgram(setsid);
        QStringList full = {QStringLiteral("-w"), program_};
        full.append(args_);
        proc_.setArguments(full);
    } else {
        proc_.setProgram(program_);
        proc_.setArguments(args_);
    }
    proc_.start();
}

void ProcessRunner::terminate(int msecs) {
    if (!isRunning())
        return;
    ::kill(static_cast<pid_t>(-pid_), SIGTERM);
    QTimer::singleShot(msecs, this, [this]() {
        if (isRunning())
            ::kill(static_cast<pid_t>(-pid_), SIGKILL);
    });
}
