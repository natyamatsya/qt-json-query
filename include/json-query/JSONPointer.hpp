#pragma once

#include <QJsonValue>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

// Primary JSONPointer header (preferred .hpp suffix).
// Implementation remains in src/JSONPointer.cpp.

class JSONPointer
{
public:
    explicit JSONPointer(const QString &pointer);

    QJsonValue evaluate(const QJsonDocument &document) const;
    QJsonValue evaluate(const QJsonValue &value) const;

    bool isValid() const;
    QString toString() const;

private:
    bool m_valid = true;
    QVector<QString> m_tokens;

    void parsePointer(const QString &pointer);
    QJsonValue evaluateInternal(const QJsonValue &value, int tokenIndex) const;
    static QString decodeToken(const QString &token);
};
