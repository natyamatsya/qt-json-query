// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "../JSONPathError.hpp"

#include <QJsonValue>
#include <QJsonArray>
#include <functional>
#include <concepts>

namespace json_query::json_path::internal
{

/**
 * @brief Concept defining the interface for result streamers
 *
 * A ResultStreamerConcept must provide methods to emit values and handle errors.
 * This concept ensures compile-time type safety for streaming operations.
 */
template <typename T>
concept ResultStreamerConcept =
    requires(T& streamer, const QJsonValue& value, const QJsonArray& array, EvalError error) {
        streamer.emitValue(value);                                   // Must have emitValue method
        streamer.emitArray(array);                                   // Must have emitArray method
        { streamer.canEmit() } -> std::convertible_to<bool>;         // Must be able to check if can emit
        { streamer.canHandleErrors() } -> std::convertible_to<bool>; // Must indicate error handling capability
        streamer.handleError(error);                                 // Must have error handling method
    };

/**
 * @brief Result collector that accumulates values into a QJsonArray
 *
 * Satisfies ResultStreamerConcept for use with IterativeRecursiveDescent.
 */
class ResultCollector
{
  public:
    explicit ResultCollector(QJsonArray* results) noexcept : results_(results) {}

    void emitValue(const QJsonValue& value) { collect(value); }

    void emitArray(const QJsonArray& array)
    {
        for (const auto& value : array)
            collect(value);
    }

    void collect(const QJsonValue& value) noexcept
    {
        if (results_)
            results_->append(value);
    }

    void handleError(EvalError error) noexcept
    {
        lastError_ = error;
        hasError_  = true;
    }

    [[nodiscard]] bool      canHandleErrors() const noexcept { return true; }
    [[nodiscard]] bool      canEmit() const noexcept { return results_ != nullptr; }
    [[nodiscard]] bool      hasError() const noexcept { return hasError_; }
    [[nodiscard]] EvalError getLastError() const noexcept { return lastError_; }
    [[nodiscard]] qsizetype size() const noexcept { return results_ ? results_->size() : 0; }
    [[nodiscard]] bool      empty() const noexcept { return results_ ? results_->isEmpty() : true; }

  private:
    QJsonArray* results_;
    EvalError   lastError_{EvalError::TypeMismatchObject};
    bool        hasError_{false};
};

} // namespace json_query::json_path::internal
