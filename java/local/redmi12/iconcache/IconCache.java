package local.redmi12.iconcache;
import android.app.Application;
import android.content.*;
import android.database.ContentObserver;
import android.os.*;
import android.provider.Settings;
import android.util.Log;
import java.lang.reflect.*;
public final class IconCache {
 static final String KEY="statusbar_colored_icons";
 static final String CONTROL="local.redmi12.ICON_CACHE_CONTROL";
 static native Method attach(Object hooker);
 static native Application application();
 static native int currentUser();
 static native void observe(ContentResolver cr,android.net.Uri uri,ContentObserver observer);
 public static void install(){Hook h=new Hook();h.backup=attach(h);if(h.backup==null)throw new IllegalStateException("hook failed");Log.i("R12IconCache","hook installed; awaiting application context");}
 public static final class Hook {
  public Method backup;
  boolean tracking,failed,enabled=true,valid;
  int user;String value;long hits,misses,other,passed;
  ContentObserver observer;BroadcastReceiver receiver;
  synchronized void invalidate(){valid=false;}
  synchronized void stats(String cause){Log.i("R12IconCache",cause+" enabled="+enabled+" tracking="+tracking+" user="+user+" hits="+hits+" misses="+misses+" passthrough="+passed+" other="+other);}
  void track(ContentResolver cr){
   if(tracking||failed)return;
   Application app=application();if(app==null)return;
   Handler handler=new Handler(Looper.getMainLooper());
   try{
    user=currentUser();if(user<0)throw new IllegalStateException("invalid current user");
    observer=new ContentObserver(handler){@Override public void onChange(boolean self){invalidate();stats("setting invalidated");}};
    observe(cr,Settings.System.getUriFor(KEY),observer);
    receiver=new BroadcastReceiver(){@Override public void onReceive(Context c,Intent i){synchronized(Hook.this){
     if("android.intent.action.USER_SWITCHED".equals(i.getAction())){user=currentUser();valid=false;stats("user changed");}
     else if(CONTROL.equals(i.getAction())){enabled=i.getBooleanExtra("enabled",enabled);valid=false;stats("control");}
    }}};
    IntentFilter filter=new IntentFilter();filter.addAction("android.intent.action.USER_SWITCHED");filter.addAction(CONTROL);
    app.registerReceiver(receiver,filter,"android.permission.STATUS_BAR_SERVICE",handler,Context.RECEIVER_EXPORTED);
    tracking=true;stats("ready");
   }catch(Throwable error){failed=true;enabled=false;valid=false;
    if(observer!=null)try{cr.unregisterContentObserver(observer);}catch(Throwable ignored){}
    Log.e("R12IconCache","tracking unavailable; reads pass through",error);
   }
  }
  Object original(Object[] args)throws Throwable{try{return backup.invoke(null,args);}catch(InvocationTargetException e){throw e.getCause();}}
  public Object callback(Object[] args)throws Throwable{
   if(!KEY.equals(args[1])){other++;return original(args);}
   synchronized(this){
    ContentResolver cr=(ContentResolver)args[0];track(cr);int requested=(Integer)args[2];
    if(!tracking||!enabled||(requested!=-2&&requested!=user)){passed++;return original(args);}
    if(valid){hits++;return value;}
    Object result=original(args);value=(String)result;valid=true;misses++;return result;
   }
  }
 }
}
