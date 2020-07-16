# extension-camera

Native extension to use device camera to capture frames.


# Installation

To use the camera extension in a Defold project this project has to be added as a [Defold library dependency](http://www.defold.com/manuals/libraries/). Open the **game.project** file and in the [Dependencies field in the Project section](https://defold.com/manuals/project-settings/#dependencies) add:

https://github.com/defold/extension-camera/archive/master.zip

Or point to the ZIP file of [a specific release](https://github.com/defold/extension-camera/releases).


# Supported platforms

The currently supported platforms are macOS, iOS and Android


# FAQ

## How do I reset macOS camera permission?

To test macOS camera permission popup multiple times you can reset the permission from the terminal:

```bash
tccutil reset Camera
```


# Lua API

## Type constants

Describes what camera should be used.

```lua
camera.CAMERA_TYPE_FRONT -- Selfie
camera.CAMERA_TYPE_BACK
```


## Quality constants

```lua
camera.CAPTURE_QUALITY_HIGH
camera.CAPTURE_QUALITY_MEDIUM
camera.CAPTURE_QUALITY_LOW
```


## Status constants

```lua
camera.CAMERA_STARTED
camera.CAMERA_STOPPED
camera.CAMERA_NOT_PERMITTED
camera.CAMERA_ERROR
```


## camera.start_capture(type, quality, callback)

Start camera capture using the specified camera (front/back) and capture quality. This may trigger a camera usage permission popup. When the popup has been dismissed the callback will be invoked with camera start status.

```lua
camera.start_capture(camera.CAMERA_TYPE_BACK, camera.CAPTURE_QUALITY_HIGH, function(self, message)
    if message == camera.CAMERA_STARTED then
        -- do stuff
    end
end)
```


## camera.stop_capture()

Stops a previously started capture session.

```lua
camera.stop_capture()
```


## camera.get_info()

Gets the info from the current capture session.

```lua
local info = camera.get_info()
print("width", info.width)
print("height", info.height)
```


## camera.get_frame()

Retrieves the camera pixel buffer. This buffer has one stream named "rgb", and is of type `buffer.VALUE_TYPE_UINT8` and has the value count of 1.

```lua
self.cameraframe = camera.get_frame()
```
