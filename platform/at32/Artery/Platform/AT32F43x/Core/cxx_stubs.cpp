/**
 * @file cxx_stubs.cpp
 * @brief C++ runtime stubs to reduce code size
 *
 * This file provides minimal implementations of C++ runtime functions
 * to avoid linking the full libstdc++ demangle code (~18KB).
 */

#include <cstdlib>

namespace __gnu_cxx {

/**
 * @brief Minimal terminate handler
 *
 * Replaces __gnu_cxx::__verbose_terminate_handler which pulls in
 * the entire C++ demangle library (~18KB) for printing exception names.
 *
 * In embedded systems, we just halt or reset on unhandled exceptions.
 */
[[noreturn]] void __verbose_terminate_handler()
{
    // Trigger a breakpoint in debug mode
    __asm volatile("bkpt #0");

    // Infinite loop (should not reach here)
    while (1) {
        __asm volatile("nop");
    }
}

} // namespace __gnu_cxx

extern "C" {

/**
 * @brief Stub for __cxa_demangle
 *
 * Returns NULL to indicate demangling is not supported.
 * This prevents the linker from pulling in the demangle code.
 */
char* __wrap___cxa_demangle(
    const char* /*mangled_name*/,
    char* /*output_buffer*/,
    size_t* /*length*/,
    int* status)
{
    if (status) {
        *status = -1; // Indicate failure
    }
    return nullptr;
}

} // extern "C"
