#pragma once

#include "json-query/json-path/JSONPathEvalError.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/internal/PathEvalCtx.hpp"

#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <expected>

namespace json_query::json_path::detail {

// ---------------------------------------------------------------------------
//  Basic evaluation helpers
// ---------------------------------------------------------------------------

// Index normalization for negative indices
int normalizeIndex(int idx, int size);

// Slice evaluation with RFC 9535 compliance
std::expected<QJsonArray, EvalError> evalSlice(const QJsonArray& array, const Slice& s);

// Token analysis helpers
bool addsMultiplicity(const Token& tk);
QJsonValue squash(QJsonArray arr, bool multi);
QJsonValue applyTrailing(json_path::FunctionType fn, const QJsonValue& v);

// ---------------------------------------------------------------------------
//  Union detection and processing helpers
// ---------------------------------------------------------------------------

struct UnionDetectionResult {
    QVector<qsizetype> unionTokens;
    bool shouldUseUnion;
    qsizetype nextIndex;
};

struct TokenProcessingResult {
    bool success;
    QJsonArray results;
    EvalError error;
};

struct KeyCollectionResult {
    QStringList keys;
    qsizetype nextIndex;
};

// Union detection micro-helpers
bool isSelectorToken(const Token& token);
QVector<qsizetype> collectConsecutiveSelectorTokens(const PathEvalCtx& ctx, qsizetype startIndex);
bool areTokensFromSameBracketGroup(const PathEvalCtx& ctx, const QVector<qsizetype>& tokenIndices);
UnionDetectionResult detectUnionTokens(const PathEvalCtx& ctx, qsizetype startIndex);

// Union processing micro-helpers
TokenProcessingResult processSingleUnionToken(
    const PathEvalCtx& ctx, 
    qsizetype tokenIdx, 
    const QJsonArray& working, 
    const QJsonValue& root);

QJsonArray mergeTokenResults(const QVector<QJsonArray>& resultArrays, const QJsonValue& root);

std::expected<QJsonArray, EvalError> processUnionTokens(
    const PathEvalCtx& ctx, 
    const QVector<qsizetype>& unionTokens, 
    const QJsonArray& working, 
    const QJsonValue& root);

// Branch selection micro-helpers
KeyCollectionResult collectKeysFromTokens(const PathEvalCtx& ctx, qsizetype startIndex);
bool objectContainsAllKeys(const QJsonObject& obj, const QStringList& keys);
void processObjectForLeafSelection(const QJsonObject& obj, const QStringList& keys, const QJsonValue& v, QJsonArray* results);
void processObjectForNonLeafSelection(const QJsonObject& obj, const QStringList& keys, const QJsonValue& v, QJsonArray* results);
QJsonArray deduplicateJsonValues(const QJsonArray& input, const QJsonValue& root);

std::expected<QJsonArray, EvalError> processBranchUniqueSelection(
    const PathEvalCtx& ctx,
    qsizetype& i,
    const QJsonArray& working,
    const QJsonValue& root,
    bool isLeaf);

} // namespace json_query::json_path::detail
