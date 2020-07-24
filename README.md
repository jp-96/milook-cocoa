# MILooK-COCOA
見ルックCOCOA is scanning COCOA near.

# How to build, on Windows 10

1. Install zephyrproject to the directory, C:\\zephyr. see: https://docs.zephyrproject.org/latest/getting_started/index.html
1. Clone [MILook-COCOA](https://github.com/jp-96/milook-cocoa) to the directory, C:\zephyr\milook-cocoa.
1. `cd C:\zephyr\zephyrproject\zephyr`
1. `west build -p auto -b bbc_microbit ..\..\milook-cocoa`
1. `west flash`
