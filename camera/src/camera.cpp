#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define EXTENSION_NAME Camera
#define LIB_NAME "Camera"
#define MODULE_NAME "camera"

// Defold SDK
#define DLIB_LOG_DOMAIN LIB_NAME
#include <dmsdk/sdk.h>

#if defined(DM_PLATFORM_IOS) || defined(DM_PLATFORM_OSX)

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
};

DefoldCamera g_DefoldCamera;


static int StartCapture(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    CameraType type = (CameraType) luaL_checkint(L, 1);
    CaptureQuality quality = (CaptureQuality)luaL_checkint(L, 2);

	int status = CameraPlatform_StartCapture(&g_DefoldCamera.m_VideoBuffer, type, quality, g_DefoldCamera.m_Params);

	lua_pushboolean(L, status > 0);
	if( status == 0 )
	{
        dmLogError("capture failed!\n");
		return 1;
	}

    // Increase ref count
    dmScript::LuaHBuffer luabuffer = {g_DefoldCamera.m_VideoBuffer, false};
    dmScript::PushBuffer(L, luabuffer);
    g_DefoldCamera.m_VideoBufferLuaRef = dmScript::Ref(L, LUA_REGISTRYINDEX);

	return 1;
}

static int StopCapture(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 0);

	int status = CameraPlatform_StopCapture();
	if( !status )
	{
        return luaL_error(L, "Failed to stop capture. Was it started?");
	}

    dmScript::Unref(L, LUA_REGISTRYINDEX, g_DefoldCamera.m_VideoBufferLuaRef); // We want it destroyed by the GC

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
    lua_rawgeti(L,LUA_REGISTRYINDEX, g_DefoldCamera.m_VideoBufferLuaRef); 
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

#undef SETCONSTANT

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

dmExtension::Result AppInitializeCamera(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result InitializeCamera(dmExtension::Params* params)
{
    LuaInit(params->m_L);
    return dmExtension::RESULT_OK;
}

dmExtension::Result AppFinalizeCamera(dmExtension::AppParams* params)
{
    return dmExtension::RESULT_OK;
}

dmExtension::Result FinalizeCamera(dmExtension::Params* params)
{
    return dmExtension::RESULT_OK;
}

#else // unsupported platforms


static dmExtension::Result AppInitializeCamera(dmExtension::AppParams* params)
{
    dmLogInfo("Registered %s (null) Extension\n", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeCamera(dmExtension::Params* params)
{
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


DM_DECLARE_EXTENSION(EXTENSION_NAME, LIB_NAME, AppInitializeCamera, AppFinalizeCamera, InitializeCamera, 0, 0, FinalizeCamera)
