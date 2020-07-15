package com.defold.android.camera;

import android.content.Context;
import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentManager;
import android.os.Build;
import android.os.Bundle;
import android.Manifest;
import android.support.v4.content.ContextCompat;
import android.content.pm.PackageManager;
import android.util.Log;

import android.hardware.Camera;
import android.hardware.Camera.CameraInfo;
import android.hardware.Camera.PreviewCallback;

import android.graphics.PixelFormat;
import android.graphics.SurfaceTexture; // API 11

import android.view.Surface;

import java.io.IOException;

class AndroidCamera
{
	private static final String TAG = AndroidCamera.class.getSimpleName();
	private static final String PERMISSION_FRAGMENT_TAG = PermissionsFragment.class.getSimpleName();

	public static class PermissionsFragment extends Fragment {
		private AndroidCamera camera;

		public PermissionsFragment(final Activity activity, final AndroidCamera camera) {
			this.camera = camera;
			final FragmentManager fragmentManager = activity.getFragmentManager();
			if (fragmentManager.findFragmentByTag(PERMISSION_FRAGMENT_TAG) == null) {
				fragmentManager.beginTransaction().add(this, PERMISSION_FRAGMENT_TAG).commit();
			}
		}

		@Override
		public void onCreate(Bundle savedInstanceState) {
			super.onCreate(savedInstanceState);
			setRetainInstance(true);
		}

		@Override
		public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
			Log.d(TAG, "onRequestPermissionsResult " + requestCode + " " + permissions[0] + " " + grantResults[0]);
			camera.onRequestPermissionsResult(grantResults[0]);
		}
	}

	enum CameraMessage
	{
		CAMERA_STARTED(0),
		CAMERA_STOPPED(1),
		CAMERA_NOT_PERMITTED(2),
		CAMERA_ERROR(3),
		CAMERA_SHOW_PERMISSION_RATIONALE(4);

		private final int value;

		private CameraMessage(int value) {
			this.value = value;
		}

		public int getValue() {
			return value;
		}
	}

	private PermissionsFragment permissionFragment;
	private Camera camera;
	private SurfaceTexture surface;
	private boolean newFrame;

	private static Context context;

	static native void frameUpdate(int[] data);
	static native void queueMessage(int message);


	public static AndroidCamera getCamera(Context context)
	{
		return new AndroidCamera(context);
	}

	private AndroidCamera(final Context context)
	{
		this.context = context;
		permissionFragment = new PermissionsFragment((Activity)context, this);
	}

	private void requestPermission() {
		Log.d(TAG, "requestPermission");
		final Activity activity = (Activity)context;
		if (Build.VERSION.SDK_INT < 23)
		{
			Log.d(TAG, "requestPermission SDK_INT < 23");
			final int grantResult = ContextCompat.checkSelfPermission(activity, Manifest.permission.CAMERA);
			onRequestPermissionsResult(grantResult);
		}
		else
		{
			Log.d(TAG, "requestPermission fragment");
			final FragmentManager fragmentManager = activity.getFragmentManager();
			final Fragment fragment = fragmentManager.findFragmentByTag(PERMISSION_FRAGMENT_TAG);
			final String[] permissions = new String[] { Manifest.permission.CAMERA };
			fragment.requestPermissions(permissions, 100);
		}
	}

	public synchronized void onRequestPermissionsResult(int grantResult) {
		Log.d(TAG, "onRequestPermissionsResult " + grantResult);
		if (grantResult == PackageManager.PERMISSION_GRANTED)
		{
			Log.d(TAG, "onRequestPermissionsResult startPreviewAuthorized");
			startPreviewAuthorized();
		}
		else
		{
			Log.d(TAG, "onRequestPermissionsResult ERROR");
			queueMessage(CameraMessage.CAMERA_ERROR.getValue());
		}
	}

	private void startPreviewAuthorized()
	{
		CameraInfo info = new CameraInfo();
		int cameraId = -1;
		int numberOfCameras = Camera.getNumberOfCameras();

		for(int i = 0; i < numberOfCameras; i++)
		{
			Camera.getCameraInfo(i, info);
			if(info.facing == CameraInfo.CAMERA_FACING_BACK)
			{
				cameraId = i;
				break;
			}
		}

		if(cameraId == -1)
		{
			queueMessage(CameraMessage.CAMERA_ERROR.getValue());
			return;
		}

		surface = new SurfaceTexture(0);
		camera = Camera.open(cameraId);

		Camera.Parameters params = camera.getParameters();
		params.setPreviewSize(640, 480);
		params.setPictureSize(640, 480);
		params.setPictureFormat(PixelFormat.JPEG);
		params.setJpegQuality(90);
		params.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE);
		camera.setParameters(params);

		final Activity activity = (Activity)context;
		int rotation = activity.getWindowManager().getDefaultDisplay().getRotation();

		camera.setPreviewCallback(new PreviewCallback() {
			public void onPreviewFrame(byte[] data, Camera arg1) {

				boolean flip = false;
				int rotation = activity.getWindowManager().getDefaultDisplay().getRotation();;

				if(rotation == Surface.ROTATION_180 || rotation == Surface.ROTATION_270)
				flip = true;

				int[] pixels = convertYUV420_NV21toARGB8888(data, 640, 480, flip);
				frameUpdate(pixels);
			}
		});

		try
		{
			camera.setPreviewTexture(surface);
		}
		catch(IOException ioe)
		{
		}
		Log.d(TAG, "startPreviewAuthorized starting camera");
		camera.startPreview();
		queueMessage(CameraMessage.CAMERA_STARTED.getValue());
	}

	public void startPreview()
	{
		Log.d(TAG, "startPreview");
		if(camera != null)
		{
			queueMessage(CameraMessage.CAMERA_STARTED.getValue());
			return;
		}

		requestPermission();
	}

	public void stopPreview()
	{
		if(camera != null)
		{
			camera.stopPreview();
			camera.release();
			camera = null;
			queueMessage(CameraMessage.CAMERA_STOPPED.getValue());
		}
	}

	private static int[] convertYUV420_NV21toARGB8888(byte [] data, int width, int height, boolean flip) {
		int size = width*height;
		int offset = size;
		int[] pixels = new int[size];
		int u, v, y1, y2, y3, y4;

		int startPos = 0;
		int helperIdx = -1;
		if(flip)
		{
			startPos = size - 1;
			helperIdx = 1;
		}

		// i along Y and the final pixels
		// k along pixels U and V
		for(int i=0, k=0; i < size; i+=2, k+=2) {
			y1 = data[i  ]&0xff;
			y2 = data[i+1]&0xff;
			y3 = data[width+i  ]&0xff;
			y4 = data[width+i+1]&0xff;

			v = data[offset+k  ]&0xff;
			u = data[offset+k+1]&0xff;
			v = v-128;
			u = u-128;

			pixels[startPos - helperIdx*i  ] = convertYUVtoARGB(y1, u, v);
			pixels[startPos - helperIdx*(i+1)] = convertYUVtoARGB(y2, u, v);
			pixels[startPos - helperIdx*(width+i)  ] = convertYUVtoARGB(y3, u, v);
			pixels[startPos - helperIdx*(width+i+1)] = convertYUVtoARGB(y4, u, v);

			if (i!=0 && (i+2)%width==0)
			i += width;
		}

		return pixels;
	}

	// Alt: https://github.com/Jaa-c/android-camera-demo/blob/master/src/com/jaa/camera/CameraSurfaceView.java
	private static int convertYUVtoARGB(int y, int u, int v) {
		int r = y + (int)(1.772f*v);
		int g = y - (int)(0.344f*v + 0.714f*u);
		int b = y + (int)(1.402f*u);
		r = r>255? 255 : r<0 ? 0 : r;
		g = g>255? 255 : g<0 ? 0 : g;
		b = b>255? 255 : b<0 ? 0 : b;
		return 0xff000000 | r | (g<<8) | (b<<16);
	}
};
