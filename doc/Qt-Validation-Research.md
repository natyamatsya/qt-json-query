# Qt Built-in Validation Research

**Goal:** Identify Qt functionality that can replace custom validation implementations in FormatValidators.

---

## Qt Classes for Validation

### 1. QDateTime / QDate / QTime (RFC 3339 / ISO 8601)

**Qt Classes:**
- `QDateTime` - Combined date and time
- `QDate` - Date only
- `QTime` - Time only

**Relevant Methods:**
```cpp
// Parse from string with format
QDateTime QDateTime::fromString(const QString &string, Qt::DateFormat format);
QDateTime QDateTime::fromString(const QString &string, const QString &format);

// ISO 8601 / RFC 3339 support
QDateTime::fromString(str, Qt::ISODate);        // ISO 8601 format
QDateTime::fromString(str, Qt::ISODateWithMs);  // ISO 8601 with milliseconds

// Validation
bool QDateTime::isValid() const;
bool QDate::isValid() const;
bool QTime::isValid() const;
```

**✅ RECOMMENDATION: Use Qt for date-time validation**

**Implementation:**
```cpp
bool FormatValidators::isDateTime(QStringView value) noexcept
{
    const auto dt{QDateTime::fromString(value.toString(), Qt::ISODateWithMs)};
    return dt.isValid();
}

bool FormatValidators::isDate(QStringView value) noexcept
{
    const auto date{QDate::fromString(value.toString(), Qt::ISODate)};
    return date.isValid();
}

bool FormatValidators::isTime(QStringView value) noexcept
{
    // Qt's QTime::fromString with ISODate handles RFC 3339 time format
    const auto time{QTime::fromString(value.toString(), Qt::ISODate)};
    return time.isValid();
}
```

**Benefits:**
- ✅ RFC 3339 / ISO 8601 compliant
- ✅ Handles timezones correctly
- ✅ Validates ranges automatically (hour 0-23, minute 0-59, etc.)
- ✅ No custom regex needed
- ✅ Well-tested Qt implementation

---

### 2. QUrl (URI / URL Validation)

**Qt Class:** `QUrl`

**Relevant Methods:**
```cpp
QUrl::QUrl(const QString &url, ParsingMode mode = TolerantMode);
bool QUrl::isValid() const;
QString QUrl::scheme() const;
QString QUrl::host() const;
```

**Parsing Modes:**
- `QUrl::TolerantMode` - Tolerant parsing (default)
- `QUrl::StrictMode` - Strict RFC 3986 compliance
- `QUrl::DecodedMode` - Decoded components

**✅ RECOMMENDATION: Use Qt for URI/URL validation**

**Implementation:**
```cpp
bool FormatValidators::isUri(QStringView value) noexcept
{
    const QUrl url{value.toString(), QUrl::StrictMode};
    return url.isValid() && !url.scheme().isEmpty();
}

bool FormatValidators::isUriReference(QStringView value) noexcept
{
    const QUrl url{value.toString(), QUrl::StrictMode};
    return url.isValid();  // Can be relative
}

bool FormatValidators::isUriTemplate(QStringView value) noexcept
{
    // URI templates need custom handling (RFC 6570)
    // Qt doesn't support templates, keep custom implementation
    return uriTemplatePattern.match(value).hasMatch();
}
```

**Benefits:**
- ✅ RFC 3986 compliant
- ✅ Handles IDN (internationalized domain names)
- ✅ Proper percent-encoding validation
- ✅ Well-tested Qt implementation

**Limitations:**
- ❌ No URI template support (RFC 6570) - need custom regex

---

### 3. QRegularExpression (Already Used)

**Qt Class:** `QRegularExpression`

**Current Usage:**
- ✅ Already using for pattern validation
- ✅ Already using for email validation
- ✅ Already using for hostname validation

**Keep as-is** - Qt's regex engine is already being used effectively.

---

### 4. Email Validation

**Qt Support:** ❌ No built-in email validator

**Options:**
1. Keep custom regex (current approach)
2. Use `QRegularExpression` with RFC 5322 pattern (current)

**✅ RECOMMENDATION: Keep custom regex implementation**

Qt doesn't provide email validation, and our regex-based approach is standard.

---

### 5. Hostname Validation

**Qt Support:** Partial via `QUrl::host()`

**Current Approach:** Custom regex

**Alternative:**
```cpp
bool FormatValidators::isHostname(QStringView value) noexcept
{
    if (value.isEmpty() || value.size() > 253)
        return false;
    
    // Use QUrl to validate hostname
    const QUrl url{u"http://"_qs + value.toString()};
    if (!url.isValid())
        return false;
    
    const auto host{url.host()};
    return !host.isEmpty() && host == value;
}
```

**⚠️ RECOMMENDATION: Consider Qt approach, but test thoroughly**

Qt's URL parser handles hostnames, but may be more permissive than JSON Schema requires.

---

### 6. IPv4 / IPv6 Validation

**Qt Classes:** `QHostAddress`

**Relevant Methods:**
```cpp
QHostAddress::QHostAddress(const QString &address);
bool QHostAddress::isNull() const;
QAbstractSocket::NetworkLayerProtocol QHostAddress::protocol() const;
```

**✅ RECOMMENDATION: Use Qt for IP address validation**

**Implementation:**
```cpp
bool FormatValidators::isIPv4(QStringView value) noexcept
{
    const QHostAddress addr{value.toString()};
    return !addr.isNull() && addr.protocol() == QAbstractSocket::IPv4Protocol;
}

bool FormatValidators::isIPv6(QStringView value) noexcept
{
    const QHostAddress addr{value.toString()};
    return !addr.isNull() && addr.protocol() == QAbstractSocket::IPv6Protocol;
}
```

**Benefits:**
- ✅ RFC 4291 compliant (IPv6)
- ✅ Handles IPv6 compressed notation
- ✅ Validates IPv4-mapped IPv6 addresses
- ✅ Well-tested Qt implementation

---

### 7. UUID Validation

**Qt Class:** `QUuid`

**Relevant Methods:**
```cpp
QUuid::QUuid(const QString &text);
bool QUuid::isNull() const;
```

**✅ RECOMMENDATION: Use Qt for UUID validation**

**Implementation:**
```cpp
bool FormatValidators::isUuid(QStringView value) noexcept
{
    const QUuid uuid{value.toString()};
    return !uuid.isNull();
}
```

**Benefits:**
- ✅ RFC 4122 compliant
- ✅ Handles various UUID formats
- ✅ Well-tested Qt implementation

---

### 8. JSON Pointer Validation

**Qt Support:** ❌ No built-in JSON Pointer support

**Current Approach:** Custom regex

**✅ RECOMMENDATION: Keep custom implementation**

We already have JSONPointer implementation in the project, could potentially use that for validation.

---

## Summary of Recommendations

| Format | Current | Recommended | Qt Class | Benefit |
|--------|---------|-------------|----------|---------|
| **date-time** | Custom regex | ✅ **Use Qt** | `QDateTime` | RFC 3339 compliant, range validation |
| **date** | Custom regex | ✅ **Use Qt** | `QDate` | ISO 8601 compliant |
| **time** | Custom regex + range check | ✅ **Use Qt** | `QTime` | Automatic range validation |
| **uri** | Custom regex | ✅ **Use Qt** | `QUrl` | RFC 3986 compliant |
| **uri-reference** | Custom regex | ✅ **Use Qt** | `QUrl` | Handles relative URIs |
| **uri-template** | Custom regex | ❌ Keep custom | N/A | Qt doesn't support RFC 6570 |
| **email** | Custom regex | ❌ Keep custom | N/A | No Qt support |
| **hostname** | Custom regex | ⚠️ Consider Qt | `QUrl::host()` | May be too permissive |
| **idn-hostname** | Custom regex | ⚠️ Consider Qt | `QUrl` with IDN | Built-in IDN support |
| **ipv4** | Custom regex | ✅ **Use Qt** | `QHostAddress` | RFC compliant |
| **ipv6** | Custom regex | ✅ **Use Qt** | `QHostAddress` | RFC 4291 compliant |
| **uuid** | Custom regex | ✅ **Use Qt** | `QUuid` | RFC 4122 compliant |
| **json-pointer** | Custom regex | ❌ Keep custom | N/A | Could use our JSONPointer |
| **regex** | Custom validation | ❌ Keep custom | N/A | Validates regex syntax |

---

## Implementation Priority

### High Priority (Clear Win)
1. ✅ **IPv4/IPv6** - Use `QHostAddress` (simpler, more reliable)
2. ✅ **UUID** - Use `QUuid` (simpler, more reliable)
3. ✅ **URI/URL** - Use `QUrl` (RFC compliant, handles edge cases)
4. ✅ **Date/Time** - Use `QDateTime/QDate/QTime` (removes custom range validation)

### Medium Priority (Evaluate)
5. ⚠️ **Hostname** - Test `QUrl::host()` vs custom regex
6. ⚠️ **IDN Hostname** - Qt has built-in IDN support

### Keep Custom
7. ❌ **Email** - No Qt support, regex is standard approach
8. ❌ **URI Template** - No Qt support for RFC 6570
9. ❌ **JSON Pointer** - Could use our own JSONPointer class
10. ❌ **Regex** - Validates regex syntax, no Qt equivalent

---

## Code Size Reduction

**Current:** ~150 lines of custom regex patterns and validation logic

**After Qt Integration:** ~80 lines (47% reduction)

**Benefits:**
- Fewer custom regex patterns to maintain
- Better RFC compliance
- Leverages well-tested Qt code
- Simpler, more readable code

---

## Next Steps

1. Update `FormatValidators.cpp` to use Qt classes
2. Remove custom regex patterns where Qt provides validation
3. Add `#include <QHostAddress>`, `<QUuid>`, `<QDateTime>`, `<QUrl>`
4. Test all format validators to ensure compatibility
5. Verify performance (Qt parsing may be slower than regex for some cases)
6. Update tests if validation behavior changes slightly

---

## Performance Considerations

**Regex Approach:**
- ✅ Fast pattern matching
- ❌ May not catch all edge cases
- ❌ Requires custom range validation (time format)

**Qt Class Approach:**
- ✅ Complete validation (all edge cases)
- ✅ RFC compliant
- ⚠️ May be slightly slower (full parsing vs pattern match)
- ✅ Simpler code

**Recommendation:** Use Qt classes. The validation correctness and code simplicity outweigh any minor performance difference.
