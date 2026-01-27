#! /bin/sh -e

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd) || exit 1
cd "$SCRIPT_DIR" || exit 1

if [ ! -f ../../opencv/lib/linux/libopencv_core.so.4.8.0 ] ||
    [ ! -f ../../opencv/lib/linux/libopencv_imgproc.so.4.8.0 ] || 
    [ ! -f ../../opencv/lib/linux/libopencv_highgui.so.4.8.0 ] ||
    [ ! -f ../../opencv/lib/linux/libopencv_imgcodecs.so.4.8.0 ]; then
    echo "opencv 库不存在"
    exit 1
fi
if [ ! -f ../../screenshot ]; then
    echo "可执行文件不存在"
    exit 1
fi

rm -rf screenshot
mkdir -p screenshot/DEBIAN/
mkdir -p screenshot/usr/bin/
mkdir -p screenshot/usr/lib/screenshot/
mkdir -p screenshot/usr/share/applications/
mkdir -p screenshot/usr/share/icons/hicolor/32x32/apps/

cp -f control postrm prerm screenshot/DEBIAN
chmod 644 screenshot/DEBIAN/control 
chmod 755 screenshot/DEBIAN/postrm
chmod 755 screenshot/DEBIAN/prerm

cp -f ../../screenshot screenshot/usr/bin
chmod 755 screenshot/usr/bin/screenshot

cp -f ../../opencv/lib/linux/libopencv_core.so.4.8.0  ../../opencv/lib/linux/libopencv_imgproc.so.4.8.0  ../../opencv/lib/linux/libopencv_highgui.so.4.8.0 ../../opencv/lib/linux/libopencv_imgcodecs.so.4.8.0 screenshot/usr/lib/screenshot
cd screenshot/usr/lib/screenshot
ln -sf libopencv_core.so.4.8.0 libopencv_core.so
ln -sf libopencv_core.so.4.8.0 libopencv_core.so.408
ln -sf libopencv_imgproc.so.4.8.0 libopencv_imgproc.so
ln -sf libopencv_imgproc.so.4.8.0 libopencv_imgproc.so.408
ln -sf libopencv_highgui.so.4.8.0 libopencv_highgui.so
ln -sf libopencv_highgui.so.4.8.0 libopencv_highgui.so.408
ln -sf libopencv_imgcodecs.so.4.8.0 libopencv_imgcodecs.so
ln -sf libopencv_imgcodecs.so.4.8.0 libopencv_imgcodecs.so.408
cd - >/dev/null

cp -f screenshot.desktop screenshot/usr/share/applications/

cp -f screenshot.png screenshot/usr/share/icons/hicolor/32x32/apps/

SIZE=$(du -sk screenshot --exclude=screenshot/DEBIAN | cut -f1)
if grep -q '^Installed-Size:' control; then
    sed -i "/^Installed-Size:/c Installed-Size: $SIZE" screenshot/DEBIAN/control
else
    sed -i "/^Maintainer:/a Installed-Size: $SIZE" screenshot/DEBIAN/control
fi

dpkg-deb --build screenshot

rm -r screenshot
