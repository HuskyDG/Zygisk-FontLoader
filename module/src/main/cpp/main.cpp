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
#include <sys/stat.h>
#include "logging.h"
#include "zygisk.hpp"
#include "misc.h"
#include "mountinfo.hpp"

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
        const char *rawProcess = env->GetStringUTFChars(args->nice_name, nullptr);
        if (rawProcess != nullptr) {
            std::string process(rawProcess);
            env->ReleaseStringUTFChars(args->nice_name, rawProcess);

            if (args->uid > 1000 && InitCompanion(process.data())) {
                PreloadFonts(env, fonts);
                HideFromMaps(fonts);
            }
        }
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

    void preServerSpecialize(ServerSpecializeArgs *args) override {
        api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
    }

private:
    Api *api{};
    JNIEnv *env{};
    std::vector<std::string> fonts;

    bool InitCompanion(const char *process) {
        struct stat st;
        if (stat("/data", &st))
            return false;
        uint32_t flags = api->getFlags();
        if ((flags & zygisk::PROCESS_ON_DENYLIST) == 0) return false;
        LOGI("[%s] is on denylist\n", process);
        auto s = parse_mount_info();
        for (auto mnt = s.begin(); mnt != s.end(); mnt++) {
            if (mnt->device == st.st_dev && (starts_with((mnt->target).data(), "/system/fonts/") || 
                starts_with((mnt->target).data(), "/system/product/fonts/") || starts_with((mnt->target).data(), "/system/vendor/fonts/") ||
                starts_with((mnt->target).data(), "/product/fonts/") || starts_with((mnt->target).data(), "/vendor/fonts/"))) {
                LOGI("Found Font file: %s\n", (mnt->target).data());
                fonts.emplace_back(mnt->target);
            }
        }
        return true;
    }
};

static void CompanionEntry(int socket) {
    return;
}

REGISTER_ZYGISK_MODULE(ZygiskModule)

REGISTER_ZYGISK_COMPANION(CompanionEntry)
