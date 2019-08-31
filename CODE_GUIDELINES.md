Code guidelines
=====================

For the most part, follow Godot conventions.

Syntax
--------

- Class and struct names `PascalCase`
- Constants, enums and macros `CAPITAL_CASE`
- Other names `snake_case`
- Globals prefixed with `g_`
- Parameters prefixed with `p_`, but not really enforced so far. Matters for big functions.
- Private and protected fields prefixed with `_`
- Some private functions start with `_`, either to mimic Godot API, or to signify it's a re-used function that performs no checks
- Enums prefixed by their name. Example: `enum Type { TYPE_ONE, TYPE_TWO }`
- Open braces at the end of line, close them next line
- Never omit braces
- Space between binary operators and control flow
- Indent with tabs
- Use Clang-format to automate most of these rules (the one included in Godot should do it)

C++ features
-------------

- Don't use `auto` unless the type is impossible to express or a horrible template (like STL ones). IDEs aren't granted (Github reviews and diffs)
- STL is ok if it measurably performs better than Godot alternatives.
- Initialize variables next to declaration

Error handling
---------------

- No exceptions
- Check invariants, fail early. Use `CRASH_COND` to make sure states are as expected even if they don't cause immediate harm.
- Crashes aren't nice to users, so in those cases use `ERR_FAIL_COND` macros for code that can recover from error

Performance
-------------

In performance-critical areas:

- Avoid allocations. Re-use memory either with `ObjectPool`, fixed-size arrays or use `std::vector` capacity.
- Avoid `virtual`, `Ref<T>`, `String`
- Don't resize `PoolVectors` or `Vector<T>`, or do it in one go if needed
- Careful about what is thread-safe and what isn't. Some major areas of this module work within threads.
- Reduce mutex locking to a minimum
- Use data structures that are fit to the most frequent use over time (will often be either array, vector or hash map)
- Consider statistics if their impact is negligible. It helps monitoring how well the module performs even in release builds.
- Profile your code, in release mode. The lightweight `util/zprofiling.h` can help but you are free to use your own technique.

Godot API
----------

- Use the most direct APIs for the job. Especially, don't use nodes. See `VisualServer` and `PhysicsServer`.
- Always use `Read` and `Write` when modifying `PoolVector`.
- Only expose a function if it is safe to use and guaranteed to remain present for a while
- use `memnew`, `memdelete`, `memalloc` and `memfree` so memory usage is counted within Godot monitors
