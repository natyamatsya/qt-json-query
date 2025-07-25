#pragma once

#include <QJsonValue>
#include <QJsonArray>
#include <functional>
#include <expected>
#include <stdcompat/function_ref.hpp>
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
    using EmitFunction = stdcompat::function_ref<void(const QJsonValue&)>;
    using ErrorHandler = stdcompat::function_ref<void(EvalError)>;

    /**
     * @brief Construct a ResultStreamer with emission callback only
     * @param emitFn Function called for each result value
     */
    constexpr explicit ResultStreamer(EmitFunction emitFn) noexcept
        : emitFn_(emitFn), hasErrorHandler_(false) {}

    /**
     * @brief Construct a ResultStreamer with emission and error handling callbacks
     * @param emitFn Function called for each result value
     * @param errorFn Function called when errors occur
     */
    constexpr ResultStreamer(EmitFunction emitFn, ErrorHandler errorFn) noexcept
        : emitFn_(emitFn), onError_(errorFn), hasErrorHandler_(true) {}

    /**
     * @brief Emit a single result value
     * @param value The JSON value to emit
     */
    constexpr void emitValue(const QJsonValue& value) const noexcept {
        emitFn_(value);
    }

    /**
     * @brief Emit all values from a QJsonArray
     * @param array The array containing values to emit
     */
    void emitArray(const QJsonArray& array) const noexcept {
        for (const auto& value : array) {
            emitFn_(value);
        }
    }

    /**
     * @brief Handle an error during streaming
     * @param error The error that occurred
     */
    constexpr void handleError(EvalError error) const noexcept {
        if (hasErrorHandler_) {
            onError_(error);
        }
    }

    /**
     * @brief Check if the streamer can emit results
     * @return true (always, since emitFn_ is required)
     */
    [[nodiscard]] constexpr bool canEmit() const noexcept {
        return true;
    }

    /**
     * @brief Check if the streamer has an error handler
     * @return true if the streamer can handle errors
     */
    [[nodiscard]] constexpr bool canHandleErrors() const noexcept {
        return hasErrorHandler_;
    }

private:
    EmitFunction emitFn_;
    ErrorHandler onError_{[](EvalError){}};  // Default no-op handler
    bool hasErrorHandler_;
};

/**
 * @brief Result collector that accumulates values into a QJsonArray
 * 
 * ResultCollector provides a convenient way to collect streaming results
 * into a traditional QJsonArray for compatibility with existing APIs.
 */
class ResultCollector {
public:
    using ErrorHandler = stdcompat::function_ref<void(EvalError)>;

    /**
     * @brief Construct a ResultCollector that accumulates into the provided array
     * @param results Reference to QJsonArray where results will be accumulated
     */
    explicit ResultCollector(QJsonArray& results) noexcept
        : results_(results), hasErrorHandler_(false) {}

    /**
     * @brief Construct a ResultCollector with error handling
     * @param results Reference to QJsonArray where results will be accumulated
     * @param errorFn Function called when errors occur
     */
    ResultCollector(QJsonArray& results, ErrorHandler errorFn) noexcept
        : results_(results), onError_(errorFn), hasErrorHandler_(true) {}

    /**
     * @brief Get a ResultStreamer that collects into this collector
     * @return ResultStreamer configured to use this collector
     */
    [[nodiscard]] ResultStreamer getStreamer() noexcept {
        if (hasErrorHandler_) {
            return ResultStreamer{
                [this](const QJsonValue& value) { collect(value); },
                onError_
            };
        } else {
            return ResultStreamer{
                [this](const QJsonValue& value) { collect(value); }
            };
        }
    }

    /**
     * @brief Emit a single value (for compatibility with streaming interface)
     * @param value Value to emit
     */
    void emitValue(const QJsonValue& value) {
        collect(value);
    }

    /**
     * @brief Collect a single value into the results array
     * @param value The value to add to the results
     */
    void collect(const QJsonValue& value) noexcept {
        results_.append(value);
    }

    /**
     * @brief Handle an error during collection
     * @param error The error that occurred
     */
    void handleError(EvalError error) noexcept {
        if (hasErrorHandler_) {
            onError_(error);
        }
        lastError_ = error;
        hasError_ = true;
    }

    /**
     * @brief Check if the collector has an error handler
     * @return true if the collector can handle errors
     */
    [[nodiscard]] bool canHandleErrors() const noexcept {
        return hasErrorHandler_;
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
     * @brief Get the number of collected results
     * @return The size of the results array
     */
    [[nodiscard]] qsizetype size() const noexcept {
        return results_.size();
    }

    /**
     * @brief Check if any results have been collected
     * @return true if the results array is empty
     */
    [[nodiscard]] bool empty() const noexcept {
        return results_.isEmpty();
    }

private:
    QJsonArray& results_;
    ErrorHandler onError_{[](EvalError){}};  // Default no-op handler
    bool hasErrorHandler_;
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
