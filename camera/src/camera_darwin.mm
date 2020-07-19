#include <dmsdk/sdk.h>
#include "camera_private.h"

#if defined(DM_PLATFORM_IOS) || defined(DM_PLATFORM_OSX)

#include <AVFoundation/AVFoundation.h>

#if defined(DM_PLATFORM_IOS)
#include <UIKit/UIKit.h>
#endif

// Some good reads on capturing camera/video for iOS/macOS
//   http://easynativeextensions.com/camera-tutorial-part-4-connect-to-the-camera-in-objective-c/
//   https://developer.apple.com/library/content/qa/qa1702/_index.html
//   http://stackoverflow.com/questions/19422322/method-to-find-devices-camera-resolution-ios
//   http://stackoverflow.com/a/32047525


@interface CameraCaptureDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
    @private AVCaptureSession*          m_captureSession;
    @private AVCaptureDevice*           m_camera;
    @private AVCaptureDeviceInput*      m_cameraInput;
    @private AVCaptureVideoDataOutput*  m_videoOutput;
    @public  CMVideoDimensions          m_Size;
}
@end

struct IOSCamera
{
    CameraCaptureDelegate* m_Delegate;
    dmBuffer::HBuffer m_VideoBuffer;
    uint32_t m_Width;
    uint32_t m_Height;
    // TODO: Support audio buffers

    IOSCamera() : m_Delegate(0), m_VideoBuffer(0)
    {
    }
};

IOSCamera g_Camera;

@implementation CameraCaptureDelegate


- ( id ) init
{
    self = [ super init ];
    m_captureSession    = NULL;
    m_camera            = NULL;
    m_cameraInput       = NULL;
    m_videoOutput       = NULL;
    return self;
}


- ( void ) onVideoError: ( NSString * ) error
{
    NSLog(@"%@",error);
}

- ( void ) onVideoStart: ( NSNotification * ) note
{
    // perhaps add callback
}

- ( void ) onVideoStop: ( NSNotification * ) note
{
    // perhaps add callback
}


// Delegate routine that is called when a sample buffer was written
- (void)captureOutput:(AVCaptureOutput *)captureOutput
         didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
         fromConnection:(AVCaptureConnection *)connection
{
    if( captureOutput == m_videoOutput && g_Camera.m_VideoBuffer != 0 )
    {
        uint8_t* data = 0;
        uint32_t datasize = 0;
        dmBuffer::GetBytes(g_Camera.m_VideoBuffer, (void**)&data, &datasize);

        CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
        CVPixelBufferLockBaseAddress(imageBuffer,0);

        size_t bytesPerRow = CVPixelBufferGetBytesPerRow(imageBuffer);
        size_t width = CVPixelBufferGetWidth(imageBuffer);
        size_t height = CVPixelBufferGetHeight(imageBuffer);

        if( width != g_Camera.m_Delegate->m_Size.width || height != g_Camera.m_Delegate->m_Size.height )
        {
            //printf("img width/height: %d, %d\n", width, height);
            CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
            return;
        }

        uint8_t* pixels = (uint8_t*)CVPixelBufferGetBaseAddress(imageBuffer);

        for( int y = 0; y < height; ++y )
        {
            for( int x = 0; x < width; ++x )
            {
                // RGB < BGR(A)
#if defined(DM_PLATFORM_IOS)
                // Flip Y
                data[y*width*3 + x*3 + 2] = pixels[(height - y - 1) * bytesPerRow + x * 4 + 0];
                data[y*width*3 + x*3 + 1] = pixels[(height - y - 1) * bytesPerRow + x * 4 + 1];
                data[y*width*3 + x*3 + 0] = pixels[(height - y - 1) * bytesPerRow + x * 4 + 2];
#else
                // Flip X + Y
                data[y*width*3 + x*3 + 2] = pixels[(height - y - 1) * bytesPerRow + bytesPerRow - (x+1) * 4 + 0];
                data[y*width*3 + x*3 + 1] = pixels[(height - y - 1) * bytesPerRow + bytesPerRow - (x+1) * 4 + 1];
                data[y*width*3 + x*3 + 0] = pixels[(height - y - 1) * bytesPerRow + bytesPerRow - (x+1) * 4 + 2];
#endif

            }
        }

        CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
        dmBuffer::ValidateBuffer(g_Camera.m_VideoBuffer);
    }
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput
  didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection
{
    //NSLog(@"DROPPING FRAME!!!");
}


// http://easynativeextensions.com/camera-tutorial-part-4-connect-to-the-camera-in-objective-c
- ( BOOL ) findCamera: (AVCaptureDevicePosition) cameraPosition
{
    // 0. Make sure we initialize our camera pointer:
    m_camera = NULL;

    // 1. Get a list of available devices:
    // specifying AVMediaTypeVideo will ensure we only get a list of cameras, no microphones
    NSArray * devices = [ AVCaptureDevice devicesWithMediaType: AVMediaTypeVideo ];

    // 2. Iterate through the device array and if a device is a camera, check if it's the one we want:
    for ( AVCaptureDevice * device in devices )
    {
        if ( cameraPosition == [ device position ] )
        {
            m_camera = device;
        }
    }

    // 3. Set a frame rate for the camera:
    if ( NULL != m_camera )
    {
#if defined(DM_PLATFORM_IOS)
        // We firt need to lock the camera, so noone else can mess with its configuration:
        if ( [ m_camera lockForConfiguration: NULL ] )
        {
            // Set a minimum frame rate of 10 frames per second
            [ m_camera setActiveVideoMinFrameDuration: CMTimeMake( 1, 10 ) ];
            // and a maximum of 30 frames per second
            [ m_camera setActiveVideoMaxFrameDuration: CMTimeMake( 1, 30 ) ];

            [ m_camera unlockForConfiguration ];
        }
#endif
    }

    // 4. If we've found the camera we want, return true
    return ( NULL != m_camera );
}

- ( BOOL ) attachCameraToCaptureSession
{
    // 0. Assume we've found the camera and set up the session first:
    assert( NULL != m_camera );
    assert( NULL != m_captureSession );

    // 1. Initialize the camera input
    m_cameraInput = NULL;

    // 2. Request a camera input from the camera
    NSError * error = NULL;
    m_cameraInput = [ AVCaptureDeviceInput deviceInputWithDevice: m_camera error: &error ];

    // 2.1. Check if we've got any errors
    if ( NULL != error )
    {
        // TODO: send an error event to ActionScript
        return false;
    }

    // 3. We've got the input from the camera, now attach it to the capture session:
    if ( [ m_captureSession canAddInput: m_cameraInput ] )
    {
        [ m_captureSession addInput: m_cameraInput ];
    }
    else
    {
        // TODO: send an error event to ActionScript
        return false;
    }

    // 4. Done, the attaching was successful, return true to signal that
    return true;
}

- ( void ) setupVideoOutput
{
    // 1. Create the video data output
    m_videoOutput = [ [ AVCaptureVideoDataOutput alloc ] init ];

    // 2. Create a queue for capturing video frames
    dispatch_queue_t captureQueue = dispatch_queue_create( "captureQueue", DISPATCH_QUEUE_SERIAL );

    // 3. Use the AVCaptureVideoDataOutputSampleBufferDelegate capabilities of CameraDelegate:
    [ m_videoOutput setSampleBufferDelegate: self queue: captureQueue ];

    // 4. Set up the video output
    // 4.1. Do we care about missing frames?
    m_videoOutput.alwaysDiscardsLateVideoFrames = YES;

    // 4.2. We want the frames in some RGB format, which is what ActionScript can deal with
    NSNumber * framePixelFormat = [ NSNumber numberWithInt: kCVPixelFormatType_32BGRA ];
    m_videoOutput.videoSettings = [ NSDictionary dictionaryWithObject: framePixelFormat
                                                               forKey: ( id ) kCVPixelBufferPixelFormatTypeKey ];

    // 5. Add the video data output to the capture session
    [ m_captureSession addOutput: m_videoOutput ];


#if defined(DM_PLATFORM_IOS)
    AVCaptureConnection *conn = [m_videoOutput connectionWithMediaType:AVMediaTypeVideo];

    AVCaptureVideoOrientation videoOrientation;
    switch ([UIDevice currentDevice].orientation) {
        case UIDeviceOrientationLandscapeLeft:
            videoOrientation = AVCaptureVideoOrientationLandscapeLeft;
            break;
        case UIDeviceOrientationLandscapeRight:
            videoOrientation = AVCaptureVideoOrientationLandscapeRight;
            break;
        case UIDeviceOrientationPortraitUpsideDown:
            videoOrientation = AVCaptureVideoOrientationPortraitUpsideDown;
            break;
        case AVCaptureVideoOrientationPortrait:
        default:
            videoOrientation = AVCaptureVideoOrientationPortrait;
            break;
    }

    conn.videoOrientation = videoOrientation;
#endif
}


static CMVideoDimensions FlipCoords(AVCaptureVideoDataOutput* output, const CMVideoDimensions& in)
{
    CMVideoDimensions out = in;
#if defined(DM_PLATFORM_IOS)
    AVCaptureConnection* conn = [output connectionWithMediaType:AVMediaTypeVideo];
    switch (conn.videoOrientation) {
        case AVCaptureVideoOrientationPortraitUpsideDown:
        case AVCaptureVideoOrientationPortrait:
            out.width = in.height;
            out.height = in.width;
            break;
        default:
            break;
    }
#endif
    return out;
}


- ( BOOL ) startCamera: (AVCaptureDevicePosition) cameraPosition
                  quality: (CaptureQuality)quality
{
    // 1. Find the back camera
    if ( ![ self findCamera: cameraPosition ] )
    {
        return false;
    }

    //2. Make sure we have a capture session
    if ( NULL == m_captureSession )
    {
        m_captureSession = [ [ AVCaptureSession alloc ] init ];
    }

    // 3. Choose a preset for the session.
    NSString * cameraResolutionPreset;
    switch(quality)
    {
        case CAPTURE_QUALITY_LOW:       cameraResolutionPreset = AVCaptureSessionPresetLow; break;
        case CAPTURE_QUALITY_MEDIUM:    cameraResolutionPreset = AVCaptureSessionPresetMedium; break;
        case CAPTURE_QUALITY_HIGH:      cameraResolutionPreset = AVCaptureSessionPresetHigh; break;
        default:
            dmLogError("Unknown quality setting: %d", quality);
            return false;
    }

    // 4. Check if the preset is supported on the device by asking the capture session:
    if ( ![ m_captureSession canSetSessionPreset: cameraResolutionPreset ] )
    {
        // Optional TODO: Send an error event to ActionScript
        return false;
    }

    // 4.1. The preset is OK, now set up the capture session to use it
    [ m_captureSession setSessionPreset: cameraResolutionPreset ];

    // 5. Plug camera and capture sesiossion together
    [ self attachCameraToCaptureSession ];

    // 6. Add the video output
    [ self setupVideoOutput ];

    // Get camera size
    CMFormatDescriptionRef formatDescription = m_camera.activeFormat.formatDescription;
    g_Camera.m_Delegate->m_Size = CMVideoFormatDescriptionGetDimensions(formatDescription);

    // In case we have a portrait mode, let's flip the coords
    g_Camera.m_Delegate->m_Size = FlipCoords(m_videoOutput, g_Camera.m_Delegate->m_Size);

    // 7. Set up a callback, so we are notified when the camera actually starts
    [ [ NSNotificationCenter defaultCenter ] addObserver: self
                                                selector: @selector( onVideoStart: )
                                                    name: AVCaptureSessionDidStartRunningNotification
                                                  object: m_captureSession ];

    // 8. 3, 2, 1, 0... Start!
    [ m_captureSession startRunning ];

    // Note: Returning true from this function only means that setting up went OK.
    // It doesn't mean that the camera has started yet.
    // We get notified about the camera having started in the videoCameraStarted() callback.
    return true;
}



- ( BOOL ) stopCamera
{
    BOOL isRunning = [m_captureSession isRunning];
    if( isRunning )
    {
        [m_captureSession stopRunning ];
    }
    return isRunning;
}

@end

void CameraPlatform_GetCameraInfo(CameraInfo& outparams)
{
    outparams.m_Width = g_Camera.m_Width;
    outparams.m_Height = g_Camera.m_Height;
}

int CameraPlatform_Initialize()
{
    return 1;
}

void CameraPlatform_StartCaptureAuthorized(dmBuffer::HBuffer* buffer, CameraType type, CaptureQuality quality)
{
    if(g_Camera.m_Delegate == 0)
    {
        g_Camera.m_Delegate = [[CameraCaptureDelegate alloc] init];
    }

    AVCaptureDevicePosition cameraposition = AVCaptureDevicePositionUnspecified;
#if defined(DM_PLATFORM_IOS)
    if( type == CAMERA_TYPE_BACK )
        cameraposition = AVCaptureDevicePositionBack;
    else if( type == CAMERA_TYPE_FRONT )
        cameraposition = AVCaptureDevicePositionFront;
#endif

    BOOL started = [g_Camera.m_Delegate startCamera: cameraposition quality: quality];

    g_Camera.m_Width = (uint32_t)g_Camera.m_Delegate->m_Size.width;
    g_Camera.m_Height = (uint32_t)g_Camera.m_Delegate->m_Size.height;

    uint32_t size = g_Camera.m_Width * g_Camera.m_Height;
    dmBuffer::StreamDeclaration streams_decl[] = {
        {dmHashString64("rgb"), dmBuffer::VALUE_TYPE_UINT8, 3}
    };

    dmBuffer::Create(size, streams_decl, 1, buffer);

    g_Camera.m_VideoBuffer = *buffer;

    if (started)
    {
        Camera_QueueMessage(CAMERA_STARTED);
    }
    else
    {
        Camera_QueueMessage(CAMERA_ERROR);
    }
}

void CameraPlatform_StartCapture(dmBuffer::HBuffer* buffer, CameraType type, CaptureQuality quality)
{
    // Only check for permission on iOS 7+ and macOS 10.14+
    if ([AVCaptureDevice respondsToSelector:@selector(authorizationStatusForMediaType:)])
    {
        // Request permission to access the camera.
        int status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
        if (status == AVAuthorizationStatusAuthorized)
        {
            // The user has previously granted access to the camera.
            dmLogInfo("AVAuthorizationStatusAuthorized");
            CameraPlatform_StartCaptureAuthorized(buffer, type, quality);
        }
        else if (status == AVAuthorizationStatusNotDetermined)
        {
            dmLogInfo("AVAuthorizationStatusNotDetermined");
            // The app hasn't yet asked the user for camera access.
            [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL granted) {
                if (granted) {
                    dmLogInfo("AVAuthorizationStatusNotDetermined - granted!");
                    CameraPlatform_StartCaptureAuthorized(buffer, type, quality);
                }
                else
                {
                    dmLogInfo("AVAuthorizationStatusNotDetermined - not granted!");
                    Camera_QueueMessage(CAMERA_NOT_PERMITTED);
                }
            }];
        }
        else if (status == AVAuthorizationStatusDenied)
        {
            // The user has previously denied access.
            dmLogInfo("AVAuthorizationStatusDenied");
            Camera_QueueMessage(CAMERA_NOT_PERMITTED);
        }
        else if (status == AVAuthorizationStatusRestricted)
        {
            // The user can't grant access due to restrictions.
            dmLogInfo("AVAuthorizationStatusRestricted");
            Camera_QueueMessage(CAMERA_NOT_PERMITTED);
        }
    }
    else
    {
        CameraPlatform_StartCaptureAuthorized(buffer, type, quality);
    }
}

void CameraPlatform_StopCapture()
{
    if(g_Camera.m_Delegate != 0)
    {
        [g_Camera.m_Delegate stopCamera];
        [g_Camera.m_Delegate release];
        g_Camera.m_Delegate = 0;

        dmBuffer::Destroy(g_Camera.m_VideoBuffer);
        g_Camera.m_VideoBuffer = 0;
    }
}

void CameraPlatform_UpdateCapture() {}

#endif // DM_PLATFORM_IOS/DM_PLATFORM_OSX
