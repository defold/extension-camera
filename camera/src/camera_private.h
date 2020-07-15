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

enum CameraMessage
{
    CAMERA_STARTED,
    CAMERA_STOPPED,
    CAMERA_NOT_PERMITTED,
    CAMERA_ERROR,
    CAMERA_SHOW_PERMISSION_RATIONALE
};

extern int CameraPlatform_Initialize();
extern void CameraPlatform_StartCapture(dmBuffer::HBuffer* buffer, CameraType type, CaptureQuality quality, CameraInfo& outparams);
extern void CameraPlatform_UpdateCapture();
extern void CameraPlatform_StopCapture();

void Camera_QueueMessage(CameraMessage message);
