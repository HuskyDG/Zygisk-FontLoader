#include <jni.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <nativehelper/scoped_utf_chars.h>
#include <pmparser.h>
#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <dirent.h>
#include <private/ScopedReaddir.h>
#include "logging.h"
#include "zygisk.hpp"
#include "misc.h"

using zygisk::Api;
using zygisk::AppSpecializeArgs;
using zygisk::ServerSpecializeArgs;

static int GetProt(const procmaps_struct *procstruct) {
    int prot = 0;
    if (procstruct->is_r) {
        prot |= PROT_READ;
    }
    if (procstruct->is_w) {
        prot |= PROT_WRITE;
    }
    if (procstruct->is_x) {
        prot |= PROT_EXEC;
    }
    return prot;
}

static void HideFromMaps(const std::vector<std::string> &fonts) {
    std::unique_ptr<procmaps_iterator, decltype(&pmparser_free)> maps{pmparser_parse(-1), &pmparser_free};
    if (!maps) {
        LOGW("failed to parse /proc/self/maps");
        return;
    }

    for (procmaps_struct *i = pmparser_next(maps.get()); i; i = pmparser_next(maps.get())) {
        using namespace std::string_view_literals;
        std::string_view pathname = i->pathname;

        if (std::find(fonts.begin(), fonts.end(), pathname) == fonts.end()) continue;

        auto start = reinterpret_cast<uintptr_t>(i->addr_start);
        auto end = reinterpret_cast<uintptr_t>(i->addr_end);
        if (end <= start) continue;
        auto len = end - start;
        auto *bk = mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE,
                        -1, 0);
        if (bk == MAP_FAILED) continue;
        auto old_prot = GetProt(i);
        if (!i->is_r && mprotect(i->addr_start, len, old_prot | PROT_READ) != 0) {
            PLOGE("Failed to hide %*s from maps", static_cast<int>(pathname.size()),
                  pathname.data());
            continue;
        }
        memcpy(bk, i->addr_start, len);
        mremap(bk, len, len, MREMAP_FIXED | MREMAP_MAYMOVE, i->addr_start);
        mprotect(i->addr_start, len, old_prot);

        LOGV("Hide %*s from maps", static_cast<int>(pathname.size()), pathname.data());
    }
}

static void PreloadFonts(JNIEnv *env, const std::vector<std::string> &fonts) {
    auto typefaceClass = env->FindClass("android/graphics/Typeface");
    auto methodId = env->GetStaticMethodID(typefaceClass, "nativeWarmUpCache", "(Ljava/lang/String;)V");
    if (!methodId) {
        env->ExceptionClear();
        return;
    }

    for (const std::string &font : fonts) {
        env->CallStaticVoidMethod(typefaceClass, methodId, env->NewStringUTF(font.c_str()));
        if (env->ExceptionCheck()) {
            LOGW("Preload font %s failed", font.c_str());
            env->ExceptionDescribe();
            env->ExceptionClear();
        } else {
            LOGV("Preloaded font %s", font.c_str());
        }
    }
}

class ZygiskModule : public zygisk::ModuleBase {
public:
    void onLoad(Api *_api, JNIEnv *_env) override {
        this->api = _api;
        this->env = _env;
    }

    void preAppSpecialize(AppSpecializeArgs *args) override {
        InitCompanion();
        PreloadFonts(env, fonts);
        HideFromMaps(fonts);

        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

    void preServerSpecialize(ServerSpecializeArgs *args) override {
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api{};
    JNIEnv *env{};
    std::vector<std::string> fonts;

    void InitCompanion() {
        auto companion = api->connectCompanion();
        if (companion == -1) {
            LOGE("Failed to connect to companion");
            return;
        }

        char path[PATH_MAX]{};
        auto size = read_int(companion);
        for (int i = 0; i < size; ++i) {
            auto string_size = read_int(companion);
            read_full(companion, path, string_size);
            fonts.emplace_back(path);
        }

        close(companion);
    }
};

static bool PrepareCompanion(std::vector<std::string> &fonts) {
    bool result = false;
    char path[PATH_MAX]{};
    struct dirent *entry;

    ScopedReaddir modules("/data/adb/modules/");
    if (modules.IsBad()) goto clean;

    while ((entry = modules.ReadEntry())) {
        if (entry->d_type != DT_DIR) continue;
        if (entry->d_name[0] == '.') continue;

        snprintf(path, PATH_MAX, "/data/adb/modules/%s/disable", entry->d_name);
        if (access(path, F_OK) == 0) {
            LOGV("Module %s is disabled", entry->d_name);
            continue;
        }

        snprintf(path, PATH_MAX, "/data/adb/modules/%s/system/fonts", entry->d_name);
        if (access(path, F_OK) != 0) {
            LOGV("Module %s does not contain font", entry->d_name);
            continue;
        }

        ScopedReaddir dir(path);
        if (dir.IsBad()) {
            LOGW("Cannot open %s", path);
            continue;
        }

        while ((entry = dir.ReadEntry())) {
            if (entry->d_type != DT_REG) continue;
            if (entry->d_name[0] == '.') continue;

            snprintf(path, PATH_MAX, "/system/fonts/%s", entry->d_name);
            if (access(path, F_OK) == 0) {
                fonts.emplace_back(path);
                LOGI("Collected font %s", path);
            } else {
                LOGW("Font %s does not exist", path);
            }
        }
    }

    result = true;

    clean:
    return result;
}

static void CompanionEntry(int socket) {
    static std::vector<std::string> fonts;
    static auto prepare = PrepareCompanion(fonts);

    write_int(socket, fonts.size());

    for (const std::string &font: fonts) {
        auto size = font.size();
        auto array = font.c_str();
        write_int(socket, size);
        write_full(socket, array, size);
    }

    close(socket);
}

REGISTER_ZYGISK_MODULE(ZygiskModule)

REGISTER_ZYGISK_COMPANION(CompanionEntry)
