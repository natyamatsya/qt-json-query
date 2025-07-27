#include "json-query/json-path/JSONPathBracketRules.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathParseUtils.hpp"
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/json-path/JSONPathFilter.hpp"
#include "json-query/json-path/JSONPathHelpers.hpp"
#include "json-query/json-path/internal/QtHash.hpp"

#include <QDebug>
#include <QJsonValue>

namespace json_query::json_path::detail
{

// ──────────────────────────────────────────────────────────────────────
//  BracketSink Implementation
// ──────────────────────────────────────────────────────────────────────

std::expected<void, Error> BracketSink::key(QString key, bool allow)
{
    // Create token with bracket group ID
    if (!allow && key.contains(u' '))
        return std::unexpected(Error::BlankInKey);
    Token t{Token::Kind::Key, 0, {}, qt_hash(key), key};
    t.bracketGroupId = currentBracketGroupId;
    tk.emplace_back(std::move(t));
    return {};
}

void BracketSink::keyList(const QVector<QString>& keys)
{
    if (keys.isEmpty()) return;

    Token t;
    t.kind = Token::Kind::KeyList;
    t.bracketGroupId = currentBracketGroupId; // Set bracket group ID

    // Pack the keys into a single QString separated by '\n' so that the
    // evaluator can split them later without ambiguity.
    QStringList list;
    list.reserve(keys.size());
    for (const QString& k : keys)
        list.append(k);

    t.key = list.join(u"\n");
    tk.emplace_back(std::move(t));
}

void BracketSink::wild()
{
    Token t{Token::Kind::Wildcard};
    t.bracketGroupId = currentBracketGroupId;
    tk.emplace_back(t);
}

void BracketSink::slice(const Slice& s)
{
    Token t{Token::Kind::Slice, 0, s, 0u};
    t.bracketGroupId = currentBracketGroupId;
    tk.emplace_back(t);
}

void BracketSink::index(int i)
{
    qCDebug(jsonPathLog) << "BracketSink::index emit" << i;
    Token t{Token::Kind::Index, i};
    t.bracketGroupId = currentBracketGroupId;
    tk.emplace_back(t);
}

void BracketSink::pushFilter(const Token& t)
{
    Token token = t;  // Copy the token
    token.bracketGroupId = currentBracketGroupId; // Set bracket group ID
    tk.emplace_back(std::move(token));
}

// ──────────────────────────────────────────────────────────────────────
//  EmbeddedBracketSink Implementation (Zero-Overhead)
// ──────────────────────────────────────────────────────────────────────

std::expected<void, Error> EmbeddedBracketSink::key(QString key, bool allow)
{
    // Create token with bracket group ID (same logic as legacy BracketSink)
    if (!allow && key.contains(u' '))
        return std::unexpected(Error::BlankInKey);
    Token t{Token::Kind::Key, 0, {}, qt_hash(key), key};
    t.bracketGroupId = currentBracketGroupId;
    tk.emplace_back(std::move(t));
    return {};
}

void EmbeddedBracketSink::keyList(const QVector<QString>& keys)
{
    if (keys.isEmpty()) return;

    Token t;
    t.kind = Token::Kind::KeyList;
    t.bracketGroupId = currentBracketGroupId; // Set bracket group ID

    // Pack the keys into a single QString separated by '\n' so that the
    // evaluator can split them later without ambiguity.
    QStringList list;
    list.reserve(keys.size());
    for (const QString& k : keys)
        list.append(k);

    t.key = list.join(u"\n");
    tk.emplace_back(std::move(t));
}

void EmbeddedBracketSink::wild()
{
    Token t{Token::Kind::Wildcard};
    t.bracketGroupId = currentBracketGroupId; // Set bracket group ID
    tk.emplace_back(t);
}

void EmbeddedBracketSink::slice(const Slice& s)
{
    Token t{Token::Kind::Slice, 0, s};
    t.bracketGroupId = currentBracketGroupId; // Set bracket group ID
    tk.emplace_back(t);
}

void EmbeddedBracketSink::index(int i)
{
    Token t{Token::Kind::Index, i};
    t.bracketGroupId = currentBracketGroupId; // Set bracket group ID
    tk.emplace_back(t);
}

void EmbeddedBracketSink::pushFilter(const Token& t)
{
    Token token = t;  // Copy the token
    token.bracketGroupId = currentBracketGroupId; // Set bracket group ID
    tk.emplace_back(std::move(token));
}

// ──────────────────────────────────────────────────────────────────────
//  Rule Matcher Functions Implementation
// ──────────────────────────────────────────────────────────────────────

namespace matchers {

bool matchesUnionComma(QStringView content)
{
    // Only match if there are top-level commas (not inside parentheses or brackets)
    int parenDepth = 0;
    int bracketDepth = 0;
    
    for (qsizetype i = 0; i < content.size(); ++i) {
        const QChar c = content[i];
        if      (c == u'(') ++parenDepth;
        else if (c == u')') --parenDepth;
        else if (c == u'[') ++bracketDepth;
        else if (c == u']') --bracketDepth;
        else if (c == u',' && parenDepth == 0 && bracketDepth == 0) {
            return true; // Found top-level comma
        }
    }
    
    return false; // No top-level commas found
}

bool matchesWildcard(QStringView content)
{
    return content.trimmed() == u"*";
}

bool matchesSingleIndex(QStringView content)
{
    return isValidIndexLiteral(content);
}

bool matchesIndexList(QStringView content)
{
    if (!content.contains(u',')) return false;
    
    const auto parts = content.split(u',');
    if (parts.size() < 2) return false;
    
    for (const auto& part : parts) {
        if (!isValidIndexLiteral(part)) return false;
    }
    return true;
}

bool matchesSlice(QStringView content)
{
    return content.contains(u':') && makeSlice(content).has_value();
}

bool matchesFilterWithParens(QStringView content)
{
    // Match any filter expression that starts with ?( - it may have additional content after )
    return content.startsWith(u"?(");
}

bool matchesFilterWithoutParens(QStringView content)
{
    return content.startsWith(u'?') && !content.startsWith(u"?(");
}

bool matchesPlaceholder(QStringView content)
{
    QStringView trimmed = content.trimmed();
    if (trimmed == u"?") return true;
    
    // Check for multiple placeholders separated by commas
    const auto parts = trimmed.split(u',');
    for (const auto& part : parts) {
        if (part.trimmed() != u"?") return false;
    }
    return true;
}

bool matchesQuotedKey(QStringView content)
{
    QStringView trimmed = content.trimmed();
    return (trimmed.startsWith(u'\'') && trimmed.endsWith(u'\'')) ||
           (trimmed.startsWith(u'"') && trimmed.endsWith(u'"'));
}

bool matchesUnquotedKey(QStringView /*content*/)
{
    // Unquoted keys are forbidden by RFC 9535
    return true; // This will be the fallback that returns an error
}

} // namespace matchers

// ──────────────────────────────────────────────────────────────────────
//  Rule Handler Functions Implementation
// ──────────────────────────────────────────────────────────────────────

namespace handlers {

std::expected<void, Error> handleWildcard(QStringView /*content*/, BracketSink& out)
{
    out.wild();
    return {};
}

std::expected<void, Error> handleSingleIndex(QStringView content, BracketSink& out)
{
    QStringView trimmed = content.trimmed();
    qCDebug(jsonPathLog) << "BR_RULE index-single check" << trimmed.toString();
    
    if (!isValidIndexLiteral(trimmed)) {
        qCDebug(jsonPathLog) << "handleSingleIndex: invalid index literal" << trimmed;
        return std::unexpected(Error::InvalidSlice);
    }
    
    bool ok = false;
    qlonglong val = trimmed.toLongLong(&ok);
    if (!ok) {
        qCDebug(jsonPathLog) << "handleSingleIndex: failed to parse" << trimmed;
        return std::unexpected(Error::InvalidSlice);
    }
    
    int idx = (val > std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max()
             : (val < std::numeric_limits<int>::min()) ? std::numeric_limits<int>::min()
             : static_cast<int>(val);
    
    qCDebug(jsonPathLog) << "  emitting index token" << idx;
    out.index(idx);
    return {};
}

std::expected<void, Error> handleIndexList(QStringView content, BracketSink& out)
{
    qCDebug(jsonPathLog) << "BR_RULE index-list raw" << content.toString();
    
    const auto parts = content.split(u',');
    for (QStringView p : parts) {
        QStringView t = p.trimmed();
        
        if (!isValidIndexLiteral(t)) {
            qCDebug(jsonPathLog) << "handleIndexList: invalid index literal" << t;
            return std::unexpected(Error::InvalidSlice);
        }
        
        bool ok = false;
        qlonglong val = t.toLongLong(&ok);
        if (!ok) return std::unexpected(Error::InvalidSlice);
        
        int idx = (val > std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max()
                 : (val < std::numeric_limits<int>::min()) ? std::numeric_limits<int>::min()
                 : static_cast<int>(val);
        
        qCDebug(jsonPathLog) << "  list element emit" << idx;
        out.index(idx);
    }
    return {};
}

std::expected<void, Error> handleSlice(QStringView content, BracketSink& out)
{
    auto slice = makeSlice(content);
    if (!slice) {
        return std::unexpected(Error::InvalidSlice);
    }
    out.slice(*slice);
    return {};
}

std::expected<void, Error> handleFilterWithParens(QStringView content, BracketSink& out)
{
    QStringView trimmed = content.trimmed();
    // Extract the full expression after the '?' prefix
    QString expr = trimmed.mid(1).toString(); // Remove '?' prefix
    
    qCDebug(jsonPathLog) << "handleFilterWithParens: processing expression" << expr;
    
    // Try embedded filter compilation first (zero-overhead)
    if (auto token = compileEmbeddedContextFilter(expr)) {
        out.pushFilter(*token);
        return {};
    }
    
    // Fall back to legacy compilation for backward compatibility
    if (auto token = compileContextFilter(expr, out.contextFilters, out.filters)) {
        out.pushFilter(*token);
        return {};
    } else {
        qCDebug(jsonPathLog) << "handleFilterWithParens: failed to compile filter" << expr;
        return std::unexpected(Error::UnsupportedFilter);
    }
}

std::expected<void, Error> handleFilterWithoutParens(QStringView content, BracketSink& out)
{
    QStringView trimmed = content.trimmed();
    // Extract the full expression after the '?' prefix
    QString expr = trimmed.mid(1).toString(); // Remove '?' prefix
    
    qCDebug(jsonPathLog) << "handleFilterWithoutParens: processing expression" << expr;
    
    // Try embedded filter compilation first (zero-overhead)
    if (auto token = compileEmbeddedContextFilter(expr)) {
        out.pushFilter(*token);
        return {};
    }
    
    // Fall back to legacy compilation for backward compatibility
    if (auto token = compileContextFilter(expr, out.contextFilters, out.filters)) {
        out.pushFilter(*token);
        return {};
    } else {
        qCDebug(jsonPathLog) << "handleFilterWithoutParens: failed to compile filter" << expr;
        return std::unexpected(Error::UnsupportedFilter);
    }
}

std::expected<void, Error> handlePlaceholder(QStringView content, BracketSink& out)
{
    QStringView trimmed = content.trimmed();
    
    if (trimmed == u"?") {
        // Single placeholder - create a filter that always returns true
        FilterFn alwaysTrue = [](const QJsonValue&) { return true; };
        out.filters.append(alwaysTrue);
        
        Token filterToken;
        filterToken.kind = Token::Kind::Filter;
        filterToken.filterId = out.filters.size() - 1;
        out.pushFilter(filterToken);
        return {};
    }
    
    // Multiple placeholders - handle each one
    const auto parts = trimmed.split(u',');
    for (const auto& part : parts) {
        if (part.trimmed() == u"?") {
            FilterFn alwaysTrue = [](const QJsonValue&) { return true; };
            out.filters.append(alwaysTrue);
            
            Token filterToken;
            filterToken.kind = Token::Kind::Filter;
            filterToken.filterId = out.filters.size() - 1;
            out.pushFilter(filterToken);
        }
    }
    return {};
}

std::expected<void, Error> handleQuotedKey(QStringView content, BracketSink& out)
{
    QStringView trimmed = content.trimmed();
    
    if (trimmed.size() < 2) {
        return std::unexpected(Error::InvalidSlice);
    }
    
    QChar quote = trimmed.front();
    QStringView keyContent = trimmed.mid(1, trimmed.size() - 2);
    
    QuoteStyle style = (quote == u'\'') ? QuoteStyle::Single : QuoteStyle::Double;
    if (!isValidQuotedKey(keyContent, style)) {
        return std::unexpected(Error::InvalidSlice);
    }
    
    QString unescapedKey = unescapeQuotedKey(keyContent);
    return out.key(unescapedKey, true); // Allow spaces in quoted keys
}

std::expected<void, Error> handleUnquotedKey(QStringView /*content*/, BracketSink& /*out*/)
{
    // Unquoted keys are forbidden by RFC 9535
    return std::unexpected(Error::UnsupportedFilter);
}

// Forward declaration for union handler
std::expected<void, Error> handleUnionComma(QStringView content, BracketSink& out)
{
    auto segmentsResult = splitTopLevelMultiple(content, QLatin1StringView(","));
    if (!segmentsResult) {
        qCDebug(jsonPathLog) << "handleUnionComma: failed to split content";
        return std::unexpected(Error::UnsupportedFilter);
    }
    
    const auto& segments = *segmentsResult;
    qCDebug(jsonPathLog) << "handleUnionComma: split into" << segments.size() << "segments";
    
    for (const QString& segment : segments) {
        QStringView segmentView(segment);
        QStringView trimmedSegment = segmentView.trimmed(); // Trim whitespace from each segment
        qCDebug(jsonPathLog) << "handleUnionComma: processing segment" << trimmedSegment.toString();
        
        // Use dispatcher to process each segment, but exclude union rule to prevent recursion
        auto result = BracketRuleDispatcher::processSegmentExcludingUnion(trimmedSegment, out);
        if (!result) {
            qCDebug(jsonPathLog) << "handleUnionComma: failed to process segment" << trimmedSegment.toString();
            return result;
        }
    }
    
    qCDebug(jsonPathLog) << "handleUnionComma: successfully processed all segments";
    return {};
}

} // namespace handlers

// ──────────────────────────────────────────────────────────────────────
//  Embedded Rule Handler Functions Implementation (Zero-Overhead)
// ──────────────────────────────────────────────────────────────────────

namespace embedded_handlers {

std::expected<void, Error> handleWildcard(QStringView /*content*/, EmbeddedBracketSink& out)
{
    out.wild();
    return {};
}

std::expected<void, Error> handleSingleIndex(QStringView content, EmbeddedBracketSink& out)
{
    QStringView trimmed = content.trimmed();
    
    if (!isValidIndexLiteral(trimmed.toString())) {
        qCDebug(jsonPathLog) << "handleSingleIndex: invalid index literal" << trimmed;
        return std::unexpected(Error::InvalidIndex);
    }
    
    bool ok = false;
    // Use toLongLong to handle large integers like ±9007199254740991 (JavaScript MIN/MAX_SAFE_INTEGER)
    qint64 index = trimmed.toString().toLongLong(&ok);
    if (!ok) {
        qCDebug(jsonPathLog) << "handleSingleIndex: failed to convert to long long" << trimmed;
        return std::unexpected(Error::InvalidIndex);
    }
    
    // Convert to int for the token (with range checking)
    if (index < INT_MIN || index > INT_MAX) {
        // For very large indices, they will be out of range for any practical array
        // but RFC 9535 requires us to accept them and let evaluation handle the bounds
        out.index(index > 0 ? INT_MAX : INT_MIN);
    } else {
        out.index(static_cast<int>(index));
    }
    return {};
}

std::expected<void, Error> handleIndexList(QStringView content, EmbeddedBracketSink& out)
{
    QStringView trimmed = content.trimmed();
    const auto parts = trimmed.split(u',');
    
    for (const auto& part : parts) {
        QStringView indexStr = part.trimmed();
        if (!isValidIndexLiteral(indexStr.toString())) {
            return std::unexpected(Error::InvalidIndex);
        }
        
        bool ok = false;
        int index = indexStr.toString().toInt(&ok);
        if (!ok) {
            return std::unexpected(Error::InvalidIndex);
        }
        
        out.index(index);
    }
    return {};
}

std::expected<void, Error> handleSlice(QStringView content, EmbeddedBracketSink& out)
{
    auto slice = makeSlice(content.toString());
    if (!slice) {
        return std::unexpected(Error::InvalidSlice);
    }
    out.slice(*slice);
    return {};
}

std::expected<void, Error> handleFilterWithParens(QStringView content, EmbeddedBracketSink& out)
{
    QStringView trimmed = content.trimmed();
    // Extract the full expression after the '?' prefix
    QString expr = trimmed.mid(1).toString(); // Remove '?' prefix
    
    qCDebug(jsonPathLog) << "handleFilterWithParens: processing expression" << expr;
    
    // Use embedded filter compilation only (zero-overhead)
    if (auto token = compileEmbeddedContextFilter(expr)) {
        out.pushFilter(*token);
        return {};
    } else {
        qCDebug(jsonPathLog) << "handleFilterWithParens: failed to compile embedded filter" << expr;
        return std::unexpected(Error::UnsupportedFilter);
    }
}

std::expected<void, Error> handleFilterWithoutParens(QStringView content, EmbeddedBracketSink& out)
{
    QStringView trimmed = content.trimmed();
    // Extract the full expression after the '?' prefix
    QString expr = trimmed.mid(1).toString(); // Remove '?' prefix
    
    qCDebug(jsonPathLog) << "handleFilterWithoutParens: processing expression" << expr;
    
    // Use embedded filter compilation only (zero-overhead)
    if (auto token = compileEmbeddedContextFilter(expr)) {
        out.pushFilter(*token);
        return {};
    } else {
        qCDebug(jsonPathLog) << "handleFilterWithoutParens: failed to compile embedded filter" << expr;
        return std::unexpected(Error::UnsupportedFilter);
    }
}

std::expected<void, Error> handlePlaceholder(QStringView content, EmbeddedBracketSink& out)
{
    QStringView trimmed = content.trimmed();
    
    if (trimmed == u"?") {
        // Single placeholder - create an embedded filter that always returns true
        Token filterToken;
        filterToken.kind = Token::Kind::Filter;
        filterToken.key = "?";
        
        filterToken.embedFilter([](const QJsonValue&) { return true; });
        
        out.pushFilter(filterToken);
        return {};
    }
    
    // Multiple placeholders - handle each one
    const auto parts = trimmed.split(u',');
    for (const auto& part : parts) {
        if (part.trimmed() == u"?") {
            Token filterToken;
            filterToken.kind = Token::Kind::Filter;
            filterToken.key = "?";
            
            filterToken.embedFilter([](const QJsonValue&) { return true; });
            
            out.pushFilter(filterToken);
        }
    }
    return {};
}

std::expected<void, Error> handleQuotedKey(QStringView content, EmbeddedBracketSink& out)
{
    QStringView trimmed = content.trimmed();
    
    if (trimmed.size() < 2) {
        return std::unexpected(Error::InvalidSlice);
    }
    
    QChar quote = trimmed.front();
    QStringView keyContent = trimmed.mid(1, trimmed.size() - 2);
    
    QuoteStyle style = (quote == u'\'') ? QuoteStyle::Single : QuoteStyle::Double;
    if (!isValidQuotedKey(keyContent, style)) {
        return std::unexpected(Error::InvalidSlice);
    }
    
    QString unescapedKey = unescapeQuotedKey(keyContent);
    return out.key(unescapedKey, true); // Allow spaces in quoted keys
}

std::expected<void, Error> handleUnquotedKey(QStringView /*content*/, EmbeddedBracketSink& /*out*/)
{
    // Unquoted keys are forbidden by RFC 9535
    return std::unexpected(Error::InvalidIdentifier);
}

// Helper function to split union segments
QVector<QString> splitUnionSegments(QStringView content)
{
    // Use the existing splitTopLevelMultiple function for consistency
    auto result = splitTopLevelMultiple(content, QLatin1StringView(","));
    if (result) {
        return *result;
    }
    // Fallback: simple split if complex parsing fails
    auto segments = content.split(u',');
    QVector<QString> stringSegments;
    stringSegments.reserve(segments.size());
    for (const auto& segment : segments) {
        stringSegments.append(segment.toString());
    }
    return stringSegments;
}

// Forward declaration for embedded union handler
std::expected<void, Error> handleUnionComma(QStringView content, EmbeddedBracketSink& out)
{
    const auto segments = splitUnionSegments(content);
    
    for (const auto& segment : segments) {
        // Trim each segment before processing (like legacy implementation)
        auto trimmedSegment = QStringView(segment).trimmed();
        auto result = EmbeddedBracketRuleDispatcher::processSegmentExcludingUnion(trimmedSegment, out);
        if (!result) {
            return result;
        }
    }
    return {};
}

} // namespace embedded_handlers

// ──────────────────────────────────────────────────────────────────────
//  BracketRuleDispatcher Implementation
// ──────────────────────────────────────────────────────────────────────

std::vector<BracketRuleMetadata> BracketRuleDispatcher::createRules()
{
    return {
        {
            .name = "union_comma",
            .priority = 1300,  // Highest priority - must split unions before processing individual elements
            .matcher = matchers::matchesUnionComma,
            .handler = handlers::handleUnionComma,
            .description = "Union of multiple selectors separated by commas (e.g., '1,2,3' or '?@.a,?@.b')"
        },
        {
            .name = "filter_with_parens",
            .priority = 1200,  // High priority for filter expressions
            .matcher = matchers::matchesFilterWithParens,
            .handler = handlers::handleFilterWithParens,
            .description = "Filter expression with parentheses (e.g., '?(@.a == 1)')"
        },
        {
            .name = "filter_without_parens",
            .priority = 1100,
            .matcher = matchers::matchesFilterWithoutParens,
            .handler = handlers::handleFilterWithoutParens,
            .description = "Filter expression without parentheses (e.g., '?@.a')"
        },
        {
            .name = "wildcard",
            .priority = 900,
            .matcher = matchers::matchesWildcard,
            .handler = handlers::handleWildcard,
            .description = "Wildcard selector (*)"
        },
        {
            .name = "index_list",
            .priority = 850,
            .matcher = matchers::matchesIndexList,
            .handler = handlers::handleIndexList,
            .description = "Comma-separated index list (e.g., '1,2,3')"
        },
        {
            .name = "single_index",
            .priority = 800,
            .matcher = matchers::matchesSingleIndex,
            .handler = handlers::handleSingleIndex,
            .description = "Single array index (e.g., '123')"
        },
        {
            .name = "slice",
            .priority = 700,
            .matcher = matchers::matchesSlice,
            .handler = handlers::handleSlice,
            .description = "Array slice (e.g., '1:3:2')"
        },
        {
            .name = "placeholder",
            .priority = 500,
            .matcher = matchers::matchesPlaceholder,
            .handler = handlers::handlePlaceholder,
            .description = "Placeholder filter (e.g., '?' or '?,?')"
        },
        {
            .name = "quoted_key",
            .priority = 400,
            .matcher = matchers::matchesQuotedKey,
            .handler = handlers::handleQuotedKey,
            .description = "Quoted string key (e.g., \"'key'\" or '\"key\"')"
        },
        {
            .name = "unquoted_key",
            .priority = 100,
            .matcher = matchers::matchesUnquotedKey,
            .handler = handlers::handleUnquotedKey,
            .description = "Unquoted key (forbidden by RFC 9535)"
        }
    };
}

const std::vector<BracketRuleMetadata>& BracketRuleDispatcher::getRules()
{
    static const auto rules = createRules();
    return rules;
}

std::expected<void, Error> BracketRuleDispatcher::dispatch(QStringView content, BracketSink& sink)
{
    qCDebug(jsonPathLog) << "BracketRuleDispatcher::dispatch content=" << content.toString();
    
    // Apply rules in priority order using monadic error handling
    for (const auto& rule : getRules()) {
        qCDebug(jsonPathLog) << "Trying rule:" << rule.name << "(priority:" << rule.priority << ")";
        
        if (rule.matcher(content)) {
            qCDebug(jsonPathLog) << "Rule" << rule.name << "matched, applying handler";
            auto result = rule.handler(content, sink);
            
            if (result) {
                qCDebug(jsonPathLog) << "Rule" << rule.name << "succeeded";
                return {};
            } else {
                qCDebug(jsonPathLog) << "Rule" << rule.name << "failed with error" << static_cast<int>(result.error());
                return result;
            }
        } else {
            qCDebug(jsonPathLog) << "Rule" << rule.name << "did not match";
        }
    }
    
    qCDebug(jsonPathLog) << "No rules matched, returning UnsupportedFilter";
    return std::unexpected(Error::UnsupportedFilter);
}

std::expected<void, Error> BracketRuleDispatcher::processSegmentExcludingUnion(QStringView content, BracketSink& sink)
{
    // Apply all rules except union_comma to prevent recursion
    for (const auto& rule : getRules()) {
        if (std::string_view(rule.name) == "union_comma") continue; // Skip union rule
        
        if (rule.matcher(content)) {
            return rule.handler(content, sink);
        }
    }
    return std::unexpected(Error::UnsupportedFilter);
}

const BracketRuleMetadata* BracketRuleDispatcher::findRuleByName(const char* name)
{
    for (const auto& rule : getRules()) {
        if (std::string_view(rule.name) == name) {
            return &rule;
        }
    }
    return nullptr;
}

} // namespace json_query::json_path::detail

// ──────────────────────────────────────────────────────────────────────
//  EmbeddedBracketRuleDispatcher Implementation (Zero-Overhead)
// ──────────────────────────────────────────────────────────────────────

namespace json_query::json_path::detail {

std::vector<EmbeddedBracketRuleMetadata> EmbeddedBracketRuleDispatcher::createRules()
{
    return {
        {
            .name = "union_comma",
            .priority = 1300,  // Highest priority - must split unions before processing individual elements
            .matcher = matchers::matchesUnionComma,
            .handler = embedded_handlers::handleUnionComma,
            .description = "Union of multiple selectors separated by commas (e.g., '1,2,3' or '?@.a,?@.b')"
        },
        {
            .name = "filter_with_parens",
            .priority = 1200,  // High priority for filter expressions
            .matcher = matchers::matchesFilterWithParens,
            .handler = embedded_handlers::handleFilterWithParens,
            .description = "Filter expression with parentheses (e.g., '?(@.a == 1)')"
        },
        {
            .name = "filter_without_parens",
            .priority = 1100,
            .matcher = matchers::matchesFilterWithoutParens,
            .handler = embedded_handlers::handleFilterWithoutParens,
            .description = "Filter expression without parentheses (e.g., '?@.a')"
        },
        {
            .name = "wildcard",
            .priority = 900,
            .matcher = matchers::matchesWildcard,
            .handler = embedded_handlers::handleWildcard,
            .description = "Wildcard selector (*)"
        },
        {
            .name = "index_list",
            .priority = 850,
            .matcher = matchers::matchesIndexList,
            .handler = embedded_handlers::handleIndexList,
            .description = "Comma-separated index list (e.g., '1,2,3')"
        },
        {
            .name = "single_index",
            .priority = 800,
            .matcher = matchers::matchesSingleIndex,
            .handler = embedded_handlers::handleSingleIndex,
            .description = "Single array index (e.g., '123')"
        },
        {
            .name = "slice",
            .priority = 700,
            .matcher = matchers::matchesSlice,
            .handler = embedded_handlers::handleSlice,
            .description = "Array slice (e.g., '1:3:2')"
        },
        {
            .name = "placeholder",
            .priority = 500,
            .matcher = matchers::matchesPlaceholder,
            .handler = embedded_handlers::handlePlaceholder,
            .description = "Placeholder filter (e.g., '?' or '?,?')"
        },
        {
            .name = "quoted_key",
            .priority = 400,
            .matcher = matchers::matchesQuotedKey,
            .handler = embedded_handlers::handleQuotedKey,
            .description = "Quoted string key (e.g., \"'key'\" or '\"key\"')"
        },
        {
            .name = "unquoted_key",
            .priority = 100,
            .matcher = matchers::matchesUnquotedKey,
            .handler = embedded_handlers::handleUnquotedKey,
            .description = "Unquoted key (forbidden by RFC 9535)"
        }
    };
}

const std::vector<EmbeddedBracketRuleMetadata>& EmbeddedBracketRuleDispatcher::getRules()
{
    static const auto rules = createRules();
    return rules;
}

std::expected<void, Error> EmbeddedBracketRuleDispatcher::dispatch(QStringView content, EmbeddedBracketSink& sink)
{
    qCDebug(jsonPathLog) << "EmbeddedBracketRuleDispatcher::dispatch content=" << content.toString();
    
    // Apply rules in priority order using monadic error handling
    for (const auto& rule : getRules()) {
        qCDebug(jsonPathLog) << "Trying rule:" << rule.name << "(priority:" << rule.priority << ")";
        
        if (rule.matcher(content)) {
            qCDebug(jsonPathLog) << "Rule" << rule.name << "matched, applying handler";
            auto result = rule.handler(content, sink);
            
            if (result) {
                qCDebug(jsonPathLog) << "Rule" << rule.name << "succeeded";
                return {};
            } else {
                qCDebug(jsonPathLog) << "Rule" << rule.name << "failed with error" << static_cast<int>(result.error());
                return result;
            }
        } else {
            qCDebug(jsonPathLog) << "Rule" << rule.name << "did not match";
        }
    }
    
    qCDebug(jsonPathLog) << "No rules matched, returning UnsupportedFilter";
    return std::unexpected(Error::UnsupportedFilter);
}

std::expected<void, Error> EmbeddedBracketRuleDispatcher::processSegmentExcludingUnion(QStringView content, EmbeddedBracketSink& sink)
{
    // Apply all rules except union_comma to prevent recursion
    for (const auto& rule : getRules()) {
        if (std::string_view(rule.name) == "union_comma") continue; // Skip union rule
        
        if (rule.matcher(content)) {
            return rule.handler(content, sink);
        }
    }
    return std::unexpected(Error::UnsupportedFilter);
}

const EmbeddedBracketRuleMetadata* EmbeddedBracketRuleDispatcher::findRuleByName(const char* name)
{
    for (const auto& rule : getRules()) {
        if (std::string_view(rule.name) == name) {
            return &rule;
        }
    }
    return nullptr;
}

} // namespace json_query::json_path::detail
