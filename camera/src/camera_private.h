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

enum CameraStatus
{
    STATUS_STARTED,
    STATUS_STOPPED,
    STATUS_NOT_PERMITTED,
    STATUS_ERROR
};

extern void CameraPlatform_StartCapture(dmBuffer::HBuffer* buffer, CameraType type, CaptureQuality quality, CameraInfo& outparams);
extern void CameraPlatform_StopCapture();

void Camera_QueueMessage(CameraStatus message);
