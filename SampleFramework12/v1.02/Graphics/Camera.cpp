//=================================================================================================
//
//  MJP's DX12 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "PCH.h"

#include "Camera.h"

#include "..\\Utility.h"

using namespace DirectX;

namespace SampleFramework12
{

//=================================================================================================
// Camera
//=================================================================================================
void Camera::Initialize(float nearClip, float farClip)
{
    nearZ = nearClip;
    farZ = farClip;
    world = Float4x4();
    view = Float4x4();
    position = Float3(0.0f, 0.0f, 0.0f);
    orientation = Quaternion();
}

void Camera::WorldMatrixChanged()
{
    view = Float4x4::Invert(world);
    viewProjection = view * projection;
}

Float3 Camera::Forward() const
{
    return world.Forward();
}

Float3 Camera::Back() const
{
    return world.Back();
}

Float3 Camera::Up() const
{
    return world.Up();
}

Float3 Camera::Down() const
{
    return world.Down();
}

Float3 Camera::Right() const
{
    return world.Right();
}

Float3 Camera::Left() const
{
    return world.Left();
}

void Camera::SetLookAt(const Float3 &eye, const Float3 &lookAt, const Float3 &up)
{
    view = Float4x4(XMMatrixLookAtLH(eye.ToSIMD(), lookAt.ToSIMD(), up.ToSIMD()));
    world = Float4x4::Invert(view);
    position = eye;
    orientation = Quaternion(XMQuaternionRotationMatrix(world.ToSIMD()));

    WorldMatrixChanged();
}

void Camera::SetWorldMatrix(const Float4x4& newWorld)
{
    world = newWorld;
    position = world.Translation();
    orientation = Quaternion(XMQuaternionRotationMatrix(world.ToSIMD()));

    WorldMatrixChanged();
}

void Camera::SetPosition(const Float3& newPosition)
{
    position = newPosition;
    world.SetTranslation(newPosition);

    WorldMatrixChanged();
}

void Camera::SetOrientation(const Quaternion& newOrientation)
{
    world = Float4x4(XMMatrixRotationQuaternion(newOrientation.ToSIMD()));
    orientation = newOrientation;
    world.SetTranslation(position);

    WorldMatrixChanged();
}

void Camera::SetNearClip(float newNearClip)
{
    nearZ = newNearClip;
    CreateProjection();
}

void Camera::SetFarClip(float newFarClip)
{
    farZ = newFarClip;
    CreateProjection();
}

void Camera::SetProjection(const Float4x4& newProjection)
{
    projection = newProjection;
    viewProjection = view * projection;
}

//=================================================================================================
// OrthographicCamera
//=================================================================================================

void OrthographicCamera::Initialize(float minX, float minY, float maxX, float maxY, float nearClip, float farClip)
{
    Camera::Initialize(nearClip, farClip);

    Assert_(maxX > minX && maxX > minY);

    nearZ = nearClip;
    farZ = farClip;
    xMin = minX;
    xMax = maxX;
    yMin = minY;
    yMax = maxY;

    CreateProjection();
}

void OrthographicCamera::CreateProjection()
{
    projection = Float4x4(XMMatrixOrthographicOffCenterLH(xMin, xMax, yMin, yMax, nearZ, farZ));
    viewProjection = view * projection;
}

void OrthographicCamera::SetMinX(float minX)
{
    xMin = minX;
    CreateProjection();
}

void OrthographicCamera::SetMinY(float minY)
{
    yMin = minY;
    CreateProjection();
}

void OrthographicCamera::SetMaxX(float maxX)
{
    xMax = maxX;
    CreateProjection();
}

void OrthographicCamera::SetMaxY(float maxY)
{
    yMax = maxY;
    CreateProjection();
}

//=================================================================================================
// PerspectiveCamera
//=================================================================================================

void PerspectiveCamera::Initialize(float aspectRatio, float fieldOfView, float nearClip, float farClip)
{
    Camera::Initialize(nearClip, farClip);
    Assert_(aspectRatio > 0);
    Assert_(fieldOfView > 0 && fieldOfView <= 3.14159f);
    nearZ = nearClip;
    farZ = farClip;
    aspect = aspectRatio;
    fov = fieldOfView;
    CreateProjection();
}

void PerspectiveCamera::SetAspectRatio(float aspectRatio)
{
    aspect = aspectRatio;
    CreateProjection();
}

void PerspectiveCamera::SetFieldOfView(float fieldOfView)
{
    fov = fieldOfView;
    CreateProjection();
}

void PerspectiveCamera::CreateProjection()
{
    projection = Float4x4(XMMatrixPerspectiveFovLH(fov, aspect, nearZ, farZ));
    viewProjection = view * projection;
}

//=================================================================================================
// FirstPersonCamera
//=================================================================================================

void FirstPersonCamera::Initialize(float aspectRatio, float fieldOfView, float nearClip, float farClip)
{
    PerspectiveCamera::Initialize(aspectRatio, fieldOfView, nearClip, farClip);
    xRot = 0.0f;
    yRot = 0.0f;
}

void FirstPersonCamera::SetXRotation(float xRotation)
{
    xRot = Clamp(xRotation, -XM_PIDIV2, XM_PIDIV2);
    SetOrientation(Quaternion(XMQuaternionRotationRollPitchYaw(xRot, yRot, 0)));
}

void FirstPersonCamera::SetYRotation(float yRotation)
{
    yRot = XMScalarModAngle(yRotation);
    SetOrientation(Quaternion(XMQuaternionRotationRollPitchYaw(xRot, yRot, 0)));
}

}