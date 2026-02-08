# Specification Documents

This directory contains the official specification documents for the standards implemented in qt-json-query.

## JSON Schema (Draft 2020-12)

JSON Schema is an IETF Internet-Draft (not yet an RFC) that defines a JSON-based format for describing JSON data structures.

- **ietf-json-schema-draft-2020-12.txt** - JSON Schema: A Media Type for Describing JSON Documents
  - Original: draft-bhutton-json-schema-00
  - URL: <https://datatracker.ietf.org/doc/html/draft-bhutton-json-schema-00>
  - Date: December 2020
  - Status: Internet-Draft
  - Description: Defines the core JSON Schema specification, including the meta-schema, schema identification, and base vocabulary

- **ietf-json-schema-validation-draft-2020-12.txt** - JSON Schema Validation: A Vocabulary for Structural Validation of JSON
  - Original: draft-bhutton-json-schema-validation-00
  - URL: <https://datatracker.ietf.org/doc/html/draft-bhutton-json-schema-validation-00>
  - Date: December 2020
  - Status: Internet-Draft
  - Description: Defines the validation keywords vocabulary including type constraints, numeric constraints, string constraints, array constraints, object constraints, and format validators

**Note:** These drafts correspond to JSON Schema Draft 2020-12, which is the version implemented in qt-json-query. While not yet published as RFCs, these are the stable specifications referenced by the JSON Schema community.

## RFC Documents

### RFC 6901 - JSON Pointer

- **rfc6901.txt** - JavaScript Object Notation (JSON) Pointer
  - URL: <https://www.rfc-editor.org/rfc/rfc6901.txt>
  - Date: April 2013
  - Status: Proposed Standard
  - Description: Defines a string syntax for identifying a specific value within a JSON document

### RFC 8259 - JSON Data Interchange Format

- **rfc8259.txt** - The JavaScript Object Notation (JSON) Data Interchange Format
  - URL: <https://www.rfc-editor.org/rfc/rfc8259.txt>
  - Date: December 2017
  - Status: Internet Standard (STD 90)
  - Description: Defines the JSON data format, superseding RFC 7159

### RFC 9535 - JSONPath

- **rfc9535.txt** - JSONPath: Query Expressions for JSON
  - URL: <https://www.rfc-editor.org/rfc/rfc9535.txt>
  - Date: February 2024
  - Status: Proposed Standard
  - Description: Defines JSONPath, a query language for selecting and extracting values from JSON documents

## Implementation Status

| Specification | Status | Tests | Compliance |
|---------------|--------|-------|------------|
| **JSON Schema Draft 2020-12** | ✅ Complete | 1932/1994 IETF (96.9%), 116/116 unit | See `doc/schema-compliance.md` |
| **RFC 6901 (JSON Pointer)** | ✅ Complete | 33/33 | Full RFC compliance |
| **RFC 9535 (JSONPath)** | ✅ Complete | 443/444 (1 skipped) | Full RFC compliance |

## References

- JSON Schema Official Site: <https://json-schema.org/>
- JSON Schema Specification: <https://json-schema.org/specification>
- IETF Datatracker: <https://datatracker.ietf.org/>
- RFC Editor: <https://www.rfc-editor.org/>

## License

These specification documents are provided for reference purposes. Each document is subject to its own copyright and licensing terms as specified within the document.
