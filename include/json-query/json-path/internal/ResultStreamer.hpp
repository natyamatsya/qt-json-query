#pragma once

#include <QJsonValue>
#include <QJsonArray>
#include <functional>
#include <expected>
#include "json-query/json-path/JSONPathEvalError.hpp"

namespace json_query::json_path::internal {

/**
 * @brief Result streaming interface for high-performance JSONPath evaluation
 * 
 * ResultStreamer provides a streaming interface that eliminates intermediate
 * QJsonArray allocations during recursive descent and fanOut operations.
 * Results are emitted directly to the consumer without creating temporary arrays.
 */
class ResultStreamer {
public:
    using EmitFunction = std::function<void(const QJsonValue&)>;
    using ErrorHandler = std::function<void(EvalError)>;

    /**
     * @brief Construct a ResultStreamer with emission and error handling callbacks
     * @param emitFn Function called for each result value
     * @param errorFn Function called when errors occur
     */
    constexpr ResultStreamer(EmitFunction emitFn, ErrorHandler errorFn = nullptr) noexcept
        : emitFn_(std::move(emitFn)), onError_(std::move(errorFn)) {}

    /**
     * @brief Emit a single result value
     * @param value The JSON value to emit
     */
    constexpr void emitValue(const QJsonValue& value) const noexcept {
        if (emitFn_) {
            emitFn_(value);
        }
    }

    /**
     * @brief Emit all values from a QJsonArray
     * @param array The array containing values to emit
     */
    void emitArray(const QJsonArray& array) const noexcept {
        if (emitFn_) {
            for (const auto& value : array) {
                emitFn_(value);
            }
        }
    }

    /**
     * @brief Handle an error during streaming
     * @param error The evaluation error that occurred
     */
    constexpr void handleError(EvalError error) const noexcept {
        if (onError_) {
            onError_(error);
        }
    }

    /**
     * @brief Check if the streamer has an emit function
     * @return true if the streamer can emit results
     */
    [[nodiscard]] constexpr bool canEmit() const noexcept {
        return static_cast<bool>(emitFn_);
    }

    /**
     * @brief Check if the streamer has an error handler
     * @return true if the streamer can handle errors
     */
    [[nodiscard]] constexpr bool canHandleErrors() const noexcept {
        return static_cast<bool>(onError_);
    }

private:
    EmitFunction emitFn_;
    ErrorHandler onError_;
};

/**
 * @brief Utility class for collecting streamed results into a QJsonArray
 * 
 * This class provides a bridge between the streaming interface and the existing
 * QJsonArray-based API, allowing gradual migration to streaming.
 */
class ResultCollector {
public:
    /**
     * @brief Construct a ResultCollector
     */
    ResultCollector() = default;

    /**
     * @brief Get a ResultStreamer that collects results into this collector
     * @return ResultStreamer configured to collect results
     */
    [[nodiscard]] ResultStreamer getStreamer() noexcept {
        return ResultStreamer(
            [this](const QJsonValue& value) { results_.append(value); },
            [this](EvalError error) { lastError_ = error; hasError_ = true; }
        );
    }

    /**
     * @brief Get the collected results
     * @return QJsonArray containing all emitted results
     */
    [[nodiscard]] const QJsonArray& getResults() const noexcept {
        return results_;
    }

    /**
     * @brief Move the collected results out of the collector
     * @return QJsonArray containing all emitted results
     */
    [[nodiscard]] QJsonArray moveResults() noexcept {
        return std::move(results_);
    }

    /**
     * @brief Check if an error occurred during collection
     * @return true if an error was handled
     */
    [[nodiscard]] bool hasError() const noexcept {
        return hasError_;
    }

    /**
     * @brief Get the last error that occurred
     * @return The last EvalError that was handled
     */
    [[nodiscard]] EvalError getLastError() const noexcept {
        return lastError_;
    }

    /**
     * @brief Get the collected results as an expected value
     * @return std::expected containing results or error
     */
    [[nodiscard]] std::expected<QJsonArray, EvalError> getExpected() noexcept {
        if (hasError_) {
            return std::unexpected(lastError_);
        }
        return std::move(results_);
    }

    /**
     * @brief Clear the collector for reuse
     */
    void clear() noexcept {
        results_ = QJsonArray{};
        hasError_ = false;
        lastError_ = EvalError::TypeMismatchObject; // Default error
    }

    /**
     * @brief Get the number of collected results
     * @return Number of results in the collector
     */
    [[nodiscard]] qsizetype size() const noexcept {
        return results_.size();
    }

    /**
     * @brief Check if the collector is empty
     * @return true if no results have been collected
     */
    [[nodiscard]] bool empty() const noexcept {
        return results_.isEmpty();
    }

private:
    QJsonArray results_;
    EvalError lastError_ = EvalError::TypeMismatchObject;
    bool hasError_ = false;
};

/**
 * @brief Create a ResultStreamer that appends to an existing QJsonArray
 * @param target The target array to append results to
 * @return ResultStreamer configured to append to the target array
 */
[[nodiscard]] inline ResultStreamer makeAppendingStreamer(QJsonArray& target) noexcept {
    return ResultStreamer([&target](const QJsonValue& value) {
        target.append(value);
    });
}

/**
 * @brief Create a ResultStreamer with custom emission logic
 * @param emitFn Function to call for each emitted result
 * @return ResultStreamer with the specified emission function
 */
template<typename EmitFn>
[[nodiscard]] constexpr ResultStreamer makeCustomStreamer(EmitFn&& emitFn) noexcept {
    return ResultStreamer(std::forward<EmitFn>(emitFn));
}

} // namespace json_query::json_path::internal
