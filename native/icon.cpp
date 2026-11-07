#include <jni.h>
#include <android/log.h>
#include <sys/system_properties.h>
#include <elf.h>
#include <cstring>
#include <string>
#include <string_view>
#include "dobby.h"
#include "xdl.h"
import lsplant;
#include "art_symbols.inc"
static bool ready=false;
static void* art=nullptr;
static void* skipped=nullptr;
static void* resolve(std::string_view name) {
 std::string n(name); auto p=xdl_sym(art,n.c_str(),nullptr);
 return p?p:xdl_dsym(art,n.c_str(),nullptr);
}
static std::string buildid() {
 xdl_info_t info{}; if(xdl_info(art,XDL_DI_DLINFO,&info)) return {};
 for(size_t i=0;i<info.dlpi_phnum;i++) {
  auto& p=info.dlpi_phdr[i]; if(p.p_type!=PT_NOTE) continue;
  auto base=reinterpret_cast<const unsigned char*>(info.dli_fbase);
  const unsigned char *cur=base+p.p_vaddr,*end=cur+p.p_memsz;
  while(end-cur>=12) {
   Elf64_Nhdr n; memcpy(&n,cur,12); cur+=12;
   size_t ns=(size_t(n.n_namesz)+3)&~size_t(3), ds=(size_t(n.n_descsz)+3)&~size_t(3);
   if(ns>size_t(end-cur)||ds>size_t(end-cur)-ns) break;
   if(n.n_type==NT_GNU_BUILD_ID && n.n_namesz==4 && !memcmp(cur,"GNU",4)) {
    std::string out; const char* hex="0123456789abcdef";
    for(size_t j=0;j<n.n_descsz;j++){auto v=cur[ns+j];out+=hex[v>>4];out+=hex[v&15];} return out;
   } cur+=ns+ds;
  }
 } return {};
}
static bool initialize(JNIEnv* env) {
 char sdk[PROP_VALUE_MAX]{}; __system_property_get("ro.build.version.sdk",sdk);
 if(strcmp(sdk,"36")) return false;
 art=xdl_open("libart.so",XDL_DEFAULT); if(!art) return false;
 if(buildid()!=expected_build_id) {__android_log_print(ANDROID_LOG_ERROR,"R12Cache","ART identity mismatch");return false;}
 skipped=resolve("_ZN3art11ClassLinker26VisiblyInitializedCallback22MarkVisiblyInitializedEPNS_6ThreadE");
 lsplant::InitInfo info;
 info.inline_hooker=[](void* target,void* replacement)->void* {
  if(target==skipped && skipped) return nullptr;
  void* backup=nullptr; return DobbyHook(target,replacement,&backup)==0?backup:nullptr;
 };
 info.inline_unhooker=[](void* target){return DobbyDestroy(target)==0;};
 info.art_symbol_resolver=resolve;
 info.art_symbol_prefix_resolver=[](std::string_view prefix)->void* {
  for(auto name:art_names) if(std::string_view(name).starts_with(prefix)) if(auto p=resolve(name)) return p;
  return nullptr;
 };
 ready=lsplant::Init(env,info);
 __android_log_print(ANDROID_LOG_INFO,"R12Cache","initialized=%d",ready);
 return ready;
}
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <vector>
#include "zygisk.hpp"
static std::vector<unsigned char> dex;
static jobject attach(JNIEnv* e,jclass,jobject hooker){
 auto c=e->FindClass("android/provider/Settings$System");
 auto m=e->GetStaticMethodID(c,"getStringForUser","(Landroid/content/ContentResolver;Ljava/lang/String;I)Ljava/lang/String;");if(!m)return nullptr;
 auto cb=e->GetMethodID(e->GetObjectClass(hooker),"callback","([Ljava/lang/Object;)Ljava/lang/Object;");if(!cb)return nullptr;
 return lsplant::Hook(e,e->ToReflectedMethod(c,m,JNI_TRUE),hooker,e->ToReflectedMethod(e->GetObjectClass(hooker),cb,JNI_FALSE));
}
static jobject application(JNIEnv* e,jclass){
 auto c=e->FindClass("android/app/ActivityThread");auto m=e->GetStaticMethodID(c,"currentApplication","()Landroid/app/Application;");return m?e->CallStaticObjectMethod(c,m):nullptr;
}
static jint currentUser(JNIEnv* e,jclass){
 auto c=e->FindClass("android/app/ActivityManager");auto m=e->GetStaticMethodID(c,"getCurrentUser","()I");return m?e->CallStaticIntMethod(c,m):-10000;
}
static void observe(JNIEnv* e,jclass,jobject cr,jobject uri,jobject observer){
 auto c=e->FindClass("android/content/ContentResolver");auto m=e->GetMethodID(c,"registerContentObserver","(Landroid/net/Uri;ZLandroid/database/ContentObserver;I)V");if(m)e->CallVoidMethod(cr,m,uri,JNI_FALSE,observer,-1);
}
#include <sys/socket.h>
#include <errno.h>
static bool receiveAll(int fd,void* data,size_t size){auto p=static_cast<unsigned char*>(data);while(size){ssize_t n=read(fd,p,size);if(n<0&&errno==EINTR)continue;if(n<=0)return false;p+=n;size-=n;}return true;}
static bool sendAll(int fd,const void* data,size_t size){auto p=static_cast<const unsigned char*>(data);while(size){ssize_t n=send(fd,p,size,MSG_NOSIGNAL);if(n<0&&errno==EINTR)continue;if(n<=0)return false;p+=n;size-=n;}return true;}
static void companion(int socket){
 uint32_t size=0;std::vector<unsigned char> bytes;
 int dir=open("/data/adb/modules/redmi12_performance",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
 if(dir>=0 && unlinkat(dir,"icon.armed",0)==0){
  int fd=openat(dir,"icon.dex",O_RDONLY|O_CLOEXEC);struct stat st{};
  if(fd>=0 && !fstat(fd,&st) && st.st_size>=64 && st.st_size<=1048576){bytes.resize(st.st_size);if(receiveAll(fd,bytes.data(),bytes.size()))size=bytes.size();}
  if(fd>=0)close(fd);
 }
 if(dir>=0)close(dir);
 if(sendAll(socket,&size,sizeof(size)) && size)sendAll(socket,bytes.data(),size);
}
class CacheModule:public zygisk::ModuleBase{
 zygisk::Api* api{};JNIEnv* env{};bool target=false;
 public:
 void onLoad(zygisk::Api* a,JNIEnv* e)override{api=a;env=e;}
 void preServerSpecialize(zygisk::ServerSpecializeArgs*)override{api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);}
 void preAppSpecialize(zygisk::AppSpecializeArgs* args)override{
  char device[PROP_VALUE_MAX]{};__system_property_get("ro.product.device",device);
  const char* name=env->GetStringUTFChars(args->nice_name,nullptr);
  bool named=!strcmp(name,"com.android.systemui");
  if(named)__android_log_print(ANDROID_LOG_INFO,"R12IconCache","pre uid=%d device=%s",args->uid,device);
  target=args->uid==10281 && named && !strcmp(device,"heat");
  env->ReleaseStringUTFChars(args->nice_name,name);
  if(target){
   int fd=api->connectCompanion();uint32_t size=0;unsigned char selector=1;
   if(fd>=0 && !sendAll(fd,&selector,1)){close(fd);fd=-1;}
   target=fd>=0 && receiveAll(fd,&size,sizeof(size)) && size>=64 && size<=1048576;
   if(target){dex.resize(size);target=receiveAll(fd,dex.data(),size);}
   if(fd>=0)close(fd);
   __android_log_print(ANDROID_LOG_INFO,"R12IconCache","one-attempt guard=%d dex=%u",target,size);
  }
  if(!target)api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
 }
 void postAppSpecialize(const zygisk::AppSpecializeArgs*)override{
  if(!target)return;
  // Never dlclose after attempting ART initialization, even after failure.
  if(!initialize(env)){__android_log_print(ANDROID_LOG_ERROR,"R12IconCache","ART init refused");if(env->ExceptionCheck())env->ExceptionClear();return;}
  auto cl=env->FindClass("java/lang/ClassLoader");auto get=env->GetStaticMethodID(cl,"getSystemClassLoader","()Ljava/lang/ClassLoader;");auto parent=env->CallStaticObjectMethod(cl,get);
  auto type=env->FindClass("dalvik/system/InMemoryDexClassLoader");auto ctor=env->GetMethodID(type,"<init>","(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
  auto buffer=env->NewDirectByteBuffer(dex.data(),dex.size());auto loader=env->NewObject(type,ctor,buffer,parent);
  if(env->ExceptionCheck()){env->ExceptionDescribe();env->ExceptionClear();return;}
  auto load=env->GetMethodID(cl,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;");auto cache=static_cast<jclass>(env->CallObjectMethod(loader,load,env->NewStringUTF("local.redmi12.iconcache.IconCache")));
  if(env->ExceptionCheck()||!cache){env->ExceptionDescribe();env->ExceptionClear();return;}
  JNINativeMethod methods[]={
   {const_cast<char*>("attach"),const_cast<char*>("(Ljava/lang/Object;)Ljava/lang/reflect/Method;"),reinterpret_cast<void*>(attach)},
   {const_cast<char*>("application"),const_cast<char*>("()Landroid/app/Application;"),reinterpret_cast<void*>(application)},
   {const_cast<char*>("currentUser"),const_cast<char*>("()I"),reinterpret_cast<void*>(currentUser)},
   {const_cast<char*>("observe"),const_cast<char*>("(Landroid/content/ContentResolver;Landroid/net/Uri;Landroid/database/ContentObserver;)V"),reinterpret_cast<void*>(observe)}};
  if(env->RegisterNatives(cache,methods,4)!=JNI_OK){env->ExceptionDescribe();env->ExceptionClear();return;}
  auto install=env->GetStaticMethodID(cache,"install","()V");if(install)env->CallStaticVoidMethod(cache,install);
  if(env->ExceptionCheck()){env->ExceptionDescribe();env->ExceptionClear();__android_log_print(ANDROID_LOG_ERROR,"R12IconCache","install failed; process restart skips module");}
 }
};




zygisk::ModuleBase* make_icon() { return new CacheModule(); }
void icon_companion(int fd) { companion(fd); }
