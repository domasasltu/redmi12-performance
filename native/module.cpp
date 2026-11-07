#include <sys/types.h>
#include "zygisk.hpp"
#include <cstring>
#include <unistd.h>
#include <errno.h>
zygisk::ModuleBase* make_icon();
zygisk::ModuleBase* make_radio();
void icon_companion(int);
void radio_companion(int);
class PerformanceModule final : public zygisk::ModuleBase {
    zygisk::Api* api{};
    JNIEnv* env{};
    zygisk::ModuleBase* selected{};
public:
    void onLoad(zygisk::Api* a, JNIEnv* e) override { api=a; env=e; }
    void preServerSpecialize(zygisk::ServerSpecializeArgs*) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }
    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        const char* name=env->GetStringUTFChars(args->nice_name,nullptr);
        if (!name) { env->ExceptionClear(); api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY); return; }
        if(args->uid==10281 && !strcmp(name,"com.android.systemui")) selected=make_icon();
        else if(args->uid==1001 && !strcmp(name,"com.android.phone")) selected=make_radio();
        env->ReleaseStringUTFChars(args->nice_name,name);
        if(selected) { selected->onLoad(api,env); selected->preAppSpecialize(args); }
        else api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }
    void postAppSpecialize(const zygisk::AppSpecializeArgs* args) override {
        if(selected) selected->postAppSpecialize(args);
    }
};
static void companion(int fd) {
    unsigned char selector=0; ssize_t n;
    do { n=read(fd,&selector,1); } while(n<0 && errno==EINTR);
    if(n!=1) return;
    if(selector==1) icon_companion(fd);
    else if(selector==2) radio_companion(fd);
}
REGISTER_ZYGISK_MODULE(PerformanceModule)
REGISTER_ZYGISK_COMPANION(companion)
