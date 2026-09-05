QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
INCLUDEPATH += D:/vcpkg-master/installed/x64-windows/include
INCLUDEPATH+=D:/SeetaFace6/build_msvc/include
SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

CONFIG(debug, debug|release) {
    LIBS += -LD:/vcpkg-master/installed/x64-windows/debug/lib \
            -lopencv_core4d -lopencv_imgproc4d -lopencv_imgcodecs4d \
            -lopencv_highgui4d -lopencv_videoio4d \
            -lopencv_objdetect4d
} else {
    LIBS += -LD:/vcpkg-master/installed/x64-windows/lib \
            -lopencv_core4 -lopencv_imgproc4 -lopencv_imgcodecs4 \
            -lopencv_highgui4 -lopencv_videoio4 \
            -lopencv_objdetect4
}

LIBS+=-LD:/SeetaFace6/build_msvc/lib \
        -lSeetaFaceDetector600 \
        -lSeetaFaceLandmarker600 \
        -lSeetaFaceRecognizer610
# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
