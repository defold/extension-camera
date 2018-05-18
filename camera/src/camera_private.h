#pragma once

#include <dmsdk/sdk.h>

enum CaptureQuality
{
    CAPTURE_QUALITY_LOW,
    CAPTURE_QUALITY_MEDIUM,
    CAPTURE_QUALITY_HIGH,
};

enum CameraType
{
    CAMERA_TYPE_FRONT, // Selfie
    CAMERA_TYPE_BACK
};

struct CameraInfo
{
    uint32_t   m_Width;
    uint32_t   m_Height;
    CameraType m_Type;
};

extern int CameraPlatform_StartCapture(dmBuffer::HBuffer* buffer, CameraType type, CaptureQuality quality, CameraInfo& outparams);
extern int CameraPlatform_StopCapture();
extern int CameraPlatform_UpdateCapture();
