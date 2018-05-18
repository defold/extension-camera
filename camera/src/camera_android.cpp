#include <dmsdk/sdk.h>
#include "camera_private.h"
#include "android/Camera.h"

#if defined(DM_PLATFORM_ANDROID)

struct AndroidCamera
{
    dmBuffer::HBuffer m_VideoBuffer;
    Camera* m_Camera;

    AndroidCamera() : m_VideoBuffer(0), m_Camera(0)
    {
    }
};

AndroidCamera g_Camera;

static void FrameUpdateCallback(void* _ctx, void* _data, int width, int height, int numChannels)
{
	AndroidCamera* ctx = (AndroidCamera*)_ctx;
	uint8_t* out;
	uint32_t outsize;
	dmBuffer::GetBytes(g_Camera.m_VideoBuffer, (void**)&out, &outsize);

	uint32_t* data = (uint32_t*)_data;
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
}

static void PhotoSavedCallback(void* ctx, bool success)
{
	(void)ctx;
	(void)success;
}


int CameraPlatform_StartCapture(dmBuffer::HBuffer* buffer, CameraType type, CaptureQuality quality, CameraInfo& outparams)
{
	if (g_Camera.m_Camera) {
		dmLogError("Camera already started!");
		return 0;
	}
	g_Camera.m_Camera = Camera::GetCamera(&g_Camera, FrameUpdateCallback, PhotoSavedCallback);
	if (!g_Camera.m_Camera) {
		dmLogError("Camera failed to start");
		return 0;
	}

	if (!g_Camera.m_Camera->Initialize()) {
		dmLogError("Camera failed to initialize");
		return 0;
	}

	if (!g_Camera.m_Camera->Start()) {
		dmLogError("Camera failed to initialize");
		return 0;
	}

	outparams.m_Width = (uint32_t)g_Camera.m_Camera->GetFrameWidth();
	outparams.m_Height = (uint32_t)g_Camera.m_Camera->GetFrameHeight();

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

    g_Camera.m_VideoBuffer = *buffer;

    return 1;
}

int CameraPlatform_StopCapture()
{
	if (g_Camera.m_Camera) {
		g_Camera.m_Camera->Stop();
		g_Camera.m_Camera->Deinitialize();
		delete g_Camera.m_Camera;
	}
	g_Camera.m_Camera = 0;
    return 1;
}

int CameraPlatform_UpdateCapture()
{
	if (!g_Camera.m_Camera) {
		dmLogError("Camera has not been started");
		return 0;
	}
	g_Camera.m_Camera->Update();
	return 1;
}

#endif // DM_PLATFORM_ANDROID