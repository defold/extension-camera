#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#define EXTENSION_NAME Camera
#define LIB_NAME "Camera"
#define MODULE_NAME "camera"

// Defold SDK
#define DLIB_LOG_DOMAIN LIB_NAME
#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_IOS) || defined(DM_PLATFORM_OSX) || defined(DM_PLATFORM_ANDROID)

#include <unistd.h>
#include "camera_private.h"

struct DefoldCamera
{
    // The buffer that receives the pixel data
    dmBuffer::HBuffer m_VideoBuffer;

    // We create the buffer once, and keep a reference to it throughout
    // the capture.
    int m_VideoBufferLuaRef;

    // Information about the currently set camera
    CameraInfo  m_Params;

    dmArray<CameraMessage> m_MessageQueue;
    dmScript::LuaCallbackInfo* m_Callback;
    dmMutex::HMutex m_Mutex;
};

DefoldCamera g_DefoldCamera;

void Camera_QueueMessage(CameraMessage message)
{
    dmLogInfo("Camera_QueueMessage %d", message);
    DM_MUTEX_SCOPED_LOCK(g_DefoldCamera.m_Mutex);

    if (g_DefoldCamera.m_MessageQueue.Full())
    {
        g_DefoldCamera.m_MessageQueue.OffsetCapacity(1);
    }
    g_DefoldCamera.m_MessageQueue.Push(message);
}

static void Camera_ProcessQueue()
{
    DM_MUTEX_SCOPED_LOCK(g_DefoldCamera.m_Mutex);

    for (uint32_t i = 0; i != g_DefoldCamera.m_MessageQueue.Size(); ++i)
    {
        lua_State* L = dmScript::GetCallbackLuaContext(g_DefoldCamera.m_Callback);
        if (!dmScript::SetupCallback(g_DefoldCamera.m_Callback))
        {
            break;
        }
        CameraMessage message = g_DefoldCamera.m_MessageQueue[i];

        if (message == CAMERA_STARTED)
        {
            // Increase ref count
            dmScript::LuaHBuffer luabuffer = {g_DefoldCamera.m_VideoBuffer, false};
            dmScript::PushBuffer(L, luabuffer);
            g_DefoldCamera.m_VideoBufferLuaRef = dmScript::Ref(L, LUA_REGISTRYINDEX);
        }
        else if (message == CAMERA_STOPPED)
        {
            dmScript::Unref(L, LUA_REGISTRYINDEX, g_DefoldCamera.m_VideoBufferLuaRef); // We want it destroyed by the GC
            g_DefoldCamera.m_VideoBufferLuaRef = 0;
        }

        lua_pushnumber(L, (lua_Number)message);
        int ret = lua_pcall(L, 2, 0, 0);
        if (ret != 0)
        {
            lua_pop(L, 1);
        }
        dmScript::TeardownCallback(g_DefoldCamera.m_Callback);
    }
    g_DefoldCamera.m_MessageQueue.SetSize(0);
}

static void Camera_DestroyCallback()
{
    if (g_DefoldCamera.m_Callback != 0)
    {
        dmScript::DestroyCallback(g_DefoldCamera.m_Callback);
        g_DefoldCamera.m_Callback = 0;
    }
}

static int StartCapture(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    CameraType type = (CameraType) luaL_checkint(L, 1);
    CaptureQuality quality = (CaptureQuality)luaL_checkint(L, 2);

    Camera_DestroyCallback();
    g_DefoldCamera.m_Callback = dmScript::CreateCallback(L, 3);

    CameraPlatform_StartCapture(&g_DefoldCamera.m_VideoBuffer, type, quality, g_DefoldCamera.m_Params);

    return 1;
}

static int StopCapture(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

    CameraPlatform_StopCapture();

    return 0;
}

static int GetInfo(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    lua_newtable(L);
    lua_pushstring(L, "width");
    lua_pushnumber(L, g_DefoldCamera.m_Params.m_Width);
    lua_rawset(L, -3);
    lua_pushstring(L, "height");
    lua_pushnumber(L, g_DefoldCamera.m_Params.m_Height);
    lua_rawset(L, -3);
    lua_pushstring(L, "bytes_per_pixel");
    lua_pushnumber(L, 3);
    lua_rawset(L, -3);
    lua_pushstring(L, "type");
    lua_pushinteger(L, g_DefoldCamera.m_Params.m_Type);
    lua_rawset(L, -3);

    return 1;
}

static int GetFrame(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    if (g_DefoldCamera.m_VideoBufferLuaRef != 0)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, g_DefoldCamera.m_VideoBufferLuaRef);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}


static const luaL_reg Module_methods[] =
{
    {"start_capture", StartCapture},
    {"stop_capture", StopCapture},
    {"get_frame", GetFrame},
    {"get_info", GetInfo},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);
    luaL_register(L, MODULE_NAME, Module_methods);

    #define SETCONSTANT(name) \
    lua_pushnumber(L, (lua_Number) name); \
    lua_setfield(L, -2, #name);\

    SETCONSTANT(CAMERA_TYPE_FRONT)
    SETCONSTANT(CAMERA_TYPE_BACK)

    SETCONSTANT(CAPTURE_QUALITY_LOW)
    SETCONSTANT(CAPTURE_QUALITY_MEDIUM)
    SETCONSTANT(CAPTURE_QUALITY_HIGH)

    SETCONSTANT(CAMERA_STARTED)
    SETCONSTANT(CAMERA_STOPPED)
    SETCONSTANT(CAMERA_NOT_PERMITTED)
    SETCONSTANT(CAMERA_ERROR)

    #undef SETCONSTANT

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

dmExtension::Result AppInitializeCamera(dmExtension::AppParams* params)
{
    dmLogInfo("Registered %s Extension", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeCamera(dmExtension::Params* params)
{
    LuaInit(params->m_L);
    g_DefoldCamera.m_Mutex = dmMutex::New();
    CameraPlatform_Initialize();
    return dmExtension::RESULT_OK;
}

static dmExtension::Result UpdateCamera(dmExtension::Params* params)
{
    CameraPlatform_UpdateCapture();
    Camera_ProcessQueue();
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeCamera(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeCamera(dmExtension::Params* params)
{
    dmMutex::Delete(g_DefoldCamera.m_Mutex);
    Camera_DestroyCallback();
    return dmExtension::RESULT_OK;
}

#else // unsupported platforms


static dmExtension::Result AppInitializeCamera(dmExtension::AppParams* params)
{
    dmLogInfo("Registered %s (null) Extension", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeCamera(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result UpdateCamera(dmExtension::Params* params)
{
    Camera_ProcessQueue()
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeCamera(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeCamera(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

#endif // platforms


DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInitializeCamera, AppFinalizeCamera, InitializeCamera, UpdateCamera, 0, FinalizeCamera)
