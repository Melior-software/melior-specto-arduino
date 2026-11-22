// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Daniel Kuběna (Melior) – see LICENSE and NOTICE
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "SpectoWs.h"

namespace specto {

class Http {
public:
    Http(Io& io, MillisFn millis) : io(io), millis(millis) {}

    int post(const char* host, const char* path, const char* key, const char* body, size_t n,
             char* out, size_t cap, size_t& len, bool& keepOpen, uint32_t timeoutMs = 8000);

private:
    int readByte(uint32_t until);
    Io& io;
    MillisFn millis;
};

}
