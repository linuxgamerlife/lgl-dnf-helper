#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class Dnf5Process : public QObject {
    Q_OBJECT

public:
    explicit Dnf5Process(QObject *parent = nullptr);

    void run(const QString &program, const QStringList &arguments, int timeoutMs = 30000);

signals:
    void started(const QString &commandLine);
    void finished(const QString &commandLine, int exitCode, const QString &stdoutText, const QString &stderrText);
    void failed(const QString &commandLine, const QString &message);

private:
    static QString commandLineFor(const QString &program, const QStringList &arguments);
};
