// jsonpointer.h - Updated for CTRE
#ifndef JSONPOINTER_H
#define JSONPOINTER_H

#include <QJsonValue>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QString>
#include <QStringList>
#include <QVector>
#include <optional>

class JSONPointer
{
public:
    // Constructs a JSONPointer from a string representation (e.g., "/foo/0/bar")
    explicit JSONPointer(const QString &pointer);

    // Evaluates the pointer against a JSON document
    QJsonValue evaluate(const QJsonDocument &document) const;

    // Evaluates the pointer against a JSON value
    QJsonValue evaluate(const QJsonValue &value) const;

    // Returns true if the pointer is valid
    bool isValid() const;

    // Returns the string representation of the pointer
    QString toString() const;

private:
    bool m_valid = true;
    QVector<QString> m_tokens;

    // Helper method to parse the pointer string into tokens
    void parsePointer(const QString &pointer);

    // Helper method for recursive evaluation
    QJsonValue evaluateInternal(const QJsonValue &value, int tokenIndex) const;

    // Helper method to decode a token according to RFC 6901
    static QString decodeToken(const QString &token);
};

#endif // JSONPOINTER_H
