//
//  Created by Gabriel Calero & Cristian Duarte on 2016/11/03
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "DaydreamDisplayPlugin.h"
#include <ViewFrustum.h>
#include <controllers/Pose.h>
#include <ui-plugins/PluginContainer.h>
#include <gl/GLWidget.h>
#include <gpu/Frame.h>
#include <CursorManager.h>

#ifdef ANDROID
#include <QtOpenGL/QGLWidget>
#endif

const QString DaydreamDisplayPlugin::NAME("Daydream");

glm::uvec2 DaydreamDisplayPlugin::getRecommendedUiSize() const {
    auto window = _container->getPrimaryWidget();
    glm::vec2 windowSize = toGlm(window->size());
    return windowSize;
}


bool DaydreamDisplayPlugin::isSupported() const {
    return true;
}

void DaydreamDisplayPlugin::resetSensors() {
    _currentRenderFrameInfo.renderPose = glm::mat4(); // identity
}

void DaydreamDisplayPlugin::internalPresent() {
    //qDebug() << "[DaydreamDisplayPlugin] internalPresent";
    PROFILE_RANGE_EX(__FUNCTION__, 0xff00ff00, (uint64_t)presentCount())

 // Composite together the scene, overlay and mouse cursor
    hmdPresent();

        GvrState *gvrState = GvrState::getInstance();
        gvr::Frame frame = gvrState->_swapchain->AcquireFrame();
        frame.BindBuffer(0);

    render([&](gpu::Batch& batch) {
        batch.enableStereo(false);
        batch.resetViewTransform();
        //batch.setFramebuffer(defaultFb);
        ivec4 viewport(0,0, gvrState->_framebuf_size.width, gvrState->_framebuf_size.height);
        batch.setViewportTransform(viewport);
        batch.setStateScissorRect(viewport);
        batch.setResourceTexture(0, _compositeFramebuffer->getRenderBuffer(0));
        if (!_presentPipeline) {
            //qDebug() << "OpenGLDisplayPlugin setting null _presentPipeline ";
        }

        batch.setPipeline(_presentPipeline);
        batch.draw(gpu::TRIANGLE_STRIP, 4);
    });

    gvr::ClockTimePoint pred_time = gvr::GvrApi::GetTimePointNow();
    pred_time.monotonic_system_time_nanos += 50000000; // 50ms

    gvr::Mat4f head_view = gvrState->_gvr_api->GetHeadSpaceFromStartSpaceRotation(pred_time);
    frame.Unbind();
    frame.Submit(gvrState->_viewport_list, head_view);

    swapBuffers();
}

ivec4 DaydreamDisplayPlugin::getViewportForSourceSize(const uvec2& size) const {
    // screen preview mirroring
    auto window = _container->getPrimaryWidget();
    auto devicePixelRatio = window->devicePixelRatio();
    auto windowSize = toGlm(window->size());
    //qDebug() << "[DaydreamDisplayPlugin] windowSize " << windowSize; 
    windowSize *= devicePixelRatio;
    float windowAspect = aspect(windowSize);
    float sceneAspect = aspect(size);
    float aspectRatio = sceneAspect / windowAspect;

    uvec2 targetViewportSize = windowSize;
    if (aspectRatio < 1.0f) {
        targetViewportSize.x *= aspectRatio;
    } else {
        targetViewportSize.y /= aspectRatio;
    }

    uvec2 targetViewportPosition;
    if (targetViewportSize.x < windowSize.x) {
        targetViewportPosition.x = (windowSize.x - targetViewportSize.x) / 2;
    } else if (targetViewportSize.y < windowSize.y) {
        targetViewportPosition.y = (windowSize.y - targetViewportSize.y) / 2;
    }
    return ivec4(targetViewportPosition, targetViewportSize);
}

float DaydreamDisplayPlugin::getLeftCenterPixel() const {
    glm::mat4 eyeProjection = _eyeProjections[Left];
    glm::mat4 inverseEyeProjection = glm::inverse(eyeProjection);
    vec2 eyeRenderTargetSize = { _renderTargetSize.x / 2, _renderTargetSize.y };
    vec4 left = vec4(-1, 0, -1, 1);
    vec4 right = vec4(1, 0, -1, 1);
    vec4 right2 = inverseEyeProjection * right;
    vec4 left2 = inverseEyeProjection * left;
    left2 /= left2.w;
    right2 /= right2.w;
    float width = -left2.x + right2.x;
    float leftBias = -left2.x / width;
    float leftCenterPixel = eyeRenderTargetSize.x * leftBias;
    return leftCenterPixel;
}


bool DaydreamDisplayPlugin::beginFrameRender(uint32_t frameIndex) {
    _currentRenderFrameInfo = FrameInfo();
    _currentRenderFrameInfo.sensorSampleTime = secTimestampNow();
    _currentRenderFrameInfo.predictedDisplayTime = _currentRenderFrameInfo.sensorSampleTime;
    // FIXME simulate head movement
    //_currentRenderFrameInfo.renderPose = ;
    //_currentRenderFrameInfo.presentPose = _currentRenderFrameInfo.renderPose;


    GvrState *gvrState = GvrState::getInstance();
    gvr::ControllerQuat orientation = gvrState->_controller_state.GetOrientation();
    //qDebug() << "[DAYDREAM-CONTROLLER]: DaydreamDisplayPlugin::beginFrameRender " << orientation.qx << "," << orientation.qy << "," << orientation.qz << "," << orientation.qw;

    gvr::Mat4f controller_matrix = ControllerQuatToMatrix(orientation);
    glm::mat4 poseMat = glm::make_mat4(&(MatrixToGLArray(controller_matrix)[0]));

    //const vec3 linearVelocity(0.5, 0.5, 0.5); //= _nextSimPoseData.linearVelocities[deviceIndex];
    //const vec3 angularVelocity(0.3, 0.2, 0.4); // = _nextSimPoseData.angularVelocities[deviceIndex];
    //auto pose = daydreamControllerPoseToHandPose(true, poseMat, linearVelocity, angularVelocity);
    // transform into avatar frame
    //glm::mat4 controllerToAvatar = glm::inverse(inputCalibrationData.avatarMat) * inputCalibrationData.sensorToWorldMat;
    //_poseStateMap[controller::RIGHT_HAND] = pose.transform(poseMat);
    //_poseStateMap[controller::LEFT_HAND] = pose.transform(poseMat);


/*
_currentRenderFrameInfo = FrameInfo();
    _currentRenderFrameInfo.sensorSampleTime = ovr_GetTimeInSeconds();
    _currentRenderFrameInfo.predictedDisplayTime = ovr_GetPredictedDisplayTime(_session, frameIndex);
    auto trackingState = ovr_GetTrackingState(_session, _currentRenderFrameInfo.predictedDisplayTime, ovrTrue);
    _currentRenderFrameInfo.renderPose = toGlm(trackingState.HeadPose.ThePose);
    _currentRenderFrameInfo.presentPose = _currentRenderFrameInfo.renderPose;

    std::array<glm::mat4, 2> handPoses;
    // Make controller poses available to the presentation thread
    ovr_for_each_hand([&](ovrHandType hand) {
        static const auto REQUIRED_HAND_STATUS = ovrStatus_OrientationTracked & ovrStatus_PositionTracked;
        if (REQUIRED_HAND_STATUS != (trackingState.HandStatusFlags[hand] & REQUIRED_HAND_STATUS)) {
            return;
        }

        auto correctedPose = ovrControllerPoseToHandPose(hand, trackingState.HandPoses[hand]);
        static const glm::quat HAND_TO_LASER_ROTATION = glm::rotation(Vectors::UNIT_Z, Vectors::UNIT_NEG_Y);
        handPoses[hand] = glm::translate(glm::mat4(), correctedPose.translation) * glm::mat4_cast(correctedPose.rotation * HAND_TO_LASER_ROTATION);
    });

    withNonPresentThreadLock([&] {
        _uiModelTransform = DependencyManager::get<CompositorHelper>()->getModelTransform();
        _handPoses = handPoses;
        _frameInfos[frameIndex] = _currentRenderFrameInfo;
    });*/


    withNonPresentThreadLock([&] {
        _uiModelTransform = DependencyManager::get<CompositorHelper>()->getModelTransform();
        _frameInfos[frameIndex] = _currentRenderFrameInfo;
        
        _handPoses[0] = poseMat;
        _handLasers[0].color = vec4(1, 0, 0, 1);
        _handLasers[0].mode = HandLaserMode::Overlay;

        _handPoses[1] = glm::translate(mat4(), vec3(0.1f, 0.3f, 0.0f));
        _handLasers[1].color = vec4(0, 1, 1, 1);
        _handLasers[1].mode = HandLaserMode::Overlay;
    });
    return Parent::beginFrameRender(frameIndex);
}

// DLL based display plugins MUST initialize GLEW inside the DLL code.
void DaydreamDisplayPlugin::customizeContext() {
    // glewContextInit undefined for android (why it isn't taking it from the ndk?) AND!!!
//#ifndef ANDROID
      //emit deviceConnected(getName());

    glewInit();
    glGetError(); // clear the potential error from glewExperimental
    Parent::customizeContext();
//#endif
}

bool DaydreamDisplayPlugin::internalActivate() {
    //qDebug() << "[DaydreamDisplayPlugin] internalActivate with _gvr_context " << __gvr_context;
    _container->setFullscreen(nullptr, true);

    GvrState::init(__gvr_context);
    GvrState *gvrState = GvrState::getInstance();

    if (gvrState->_gvr_api) {
        qDebug() << "Initialize _gvr_api GL " << gvrState;
        gvrState->_gvr_api->InitializeGl();
    }

    //qDebug() << "[DaydreamDisplayPlugin] internalActivate with _gvr_api " << gvrState->_gvr_api->GetTimePointNow().monotonic_system_time_nanos;

    std::vector<gvr::BufferSpec> specs;
    specs.push_back(gvrState->_gvr_api->CreateBufferSpec());
    gvrState->_framebuf_size = gvrState->_gvr_api->GetMaximumEffectiveRenderTargetSize();

    //qDebug() << "_framebuf_size " << gvrState->_framebuf_size.width << ", " << gvrState->_framebuf_size.height; //  3426 ,  1770

    auto window = _container->getPrimaryWidget();
    glm::vec2 windowSize = toGlm(window->size());

    // Because we are using 2X MSAA, we can render to half as many pixels and
    // achieve similar quality. Scale each dimension by sqrt(2)/2 ~= 7/10ths.
    gvrState->_framebuf_size.width = windowSize.x;//(7 * gvrState->_framebuf_size.width) / 10;
    gvrState->_framebuf_size.height = windowSize.y; //(7 * gvrState->_framebuf_size.height) / 10;

    specs[0].SetSize(gvrState->_framebuf_size);
    specs[0].SetColorFormat(GVR_COLOR_FORMAT_RGBA_8888);
    specs[0].SetDepthStencilFormat(GVR_DEPTH_STENCIL_FORMAT_DEPTH_16);
    specs[0].SetSamples(2);
    //qDebug() << "Resetting swapchain";
    gvrState->_swapchain.reset(new gvr::SwapChain(gvrState->_gvr_api->CreateSwapChain(specs)));
    gvrState->_viewport_list.SetToRecommendedBufferViewports();


    resetEyeProjections(gvrState);

    _ipd = 0.0327499993f * 2.0f;

    _eyeOffsets[0][3] = vec4{ -0.0327499993, 0.0, 0.0149999997, 1.0 };
    _eyeOffsets[1][3] = vec4{ 0.0327499993, 0.0, 0.0149999997, 1.0 };

    _renderTargetSize = glm::vec2(gvrState->_framebuf_size.width ,  gvrState->_framebuf_size.height);

    // This must come after the initialization, so that the values calculated
    // above are available during the customizeContext call (when not running
    // in threaded present mode)
    return Parent::internalActivate();
}

void DaydreamDisplayPlugin::updatePresentPose() {
    gvr::ClockTimePoint pred_time = gvr::GvrApi::GetTimePointNow();
    pred_time.monotonic_system_time_nanos += 50000000; // 50ms
    
    GvrState *gvrState = GvrState::getInstance();
    gvr::Mat4f head_view =
    gvrState->_gvr_api->GetHeadSpaceFromStartSpaceRotation(pred_time);

    glm::mat4 glmHeadView = glm::inverse(glm::make_mat4(&(MatrixToGLArray(head_view)[0])));

    _currentPresentFrameInfo.presentPose = glmHeadView;

    if (gvrState->_controller_state.GetApiStatus() == gvr_controller_api_status::GVR_CONTROLLER_API_OK &&
        gvrState->_controller_state.GetConnectionState() == gvr_controller_connection_state::GVR_CONTROLLER_CONNECTED) {

      if (gvrState->_controller_state.GetRecentered()) {
        //qDebug() << "[DAYDREAM-CONTROLLER] Recenter";
        resetEyeProjections(gvrState);
      }
    }
}

void DaydreamDisplayPlugin::resetEyeProjections(GvrState *gvrState) {
        gvr::ClockTimePoint pred_time = gvr::GvrApi::GetTimePointNow();
    pred_time.monotonic_system_time_nanos += 50000000; // 50ms

    gvr::Mat4f left_eye_view = gvrState->_gvr_api->GetEyeFromHeadMatrix(GVR_LEFT_EYE);
    gvr::Mat4f right_eye_view =gvrState->_gvr_api->GetEyeFromHeadMatrix(GVR_RIGHT_EYE);

    gvr::BufferViewport scratch_viewport(gvrState->_gvr_api->CreateBufferViewport());

    gvrState->_viewport_list.GetBufferViewport(0, &scratch_viewport);
    gvr::Mat4f proj_matrix = PerspectiveMatrixFromView(scratch_viewport.GetSourceFov(), 0.1, 1000.0);

    gvr::Mat4f mvp = MatrixMul(proj_matrix, left_eye_view);
    std::array<float, 16> mvpArr = MatrixToGLArray(mvp);

    _eyeProjections[0][0] = vec4{mvpArr[0],mvpArr[1],mvpArr[2],mvpArr[3]};
    _eyeProjections[0][1] = vec4{mvpArr[4],mvpArr[5],mvpArr[6],mvpArr[7]};
    _eyeProjections[0][2] = vec4{mvpArr[8],mvpArr[9],mvpArr[10],mvpArr[11]};
    _eyeProjections[0][3] = vec4{mvpArr[12],mvpArr[13],mvpArr[14],mvpArr[15]};

    gvrState->_viewport_list.GetBufferViewport(1, &scratch_viewport);
    
    proj_matrix = PerspectiveMatrixFromView(scratch_viewport.GetSourceFov(), 0.1, 1000.0);
    mvp = MatrixMul(proj_matrix, right_eye_view);
    mvpArr = MatrixToGLArray(mvp);

    _eyeProjections[1][0] = vec4{mvpArr[0],mvpArr[1],mvpArr[2],mvpArr[3]};
    _eyeProjections[1][1] = vec4{mvpArr[4],mvpArr[5],mvpArr[6],mvpArr[7]};
    _eyeProjections[1][2] = vec4{mvpArr[8],mvpArr[9],mvpArr[10],mvpArr[11]};
    _eyeProjections[1][3] = vec4{mvpArr[12],mvpArr[13],mvpArr[14],mvpArr[15]};

    for_each_eye([&](Eye eye) {
        _eyeInverseProjections[eye] = glm::inverse(_eyeProjections[eye]);
    });

    _cullingProjection = _eyeProjections[0];
}

void DaydreamDisplayPlugin::compositePointer() {
    //qDebug() << "DaydreamDisplayPlugin::compositePointer()";
    auto& cursorManager = Cursor::Manager::instance();
    const auto& cursorData = _cursorsData[cursorManager.getCursor()->getIcon()];
    auto compositorHelper = DependencyManager::get<CompositorHelper>();

    // Reconstruct the headpose from the eye poses
    //auto headPosition = vec3(glm::inverse(_currentPresentFrameInfo.presentPose)[3]);
    GvrState *gvrState = GvrState::getInstance();

    gvr::Mat4f controller_matrix = ControllerQuatToMatrix(gvrState->_controller_state.GetOrientation());
    float scale = 5.0f;
    gvr::Mat4f neutral_matrix = {
      scale, 0.0f, 0.0f,   0.0f,
      0.0f,  scale, 0.0f,  0.0f,
      0.0f,  0.0f, scale,  -200.0f,
      0.0f,  0.0f, 0.0f,  1.0f,
    };
    gvr::Mat4f model_matrix = MatrixMul(controller_matrix, neutral_matrix);
    //auto controllerOrientation = vec3(orientation.qx, orientation.qy, orientation.qz);
    render([&](gpu::Batch& batch) {
        // FIXME use standard gpu stereo rendering for this.
        batch.enableStereo(false);
        batch.setFramebuffer(_compositeFramebuffer);
        batch.setPipeline(_cursorPipeline);
        batch.setResourceTexture(0, cursorData.texture);
        batch.resetViewTransform();
        for_each_eye([&](Eye eye) {
            batch.setViewportTransform(eyeViewport(eye));
            batch.setProjectionTransform(_eyeProjections[eye]);
            mat4 cursor_matrix = glm::inverse(_currentPresentFrameInfo.presentPose * getEyeToHeadTransform(eye)) * glm::make_mat4(&(MatrixToGLArray(model_matrix)[0]));
            batch.setModelTransform(cursor_matrix);
            batch.draw(gpu::TRIANGLE_STRIP, 4);
        });
    });
}



