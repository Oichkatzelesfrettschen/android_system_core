/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef VNDKSUPPORT_LINKER_H_
#define VNDKSUPPORT_LINKER_H_

#ifdef __cplusplus
extern "C" {
#endif

int android_is_in_vendor_process() __attribute__((
        deprecated("This function would not give exact result if VNDK is deprecated.")));

// The loaded-only operation accepts a SONAME and acquires an existing instance
// without filesystem lookup. Use the flag alone, rather than combining RTLD flags.
enum { ANDROID_SPHAL_LIBRARY_LOADED_ONLY = 0x10000000 };

void* android_load_sphal_library(const char* name, int flag);

// Acquires an already loaded SONAME in the SP-HAL namespace or its permitted links.
// Processes without an exported vendor namespace use the caller namespace.
// Misses leave dlerror unchanged. Release each hit with android_unload_sphal_library.
static inline void* android_get_loaded_sphal_library(const char* soname) {
    return android_load_sphal_library(soname, ANDROID_SPHAL_LIBRARY_LOADED_ONLY);
}

int android_unload_sphal_library(void* handle);

#ifdef __cplusplus
}
#endif

#endif  // VNDKSUPPORT_LINKER_H_
