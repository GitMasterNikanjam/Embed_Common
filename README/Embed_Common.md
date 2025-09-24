# Embed\_Common

Small, dependency-free portability helpers and utilities for embedded and desktop C++ projects.
It centralizes common attributes (e.g., `PACKED`, `WEAK`), compile-time checks, format annotations, tiny bit/byte helpers, and a few safe convenience functions. The implementation is minimal and suitable for bare-metal MCUs, RTOS targets, and hosted OS builds alike.&#x20;

---

## Features

* **Portability attributes/macros**

  * `PACKED`, `WEAK`, `NOINLINE`, `FALLTHROUGH`, `WARN_IF_UNUSED`, `NORETURN`, `FMT_PRINTF`, `FMT_SCANF`, `CLASS_NO_COPY`, etc.&#x20;
* **Bit/byte helpers**

  * `BIT_IS_SET`, `BIT_IS_SET_64`, `BIT_SET`, `BIT_CLEAR`, `LOWBYTE`, `HIGHBYTE`, `UINT16_VALUE`, `UINT32_VALUE`, `ARRAY_SIZE`, `STR_VALUE`.&#x20;
* **Compile-time size checks**

  * `ASSERT_STORAGE_SIZE(struct, size)` to enforce ABI layouts for packed structs.&#x20;
* **Tiny safe utilities**

  * `is_bounded_int32()` – range check for `int32_t`.&#x20;
  * `hex_to_uint8()` and `char_to_hex()` – ASCII ↔ nibble conversion.&#x20;
  * `strncpy_noterm()` – bounded copy that reports source length and copies the NUL if space allows.&#x20;
* **Overridable allocator hook**

  * `void* WEAK mem_realloc(void* ptr, size_t old_size, size_t new_size);`
    Default implementation wraps `realloc/free`; platforms may override.

---

## Files

* `Embed_Common.h` – header-only macros, templates, and declarations.&#x20;
* `Embed_Common.cpp` – tiny, standalone implementations for the helpers (no AP\_HAL or OS deps).&#x20;

---

## Requirements

* **Language:** C++11 or newer
* **Dependencies:** Standard C/C++ headers only (`<cstdint>`, `<cstdlib>`, `<cstring>`, `<type_traits>`, `<new>`). No AP\_HAL / AP\_Param / internal ArduPilot libs.

---

## Getting started

### 1) Add to your project

Place the files in a folder (e.g., `Embed_Common/`) and add the include path.

```cpp
#include "Embed_Common.h"
```

### 2) Example usage

#### Packed struct + size assertion

```cpp
struct PACKED Record {
    uint8_t  id;
    uint16_t len;
};
ASSERT_STORAGE_SIZE(Record, 3);
```

#### Bit operations

```cpp
uint32_t flags = 0;
BIT_SET(flags, 3);      // set bit 3
BIT_CLEAR(flags, 1);    // clear bit 1
bool set = BIT_IS_SET(flags, 3);
```

#### Bounded copy reporting source length

```cpp
char buf[8];
size_t src_len = strncpy_noterm(buf, "hello_world", sizeof(buf));
// buf contains "hello_w" + '\0'; src_len == 11
```

#### ASCII hex helpers

```cpp
uint8_t v;
bool ok = hex_to_uint8('B', v);  // v==11, ok==true
uint8_t n = char_to_hex('f');    // n==15 (255 if invalid)
```

#### Overriding `mem_realloc` (optional)

```cpp
// In a platform file you link before Embed_Common.cpp:
extern "C" void* mem_realloc(void* ptr, size_t old_size, size_t new_size) {
    (void)old_size;
    // route to your RTOS heap or custom pool here
    return my_custom_realloc(ptr, new_size);
}
```

*Default fallback wraps `std::realloc/free` and is provided in `Embed_Common.cpp`.*&#x20;

---

## Build

### Plain g++

```bash
g++ -std=c++11 -O2 -Wall -Wextra -IEmbed_Common \
    -c Embed_Common/Embed_Common.cpp
```

Link as part of your application or a static library.

### Example (Makefile snippet)

```make
CXX      := g++
CXXFLAGS := -std=c++11 -O2 -Wall -Wextra
INCLUDES := -I./Embed_Common

SRCS := main.cpp Embed_Common/Embed_Common.cpp
OBJS := $(SRCS:.cpp=.o)

all: app
app: $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) app
```

---

## Notes & portability

* **Weak symbols:** `WEAK` becomes a no-op on Cygwin/PE platforms; code still compiles, but link-time overrides are unavailable on that toolchain.&#x20;
* **Thread safety:** All helpers are stateless; functions are reentrant.
* **Embedded friendliness:** No dynamic allocation unless you call `mem_realloc()`. The default relies on your C runtime heap (or your override).

---

## Doxygen

The header is documented with Doxygen comments. To generate HTML docs:

1. `doxygen -g` (create `Doxyfile`)
2. Set:

   * `INPUT = Embed_Common/Embed_Common.h Embed_Common/Embed_Common.cpp`
   * `EXTRACT_ALL = YES`
3. `doxygen`

You’ll get cross-referenced docs for all macros and functions.&#x20;

---

## License

GNU GPLv3 (see file headers). If you need a more permissive license for this module, call it out and we can factor a dual-licensed subset.

---

## Contributing

* Keep the header dependency-free and OS-agnostic.
* Favor `constexpr`/templates for zero-overhead utilities.
* When adding a helper that can vary by platform, expose a sane default and allow an override via `WEAK` or compile-time macros.

---

## FAQ

**Q: Does this depend on AP\_HAL, AP\_Param, or AP\_InternalError?**
A: No. It’s a standalone utility layer using only the standard C/C++ library.

**Q: Why `strncpy_noterm` instead of `strlcpy`?**
A: `strlcpy` isn’t universally available; this function provides predictable, bounded copy behavior and returns the source length so you can detect truncation.&#x20;

**Q: Can I disable the weak allocator hook?**
A: You don’t need to do anything—if you don’t override `mem_realloc`, the default implementation in `Embed_Common.cpp` is used.&#x20;

---

