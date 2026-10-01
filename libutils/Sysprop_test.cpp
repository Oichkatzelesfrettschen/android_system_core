#include <android/dlext.h>
#include <dlfcn.h>
#include <gtest/gtest.h>
#include <stdio.h>
#include <unistd.h>
#include <utils/misc.h>
#include <vndksupport/linker.h>

#include <string>

extern "C" android_namespace_t* android_get_exported_namespace(const char* name);

namespace {

int local_notifications = 0;
int sphal_notifications = 0;
void* sphal_owner = nullptr;
bool close_owner_during_callback = false;
int callback_close_result = -1;
int local_only_notifications = 0;

void RecordExecutionContext() {
    FILE* context_file = fopen("/proc/self/attr/current", "re");
    ASSERT_NE(nullptr, context_file);
    char context[256] = {};
    char* result = fgets(context, sizeof(context), context_file);
    const int close_result = fclose(context_file);
    ASSERT_NE(nullptr, result);
    ASSERT_EQ(0, close_result);
    ::testing::Test::RecordProperty("selinux_context", context);
    ::testing::Test::RecordProperty("uid", std::to_string(getuid()));
}

void CountLocalNotification() { ++local_notifications; }

void CountSphalNotification() {
    ++sphal_notifications;
    if (close_owner_during_callback) {
        callback_close_result = dlclose(sphal_owner);
        sphal_owner = nullptr;
    }
}

}  // namespace

TEST(Sysprop, LocalNotification) {
    ASSERT_NO_FATAL_FAILURE(RecordExecutionContext());
    RecordProperty("has_sphal_namespace",
                   android_get_exported_namespace("sphal") != nullptr ? "true" : "false");
    android::add_sysprop_change_callback([] { ++local_only_notifications; }, 0);
    const int before = local_only_notifications;
    android::report_sysprop_change();
    ASSERT_EQ(before + 1, local_only_notifications);
}

TEST(Sysprop, LoadedSphalNotificationLifecycle) {
    ASSERT_NO_FATAL_FAILURE(RecordExecutionContext());
    Dl_info info = {};
    ASSERT_NE(0, dladdr(reinterpret_cast<void*>(android::report_sysprop_change), &info));
    RecordProperty("local_libutils_path", info.dli_fname);
    if (android_get_exported_namespace("sphal") == nullptr) {
        GTEST_SKIP() << "The process requires an exported SP-HAL namespace";
    }
    ASSERT_EQ(nullptr, android_get_loaded_sphal_library("libutils.so"));
    android::add_sysprop_change_callback(CountLocalNotification, 0);
    android::report_sysprop_change();
    ASSERT_EQ(1, local_notifications);
    ASSERT_EQ(0, sphal_notifications);
    ASSERT_EQ(nullptr, android_get_loaded_sphal_library("libutils.so"));

    using AddCallback = void (*)(void (*)(), int);
    for (int generation = 0; generation < 2; ++generation) {
        sphal_owner = android_load_sphal_library("libutils.so", RTLD_NOW);
        ASSERT_NE(nullptr, sphal_owner) << dlerror();
        auto add_callback = reinterpret_cast<AddCallback>(
            dlsym(sphal_owner, "_ZN7android27add_sysprop_change_callbackEPFvvEi"));
        auto sphal_report = reinterpret_cast<void (*)()>(
            dlsym(sphal_owner, "_ZN7android21report_sysprop_changeEv"));
        ASSERT_NE(nullptr, add_callback) << dlerror();
        ASSERT_NE(nullptr, sphal_report) << dlerror();
        add_callback(CountSphalNotification, 0);
        const int local_before = local_notifications;
        const int sphal_before = sphal_notifications;
        sphal_report();
        ASSERT_EQ(local_before, local_notifications);
        ASSERT_EQ(sphal_before + 1, sphal_notifications);
        close_owner_during_callback = true;
        android::report_sysprop_change();
        close_owner_during_callback = false;
        ASSERT_EQ(0, callback_close_result);
        ASSERT_EQ(nullptr, sphal_owner);
        ASSERT_EQ(local_before + 1, local_notifications);
        ASSERT_EQ(sphal_before + 2, sphal_notifications);
        ASSERT_EQ(nullptr, android_get_loaded_sphal_library("libutils.so"));
        android::report_sysprop_change();
        ASSERT_EQ(local_before + 2, local_notifications);
        ASSERT_EQ(sphal_before + 2, sphal_notifications);
    }
}
