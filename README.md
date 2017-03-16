# extension-camera

Example of interacting with the camera through native extensions.

# Disclaimer

Although we aim to provide good example functionality in this example, we cannot guarantee the quality/stability at all times.
Please regard it as just that, an example, and don't rely on this as a dependency for your production code.

# Known issues

The currently supported platforms are: OSX + iOS


# FAQ

## How do I use this extension?

Add the package link (https://github.com/defold/extension-camera/archive/master.zip)
to the project setting `project.dependencies`, and you should be good to go.

See the [manual](http://www.defold.com/manuals/libraries/) for further info.


# Lua API

## Type constants

Describes what camera should be used

    camera.CAMERA_TYPE_FRONT -- Selfie
    camera.CAMERA_TYPE_BACK

## Quality constants

    camera.CAPTURE_QUALITY_HIGH
    camera.CAPTURE_QUALITY_MEDIUM
    camera.CAPTURE_QUALITY_LOW

## camera.start_capture(type, quality)

Returns true if the capture starts well

    if camera.start_capture(camera.CAMERA_TYPE_BACK, camera.CAPTURE_QUALITY_HIGH) then
        -- do stuff
    end
  
## camera.stop_capture()

Stops a previously started capture session

## camera.get_info()

Gets the info from the current capture session

    local info = camera.get_info()
    print("width", info.width)
    print("height", info.height)
  
## camera.get_frame()

Retrieves the camera pixel buffer
This buffer has one stream named "rgb", and is of type buffer.VALUE_TYPE_UINT8 and has the value count of 1

    self.cameraframe = camera.get_frame()
  
  
