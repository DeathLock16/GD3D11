#pragma once
#include "pch.h"

/** Global engine information */
class BaseGraphicsEngine;
class GothicAPI;
class BaseAntTweakBar;
class GGame;
class ThreadPool;

__declspec(selectany) const char* ENGINE_BASE_DIR = "system\\GD3D11\\";

__declspec(selectany) const char* VERSION_STRING = "Version " VERSION_NUMBER " (" __DATE__ ") Vargoth MMO";

namespace Engine {
    /** If true, we will just pass everything to the usual ddraw.dll */
    __declspec(selectany) bool PassThrough;

    /** Global engine object */
    __declspec(selectany) BaseGraphicsEngine* GraphicsEngine;

    /** Global GothicAPI object */
    __declspec(selectany) GothicAPI* GAPI;

    /** Global AntTweakBar object */
    __declspec(selectany) BaseAntTweakBar* AntTweakBar;

    /** Global rendering threadpool */
    __declspec(selectany) ThreadPool* RenderingThreadPool;

    /** Global worker threadpool */
    __declspec(selectany) ThreadPool* WorkerThreadPool;

    /** Refresh worker threadpool */
    void RefreshWorkerThreadpool();

    /** Creates main graphics engine */
    void CreateGraphicsEngine();

    /** Creates the Global GAPI-Object */
    void CreateGothicAPI();

    /** Called when the game is about to close */
    void OnShutDown();
};

