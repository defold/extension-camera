#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_ANDROID)

#include "camera_private.h"

static JNIEnv* Attach()
{
  JNIEnv* env;
  JavaVM* vm = dmGraphics::GetNativeAndroidJavaVM();
  vm->AttachCurrentThread(&env, NULL);
  return env;
}

static bool Detach(JNIEnv* env)
{
  bool exception = (bool) env->ExceptionCheck();
  env->ExceptionClear();
  JavaVM* vm = dmGraphics::GetNativeAndroidJavaVM();
  vm->DetachCurrentThread();
  return !exception;
}

namespace {
  struct AttachScope
  {
    JNIEnv* m_Env;
    AttachScope() : m_Env(Attach())
    {
    }
    ~AttachScope()
    {
      Detach(m_Env);
    }
  };
}

static jclass GetClass(JNIEnv* env, const char* classname)
{
    jclass activity_class = env->FindClass("android/app/NativeActivity");
    jmethodID get_class_loader = env->GetMethodID(activity_class,"getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject cls = env->CallObjectMethod(dmGraphics::GetNativeAndroidActivity(), get_class_loader);
    jclass class_loader = env->FindClass("java/lang/ClassLoader");
    jmethodID find_class = env->GetMethodID(class_loader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

    jstring str_class_name = env->NewStringUTF(classname);
    jclass outcls = (jclass)env->CallObjectMethod(cls, find_class, str_class_name);
    env->DeleteLocalRef(str_class_name);
    return outcls;
}


struct AndroidCamera {
  dmBuffer::HBuffer m_VideoBuffer;

  AndroidCamera() : m_VideoBuffer(0)
	{
	}
};

AndroidCamera g_Camera;



#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL Java_com_defold_android_camera_CameraExtension_helloworld(JNIEnv *env, jobject obj)  {
  dmLogError("Hello World!\n");
  return;
}

JNIEXPORT void JNICALL Java_com_defold_android_camera_CameraExtension_sendPicture(JNIEnv *env, jobject obj, jbyteArray array)  {
  dmLogError("send Picture!\n");

  uint8_t* data = 0;
  uint32_t datasize = 0;
  dmBuffer::GetBytes(g_Camera.m_VideoBuffer, (void**)&data, &datasize);

  int len = env->GetArrayLength(array);
  dmLogError("sendPicture received array length %d expected %d", len, datasize);
  //unsigned char* buf = new unsigned char[len];
  env->GetByteArrayRegion (array, 0, len, reinterpret_cast<jbyte*>(data));

  dmBuffer::ValidateBuffer(g_Camera.m_VideoBuffer);
  return;
}



JNIEXPORT void JNICALL Java_com_defold_android_camera_CameraExtension_handleCameraFrame(JNIEnv *env, jobject obj, jbyteArray yuv420sp, jint width, jint height) {
  uint8_t* data = 0;
  uint32_t datasize = 0;
  dmBuffer::GetBytes(g_Camera.m_VideoBuffer, (void**)&data, &datasize);

  int i;
  int j;
  int Y;
  int Cr = 0;
  int Cb = 0;
  int pixPtr = 0;
  int jDiv2 = 0;
  int R = 0;
  int G = 0;
  int B = 0;
  int cOff;
  int w = width;
  int h = height;
  int sz = w * h;

  jbyte* yuv = (jbyte*) (env->GetPrimitiveArrayCritical(yuv420sp, 0));

  for(j = 0; j < h; j++) {
    pixPtr = j * w;
    jDiv2 = j >> 1;
    for(i = 0; i < w; i++) {
      Y = yuv[pixPtr];
      if(Y < 0) Y += 255;
      if((i & 0x1) != 1) {
        cOff = sz + jDiv2 * w + (i >> 1) * 2;
        Cb = yuv[cOff];
        if(Cb < 0) Cb += 127; else Cb -= 128;
        Cr = yuv[cOff + 1];
        if(Cr < 0) Cr += 127; else Cr -= 128;
      }

      //ITU-R BT.601 conversion
      //
      //R = 1.164*(Y-16) + 2.018*(Cr-128);
      //G = 1.164*(Y-16) - 0.813*(Cb-128) - 0.391*(Cr-128);
      //B = 1.164*(Y-16) + 1.596*(Cb-128);
      //
      Y = Y + (Y >> 3) + (Y >> 5) + (Y >> 7);
      R = Y + (Cr << 1) + (Cr >> 6);
      if(R < 0) R = 0; else if(R > 255) R = 255;
      G = Y - Cb + (Cb >> 3) + (Cb >> 4) - (Cr >> 1) + (Cr >> 3);
      if(G < 0) G = 0; else if(G > 255) G = 255;
      B = Y + Cb + (Cb >> 1) + (Cb >> 4) + (Cb >> 5);
      if(B < 0) B = 0; else if(B > 255) B = 255;

      data[0] = R;
      data++;
      data[0] = G;
      data++;
      data[0] = B;
      data++;
      //data[(pixPtr * 3) + 0] = R;
      //data[(pixPtr * 3) + 1] = G;
      //data[(pixPtr * 3) + 2] = B;
      //data[pixPtr++] = 0xff000000 + (R << 16) + (G << 8) + B;
    }
  }

  env->ReleasePrimitiveArrayCritical(yuv420sp, yuv, 0);

  dmBuffer::ValidateBuffer(g_Camera.m_VideoBuffer);
}


#ifdef __cplusplus
}
#endif



int CameraPlatform_StartCapture(dmBuffer::HBuffer* buffer, CameraType type, CaptureQuality quality, CameraInfo& outparams)
{
  dmLogError("Android start capture");

  int facing = (type == CAMERA_TYPE_BACK) ? 0 : 1;

  // prepare JNI
  AttachScope attachscope;
  JNIEnv* env = attachscope.m_Env;
  jclass cls = GetClass(env, "com.defold.android.camera.CameraExtension");

  // prepare camera for capture and get width and height
  jmethodID prepare_capture = env->GetStaticMethodID(cls, "PrepareCapture", "(Landroid/content/Context;I)Z");
  jboolean prepare_success = env->CallStaticBooleanMethod(cls, prepare_capture, dmGraphics::GetNativeAndroidActivity(), facing);
  jint width = env->CallStaticIntMethod(cls, env->GetStaticMethodID(cls, "GetWidth", "()I"));
  jint height = env->CallStaticIntMethod(cls, env->GetStaticMethodID(cls, "GetHeight", "()I"));

  // set out parameters and create video buffer
  outparams.m_Width = (uint32_t)width;
  outparams.m_Height = (uint32_t)height;
  uint32_t size = outparams.m_Width * outparams.m_Height;
  dmBuffer::StreamDeclaration streams_decl[] = {
      {dmHashString64("rgb"), dmBuffer::VALUE_TYPE_UINT8, 3}
  };
  dmBuffer::Create(size, streams_decl, 1, buffer);
  g_Camera.m_VideoBuffer = *buffer;

  // Start capture
  if (prepare_success == JNI_TRUE) {
    jmethodID start_capture = env->GetStaticMethodID(cls, "StartCapture", "(Landroid/content/Context;)V");
    env->CallStaticBooleanMethod(cls, start_capture, dmGraphics::GetNativeAndroidActivity());
  }

  return (prepare_success == JNI_TRUE) ? 1 : 0;
}

int CameraPlatform_StopCapture()
{
  dmLogError("Android stop capture");

  // stop Android camera
  AttachScope attachscope;
  JNIEnv* env = attachscope.m_Env;
  jclass cls = GetClass(env, "com.defold.android.camera.CameraExtension");
  jmethodID method = env->GetStaticMethodID(cls, "StopCapture", "(Landroid/content/Context;)V");
  dmLogError("Android stop capture - calling java");
  env->CallStaticVoidMethod(cls, method, dmGraphics::GetNativeAndroidActivity());

  // destroy the video buffer
  dmBuffer::Destroy(g_Camera.m_VideoBuffer);
  g_Camera.m_VideoBuffer = 0;

  return 1;
}


#endif // DM_PLATFORM_ANDROID
