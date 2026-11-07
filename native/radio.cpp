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
static jclass rilClass(JNIEnv* e){return e->FindClass("com/android/internal/telephony/RIL");}
static jclass configClass(JNIEnv* e){return e->FindClass("com/android/internal/telephony/RadioConfig");}
static jobject attach(JNIEnv* e,jclass,jint kind,jobject hooker){
 if(!ready)return nullptr;
 auto c=kind==0?rilClass(e):kind==1?configClass(e):e->FindClass("com/android/internal/telephony/data/PhoneSwitcher");if(!c)return nullptr;
 auto target=e->GetMethodID(c,kind==0?"getRadioServiceProxy":kind==1?"updateRadioConfigProxy":"onDdsSwitchResponse",kind==0?"(I)Lcom/android/internal/telephony/RadioServiceProxy;":kind==1?"()V":"(Landroid/os/AsyncResult;)V");if(!target)return nullptr;
 auto cb=e->GetMethodID(e->GetObjectClass(hooker),"callback","([Ljava/lang/Object;)Ljava/lang/Object;");if(!cb)return nullptr;
 auto reflected=e->ToReflectedMethod(c,target,JNI_FALSE);
 auto result=lsplant::Hook(e,reflected,hooker,e->ToReflectedMethod(e->GetObjectClass(hooker),cb,JNI_FALSE));
 if(kind==1 && result){auto getter=e->GetMethodID(c,"getRadioConfigProxy","(Landroid/os/Message;)Lcom/android/internal/telephony/RadioConfigProxy;");if(getter){bool ok=lsplant::Deoptimize(e,e->ToReflectedMethod(c,getter,JNI_FALSE));__android_log_print(ANDROID_LOG_INFO,"R12RadioStartup","config caller deopt=%d",ok);}else e->ExceptionClear();}
 if(kind==2 && result){auto caller=e->GetMethodID(c,"handleMessage","(Landroid/os/Message;)V");if(caller){bool ok=lsplant::Deoptimize(e,e->ToReflectedMethod(c,caller,JNI_FALSE));__android_log_print(ANDROID_LOG_INFO,"R12RadioStartup","DDS caller deopt=%d",ok);}else e->ExceptionClear();}
 return result;
}
static bool declaredMissing(JNIEnv* e,jstring name){
 auto c=e->FindClass("android/os/ServiceManager");if(!c)return false;
 auto declared=e->GetStaticMethodID(c,"isDeclared","(Ljava/lang/String;)Z");auto check=e->GetStaticMethodID(c,"checkService","(Ljava/lang/String;)Landroid/os/IBinder;");if(!declared||!check)return false;
 bool yes=e->CallStaticBooleanMethod(c,declared,name);if(e->ExceptionCheck()||!yes)return false;
 auto binder=e->CallStaticObjectMethod(c,check,name);if(e->ExceptionCheck())return false;
 return binder==nullptr;
}
static jboolean proxyEmpty(JNIEnv* e,jclass,jobject proxy){if(!proxy)return true;auto m=e->GetMethodID(e->GetObjectClass(proxy),"isEmpty","()Z");return m?e->CallBooleanMethod(proxy,m):false;}
static jobjectArray missingRil(JNIEnv* e,jclass,jobject ril,jint service){
 const char* descriptors[]={nullptr,"android.hardware.radio.data.IRadioData","android.hardware.radio.messaging.IRadioMessaging","android.hardware.radio.modem.IRadioModem","android.hardware.radio.network.IRadioNetwork","android.hardware.radio.sim.IRadioSim","android.hardware.radio.voice.IRadioVoice","android.hardware.radio.ims.IRadioIms"};
 if(service<1||service>7)return nullptr;
 auto c=rilClass(e);if(!c)return nullptr;
 auto phoneField=e->GetFieldID(c,"mPhoneId","Ljava/lang/Integer;");auto proxiesField=e->GetFieldID(c,"mServiceProxies","Landroid/util/SparseArray;");if(!phoneField||!proxiesField)return nullptr;
 auto phone=e->GetObjectField(ril,phoneField);auto proxies=e->GetObjectField(ril,proxiesField);if(!phone||!proxies)return nullptr;
 auto intValue=e->GetMethodID(e->GetObjectClass(phone),"intValue","()I");auto get=e->GetMethodID(e->GetObjectClass(proxies),"get","(I)Ljava/lang/Object;");if(!intValue||!get)return nullptr;
 int slot=e->CallIntMethod(phone,intValue)+1;if(e->ExceptionCheck()||slot<1||slot>2)return nullptr;
 auto proxy=e->CallObjectMethod(proxies,get,service);if(e->ExceptionCheck()||!proxy)return nullptr;
 if(!proxyEmpty(e,nullptr,proxy)||e->ExceptionCheck())return nullptr;
 std::string name=std::string(descriptors[service])+"/slot"+std::to_string(slot);auto key=e->NewStringUTF(name.c_str());
 if(!declaredMissing(e,key)||e->ExceptionCheck())return nullptr;
 auto object=e->FindClass("java/lang/Object");auto out=e->NewObjectArray(2,object,nullptr);e->SetObjectArrayElement(out,0,proxy);e->SetObjectArrayElement(out,1,key);return out;
}
static jstring missingConfig(JNIEnv* e,jclass){auto key=e->NewStringUTF("android.hardware.radio.config.IRadioConfig/default");return declaredMissing(e,key)?key:nullptr;}
static jclass callbackInterface(JNIEnv* e,jclass){return e->FindClass("android/os/IServiceCallback");}
static void registerService(JNIEnv* e,jclass,jstring name,jobject callback){auto c=e->FindClass("android/os/ServiceManager");if(!c)return;auto m=e->GetStaticMethodID(c,"registerForNotifications","(Ljava/lang/String;Landroid/os/IServiceCallback;)V");if(m)e->CallStaticVoidMethod(c,m,name,callback);}
static jboolean awaitService(JNIEnv* e,jclass,jstring name){auto c=e->FindClass("android/os/ServiceManager");if(!c)return false;auto m=e->GetStaticMethodID(c,"waitForDeclaredService","(Ljava/lang/String;)Landroid/os/IBinder;");if(!m)return false;auto value=e->CallStaticObjectMethod(c,m,name);return !e->ExceptionCheck()&&value;}
static jboolean validateAbi(JNIEnv* e,jclass){
 auto tm=e->FindClass("android/telephony/TelephonyManager");if(!tm)return false;
 const char* ids[]={"HAL_SERVICE_DATA","HAL_SERVICE_MESSAGING","HAL_SERVICE_MODEM","HAL_SERVICE_NETWORK","HAL_SERVICE_SIM","HAL_SERVICE_VOICE","HAL_SERVICE_IMS"};
 for(int i=0;i<7;i++){auto f=e->GetStaticFieldID(tm,ids[i],"I");if(!f||e->GetStaticIntField(tm,f)!=i+1)return false;}
 auto rc=rilClass(e);auto cc=configClass(e);if(!rc||!cc)return false;
 if(!e->GetFieldID(rc,"mPhoneId","Ljava/lang/Integer;")||!e->GetFieldID(rc,"mServiceProxies","Landroid/util/SparseArray;"))return false;
 if(!e->GetMethodID(rc,"getRadioServiceProxy","(I)Lcom/android/internal/telephony/RadioServiceProxy;")||!e->GetMethodID(cc,"updateRadioConfigProxy","()V"))return false;
 return !e->ExceptionCheck();
}
static jobject retryRil(JNIEnv* e,jclass,jobject ril,jint service){auto c=rilClass(e);if(!c)return nullptr;auto m=e->GetMethodID(c,"getRadioServiceProxy","(I)Lcom/android/internal/telephony/RadioServiceProxy;");if(!m)return nullptr;auto value=e->CallObjectMethod(ril,m,service);return e->ExceptionCheck()?nullptr:value;}
static void retryConfig(JNIEnv* e,jclass,jobject config){auto c=configClass(e);if(!c)return;auto m=e->GetMethodID(c,"updateRadioConfigProxy","()V");if(m)e->CallVoidMethod(config,m);}
static jintArray backoff(JNIEnv* e,jclass,jobject switcher,jobject result){
 if(!result)return nullptr;
 auto c=e->FindClass("com/android/internal/telephony/data/PhoneSwitcher");if(!c)return nullptr;
 auto emergency=e->GetFieldID(c,"mEmergencyOverride","Lcom/android/internal/telephony/data/PhoneSwitcher$EmergencyOverrideRequest;");
 auto mode=e->GetFieldID(c,"mHalCommandToUse","I");auto allow=e->GetStaticFieldID(c,"HAL_COMMAND_ALLOW_DATA","I");if(!emergency||!mode||!allow)return nullptr;
 if(e->GetObjectField(switcher,emergency)||e->GetIntField(switcher,mode)!=e->GetStaticIntField(c,allow))return nullptr;
 auto ar=e->FindClass("android/os/AsyncResult");auto exception=e->GetFieldID(ar,"exception","Ljava/lang/Throwable;");auto user=e->GetFieldID(ar,"userObj","Ljava/lang/Object;");if(!exception||!user)return nullptr;
 auto error=e->GetObjectField(result,exception);auto id=e->GetObjectField(result,user);if(!error||!id)return nullptr;
 auto ce=e->FindClass("com/android/internal/telephony/CommandException");auto integer=e->FindClass("java/lang/Integer");if(!e->IsInstanceOf(error,ce)||!e->IsInstanceOf(id,integer))return nullptr;
 auto getError=e->GetMethodID(ce,"getCommandError","()Lcom/android/internal/telephony/CommandException$Error;");if(!getError)return nullptr;
 auto commandError=e->CallObjectMethod(error,getError);if(e->ExceptionCheck()||!commandError)return nullptr;
 auto enumClass=e->FindClass("java/lang/Enum");auto name=e->GetMethodID(enumClass,"name","()Ljava/lang/String;");auto value=static_cast<jstring>(e->CallObjectMethod(commandError,name));if(e->ExceptionCheck()||!value)return nullptr;
 const char* text=e->GetStringUTFChars(value,nullptr);bool unavailable=!strcmp(text,"RADIO_NOT_AVAILABLE");e->ReleaseStringUTFChars(value,text);if(!unavailable)return nullptr;
 auto intValue=e->GetMethodID(integer,"intValue","()I");int phone=e->CallIntMethod(id,intValue);
 auto historyField=e->GetFieldID(c,"mCurrentDdsSwitchFailure","Ljava/util/List;");auto history=e->GetObjectField(switcher,historyField);if(e->ExceptionCheck()||!history)return nullptr;
 auto list=e->FindClass("java/util/List");auto size=e->GetMethodID(list,"size","()I");auto get=e->GetMethodID(list,"get","(I)Ljava/lang/Object;");if(phone<0||phone>=e->CallIntMethod(history,size))return nullptr;
 auto errors=e->CallObjectMethod(history,get,phone);if(e->ExceptionCheck()||!errors)return nullptr;
 auto set=e->FindClass("java/util/Set");auto add=e->GetMethodID(set,"add","(Ljava/lang/Object;)Z");e->CallBooleanMethod(errors,add,commandError);if(e->ExceptionCheck())return nullptr;
 // Constants are inlined in the exact SHA-guarded device DEX: event0x71, delay0x1388.
 jint args[]={phone,113,5000};auto out=e->NewIntArray(3);e->SetIntArrayRegion(out,0,3,args);return out;
}
static jboolean testBackoff(JNIEnv* e,jclass){
 auto c=e->FindClass("com/android/internal/telephony/data/PhoneSwitcher");if(!c)return false;auto target=e->AllocObject(c);
 auto mode=e->GetFieldID(c,"mHalCommandToUse","I");auto historyField=e->GetFieldID(c,"mCurrentDdsSwitchFailure","Ljava/util/List;");auto emergencyField=e->GetFieldID(c,"mEmergencyOverride","Lcom/android/internal/telephony/data/PhoneSwitcher$EmergencyOverrideRequest;");
 auto allow=e->GetStaticFieldID(c,"HAL_COMMAND_ALLOW_DATA","I");auto preferred=e->GetStaticFieldID(c,"HAL_COMMAND_PREFERRED_DATA","I");if(!mode||!historyField||!emergencyField||!allow||!preferred)return false;
 auto listClass=e->FindClass("java/util/ArrayList");auto setClass=e->FindClass("java/util/HashSet");auto list=e->NewObject(listClass,e->GetMethodID(listClass,"<init>","()V"));auto set=e->NewObject(setClass,e->GetMethodID(setClass,"<init>","()V"));e->CallBooleanMethod(list,e->GetMethodID(listClass,"add","(Ljava/lang/Object;)Z"),set);e->SetObjectField(target,historyField,list);e->SetIntField(target,mode,e->GetStaticIntField(c,allow));
 auto errorClass=e->FindClass("com/android/internal/telephony/CommandException$Error");auto error=e->GetStaticObjectField(errorClass,e->GetStaticFieldID(errorClass,"RADIO_NOT_AVAILABLE","Lcom/android/internal/telephony/CommandException$Error;"));
 auto ceClass=e->FindClass("com/android/internal/telephony/CommandException");auto ctor=e->GetMethodID(ceClass,"<init>","(Lcom/android/internal/telephony/CommandException$Error;)V");auto exception=e->NewObject(ceClass,ctor,error);
 auto integer=e->FindClass("java/lang/Integer");auto zero=e->CallStaticObjectMethod(integer,e->GetStaticMethodID(integer,"valueOf","(I)Ljava/lang/Integer;"),0);
 auto arClass=e->FindClass("android/os/AsyncResult");auto ar=e->NewObject(arClass,e->GetMethodID(arClass,"<init>","(Ljava/lang/Object;Ljava/lang/Object;Ljava/lang/Throwable;)V"),zero,nullptr,exception);auto exceptionField=e->GetFieldID(arClass,"exception","Ljava/lang/Throwable;");if(e->ExceptionCheck())return false;
 auto retry=backoff(e,nullptr,target,ar);if(e->ExceptionCheck()||!retry||e->GetArrayLength(retry)!=3)return false;jint args[3];e->GetIntArrayRegion(retry,0,3,args);if(args[0]!=0||args[1]!=113||args[2]!=5000)return false;
 if(!e->CallBooleanMethod(set,e->GetMethodID(setClass,"contains","(Ljava/lang/Object;)Z"),error))return false;
 e->SetObjectField(ar,exceptionField,nullptr);if(backoff(e,nullptr,target,ar)!=nullptr||e->ExceptionCheck())return false;
 auto other=e->GetStaticObjectField(errorClass,e->GetStaticFieldID(errorClass,"INVALID_SIM_STATE","Lcom/android/internal/telephony/CommandException$Error;"));e->SetObjectField(ar,exceptionField,e->NewObject(ceClass,ctor,other));if(backoff(e,nullptr,target,ar)!=nullptr||e->ExceptionCheck())return false;
 e->SetObjectField(ar,exceptionField,exception);e->SetIntField(target,mode,e->GetStaticIntField(c,preferred));if(backoff(e,nullptr,target,ar)!=nullptr||e->ExceptionCheck())return false;
 e->SetIntField(target,mode,e->GetStaticIntField(c,allow));auto emergencyClass=e->FindClass("com/android/internal/telephony/data/PhoneSwitcher$EmergencyOverrideRequest");e->SetObjectField(target,emergencyField,e->AllocObject(emergencyClass));if(backoff(e,nullptr,target,ar)!=nullptr||e->ExceptionCheck())return false;
 return true;
}
static bool registerHelpers(JNIEnv* e,jclass c){
 JNINativeMethod methods[]={
 {const_cast<char*>("testBackoff"),const_cast<char*>("()Z"),reinterpret_cast<void*>(testBackoff)},
 {const_cast<char*>("backoff"),const_cast<char*>("(Ljava/lang/Object;Ljava/lang/Object;)[I"),reinterpret_cast<void*>(backoff)},
 {const_cast<char*>("retryRil"),const_cast<char*>("(Ljava/lang/Object;I)Ljava/lang/Object;"),reinterpret_cast<void*>(retryRil)},
 {const_cast<char*>("retryConfig"),const_cast<char*>("(Ljava/lang/Object;)V"),reinterpret_cast<void*>(retryConfig)},
 {const_cast<char*>("validateAbi"),const_cast<char*>("()Z"),reinterpret_cast<void*>(validateAbi)},
 {const_cast<char*>("attach"),const_cast<char*>("(ILjava/lang/Object;)Ljava/lang/reflect/Method;"),reinterpret_cast<void*>(attach)},
 {const_cast<char*>("missingRil"),const_cast<char*>("(Ljava/lang/Object;I)[Ljava/lang/Object;"),reinterpret_cast<void*>(missingRil)},
 {const_cast<char*>("missingConfig"),const_cast<char*>("()Ljava/lang/String;"),reinterpret_cast<void*>(missingConfig)},
 {const_cast<char*>("proxyEmpty"),const_cast<char*>("(Ljava/lang/Object;)Z"),reinterpret_cast<void*>(proxyEmpty)},
 {const_cast<char*>("callbackInterface"),const_cast<char*>("()Ljava/lang/Class;"),reinterpret_cast<void*>(callbackInterface)},
 {const_cast<char*>("register"),const_cast<char*>("(Ljava/lang/String;Ljava/lang/Object;)V"),reinterpret_cast<void*>(registerService)},
 {const_cast<char*>("await"),const_cast<char*>("(Ljava/lang/String;)Z"),reinterpret_cast<void*>(awaitService)}};
 return e->RegisterNatives(c,methods,12)==JNI_OK;
}
// Root-only transport test loads this without initializing ART or attaching hooks.
#include <sys/socket.h>
#include <errno.h>
static bool receiveAll(int fd,void* data,size_t size){auto p=static_cast<unsigned char*>(data);while(size){ssize_t n=read(fd,p,size);if(n<0&&errno==EINTR)continue;if(n<=0)return false;p+=n;size-=n;}return true;}
static bool sendAll(int fd,const void* data,size_t size){auto p=static_cast<const unsigned char*>(data);while(size){ssize_t n=send(fd,p,size,MSG_NOSIGNAL);if(n<0&&errno==EINTR)continue;if(n<=0)return false;p+=n;size-=n;}return true;}
static void companion(int socket){
 uint32_t size=0;std::vector<unsigned char> bytes;
 int dir=open("/data/adb/modules/redmi12_performance",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
 if(dir>=0 && renameat(dir,"radio.armed",dir,"radio.attempted")==0){
  int fd=openat(dir,"radio.dex",O_RDONLY|O_CLOEXEC);struct stat st{};
  if(fd>=0 && !fstat(fd,&st) && st.st_size>=64 && st.st_size<=1048576){bytes.resize(st.st_size);if(receiveAll(fd,bytes.data(),bytes.size()))size=bytes.size();}
  if(fd>=0)close(fd);
 }
 if(dir>=0)close(dir);
 if(sendAll(socket,&size,sizeof(size)) && size)sendAll(socket,bytes.data(),size);
}
class RadioModule:public zygisk::ModuleBase{
 zygisk::Api* api{};JNIEnv* env{};bool target=false;
 public:
 void onLoad(zygisk::Api* a,JNIEnv* e)override{api=a;env=e;}
 void preServerSpecialize(zygisk::ServerSpecializeArgs*)override{api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);}
 void preAppSpecialize(zygisk::AppSpecializeArgs* args)override{
  char device[PROP_VALUE_MAX]{};__system_property_get("ro.product.device",device);
  const char* name=env->GetStringUTFChars(args->nice_name,nullptr);
  bool named=!strcmp(name,"com.android.phone");
  if(named)__android_log_print(ANDROID_LOG_INFO,"R12RadioStartup","pre uid=%d device=%s",args->uid,device);
  target=args->uid==1001 && named && !strcmp(device,"heat");
  env->ReleaseStringUTFChars(args->nice_name,name);
  if(target){
   int fd=api->connectCompanion();uint32_t size=0;unsigned char selector=2;
   if(fd>=0 && !sendAll(fd,&selector,1)){close(fd);fd=-1;}
   target=fd>=0 && receiveAll(fd,&size,sizeof(size)) && size>=64 && size<=1048576;
   if(target){dex.resize(size);target=receiveAll(fd,dex.data(),size);}
   if(fd>=0)close(fd);
   __android_log_print(ANDROID_LOG_INFO,"R12RadioStartup","one-attempt guard=%d dex=%u",target,size);
  }
  if(!target)api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
 }
 void postAppSpecialize(const zygisk::AppSpecializeArgs*)override{
  if(!target)return;
  // Never dlclose after attempting ART initialization, even after failure.
  if(!initialize(env)){__android_log_print(ANDROID_LOG_ERROR,"R12RadioStartup","ART init refused");if(env->ExceptionCheck())env->ExceptionClear();return;}
  auto cl=env->FindClass("java/lang/ClassLoader");auto get=env->GetStaticMethodID(cl,"getSystemClassLoader","()Ljava/lang/ClassLoader;");auto parent=env->CallStaticObjectMethod(cl,get);
  auto type=env->FindClass("dalvik/system/InMemoryDexClassLoader");auto ctor=env->GetMethodID(type,"<init>","(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
  auto buffer=env->NewDirectByteBuffer(dex.data(),dex.size());auto loader=env->NewObject(type,ctor,buffer,parent);
  if(env->ExceptionCheck()){env->ExceptionDescribe();env->ExceptionClear();return;}
  auto load=env->GetMethodID(cl,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;");auto cache=static_cast<jclass>(env->CallObjectMethod(loader,load,env->NewStringUTF("local.redmi12.radiostartup.RadioStartup")));
  if(env->ExceptionCheck()||!cache){env->ExceptionDescribe();env->ExceptionClear();return;}
  if(!registerHelpers(env,cache)){env->ExceptionDescribe();env->ExceptionClear();return;}
  auto install=env->GetStaticMethodID(cache,"install","()V");if(install)env->CallStaticVoidMethod(cache,install);
  if(env->ExceptionCheck()){env->ExceptionDescribe();env->ExceptionClear();__android_log_print(ANDROID_LOG_ERROR,"R12RadioStartup","install failed; process restart skips module");}
 }
};




zygisk::ModuleBase* make_radio() { return new RadioModule(); }
void radio_companion(int fd) { companion(fd); }
