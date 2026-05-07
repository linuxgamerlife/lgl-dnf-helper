#include "Dnf5Process.h"

#include <QProcess>
#include <QSharedPointer>
#include <QTimer>

Dnf5Process::Dnf5Process(QObject *parent) : QObject(parent) {}

void Dnf5Process::run(const QString &program, const QStringList &arguments, int timeoutMs) {
    auto *process = new QProcess(this);
    auto *timer = new QTimer(process);
    auto completed = QSharedPointer<bool>::create(false);
    timer->setSingleShot(true);

    const QString commandLine = commandLineFor(program, arguments);
    emit started(commandLine);

    connect(timer, &QTimer::timeout, process, [process, commandLine, completed, this]() {
        if (*completed)
            return;
        *completed = true;
        process->kill();
        emit failed(commandLine, "Command timed out");
    });

    connect(process, &QProcess::errorOccurred, this, [process, timer, commandLine, completed, this](QProcess::ProcessError error) {
        if (*completed)
            return;
        *completed = true;
        timer->stop();
        QString message;
        switch (error) {
            case QProcess::FailedToStart: message = "Failed to start command"; break;
            case QProcess::Crashed: message = "Command crashed"; break;
            case QProcess::Timedout: message = "Command timed out"; break;
            case QProcess::WriteError: message = "Command write error"; break;
            case QProcess::ReadError: message = "Command read error"; break;
            case QProcess::UnknownError: message = "Unknown command error"; break;
        }
        emit failed(commandLine, message);
        process->deleteLater();
    });

    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [process, timer, commandLine, completed, this](int exitCode, QProcess::ExitStatus) {
        if (*completed) {
            process->deleteLater();
            return;
        }
        *completed = true;
        timer->stop();
        const QString stdoutText = QString::fromUtf8(process->readAllStandardOutput());
        const QString stderrText = QString::fromUtf8(process->readAllStandardError());
        emit finished(commandLine, exitCode, stdoutText, stderrText);
        process->deleteLater();
    });

    timer->start(timeoutMs);
    process->start(program, arguments);
}

QString Dnf5Process::commandLineFor(const QString &program, const QStringList &arguments) {
    QStringList parts{program};
    for (const QString &argument : arguments) {
        QString escaped = argument;
        escaped.replace("'", "'\\''");
        parts << "'" + escaped + "'";
    }
    return parts.join(" ");
}
