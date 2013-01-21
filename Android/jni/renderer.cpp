#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <android/native_window.h> // requires ndk r5 or newer
#include <EGL/egl.h> // requires ndk r5 or newer
#include <GLES/gl.h>
#include <EGL/eglplatform.h>
#include <string>
#include <time.h>
#include "renderer.h"

#include <common/MapController.hpp>
#include <common/MapDisplay.hpp>
#include <common/Camera.hpp>

void DetachThreadFromVM(void);

Renderer::Renderer() {
    LOG_INFO("Renderer instance created");
    pthread_mutex_init(&_mutex, 0);
    _window = NULL;
    _display = EGL_NO_DISPLAY;
    _surface = EGL_NO_SURFACE;
    _context = EGL_NO_CONTEXT;

    _currentTimeSec = double(clock()) / double(CLOCKS_PER_SEC);
    _initialTimeSec = _currentTimeSec;

    _rotateX = 0.0f;
    _rotateY = 0.0f;
    _rotateZ = 0.0f;
    _zoom = 0.0f;

    // Lock the mutex to keep the thread from doing anything until we get resumed
    pthread_mutex_lock(&_mutex);
    _paused = true;

    // Create the rendering thread
    _done = false;
    pthread_create(&_threadId, 0, threadStartCallback, this);

    return;
}

Renderer::~Renderer() {
    LOG_INFO("Renderer instance destroyed");
    _done = true;
    if (_paused) {
        pthread_mutex_unlock(&_mutex);
    }
    pthread_join(_threadId, 0);
    destroy();
    pthread_mutex_destroy(&_mutex);
    return;
}

void Renderer::resume() {
    assert(_paused);
    _paused = false;
    pthread_mutex_unlock(&_mutex);
    return;
}

void Renderer::pause() {
    assert(!_paused);
    pthread_mutex_lock(&_mutex);
    _paused = true;
    return;
}

void Renderer::bufferedRotationX(float radiansX) {
    _rotateX += radiansX;
}

void Renderer::bufferedRotationY(float radiansY) {
    _rotateY += radiansY;
}

void Renderer::bufferedRotationZ(float radiansZ) {
    _rotateZ += radiansZ;
}

void Renderer::bufferedZoom(float zoom) {
    _zoom += zoom;
}

void Renderer::setWindow(ANativeWindow *window, float displayScale) {
    _displayScale = displayScale;

    // notify render thread that window has changed
    if (!_paused) {
        pthread_mutex_lock(&_mutex);
    }
    _window = window;
    if (!_paused) {
        pthread_mutex_unlock(&_mutex);
    }

    return;
}

void Renderer::renderLoop() {
    pthread_mutex_lock(&_mutex);

    while (!_done) {
        // TODO: do we need to handle the window changing after creation, don't think so, but not sure
        if ((_display == EGL_NO_DISPLAY) && _window) {
            initialize();
        }

        // Main thread might want to take this context current during the gap below, need to release it here
        if (_display != EGL_NO_DISPLAY) {
            if (!eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
                    EGL_NO_CONTEXT)) {
                LOG_ERROR("eglMakeCurrent() returned error %d", eglGetError());
            }
        }

        // Let the main thread do any work that it is waiting on us for
        pthread_mutex_unlock(&_mutex);
        pthread_mutex_lock(&_mutex);

        if(_rotateX != 0.0f) {
            _mapController->display->camera->rotateRadiansX(_rotateX);
        }

        if(_rotateY != 0.0f) {
            _mapController->display->camera->rotateRadiansY(_rotateY);
        }

        if(_rotateZ != 0.0f) {
            _mapController->display->camera->rotateRadiansZ(_rotateZ);
        }

        if(_zoom != 0.0f) {
            _mapController->display->camera->zoomByScale(_zoom);
        }

        _rotateX = 0.0f;
        _rotateY = 0.0f;
        _rotateZ = 0.0f;
        _zoom = 0.0f;


        if (_display != EGL_NO_DISPLAY) {
            // Take back control of GL context
            if (!eglMakeCurrent(_display, _surface, _surface, _context)) {
                LOG_ERROR("eglMakeCurrent() returned error %d", eglGetError());
                // Hack: this fails when we rotate, delete the context to force a recreation
                // TODO: need to detect and handle this better
                destroy();
            }
        }

        if (_display != EGL_NO_DISPLAY) {
            drawFrame();

            if (!eglSwapBuffers(_display, _surface)) {
                LOG_ERROR("eglSwapBuffers() returned error %d", eglGetError());
            }
        }

    }

    pthread_mutex_unlock(&_mutex);

    DetachThreadFromVM();

    LOG_INFO("Render loop exits");

    return;
}

bool Renderer::initialize() {
    const EGLint attribs[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_NONE };

    const EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

    EGLDisplay display;
    EGLConfig config;
    EGLint numConfigs;
    EGLint format;
    EGLSurface surface;
    EGLContext context;
    EGLint width;
    EGLint height;

    LOG_INFO("Initializing context");

    if ((display = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY) {
        LOG_ERROR("eglGetDisplay() returned error %d", eglGetError());
        return false;
    }

    if (!eglInitialize(display, 0, 0)) {
        LOG_ERROR("eglInitialize() returned error %d", eglGetError());
        return false;
    }

    if (!eglChooseConfig(display, attribs, &config, 1, &numConfigs)) {
        LOG_ERROR("eglChooseConfig() returned error %d", eglGetError());
        destroy();
        return false;
    }

    if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format)) {
        LOG_ERROR("eglGetConfigAttrib() returned error %d", eglGetError());
        destroy();
        return false;
    }

    ANativeWindow_setBuffersGeometry(_window, 0, 0, format);

    if (!(surface = eglCreateWindowSurface(display, config, _window, 0))) {
        LOG_ERROR("eglCreateWindowSurface() returned error %d", eglGetError());
        destroy();
        return false;
    }

    if (!(context = eglCreateContext(display, config, 0, contextAttribs))) {
        LOG_ERROR("eglCreateContext() returned error %d", eglGetError());
        destroy();
        return false;
    }

    if (!eglMakeCurrent(display, surface, surface, context)) {
        LOG_ERROR("eglMakeCurrent() returned error %d", eglGetError());
        destroy();
        return false;
    }

    if (!eglQuerySurface(display, surface, EGL_WIDTH, &width)
            || !eglQuerySurface(display, surface, EGL_HEIGHT, &height)) {
        LOG_ERROR("eglQuerySurface() returned error %d", eglGetError());
        destroy();
        return false;
    }

    LOG("display size: %d %d %.2f", width, height, _displayScale);
    _display = display;
    _surface = surface;
    _context = context;
    _width = width;
    _height = height;

    LOG("created GL context");

    _mapController = new MapController;
    _mapController->display->camera->setDisplaySize(width, height);

    _mapController->display->setDisplayScale(_displayScale);

    _mapController->data->updateDisplay(_mapController->display);

    _mapController->display->camera->setAllowIdleAnimation(true);

    LOG("created map controller");

    return true;
}

void Renderer::destroy() {
    LOG_INFO("Destroying context");

    delete _mapController;
    _mapController = NULL;

    eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(_display, _context);
    eglDestroySurface(_display, _surface);
    eglTerminate(_display);

    _display = EGL_NO_DISPLAY;
    _surface = EGL_NO_SURFACE;
    _context = EGL_NO_CONTEXT;
    _width = 0;
    _height = 0;

    return;
}

MapController* Renderer::beginControllerModification(void) {
    // Wait for render thread to block
    if (!_paused) {
        pthread_mutex_lock(&_mutex);
    }

    // Make context current in this thread
    if (!eglMakeCurrent(_display, _surface, _surface, _context)) {
        LOG_ERROR("eglMakeCurrent() returned error %d", eglGetError());
    }

    return _mapController;
}

void Renderer::endControllerModification(void) {
    // Release context so renderer can grab it
    if (!eglMakeCurrent(_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
            EGL_NO_CONTEXT)) {
        LOG_ERROR("eglMakeCurrent() returned error %d", eglGetError());
    }

    if (!_paused) {
        // let the renderer go
        pthread_mutex_unlock(&_mutex);
    }
}

void Renderer::drawFrame() {
    _currentTimeSec = double(clock()) / double(CLOCKS_PER_SEC);
    _mapController->display->update(_currentTimeSec - _initialTimeSec);
    _mapController->display->draw();
}

void* Renderer::threadStartCallback(void *myself) {
    Renderer *renderer = (Renderer*) myself;

    renderer->renderLoop();
    pthread_exit(0);

    return 0;
}

void cameraMoveFinishedCallback(void) {

}