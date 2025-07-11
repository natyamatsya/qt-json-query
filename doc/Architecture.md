# qt-json-query Architecture Overview

## 1. Motivation for the `ContainerFrame` Traversal Design

The original recursive-descent implementation of **JSONPath** used
`QJsonValue` copies and deep recursion. This had three drawbacks:

1. **Performance** – unnecessary `QJsonValue` constructions / destructions for
   every node visited.
2. **Stack usage** – very deep JSON documents could overflow the C++ call
   stack.
3. **Qt 6 reference optimisation** – Qt 6 introduces `QJsonValueConstRef`
   which allows *zero-copy* access to JSON values *if* the referenced
   container (`QJsonObject` / `QJsonArray`) out-lives the reference.  The old
   algorithm frequently materialised temporaries, making safe use of
   `QJsonValueConstRef` impossible.

To address this a lightweight **iterator-frame** machine was introduced.

## 2. The `ContainerFrame` Struct

```cpp
struct ContainerFrame
{
    QJsonObject object;                   // owning ref to keep data alive
    QJsonArray  array;
    QJsonObject::const_iterator objIt;    // current position within object
    int         arrIndex = -1;            // current position within array

    explicit ContainerFrame(const QJsonObject &o)
        : object(o), objIt(object.begin()) {}
    explicit ContainerFrame(const QJsonArray  &a)
        : array(a),  arrIndex(0)          {}

    bool hasNext() const;
    QJsonValue next();                    // returns current child & advances
};
```

### Key Points

* **Owning containers** (`object` / `array`) are stored *by value*.  Qt’s
  implicit-sharing keeps this cost minimal while guaranteeing that nested
  references stay valid for the lifetime of the frame.
* The frame holds the **current iterator / index**, enabling single-pass
  traversal without recursion.
* `next()` produces a `QJsonValue` (cheap, 16 bytes) which the main loop
  processes immediately; containers are appended to the result list and a new
  frame is `emplace_back`-ed for depth-first expansion.
* Using a `std::vector<ContainerFrame>` as a manual stack allows explicit
  capacity reservation and cache-friendly contiguous memory layout.

## 3. Updated Traversal Loop (`evaluateRecursive`)

```cpp
vector<ContainerFrame> stack;
stack.emplace_back(rootObjectOrArray);

while (!stack.empty()) {
    ContainerFrame &frame = stack.back();
    if (!frame.hasNext()) { stack.pop_back(); continue; }

    QJsonValue child = frame.next();
    if (child.isObject()) {
        result.append(child);
        stack.emplace_back(child.toObject());
    } else if (child.isArray()) {
        result.append(child);
        stack.emplace_back(child.toArray());
    }
}
```

### Benefits

| Aspect               | Old Recursion | New `ContainerFrame` |
| -------------------- | ------------ | -------------------- |
| Call-stack depth     | O(depth)     | O(1)                 |
| `QJsonValue` copies  | Many         | Minimal              |
| `QJsonValueConstRef` | Unsafe       | **Safe** (containers alive) |
| Memory locality      | Mixed        | Contiguous frames    |

## 4. Why `QJsonValue`, not `QJsonValueConstRef` inside `ContainerFrame`

`QJsonValueConstRef` cannot be stored *inside* the frame because it points
into *another* container (the parent). By keeping the parent container alive
inside the frame and returning a **temporary** `QJsonValue` instead, we avoid
lifetime pitfalls while still removing the bulk of copies.

The outer evaluation loop may still convert to `constRef()` if further
optimisations are pursued, but the current design focuses on correctness and
simplicity first.

## 5. Summary

The `ContainerFrame` iterator stack provides a copy-free, iterative traversal
mechanism that:

* Eliminates deep recursion and reduces memory allocations.
* Makes safe future use of `QJsonValueConstRef` possible.
* Improves cache locality and prepares the codebase for additional
  performance work (e.g. breadth-first traversal, parallel evaluation).

This refactor modernises **qt-json-query** by embracing Qt 6 features and
standard C++ containers while retaining a clean and maintainable codebase.
