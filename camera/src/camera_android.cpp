#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_ANDROID)

#include "camera_private.h"

static jclass		g_CameraClass = 0;
static jobject		g_CameraObject = 0;
static jmethodID	g_GetCameraMethodId = 0;
static jmethodID	g_StartPreviewMethodId = 0;
static jmethodID	g_StopPreviewMethodId = 0;

static jint *g_Data = 0;
static bool g_FrameLock = false;

static uint32_t g_Width = 0;
static uint32_t g_Height = 0;
static CameraType g_Type;
static CaptureQuality g_Quality;

static dmBuffer::HBuffer* g_Buffer = 0;


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


extern "C"
{
	JNIEXPORT void JNICALL Java_com_defold_android_camera_AndroidCamera_frameUpdate(JNIEnv * env, jobject jobj, jintArray data);
    JNIEXPORT void JNICALL Java_com_defold_android_camera_AndroidCamera_queueMessage(JNIEnv * env, jobject jobj, jint message);
    JNIEXPORT void JNICALL Java_com_defold_android_camera_AndroidCamera_captureStarted(JNIEnv * env, jobject jobj, jint width, jint height);
}

JNIEXPORT void JNICALL Java_com_defold_android_camera_AndroidCamera_frameUpdate(JNIEnv * env, jobject jobj, jintArray data)
{
	if(!g_FrameLock)
	{
		env->GetIntArrayRegion(data, 0, g_Width * g_Height, g_Data);
		g_FrameLock = true;
	}
}

JNIEXPORT void JNICALL Java_com_defold_android_camera_AndroidCamera_queueMessage(JNIEnv * env, jobject jobj, jint message)
{
    Camera_QueueMessage((CameraMessage)message);
}

JNIEXPORT void JNICALL Java_com_defold_android_camera_AndroidCamera_captureStarted(JNIEnv * env, jobject jobj, jint width, jint height)
{
    g_Width = (uint32_t)width;
    g_Height = (uint32_t)height;

    uint32_t size = g_Width * g_Height;
    delete g_Data;
    g_Data = new jint[size];
    dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("rgb"), dmBuffer::VALUE_TYPE_UINT8, 3}
    };
    dmBuffer::Create(size, streams_decl, 1, g_Buffer);
}


void CameraPlatform_GetCameraInfo(CameraInfo& outparams)
{
    outparams.m_Width = (g_Width > g_Height) ? g_Height : g_Width;
    outparams.m_Height = (g_Width > g_Height) ? g_Width : g_Height;
    outparams.m_Type = g_Type;
}

int CameraPlatform_Initialize()
{
    JNIEnv* env = Attach();
    if(!env)
    {
        return false;
    }

    // get the AndroidCamera class
    jclass tmp = GetClass(env, "com.defold.android.camera/AndroidCamera");
    g_CameraClass = (jclass)env->NewGlobalRef(tmp);
    if(!g_CameraClass)
    {
        dmLogError("Could not find class 'com.defold.android.camera/AndroidCamera'.");
        Detach(env);
        return false;
    }

    // get an instance of the AndroidCamera class using the getCamera() method
    g_GetCameraMethodId = env->GetStaticMethodID(g_CameraClass, "getCamera", "(Landroid/content/Context;)Lcom/defold/android/camera/AndroidCamera;");
    if(!g_GetCameraMethodId)
    {
        dmLogError("Could not get static method 'getCamera'.");
        Detach(env);
        return false;
    }

    jobject tmp1 = env->CallStaticObjectMethod(g_CameraClass, g_GetCameraMethodId, dmGraphics::GetNativeAndroidActivity());
    g_CameraObject = (jobject)env->NewGlobalRef(tmp1);
    if(!g_CameraObject)
    {
        dmLogError("Could not create instance.");
        Detach(env);
        return false;
    }

    // get reference to startPreview() and stopPreview() methods
    g_StartPreviewMethodId = env->GetMethodID(g_CameraClass, "startPreview", "(II)V");
    if(!g_StartPreviewMethodId)
    {
        dmLogError("Could not get startPreview() method.");
        Detach(env);
        return false;
    }
    g_StopPreviewMethodId = env->GetMethodID(g_CameraClass, "stopPreview", "()V");
    if(!g_StopPreviewMethodId)
    {
        dmLogError("Could not get stopPreview() method.");
        Detach(env);
        return false;
    }

    Detach(env);
    return true;
}

void CameraPlatform_StartCapture(dmBuffer::HBuffer* buffer, CameraType type, CaptureQuality quality)
{
    if (!g_CameraObject)
    {
        Camera_QueueMessage(CAMERA_ERROR);
        return;
    }

    g_Buffer = buffer;
    g_Type = type;
    g_Quality = quality;

    JNIEnv* env = Attach();
	env->CallVoidMethod(g_CameraObject, g_StartPreviewMethodId);
	Detach(env);
}

void CameraPlatform_StopCapture()
{
    if (!g_CameraObject)
    {
        Camera_QueueMessage(CAMERA_ERROR);
        return;
    }

    JNIEnv* env = Attach();
    env->CallVoidMethod(g_CameraObject, g_StopPreviewMethodId);
    Detach(env);
}

void CameraPlatform_UpdateCapture()
{
	if(g_FrameLock)
	{
        int width = g_Width;
        int height = g_Height;
        int numChannels = 4;
        uint8_t* out;
        uint32_t outsize;
        dmBuffer::GetBytes(*g_Buffer, (void**)&out, &outsize);

        uint32_t* data = (uint32_t*)g_Data;
        for( int y = 0; y < height; ++y)
        {
            for( int x = 0; x < width; ++x)
            {
                // We get the image in landscape mode, so we flip it to portrait mode
                int index = (width-x-1)*height*3 + (height-y-1)*3;

                // RGB <- ARGB
                uint32_t argb = data[y*width+x];

                out[index+0] = (argb>>0)&0xFF; 	// R
                out[index+1] = (argb>>8)&0xFF;	// G
                out[index+2] = (argb>>16)&0xFF;	// B
            }
        }
		g_FrameLock = false;
	}
}

#endif // DM_PLATFORM_ANDROID
