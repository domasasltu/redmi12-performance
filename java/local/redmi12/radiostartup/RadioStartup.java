package local.redmi12.radiostartup;
import android.os.*;
import android.util.Log;
import java.lang.reflect.*;
import java.lang.ref.WeakReference;
import java.util.*;
import java.util.concurrent.*;
public final class RadioStartup {
 static native boolean validateAbi();
 static native boolean testBackoff();
 static native int[] backoff(Object switcher,Object result);
 static native Object retryRil(Object ril,int service);
 static native void retryConfig(Object config);
 static native Method attach(int kind,Object hooker);
 static native Object[] missingRil(Object ril,int service);
 static native String missingConfig();
 static native boolean proxyEmpty(Object proxy);
 static native Class<?> callbackInterface();
 static native void register(String name,Object callback);
 static native boolean await(String name);
 static final String TAG="R12RadioStartup",DESCRIPTOR="android.os.IServiceCallback";
 static final Map<Object,Set<Integer>> pendingRil=new WeakHashMap<>();
 static final Set<Object> pendingConfig=Collections.newSetFromMap(new WeakHashMap<Object,Boolean>());
 static final Map<String,Object> listeners=new HashMap<>();
 static final Set<String> kickGroups=new HashSet<>();
 static Handler handler;
 static long deferred,registrations;
 static Handler main(){synchronized(RadioStartup.class){if(handler==null)handler=new Handler(Looper.getMainLooper());return handler;}}
 static Object invoke(Method method,Object receiver,Object[] args)throws Throwable{try{return method.invoke(receiver,args);}catch(InvocationTargetException e){throw e.getCause();}}
 public static void install(){
  if(!validateAbi())throw new IllegalStateException("telephony ABI mismatch");
  RilHook rh=new RilHook();ConfigHook ch=new ConfigHook();DdsHook dh=new DdsHook();
  dh.backup=attach(2,dh);
  ch.backup=attach(1,ch);rh.backup=attach(0,rh);
  if(rh.backup==null||ch.backup==null||dh.backup==null)throw new IllegalStateException("hook failed");
  Log.i(TAG,"RIL/RadioConfig startup hooks installed; no radio services disabled");
 }
 static synchronized void remember(Object ril,int service){Set<Integer>s=pendingRil.get(ril);if(s==null){s=new HashSet<>();pendingRil.put(ril,s);}s.add(service);}
 static void watch(String name){
  synchronized(RadioStartup.class){
   if(listeners.containsKey(name))return;
   CallbackBinder binder=new CallbackBinder(name);
   Object proxy=Proxy.newProxyInstance(RadioStartup.class.getClassLoader(),new Class<?>[]{callbackInterface()},(object,method,args)->{
    if(method.getName().equals("asBinder"))return binder;
    if(method.getName().equals("toString"))return "RadioStartupCallback("+name+")";
    if(method.getName().equals("hashCode"))return System.identityHashCode(object);
    if(method.getName().equals("equals"))return object==args[0];
    return null;
   });
   register(name,proxy);listeners.put(name,proxy);Log.i(TAG,"deferred service "+name);
   // Existing radio backend groups: config and data. One native waiter per group
   // requests lazy startup; event notifications reconnect all other interfaces.
   String group=name.startsWith("android.hardware.radio.config.")?"config":"radio";
   if(name.startsWith("android.hardware.radio.")&&kickGroups.add(group)){
    Thread thread=new Thread(()->{try{await(name);}catch(Throwable e){Log.w(TAG,"lazy-start waiter exited",e);}},"radio-availability-"+group);thread.setDaemon(true);thread.start();
   }
  }
 }
 static void retryAll(){
  Map<Object,Set<Integer>> rils=new HashMap<>();List<Object> configs;
  synchronized(RadioStartup.class){for(Map.Entry<Object,Set<Integer>> e:pendingRil.entrySet())rils.put(e.getKey(),new HashSet<>(e.getValue()));configs=new ArrayList<>(pendingConfig);}
  for(Map.Entry<Object,Set<Integer>> e:rils.entrySet())for(int service:e.getValue())try{
   Object proxy=retryRil(e.getKey(),service);
   if(proxy!=null&&!proxyEmpty(proxy)){synchronized(RadioStartup.class){Set<Integer>s=pendingRil.get(e.getKey());if(s!=null){s.remove(service);if(s.isEmpty())pendingRil.remove(e.getKey());}}Log.i(TAG,"radio interface reconnected "+service);}
  }catch(Throwable error){Log.w(TAG,"radio reconnect deferred",error);}
  for(Object config:configs)try{retryConfig(config);}catch(Throwable error){Log.w(TAG,"config reconnect deferred",error);}
 }
 static final class CallbackBinder extends Binder{
  final String expected;CallbackBinder(String n){expected=n;}
  @Override protected boolean onTransact(int code,Parcel data,Parcel reply,int flags)throws RemoteException{
   if(code==INTERFACE_TRANSACTION){if(reply!=null)reply.writeString(DESCRIPTOR);return true;}
   if(code==FIRST_CALL_TRANSACTION){
    int uid=Binder.getCallingUid();if(uid!=0&&uid!=1000)throw new SecurityException("service manager only");
    data.enforceInterface(DESCRIPTOR);String name=data.readString();IBinder service=data.readStrongBinder();
    if(!expected.equals(name)||service==null)throw new IllegalArgumentException("unexpected service callback");
    registrations++;Log.i(TAG,"service registered "+name);main().post(RadioStartup::retryAll);
    CountDownLatch test=testLatch;if(test!=null&&name.equals("activity"))test.countDown();
    if(reply!=null)reply.writeNoException();return true;
   }return super.onTransact(code,data,reply,flags);
  }
 }
 public static final class RilHook{
  public Method backup;
  public Object callback(Object[] args)throws Throwable{
   Object[] missing=missingRil(args[0],(Integer)args[1]);
   if(missing!=null){remember(args[0],(Integer)args[1]);deferred++;watch((String)missing[1]);return missing[0];}
   return invoke(backup,args[0],new Object[]{args[1]});
  }
 }
 public static final class ConfigHook{
  public Method backup;
  public Object callback(Object[] args)throws Throwable{
   String missing=missingConfig();
   if(missing!=null){synchronized(RadioStartup.class){pendingConfig.add(args[0]);}deferred++;watch(missing);return null;}
   Object result=invoke(backup,args[0],new Object[0]);synchronized(RadioStartup.class){pendingConfig.remove(args[0]);}return result;
  }
 }
 public static final class DdsHook{
  public Method backup;long delayed;
  public Object callback(Object[] args)throws Throwable{
   int[] retry=backoff(args[0],args[1]);
   if(retry==null)return invoke(backup,args[0],new Object[]{args[1]});
   Handler target=(Handler)args[0];Integer phone=retry[0];
   if(!target.hasMessages(retry[1],phone))target.sendMessageDelayed(Message.obtain(target,retry[1],phone),retry[2]);
   if(++delayed<=4||delayed%100==0)Log.i(TAG,"existing modem retry scheduled phone="+phone+" delayMs="+retry[2]+" failures="+delayed);
   return null;
  }
 }
 static volatile CountDownLatch testLatch;
 public static String testProxy()throws Exception{
  testLatch=new CountDownLatch(1);watch("activity");boolean got=testLatch.await(8,TimeUnit.SECONDS);
  Object callback; synchronized(RadioStartup.class){callback=listeners.remove("activity");}
  // Framework unregister is unimplemented. Binder death cleans this process-scoped registration.
  return "{\"cleanup\":\"process_exit_binder_death\",\"callback_received\":"+got+",\"registrations\":"+registrations+",\"status\":\""+(got?"PASS":"FAIL")+"\"}";
 }
}
