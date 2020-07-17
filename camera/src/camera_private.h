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
    CAMERA_ERROR
};

extern int CameraPlatform_Initialize();
extern void CameraPlatform_StartCapture(dmBuffer::HBuffer* buffer, CameraType type, CaptureQuality quality);
extern void CameraPlatform_UpdateCapture();
extern void CameraPlatform_StopCapture();
extern void CameraPlatform_GetCameraInfo(CameraInfo& outparams);

void Camera_QueueMessage(CameraMessage message);
