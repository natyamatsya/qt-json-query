// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

namespace json_query::json_path
{

// High-level evaluation options formerly nested inside JSONPath.
// Extracted into a standalone header so lower-level helpers can use
// the enum without including the heavy JSONPath interface.
enum class Option
{
    None       = 0,
    AsPathList = 1
};

} // namespace json_query::json_path
