/*
 * NXF Pro - host shim.
 *
 * Lets the real FTP server (src/ftp_server.cpp, unmodified) run on a PC for
 * protocol testing against a standard FTP client. Only the threading and
 * result-code primitives are provided; everything else is plain POSIX.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <pthread.h>

using u8 = std::uint8_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

struct Result {
    int value = 0;
};

inline bool R_SUCCEEDED(Result result) { return result.value == 0; }
inline bool R_FAILED(Result result) { return result.value != 0; }

struct Thread {
    pthread_t handle {};
    void (*entry)(void*) = nullptr;
    void* argument = nullptr;
};

inline Result threadCreate(Thread* thread, void (*entry)(void*), void* argument, void*, std::size_t,
                           int, int) {
    thread->entry = entry;
    thread->argument = argument;
    return Result {0};
}

inline Result threadStart(Thread* thread) {
    if (thread->entry == nullptr) {
        return Result {-1};
    }
    const int rc = pthread_create(
        &thread->handle, nullptr,
        [](void* data) -> void* {
            auto* self = static_cast<Thread*>(data);
            self->entry(self->argument);
            return nullptr;
        },
        thread);
    return Result {rc};
}

inline void threadWaitForExit(Thread* thread) { pthread_join(thread->handle, nullptr); }
inline void threadClose(Thread*) {}
