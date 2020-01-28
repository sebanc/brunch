if [ -d /system/etc/camera ]; then rm -r /system/etc/camera; fi
if [ -f /system/lib/udev/rules.d/50-camera.rules ]; then rm /system/lib/udev/rules.d/50-camera.rules; fi

ret=0

mkdir /system/etc/camera
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 0))); fi

nr=0
for i in $(dmesg | grep "uvcvideo: Found UVC" | sed 's/^.*(//;s/)$//'); do
	vendor=$(echo $i | cut -d: -f1)
	product=$(echo $i | cut -d: -f2)
	echo camera"$nr".module0.usb_vid_pid=$vendor:$product >> /system/etc/camera/camera_characteristics.conf
	echo "SUBSYSTEM==\"video4linux\", ATTRS{idVendor}==\"$vendor\", ATTRS{idProduct}==\"$product\", SYMLINK+=\"camera-internal$nr\"" >> /system/lib/udev/rules.d/50-camera.rules
	nr=$((nr+1))
done
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 1))); fi

mkdir /system/etc/camera/one_camera
cat >/system/etc/camera/one_camera/media_profiles.xml <<ONECAMERA
<?xml version="1.0" encoding="utf-8"?>
<!-- Copyright 2017 The Android Open Source Project

     Licensed under the Apache License, Version 2.0 (the "License");
     you may not use this file except in compliance with the License.
     You may obtain a copy of the License at

          http://www.apache.org/licenses/LICENSE-2.0

     Unless required by applicable law or agreed to in writing, software
     distributed under the License is distributed on an "AS IS" BASIS,
     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     See the License for the specific language governing permissions and
     limitations under the License.
-->
<!DOCTYPE MediaSettings [
<!ELEMENT MediaSettings (CamcorderProfiles,
                         EncoderOutputFileFormat+,
                         VideoEncoderCap+,
                         AudioEncoderCap+,
                         VideoDecoderCap,
                         AudioDecoderCap)>
<!ELEMENT CamcorderProfiles (EncoderProfile+, ImageEncoding+, ImageDecoding, Camera)>
<!ELEMENT EncoderProfile (Video, Audio)>
<!ATTLIST EncoderProfile quality (high|low) #REQUIRED>
<!ATTLIST EncoderProfile fileFormat (mp4|3gp) #REQUIRED>
<!ATTLIST EncoderProfile duration (30|60) #REQUIRED>
<!ELEMENT Video EMPTY>
<!ATTLIST Video codec (h264|h263|m4v) #REQUIRED>
<!ATTLIST Video bitRate CDATA #REQUIRED>
<!ATTLIST Video width CDATA #REQUIRED>
<!ATTLIST Video height CDATA #REQUIRED>
<!ATTLIST Video frameRate CDATA #REQUIRED>
<!ELEMENT Audio EMPTY>
<!ATTLIST Audio codec (amrnb|amrwb|aac) #REQUIRED>
<!ATTLIST Audio bitRate CDATA #REQUIRED>
<!ATTLIST Audio sampleRate CDATA #REQUIRED>
<!ATTLIST Audio channels (1|2) #REQUIRED>
<!ELEMENT ImageEncoding EMPTY>
<!ATTLIST ImageEncoding quality (90|80|70|60|50|40) #REQUIRED>
<!ELEMENT ImageDecoding EMPTY>
<!ATTLIST ImageDecoding memCap CDATA #REQUIRED>
<!ELEMENT Camera EMPTY>
<!ELEMENT EncoderOutputFileFormat EMPTY>
<!ATTLIST EncoderOutputFileFormat name (mp4|3gp) #REQUIRED>
<!ELEMENT VideoEncoderCap EMPTY>
<!ATTLIST VideoEncoderCap name (h264|h263|m4v|wmv) #REQUIRED>
<!ATTLIST VideoEncoderCap enabled (true|false) #REQUIRED>
<!ATTLIST VideoEncoderCap minBitRate CDATA #REQUIRED>
<!ATTLIST VideoEncoderCap maxBitRate CDATA #REQUIRED>
<!ATTLIST VideoEncoderCap minFrameWidth CDATA #REQUIRED>
<!ATTLIST VideoEncoderCap maxFrameWidth CDATA #REQUIRED>
<!ATTLIST VideoEncoderCap minFrameHeight CDATA #REQUIRED>
<!ATTLIST VideoEncoderCap maxFrameHeight CDATA #REQUIRED>
<!ATTLIST VideoEncoderCap minFrameRate CDATA #REQUIRED>
<!ATTLIST VideoEncoderCap maxFrameRate CDATA #REQUIRED>
<!ELEMENT AudioEncoderCap EMPTY>
<!ATTLIST AudioEncoderCap name (amrnb|amrwb|aac|wma) #REQUIRED>
<!ATTLIST AudioEncoderCap enabled (true|false) #REQUIRED>
<!ATTLIST AudioEncoderCap minBitRate CDATA #REQUIRED>
<!ATTLIST AudioEncoderCap maxBitRate CDATA #REQUIRED>
<!ATTLIST AudioEncoderCap minSampleRate CDATA #REQUIRED>
<!ATTLIST AudioEncoderCap maxSampleRate CDATA #REQUIRED>
<!ATTLIST AudioEncoderCap minChannels (1|2) #REQUIRED>
<!ATTLIST AudioEncoderCap maxChannels (1|2) #REQUIRED>
<!ELEMENT VideoDecoderCap EMPTY>
<!ATTLIST VideoDecoderCap name (wmv) #REQUIRED>
<!ATTLIST VideoDecoderCap enabled (true|false) #REQUIRED>
<!ELEMENT AudioDecoderCap EMPTY>
<!ATTLIST AudioDecoderCap name (wma) #REQUIRED>
<!ATTLIST AudioDecoderCap enabled (true|false) #REQUIRED>
]>
<!--
     This file is used to declare the multimedia profiles and capabilities
     on an android-powered device.
-->
<MediaSettings>
    <!-- Each camcorder profile defines a set of predefined configuration parameters -->
    <CamcorderProfiles cameraId="0">
        <EncoderProfile quality="720p" fileFormat="mp4" duration="60">
            <Video codec="h264"
                   bitRate="8000000"
                   width="1280"
                   height="720"
                   frameRate="30" />
            <Audio codec="aac"
                   bitRate="96000"
                   sampleRate="44100"
                   channels="1" />
        </EncoderProfile>
        <EncoderProfile quality="timelapse720p" fileFormat="mp4" duration="60">
            <Video codec="h264"
                   bitRate="8000000"
                   width="1280"
                   height="720"
                   frameRate="30" />
            <!-- Audio settings are not used for timealpse video recording -->
            <Audio codec="aac"
                   bitRate="96000"
                   sampleRate="44100"
                   channels="1" />
        </EncoderProfile>
        <ImageEncoding quality="90" />
        <ImageEncoding quality="80" />
        <ImageEncoding quality="70" />
        <ImageDecoding memCap="20000000" />
    </CamcorderProfiles>

    <EncoderOutputFileFormat name="3gp" />
    <EncoderOutputFileFormat name="mp4" />

    <!--
         If a codec is not enabled, it is invisible to the applications
         In other words, the applications won't be able to use the codec
         or query the capabilities of the codec at all if it is disabled
    -->
    <VideoEncoderCap name="h264" enabled="true"
        minBitRate="64000" maxBitRate="12000000"
        minFrameWidth="320" maxFrameWidth="1920"
        minFrameHeight="240" maxFrameHeight="1080"
        minFrameRate="15" maxFrameRate="30" />

    <VideoEncoderCap name="h263" enabled="true"
        minBitRate="64000" maxBitRate="1000000"
        minFrameWidth="320" maxFrameWidth="1920"
        minFrameHeight="240" maxFrameHeight="1080"
        minFrameRate="15" maxFrameRate="30" />

    <VideoEncoderCap name="m4v" enabled="true"
        minBitRate="64000" maxBitRate="2000000"
        minFrameWidth="320" maxFrameWidth="1920"
        minFrameHeight="240" maxFrameHeight="1080"
        minFrameRate="15" maxFrameRate="30" />

    <AudioEncoderCap name="aac" enabled="true"
        minBitRate="758" maxBitRate="288000"
        minSampleRate="8000" maxSampleRate="48000"
        minChannels="1" maxChannels="1" />

    <AudioEncoderCap name="heaac" enabled="true"
        minBitRate="8000" maxBitRate="64000"
        minSampleRate="16000" maxSampleRate="48000"
        minChannels="1" maxChannels="1" />

    <AudioEncoderCap name="aaceld" enabled="true"
        minBitRate="16000" maxBitRate="192000"
        minSampleRate="16000" maxSampleRate="48000"
        minChannels="1" maxChannels="1" />

    <AudioEncoderCap name="amrwb" enabled="true"
        minBitRate="6600" maxBitRate="23050"
        minSampleRate="16000" maxSampleRate="16000"
        minChannels="1" maxChannels="1" />

    <AudioEncoderCap name="amrnb" enabled="true"
        minBitRate="5525" maxBitRate="12200"
        minSampleRate="8000" maxSampleRate="8000"
        minChannels="1" maxChannels="1" />

    <!--
        FIXME:
        We do not check decoder capabilities at present
        At present, we only check whether windows media is visible
        for TEST applications. For other applications, we do
        not perform any checks at all.
    -->
    <VideoDecoderCap name="wmv" enabled="false"/>
    <AudioDecoderCap name="wma" enabled="false"/>
</MediaSettings>
ONECAMERA
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 2))); fi

mkdir /system/etc/camera/two_cameras
cat >/system/etc/camera/two_cameras/media_profiles.xml <<TWOCAMERAS
<?xml version="1.0" encoding="utf-8"?>
<!-- Copyright 2017 The Android Open Source Project

     Licensed under the Apache License, Version 2.0 (the "License");
     you may not use this file except in compliance with the License.
     You may obtain a copy of the License at

          http://www.apache.org/licenses/LICENSE-2.0

     Unless required by applicable law or agreed to in writing, software
     distributed under the License is distributed on an "AS IS" BASIS,
     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
     See the License for the specific language governing permissions and
     limitations under the License.
-->
<!DOCTYPE MediaSettings [
<!ELEMENT MediaSettings (CamcorderProfiles,
                         EncoderOutputFileFormat+,
                         VideoEncoderCap+,
                         AudioEncoderCap+,
                         VideoDecoderCap,
                         AudioDecoderCap)>
<!ELEMENT CamcorderProfiles (EncoderProfile+, ImageEncoding+, ImageDecoding, Camera)>
<!ELEMENT EncoderProfile (Video, Audio)>
<!ATTLIST EncoderProfile quality (high|low) #REQUIRED>
<!ATTLIST EncoderProfile fileFormat (mp4|3gp) #REQUIRED>
<!ATTLIST EncoderProfile duration (30|60) #REQUIRED>
<!ELEMENT Video EMPTY>
<!ATTLIST Video codec (h264|h263|m4v) #REQUIRED>
<!ATTLIST Video bitRate CDATA #REQUIRED>
<!ATTLIST Video width CDATA #REQUIRED>
<!ATTLIST Video height CDATA #REQUIRED>
<!ATTLIST Video frameRate CDATA #REQUIRED>
<!ELEMENT Audio EMPTY>
<!ATTLIST Audio codec (amrnb|amrwb|aac) #REQUIRED>
<!ATTLIST Audio bitRate CDATA #REQUIRED>
<!ATTLIST Audio sampleRate CDATA #REQUIRED>
<!ATTLIST Audio channels (1|2) #REQUIRED>
<!ELEMENT ImageEncoding EMPTY>
<!ATTLIST ImageEncoding quality (90|80|70|60|50|40) #REQUIRED>
<!ELEMENT ImageDecoding EMPTY>
<!ATTLIST ImageDecoding memCap CDATA #REQUIRED>
<!ELEMENT Camera EMPTY>
<!ELEMENT EncoderOutputFileFormat EMPTY>
<!ATTLIST EncoderOutputFileFormat name (mp4|3gp) #REQUIRED>
<!ELEMENT VideoEncoderCap EMPTY>
<!ATTLIST VideoEncoderCap name (h264|h263|m4v|wmv) #REQUIRED>
<!ATTLIST VideoEncoderCap enabled (true|false) #REQUIRED>
<!ATTLIST VideoEncoderCap minBitRate CDATA #REQUIRED>
<!ATTLIST VideoEncoderCap maxBitRate CDATA #REQUIRED>
<!ATTLIST VideoEncoderCap minFrameWidth CDATA #REQUIRED>
<!ATTLIST VideoEncoderCap maxFrameWidth CDATA #REQUIRED>
<!ATTLIST VideoEncoderCap minFrameHeight CDATA #REQUIRED>
<!ATTLIST VideoEncoderCap maxFrameHeight CDATA #REQUIRED>
<!ATTLIST VideoEncoderCap minFrameRate CDATA #REQUIRED>
<!ATTLIST VideoEncoderCap maxFrameRate CDATA #REQUIRED>
<!ELEMENT AudioEncoderCap EMPTY>
<!ATTLIST AudioEncoderCap name (amrnb|amrwb|aac|wma) #REQUIRED>
<!ATTLIST AudioEncoderCap enabled (true|false) #REQUIRED>
<!ATTLIST AudioEncoderCap minBitRate CDATA #REQUIRED>
<!ATTLIST AudioEncoderCap maxBitRate CDATA #REQUIRED>
<!ATTLIST AudioEncoderCap minSampleRate CDATA #REQUIRED>
<!ATTLIST AudioEncoderCap maxSampleRate CDATA #REQUIRED>
<!ATTLIST AudioEncoderCap minChannels (1|2) #REQUIRED>
<!ATTLIST AudioEncoderCap maxChannels (1|2) #REQUIRED>
<!ELEMENT VideoDecoderCap EMPTY>
<!ATTLIST VideoDecoderCap name (wmv) #REQUIRED>
<!ATTLIST VideoDecoderCap enabled (true|false) #REQUIRED>
<!ELEMENT AudioDecoderCap EMPTY>
<!ATTLIST AudioDecoderCap name (wma) #REQUIRED>
<!ATTLIST AudioDecoderCap enabled (true|false) #REQUIRED>
]>
<!--
     This file is used to declare the multimedia profiles and capabilities
     on an android-powered device.
-->
<MediaSettings>
    <!-- Each camcorder profile defines a set of predefined configuration parameters -->
    <CamcorderProfiles cameraId="0">
        <EncoderProfile quality="720p" fileFormat="mp4" duration="60">
            <Video codec="h264"
                   bitRate="8000000"
                   width="1280"
                   height="720"
                   frameRate="30" />
            <Audio codec="aac"
                   bitRate="96000"
                   sampleRate="44100"
                   channels="1" />
        </EncoderProfile>
        <EncoderProfile quality="timelapse720p" fileFormat="mp4" duration="60">
            <Video codec="h264"
                   bitRate="8000000"
                   width="1280"
                   height="720"
                   frameRate="30" />
            <!-- Audio settings are not used for timealpse video recording -->
            <Audio codec="aac"
                   bitRate="96000"
                   sampleRate="44100"
                   channels="1" />
        </EncoderProfile>
        <ImageEncoding quality="90" />
        <ImageEncoding quality="80" />
        <ImageEncoding quality="70" />
        <ImageDecoding memCap="20000000" />
    </CamcorderProfiles>
    <CamcorderProfiles cameraId="1">
        <EncoderProfile quality="720p" fileFormat="mp4" duration="60">
            <Video codec="h264"
                   bitRate="8000000"
                   width="1280"
                   height="720"
                   frameRate="30" />
            <Audio codec="aac"
                   bitRate="96000"
                   sampleRate="44100"
                   channels="1" />
        </EncoderProfile>
        <EncoderProfile quality="timelapse720p" fileFormat="mp4" duration="60">
            <Video codec="h264"
                   bitRate="8000000"
                   width="1280"
                   height="720"
                   frameRate="30" />
            <!-- Audio settings are not used for timealpse video recording -->
            <Audio codec="aac"
                   bitRate="96000"
                   sampleRate="44100"
                   channels="1" />
        </EncoderProfile>
        <ImageEncoding quality="90" />
        <ImageEncoding quality="80" />
        <ImageEncoding quality="70" />
        <ImageDecoding memCap="20000000" />
    </CamcorderProfiles>

    <EncoderOutputFileFormat name="3gp" />
    <EncoderOutputFileFormat name="mp4" />

    <!--
         If a codec is not enabled, it is invisible to the applications
         In other words, the applications won't be able to use the codec
         or query the capabilities of the codec at all if it is disabled
    -->
    <VideoEncoderCap name="h264" enabled="true"
        minBitRate="64000" maxBitRate="12000000"
        minFrameWidth="320" maxFrameWidth="1920"
        minFrameHeight="240" maxFrameHeight="1080"
        minFrameRate="15" maxFrameRate="30" />

    <VideoEncoderCap name="h263" enabled="true"
        minBitRate="64000" maxBitRate="1000000"
        minFrameWidth="320" maxFrameWidth="1920"
        minFrameHeight="240" maxFrameHeight="1080"
        minFrameRate="15" maxFrameRate="30" />

    <VideoEncoderCap name="m4v" enabled="true"
        minBitRate="64000" maxBitRate="2000000"
        minFrameWidth="320" maxFrameWidth="1920"
        minFrameHeight="240" maxFrameHeight="1080"
        minFrameRate="15" maxFrameRate="30" />

    <AudioEncoderCap name="aac" enabled="true"
        minBitRate="758" maxBitRate="288000"
        minSampleRate="8000" maxSampleRate="48000"
        minChannels="1" maxChannels="1" />

    <AudioEncoderCap name="heaac" enabled="true"
        minBitRate="8000" maxBitRate="64000"
        minSampleRate="16000" maxSampleRate="48000"
        minChannels="1" maxChannels="1" />

    <AudioEncoderCap name="aaceld" enabled="true"
        minBitRate="16000" maxBitRate="192000"
        minSampleRate="16000" maxSampleRate="48000"
        minChannels="1" maxChannels="1" />

    <AudioEncoderCap name="amrwb" enabled="true"
        minBitRate="6600" maxBitRate="23050"
        minSampleRate="16000" maxSampleRate="16000"
        minChannels="1" maxChannels="1" />

    <AudioEncoderCap name="amrnb" enabled="true"
        minBitRate="5525" maxBitRate="12200"
        minSampleRate="8000" maxSampleRate="8000"
        minChannels="1" maxChannels="1" />

    <!--
        FIXME:
        We do not check decoder capabilities at present
        At present, we only check whether windows media is visible
        for TEST applications. For other applications, we do
        not perform any checks at all.
    -->
    <VideoDecoderCap name="wmv" enabled="false"/>
    <AudioDecoderCap name="wma" enabled="false"/>
</MediaSettings>
TWOCAMERAS
if [ ! "$?" -eq 0 ]; then ret=$((ret + (2 ** 3))); fi
exit $ret
