// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT
#pragma once

// ComplianceDataGTest.hpp — shared plumbing for the data-driven compliance
// suites (RFC 6901 read/write, RFC 6902, RFC 7386, ...): JSON data-file
// loading with diagnostics, and case-name sanitizing for
// INSTANTIATE_TEST_SUITE_P. Suite-specific case parsing stays in each driver;
// pair collection with a count-guard TEST so a missing data file or submodule
// is a red build, not a silently empty suite.

#include <QFile>
#include <QJsonDocument>
#include <QString>
#include <string>

namespace compliance_framework
{

/// Load a JSON data file, warning (and returning a null document) on failure.
inline QJsonDocument loadComplianceJson(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        qWarning("Failed to open compliance test file %s", qPrintable(path));
        return {};
    }

    QJsonParseError perr{};
    const auto      doc = QJsonDocument::fromJson(f.readAll(), &perr);
    if (perr.error != QJsonParseError::NoError || doc.isNull())
        qWarning("Invalid JSON in compliance test file %s: %s", qPrintable(path), qPrintable(perr.errorString()));
    return doc;
}

/// GoogleTest-safe parameterized-test name: letters/digits only, with the
/// case index appended to guarantee uniqueness.
inline std::string sanitizeTestName(QString name, int index)
{
    for (auto& ch : name)
        if (!ch.isLetterOrNumber())
            ch = QChar('_');
    name += QStringLiteral("_") + QString::number(index);
    return name.toStdString();
}

} // namespace compliance_framework
