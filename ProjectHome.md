## Overview ##

Allows loading DDS (DirectDraw Surface) and PVR (PowerVR) files in WIC-enabled applications. Saving feature is planned for the future releases.

Can be built for x86 and x86\_64 platforms.

### DDS ###

Features:
  * Support for DXT1, DXT3 and DXT5 formats via libsquish library.
  * Support for any linear (noncompressed) format.
  * Optimized for SSE-2

Issues:
  * Some fields (for example, pitch) in DDS header are ignored in current version.

### PVR ###

Features:
  * Support for PVRTC-2 and PVRTC-4 formats via PowerVR SDK code.

## Installation ##

Check-out source, then build and execute **regsvr32 dds-wic-codec.dll** command (administrator rights **are required** to register the codec properly).

_dds-wic-codec.dll_ should be signed with trusted certificate. You can easily make your own certificate, sign the dll and put the certificate to Windows trusted root certificates store.

In Windows Vista and Windows 7 you can use this codec to show thumbnails of files in supported formats in Windows Explorer. To enable thumbnails you need the following registry entries (here for PVR):
```
[HKEY_CLASSES_ROOT\.pvr\ShellEx]

[HKEY_CLASSES_ROOT\.pvr\ShellEx\{e357fccd-a995-4576-b01f-234630154e96}]
@="{C7657C4A-9F68-40fa-A4DF-96BC08EB3551}"
```