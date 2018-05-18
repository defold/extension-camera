#include "Camera.h"
#include <dmsdk/sdk.h> // dmGraphics::GetNativeAndroidActivity()

static jclass		g_cameraClass = 0;
static jmethodID	g_initMethodId = 0;
static jmethodID	g_startPreviewMethodId = 0;
static jmethodID	g_stopPreviewMethodId = 0;
static jmethodID	g_takePhotoMethodId = 0;
static jmethodID	g_getCameraMethodId = 0;
static jmethodID	g_setCallbackDataMethodId = 0;
static jobject		g_cameraObject = 0;

static jint g_data[640*480];
static bool g_frameLock = false;

class PCamera : public Camera
{
public:

	enum PhotoSavedState
	{
		PhotoSaved_Failed,
		PhotoSaved_OK,
		PhotoSaved_Waiting
	};

	PCamera();
	virtual ~PCamera();

	bool	Initialize();
	void	Deinitialize();
	bool	Start();
	void	Stop();
	void	Update();
	//void	TakePhoto(const char* path);
	//void	SetFocusPoint(const Vector2& focusPoint);

	void	PhotoSavedCB(bool success);


private:
	bool	m_initialized;
	PhotoSavedState m_photoSavedState;
};

PCamera::PCamera()
{
	m_initialized = false;
	//m_takePhoto = false;
	m_photoSavedState = PhotoSaved_Waiting;
}

PCamera::~PCamera()
{

}

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

bool PCamera::Initialize()
{
	JNIEnv* env = Attach();
	if(!env)
		return false;

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

	if(!g_startPreviewMethodId)
		g_startPreviewMethodId = env->GetMethodID(g_cameraClass, "startPreview", "()V");
	assert(g_startPreviewMethodId);

	if(!g_stopPreviewMethodId)
		g_stopPreviewMethodId = env->GetMethodID(g_cameraClass, "stopPreview", "()V");
	assert(g_stopPreviewMethodId);

	// if(!g_takePhotoMethodId)
	// 	g_takePhotoMethodId = env->GetMethodID(g_cameraClass, "takePhoto", "(Ljava/lang/String;)V");
	// assert(g_takePhotoMethodId);

	if(!g_setCallbackDataMethodId)
		g_setCallbackDataMethodId = env->GetMethodID(g_cameraClass, "setCallbackData", ("(J)V"));
	assert(g_setCallbackDataMethodId);

	env->CallVoidMethod(g_cameraObject, g_setCallbackDataMethodId, (long long)this);

	m_initialized = true;
	Detach(env);
	return true;
}

void PCamera::Deinitialize()
{

}

bool PCamera::Start()
{
	JNIEnv* env = Attach();
	env->CallVoidMethod(g_cameraObject, g_startPreviewMethodId);
	Detach(env);
	return true;
}

void PCamera::Stop()
{
	JNIEnv* env = Attach();
	env->CallVoidMethod(g_cameraObject, g_stopPreviewMethodId);
	Detach(env);
}

void PCamera::Update()
{
	if(!m_initialized && !Initialize())
		return;

	if(g_frameLock)
	{
		m_frameUpdateCB(m_callbackData, (void*)g_data, 640, 480, 4);
		g_frameLock = false;
	}

	// if(m_takePhoto)
	// {
	// 	if(m_photoSavedState != PhotoSaved_Waiting)
	// 	{
	// 		m_takePhoto = false;
	// 		m_photoSavedState = PhotoSaved_Waiting;

	// 		m_photoSavedCB(m_callbackData, m_photoSavedState);
	// 	}
	// }
}

// void PCamera::TakePhoto(const char* path)
// {
// 	m_takePhoto = true;

// 	jstring jPath = g_env->NewStringUTF(path);
// 	g_env->CallVoidMethod(g_cameraObject, g_takePhotoMethodId, jPath);
// 	g_env->DeleteLocalRef(jPath);
// }

// void PCamera::SetFocusPoint(const Vector2& focusPoint)
// {

// }

void PCamera::PhotoSavedCB(bool success)
{
	m_photoSavedState = success ? PhotoSaved_OK : PhotoSaved_Failed;
}

Camera* Camera::GetCamera(void* callbackData, FrameUpdateCallback frameUpdateCB, PhotoSavedCallback photoSavedCB)
{
	PCamera *camera = new PCamera;
	if(!camera)
		return 0;

	camera->m_frameUpdateCB = frameUpdateCB;
	camera->m_photoSavedCB = photoSavedCB;
	camera->m_callbackData = callbackData;

	return camera;
}

Camera::~Camera()
{

}

// Vector2 Camera::GetFrameSize()
// {
// 	return Vector2(640, 480);
// }


int Camera::GetFrameWidth() { return 640; }
int Camera::GetFrameHeight() { return 480; }

extern "C"
{
	JNIEXPORT void JNICALL Java_com_defold_android_camera_AndroidCamera_frameUpdate(JNIEnv * env, jobject jobj, jintArray data);
	JNIEXPORT void JNICALL Java_com_defold_android_camera_AndroidCamera_photoSaved(JNIEnv * env, jobject jobj, jlong callbackData, jboolean success);
}

JNIEXPORT void JNICALL Java_com_defold_android_camera_AndroidCamera_frameUpdate(JNIEnv * env, jobject jobj, jintArray data)
{
	if(!g_frameLock)
	{
		env->GetIntArrayRegion(data, 0, 640*480, g_data);
		g_frameLock = true;
	}
}

JNIEXPORT void JNICALL Java_com_defold_android_camera_AndroidCamera_photoSaved(JNIEnv * env, jobject jobj, jlong callbackData, jboolean success)
{
	PCamera* c = (PCamera*)callbackData;
	if(c)
		c->PhotoSavedCB(success);
}