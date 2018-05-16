package com.defold.android.camera;

import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraDevice;

import android.hardware.Camera;

import android.Manifest;
import android.content.pm.PackageManager;
import android.content.Context;
import android.os.Build;
import android.util.Log;
import android.app.Activity;
import android.media.ImageReader;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.SurfaceHolder;
import android.view.ViewGroup;
import android.view.View;
import android.app.Activity;
import android.graphics.ImageFormat;

import java.util.Arrays;
import java.util.List;
import java.lang.Runnable;
import java.lang.Math;



class CameraExtension implements Camera.PreviewCallback {

  private static final String TAG = "defold";

  private static final String[] PERMISSIONS = { Manifest.permission.CAMERA };

  private Camera camera;
  private Camera.Size pictureSize;
  private Camera.Size previewSize;
  private Camera.Parameters parameters;
  private SurfaceView surfaceView;

  private static CameraExtension instance;

  private CameraExtension() {

  }

  private static CameraExtension getInstance() {
    if (instance == null) {
      //Log.v(TAG, "No instance, creating");
      instance = new CameraExtension();
    }
    return instance;
  }

  private static boolean hasAllPermissions(Context context, String... permissions) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && context != null && permissions != null) {
      for (String permission : permissions) {
        if (context.checkSelfPermission(permission) != PackageManager.PERMISSION_GRANTED) {
          return false;
        }
      }
    }
    return true;
  }

  private static void requestPermissions(Context context, String... permissions) {
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
      if (!hasAllPermissions(context, permissions)) {
        ((Activity)context).requestPermissions(permissions, 1);
      }
    }
  }


  private static Camera openCamera(int facing) {
    Camera.CameraInfo cameraInfo = new Camera.CameraInfo();
    int cameraCount = Camera.getNumberOfCameras();
    for (int i = 0; i < cameraCount; i++) {
      Camera.getCameraInfo(i, cameraInfo);
      if (cameraInfo.facing == facing) {
        return Camera.open(i);
      }
    }
    return null;
  }

  private static Camera openFrontCamera() {
    return openCamera(Camera.CameraInfo.CAMERA_FACING_FRONT);
  }

  private static Camera openBackCamera() {
    return openCamera(Camera.CameraInfo.CAMERA_FACING_BACK);
  }

  private native void handleCameraFrame(byte[] cameraFrame, int width, int height);

  @Override
  public void onPreviewFrame(byte[] data, Camera camera) {
    //Log.v(TAG, "onPreviewFrame " + previewSize.width + "x" + previewSize.height);
    if (data != null) {
      handleCameraFrame(data, previewSize.width, previewSize.height);
      // surfaceView.setVisibility(View.VISIBLE);
      // camera.startPreview();
      // surfaceView.setVisibility(View.INVISIBLE);
    }
    else {
      //Log.v(TAG, "onPreviewFrame - data is null");
    }
  }


  /**
   * Get the preferred preview size. The function will return a preview size
   * that is in the middle of the range of supported preview sizes.
   */
  private Camera.Size getPreferredPreviewSize() {
    List<Camera.Size> previewSizes = parameters.getSupportedPreviewSizes();
    return previewSizes.get((int)Math.ceil(previewSizes.size() / 2));
  }


  /**
   * Prepare the camera for capture. This will open the camera and set the
  * camera properties.
   */
  private boolean prepareCapture(final Context context, int facing) {
    //Log.v(TAG, "prepareCapture");
    if (!hasAllPermissions(context, PERMISSIONS)) {
      requestPermissions(context, PERMISSIONS);
      return false;
    }

    if (facing == 0) {
      camera = openBackCamera();
    }
    else {
      camera = openFrontCamera();
    }
    if (camera == null) {
      //Log.v(TAG, "Unable to open camera");
      return false;
    }

    try {
      parameters = camera.getParameters();
      // for(Camera.Size size : parameters.getSupportedPictureSizes()) {
      //   Log.v(TAG, "Supported picture size: " + size.width + "x" + size.height);
      // }
      // for(Camera.Size size : parameters.getSupportedPreviewSizes()) {
      //   Log.v(TAG, "Supported preview size: " + size.width + "x" + size.height);
      // }
      // for(int format : parameters.getSupportedPictureFormats()) {
      //   Log.v(TAG, "Supported picture format: " + format);
      // }
      // for(int format : parameters.getSupportedPreviewFormats()) {
      //   Log.v(TAG, "Supported preview format: " + format);
      // }
      // Log.v(TAG, "Current preview format:" + parameters.getPreviewFormat());

      pictureSize = parameters.getPictureSize();
      previewSize = getPreferredPreviewSize();
      parameters.setPreviewSize(previewSize.width, previewSize.height);
      //Log.v(TAG, "Camera parameters. picture size: " + pictureSize.width + "x" + pictureSize.height + " preview size: " + previewSize.width + "x" + previewSize.height);
      //parameters.setPictureFormat(ImageFormat.RGB_565);
      //parameters.setPictureSize(previewSize.width, previewSize.height);
      camera.setParameters(parameters);

      //camera.setDisplayOrientation() // use utility function from Google example
    }
    catch(Exception e) {
      Log.e(TAG, e.toString());
    }
    return true;
  }

  /**
   * Get the width of the camera image
   */
  private int getWidth() {
    return (previewSize != null) ? previewSize.width : 0;
  }

  /**
   * Get the height of the camera image
   */
  private int getHeight() {
    return (previewSize != null) ? previewSize.height : 0;
  }

  /**
   * Start capturing images with the camera
   */
  private void startCapture(final Context context) {
    //Log.v(TAG, "startCapture");
    ((Activity)context).runOnUiThread(new Runnable() {
      @Override
      public void run() {
        //Log.v(TAG, "runOnUiThread");

        try {
          //Log.v(TAG, "Create surface");
          surfaceView = new SurfaceView(context);
          Activity activity = (Activity)context;
          ViewGroup viewGroup = (ViewGroup)activity.findViewById(android.R.id.content);
          viewGroup.addView(surfaceView);

          final SurfaceHolder surfaceHolder = surfaceView.getHolder();
          surfaceHolder.addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
              //Log.v(TAG, "surfaceChanged");
            }

            @Override
            public void surfaceCreated(SurfaceHolder holder) {
              //Log.v(TAG, "surfaceCreated");
              try {
                camera.stopPreview();
                camera.setPreviewCallback(CameraExtension.this);
                camera.setPreviewDisplay(surfaceHolder);
                surfaceView.setVisibility(View.VISIBLE);
                camera.startPreview();
                surfaceView.setVisibility(View.INVISIBLE);
              }
              catch(Exception e) {
                Log.e(TAG, e.toString());
              }
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
              //Log.v(TAG, "surfaceDestroyed");
            }
          });

        }
        catch(Exception e) {
          Log.e(TAG, e.toString());
        }
      }
    });
  }

  /**
   * Stop capturing images with the camera
   */
  private void stopCapture() {
    if (camera != null) {
      camera.stopPreview();
      camera.release();
      camera = null;
    }
    if (surfaceView != null) {
      ((ViewGroup)surfaceView.getParent()).removeView(surfaceView);
      surfaceView = null;
    }
  }

  public static boolean PrepareCapture(final Context context, int facing) {
    //Log.v(TAG, "PrepareCapture " + facing);
    return CameraExtension.getInstance().prepareCapture(context, facing);
  }

  public static void StartCapture(final Context context) {
    //Log.v(TAG, "StartCapture");
    CameraExtension.getInstance().startCapture(context);
  }

  public static void StopCapture(final Context context) {
    //Log.v(TAG, "StopCapture");
    CameraExtension.getInstance().stopCapture();
  }

  public static int GetWidth() {
    //Log.v(TAG, "GetWidth");
    return CameraExtension.getInstance().getWidth();
  }

  public static int GetHeight() {
    //Log.v(TAG, "GetHeight");
    return CameraExtension.getInstance().getHeight();
  }

}
