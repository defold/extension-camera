#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_ANDROID)

#include "camera_private.h"

static const uint32_t CAMERA_WIDTH = 640;
static const uint32_t CAMERA_HEIGHT = 480;

static jclass		g_cameraClass = 0;
static jmethodID	g_initMethodId = 0;
static jmethodID	g_startPreviewMethodId = 0;
static jmethodID	g_stopPreviewMethodId = 0;
static jmethodID	g_getCameraMethodId = 0;
static jmethodID	g_setCallbackDataMethodId = 0;
static jobject		g_cameraObject = 0;

static jint g_data[CAMERA_WIDTH * CAMERA_HEIGHT];
static bool g_frameLock = false;

static dmBuffer::HBuffer g_VideoBuffer = 0;



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
}

JNIEXPORT void JNICALL Java_com_defold_android_camera_AndroidCamera_frameUpdate(JNIEnv * env, jobject jobj, jintArray data)
{
	if(!g_frameLock)
	{
		env->GetIntArrayRegion(data, 0, CAMERA_WIDTH*CAMERA_HEIGHT, g_data);
		g_frameLock = true;
	}
}

JNIEXPORT void JNICALL Java_com_defold_android_camera_AndroidCamera_queueMessage(JNIEnv * env, jobject jobj, jint message)
{
    dmLogInfo("Java_com_defold_android_camera_AndroidCamera_queueMessage %d", (int)message);
    Camera_QueueMessage((CameraMessage)message);
}


int CameraPlatform_Initialize()
{
    JNIEnv* env = Attach();
    if(!env)
    {
        return false;
    }

    // get the AndroidCamera class
    if(!g_cameraClass)
    {
        jclass tmp = GetClass(env, "com.defold.android.camera/AndroidCamera");
        g_cameraClass = (jclass)env->NewGlobalRef(tmp);
        if(!g_cameraClass)
        {
            dmLogError("Could not find class 'com.defold.android.camera/AndroidCamera'.");
            Detach(env);
            return false;
        }
    }

    // get an instance of the AndroidCamera class using the getCamera() method
    if(!g_getCameraMethodId)
    {
        g_getCameraMethodId = env->GetStaticMethodID(g_cameraClass, "getCamera", "(Landroid/content/Context;)Lcom/defold/android/camera/AndroidCamera;");
        if(!g_getCameraMethodId)
        {
            dmLogError("Could not get static method 'getCamera'.");
            Detach(env);
            return false;
        }
    }
    if(!g_cameraObject)
    {
        jobject tmp1 = env->CallStaticObjectMethod(g_cameraClass, g_getCameraMethodId, dmGraphics::GetNativeAndroidActivity());
        g_cameraObject = (jobject)env->NewGlobalRef(tmp1);
    }

    // get reference to startPreview() and stopPreview() methods
    if(!g_startPreviewMethodId)
    {
        g_startPreviewMethodId = env->GetMethodID(g_cameraClass, "startPreview", "()V");
        assert(g_startPreviewMethodId);
    }
    if(!g_stopPreviewMethodId)
    {
        g_stopPreviewMethodId = env->GetMethodID(g_cameraClass, "stopPreview", "()V");
        assert(g_stopPreviewMethodId);
    }

    Detach(env);
    return true;
}

void CameraPlatform_StartCapture(dmBuffer::HBuffer* buffer, CameraType type, CaptureQuality quality, CameraInfo& outparams)
{
    dmLogInfo("CameraPlatform_StartCapture");
    outparams.m_Width = (uint32_t)CAMERA_WIDTH;
    outparams.m_Height = (uint32_t)CAMERA_HEIGHT;

    // As default behavior, we want portrait mode
    if (outparams.m_Width > outparams.m_Height) {
        uint32_t tmp = outparams.m_Width;
        outparams.m_Width = outparams.m_Height;
        outparams.m_Height = tmp;
    }

    uint32_t size = outparams.m_Width * outparams.m_Height;
    dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("rgb"), dmBuffer::VALUE_TYPE_UINT8, 3}
    };
    dmBuffer::Create(size, streams_decl, 1, buffer);

    g_VideoBuffer = *buffer;

    if (g_cameraObject)
    {
        dmLogInfo("CameraPlatform_StartCapture JNI");
        JNIEnv* env = Attach();
    	env->CallVoidMethod(g_cameraObject, g_startPreviewMethodId);
    	Detach(env);
    }
}

void CameraPlatform_StopCapture()
{
    if (g_cameraObject)
    {
        JNIEnv* env = Attach();
        env->CallVoidMethod(g_cameraObject, g_stopPreviewMethodId);
        Detach(env);
    }
}

void CameraPlatform_UpdateCapture()
{
	if(g_frameLock)
	{
        int width = CAMERA_WIDTH;
        int height = CAMERA_HEIGHT;
        int numChannels = 4;
        uint8_t* out;
        uint32_t outsize;
        dmBuffer::GetBytes(g_VideoBuffer, (void**)&out, &outsize);

        uint32_t* data = (uint32_t*)g_data;
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
		g_frameLock = false;
	}
}

#endif // DM_PLATFORM_ANDROID
