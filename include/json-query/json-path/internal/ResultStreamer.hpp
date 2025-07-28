// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <QJsonValue>
#include <QJsonArray>
#include <functional>
#include <expected>
#include <memory>
#include <concepts>
#include <stdcompat/function_ref.hpp>
#include "json-query/json-path/JSONPathEvalError.hpp"

namespace json_query::json_path::internal
{

/**
 * @brief Concept defining the interface for result collectors
 *
 * A ResultCollectorConcept must provide methods to collect values and handle errors.
 * This concept ensures compile-time type safety without runtime overhead.
 */
template <typename T>
concept ResultCollectorConcept = requires(T& collector, const QJsonValue& value, EvalError error) {
    collector.collect(value);                                       // Must be able to collect values
    collector.handleError(error);                                   // Must be able to handle errors
    { collector.canHandleErrors() } -> std::convertible_to<bool>;   // Must indicate error handling capability
    { collector.hasError() } -> std::convertible_to<bool>;          // Must indicate if error occurred
    { collector.getLastError() } -> std::convertible_to<EvalError>; // Must provide last error
};

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
 * @brief Concept for error handlers - zero-overhead callable interface
 *
 * An ErrorHandlerConcept must provide a callable method that takes an EvalError.
 * This concept ensures compile-time type safety without runtime overhead.
 */
template <typename T>
concept ErrorHandlerConcept = requires(T& handler, EvalError error) {
    handler(error); // Must be callable with EvalError
};

/**
 * @brief Zero-overhead result streaming interface using concepts
 *
 * ResultStreamer provides direct member function calls without any
 * type erasure or function pointer overhead. It works with any type that
 * satisfies the ResultCollectorConcept.
 */
template <ResultCollectorConcept CollectorType>
class ResultStreamer
{
  public:
    /**
     * @brief Construct a ResultStreamer with a collector reference
     * @param collector Reference to the collector that will receive results
     */
    explicit constexpr ResultStreamer(CollectorType& collector) noexcept : collector_(collector) {}

    /**
     * @brief Emit a single result value with zero overhead
     * @param value The JSON value to emit
     */
    constexpr void emitValue(const QJsonValue& value) const noexcept
    {
        collector_.collect(value); // Direct member call - zero overhead!
    }

    /**
     * @brief Emit all values from a QJsonArray with zero overhead
     * @param array The array containing values to emit
     */
    void emitArray(const QJsonArray& array) const noexcept
    {
        for (const auto& value : array)
            collector_.collect(value); // Direct member call - zero overhead!
    }

    /**
     * @brief Handle an error during streaming with zero overhead
     * @param error The error that occurred
     */
    constexpr void handleError(EvalError error) const noexcept
    {
        if constexpr (requires { collector_.handleError(error); })
            collector_.handleError(error); // Direct member call - zero overhead!
    }

    /**
     * @brief Check if the streamer can emit results
     * @return true (always, since collector is required)
     */
    [[nodiscard]] constexpr bool canEmit() const noexcept { return true; }

    /**
     * @brief Check if the streamer can handle errors with zero overhead
     * @return true if the collector can handle errors
     */
    [[nodiscard]] constexpr bool canHandleErrors() const noexcept
    {
        if constexpr (requires { collector_.canHandleErrors(); })
            return collector_.canHandleErrors(); // Direct member call - zero overhead!
        else
            return false;
    }

    /**
     * @brief Get direct access to the underlying collector
     * @return Reference to the collector
     */
    [[nodiscard]] constexpr CollectorType& getCollector() const noexcept { return collector_; }

    /**
     * @brief Check if the collector has encountered an error
     * @return true if an error occurred
     */
    [[nodiscard]] constexpr bool hasError() const noexcept
    {
        if constexpr (requires { collector_.hasError(); })
            return collector_.hasError(); // Direct member call - zero overhead!
        else
            return false;
    }

  private:
    CollectorType& collector_; ///< Reference to the result collector
};

/**
 * @brief Zero-overhead result collector with concept-based error handling
 *
 * This class provides efficient result collection with optional error handling.
 * Error handlers are stored as template parameters for zero runtime overhead.
 */
class ResultCollector
{
  public:
    /**
     * @brief Construct a ResultCollector that accumulates into the provided array
     * @param results Pointer to QJsonArray where results will be accumulated
     */
    explicit ResultCollector(QJsonArray* results) noexcept
        : results_(results), hasErrorHandler_(true) {} // Enable error handling by default

    /**
     * @brief Construct a ResultCollector with custom error handling
     * @param results Pointer to QJsonArray where results will be accumulated
     * @param errorHandler Function called when errors occur
     */
    template <ErrorHandlerConcept ErrorHandler>
    ResultCollector(QJsonArray* results, ErrorHandler&& errorHandler) noexcept
        : results_(results), onError_(std::forward<ErrorHandler>(errorHandler)), hasErrorHandler_(true)
    {
    }

    /**
     * @brief Get a zero-overhead ResultStreamer for this collector
     * @return ResultStreamer with direct member function calls
     */
    [[nodiscard]] auto getStreamer() noexcept { return ResultStreamer<ResultCollector>(*this); }

    /**
     * @brief Emit a single value (for compatibility with streaming interface)
     * @param value Value to emit
     */
    void emitValue(const QJsonValue& value) { collect(value); }

    /**
     * @brief Emit all values from a QJsonArray (for compatibility with streaming interface)
     * @param array Array containing values to emit
     */
    void emitArray(const QJsonArray& array)
    {
        for (const auto& value : array)
            collect(value);
    }

    /**
     * @brief Collect a single value into the results array (concept requirement)
     * @param value The value to add to the results
     */
    void collect(const QJsonValue& value) noexcept
    {
        if (results_)
            results_->append(value);
    }

    /**
     * @brief Handle an error during collection (concept requirement)
     * @param error The error that occurred
     */
    void handleError(EvalError error) noexcept
    {
        if (hasErrorHandler_ && onError_)
            onError_(error);
        lastError_ = error;
        hasError_  = true;
    }

    /**
     * @brief Check if the collector can handle errors (concept requirement)
     * @return true if the collector can handle errors
     */
    [[nodiscard]] bool canHandleErrors() const noexcept { return hasErrorHandler_; }

    /**
     * @brief Check if the collector can emit results (concept requirement)
     * @return true (always, since results array is required)
     */
    [[nodiscard]] bool canEmit() const noexcept { return results_ != nullptr; }

    /**
     * @brief Check if an error occurred during collection (concept requirement)
     * @return true if an error was handled
     */
    [[nodiscard]] bool hasError() const noexcept { return hasError_; }

    /**
     * @brief Get the last error that occurred
     * @return The last EvalError that was handled
     */
    [[nodiscard]] EvalError getLastError() const noexcept { return lastError_; }

    /**
     * @brief Get the number of collected results
     * @return The size of the results array
     */
    [[nodiscard]] qsizetype size() const noexcept { return results_ ? results_->size() : 0; }

    /**
     * @brief Check if any results have been collected
     * @return true if the results array is empty
     */
    [[nodiscard]] bool empty() const noexcept { return results_ ? results_->isEmpty() : true; }

  private:
    QJsonArray*                    results_;
    std::function<void(EvalError)> onError_{[](EvalError) {}}; // Keep std::function for now for compatibility
    // Only used if error handling needed
    bool      hasErrorHandler_;
    EvalError lastError_ = EvalError::TypeMismatchObject;
    bool      hasError_{false};
};

/**
 * @brief Zero-overhead result collector with concept-based error handling
 *
 * This class provides efficient result collection with optional error handling.
 * Error handlers are stored as template parameters for zero runtime overhead.
 */
template <ErrorHandlerConcept ErrorHandler = decltype([](EvalError) {})>
class ResultCollectorTemplate
{
  public:
    /**
     * @brief Construct a ResultCollector that accumulates into the provided array
     * @param results Pointer to QJsonArray where results will be accumulated
     */
    explicit ResultCollectorTemplate(QJsonArray* results) noexcept
        : results_(results), hasErrorHandler_(true) {} // Enable error handling by default

    /**
     * @brief Construct a ResultCollector with custom error handling
     * @param results Pointer to QJsonArray where results will be accumulated
     * @param errorHandler Function called when errors occur
     */
    template <ErrorHandlerConcept CustomErrorHandler>
    ResultCollectorTemplate(QJsonArray* results, CustomErrorHandler&& errorHandler) noexcept
        : results_(results), onError_(std::forward<CustomErrorHandler>(errorHandler)), hasErrorHandler_(true)
    {
    }

    /**
     * @brief Get a zero-overhead ResultStreamer for this collector
     * @return ResultStreamer with direct member function calls
     */
    [[nodiscard]] auto getStreamer() noexcept { return ResultStreamer<ResultCollectorTemplate>(*this); }

    /**
     * @brief Emit a single value (for compatibility with streaming interface)
     * @param value Value to emit
     */
    void emitValue(const QJsonValue& value) { collect(value); }

    /**
     * @brief Emit all values from a QJsonArray (for compatibility with streaming interface)
     * @param array Array containing values to emit
     */
    void emitArray(const QJsonArray& array)
    {
        for (const auto& value : array)
            collect(value);
    }

    /**
     * @brief Collect a single value into the results array (concept requirement)
     * @param value The value to add to the results
     */
    void collect(const QJsonValue& value) noexcept
    {
        if (results_)
            results_->append(value);
    }

    /**
     * @brief Handle an error during collection (concept requirement)
     * @param error The error that occurred
     */
    void handleError(EvalError error) noexcept
    {
        if (hasErrorHandler_ && onError_)
            onError_(error);
        lastError_ = error;
        hasError_  = true;
    }

    /**
     * @brief Check if the collector can handle errors (concept requirement)
     * @return true if the collector can handle errors
     */
    [[nodiscard]] bool canHandleErrors() const noexcept { return hasErrorHandler_; }

    /**
     * @brief Check if the collector can emit results (concept requirement)
     * @return true (always, since results array is required)
     */
    [[nodiscard]] bool canEmit() const noexcept { return results_ != nullptr; }

    /**
     * @brief Check if an error occurred during collection (concept requirement)
     * @return true if an error was handled
     */
    [[nodiscard]] bool hasError() const noexcept { return hasError_; }

    /**
     * @brief Get the last error that occurred
     * @return The last EvalError that was handled
     */
    [[nodiscard]] EvalError getLastError() const noexcept { return lastError_; }

    /**
     * @brief Get the number of collected results
     * @return The size of the results array
     */
    [[nodiscard]] qsizetype size() const noexcept { return results_ ? results_->size() : 0; }

    /**
     * @brief Check if any results have been collected
     * @return true if the results array is empty
     */
    [[nodiscard]] bool empty() const noexcept { return results_ ? results_->isEmpty() : true; }

  private:
    QJsonArray*  results_;
    ErrorHandler onError_{}; // Zero-overhead with empty base optimization
    // Only used if error handling needed
    bool      hasErrorHandler_;
    EvalError lastError_ = EvalError::TypeMismatchObject;
    bool      hasError_{false};
};

} // namespace json_query::json_path::internal
