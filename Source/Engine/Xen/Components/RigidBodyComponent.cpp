//
// Created by Jake Rieger on 10/6/2026.
//

#include "RigidBodyComponent.hpp"
#include "MeshComponent.hpp"

#include <Common/Log.hpp>
#include <Xen/Actor.hpp>
#include <Xen/Scene.hpp>

#include <algorithm>
#include <cmath>

namespace Xen {
    namespace {
        // The thinnest a fitted box side may be: a flat plane mesh has no
        // thickness of its own, and a paper-thin body lets fast things fall
        // straight through.
        constexpr f32 MinFitHalfExtent = 0.05f;

        bool PoseDiffers(const Float3& PosA, const Quat& RotA, const Float3& PosB, const Quat& RotB) {
            constexpr f32 PosEpsilon = 1e-5f;
            constexpr f32 RotEpsilon = 1e-6f;
            return std::abs(PosA.x - PosB.x) > PosEpsilon || std::abs(PosA.y - PosB.y) > PosEpsilon ||
                   std::abs(PosA.z - PosB.z) > PosEpsilon || std::abs(RotA.x - RotB.x) > RotEpsilon ||
                   std::abs(RotA.y - RotB.y) > RotEpsilon || std::abs(RotA.z - RotB.z) > RotEpsilon ||
                   std::abs(RotA.w - RotB.w) > RotEpsilon;
        }
    }  // namespace

    void RigidBodyComponent::Reflect(IReflector& R) {
        R.EnumProperty("Motion",
                       _Motion,
                       {.ToolTip  = "0 = Static (never moves), 1 = Dynamic (simulated), 2 = Kinematic (follows the "
                                    "actor, pushes dynamic bodies).",
                        .Category = "Rigid Body"});
        R.Property("Mass",
                   _Mass,
                   {.ToolTip = "Dynamic only, in kilograms.", .Category = "Rigid Body", .Min = 0.001f, .Max = 100000.0f});
        R.Property("Friction", _Friction, {.Category = "Rigid Body", .Min = 0.0f, .Max = 5.0f});
        R.Property("Restitution",
                   _Restitution,
                   {.ToolTip = "Bounciness: 0 stops dead, 1 bounces back with full speed.",
                    .Category = "Rigid Body",
                    .Min      = 0.0f,
                    .Max      = 1.0f});
        R.Property("LinearDamping", _LinearDamping, {.Category = "Rigid Body", .Min = 0.0f, .Max = 100.0f});
        R.Property("AngularDamping", _AngularDamping, {.Category = "Rigid Body", .Min = 0.0f, .Max = 100.0f});
        R.Property("GravityScale",
                   _GravityScale,
                   {.ToolTip = "Multiplies gravity for this body: 0 floats, negative rises.", .Category = "Rigid Body"});

        R.EnumProperty("Shape",
                       _Shape,
                       {.ToolTip  = "0 = Box, 1 = Sphere, 2 = Capsule, 3 = Mesh (Static bodies only).",
                        .Category = "Collider"});
        R.Property("Size",
                   _Size,
                   {.ToolTip  = "Box: full size on each axis. A zero axis is fitted to the actor's mesh.",
                    .Category = "Collider"});
        R.Property("Radius",
                   _Radius,
                   {.ToolTip  = "Sphere/Capsule radius. Zero is fitted to the actor's mesh.", .Category = "Collider"});
        R.Property("Height",
                   _Height,
                   {.ToolTip  = "Capsule total height. Zero is fitted to the actor's mesh.", .Category = "Collider"});
        R.Property("Offset", _Offset, {.ToolTip = "Shape center relative to the actor.", .Category = "Collider"});
        R.Property("MeshAsset",
                   _MeshAsset,
                   {.ToolTip  = "Mesh shape only. Unset uses the actor's MeshComponent's mesh.",
                    .Category = "Collider",
                    .Asset    = AssetKind::Mesh});
        // _Body and the last-synced pose are runtime state, rebuilt in BeginPlay.
    }

    PhysicsWorld* RigidBodyComponent::GetWorld() const {
        const Scene* S = GetScene();
        return S ? S->GetContext().Physics : nullptr;
    }

    BodyDesc RigidBodyComponent::BuildDesc() const {
        const Actor* Owner       = GetOwner();
        const Transform World    = Owner->GetWorldTransform();
        const Float3 Scale       = {std::abs(World.Scale.x), std::abs(World.Scale.y), std::abs(World.Scale.z)};
        const MeshComponent* Mesh = Owner->GetComponent<MeshComponent>();
        const MeshCache* Meshes   = GetScene()->GetContext().Meshes;

        BodyDesc Desc;
        Desc.Motion         = _Motion;
        Desc.Position       = World.Position;
        Desc.Rotation       = World.Rotation;
        Desc.Mass           = _Mass;
        Desc.Friction       = _Friction;
        Desc.Restitution    = _Restitution;
        Desc.LinearDamping  = _LinearDamping;
        Desc.AngularDamping = _AngularDamping;
        Desc.GravityScale   = _GravityScale;

        ShapeDesc& Shape = Desc.Shape;
        Shape.Type       = _Shape;
        Shape.Offset     = {_Offset.x * Scale.x, _Offset.y * Scale.y, _Offset.z * Scale.z};

        // The mesh's local bounds, when something needs fitting to it.
        const auto GetBounds = [&](Float3& Min, Float3& Max) {
            if (!Mesh || !Meshes) return false;

            MeshInfo Info;
            if (const MeshHandle Handle = Mesh->GetMesh(); Handle.IsValid()) {
                Info = Meshes->GetInfo(Handle);
            } else if (Mesh->GetMeshAsset().IsValid()) {
                // The MeshComponent hasn't begun play yet (it's listed after
                // this one): read the bounds from the file instead.
                try {
                    Info = Meshes->DecodeAsset(Mesh->GetMeshAsset()).Info;
                } catch (const std::exception& Error) {
                    LOG_ERR("RigidBody: couldn't read the actor's mesh to fit a collider: %s", Error.what());
                    return false;
                }
            } else {
                return false;
            }

            Min = Info.BoundsMin;
            Max = Info.BoundsMax;
            return true;
        };

        switch (_Shape) {
            case ShapeType::Box: {
                // A unit cube where there is neither a size nor a mesh to fit.
                Float3 Half {(_Size.x > 0.0f ? _Size.x * 0.5f : 0.5f) * Scale.x,
                             (_Size.y > 0.0f ? _Size.y * 0.5f : 0.5f) * Scale.y,
                             (_Size.z > 0.0f ? _Size.z * 0.5f : 0.5f) * Scale.z};
                if (_Size.x <= 0.0f || _Size.y <= 0.0f || _Size.z <= 0.0f) {
                    Float3 Min, Max;
                    if (GetBounds(Min, Max)) {
                        const Float3 Center = {(Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f, (Min.z + Max.z) * 0.5f};
                        if (_Size.x <= 0.0f) Half.x = (Max.x - Min.x) * 0.5f * Scale.x;
                        if (_Size.y <= 0.0f) Half.y = (Max.y - Min.y) * 0.5f * Scale.y;
                        if (_Size.z <= 0.0f) Half.z = (Max.z - Min.z) * 0.5f * Scale.z;
                        Shape.Offset.x += Center.x * Scale.x;
                        Shape.Offset.y += Center.y * Scale.y;
                        Shape.Offset.z += Center.z * Scale.z;
                    }
                }
                Shape.HalfExtents = {std::max(Half.x, MinFitHalfExtent),
                                     std::max(Half.y, MinFitHalfExtent),
                                     std::max(Half.z, MinFitHalfExtent)};
                break;
            }
            case ShapeType::Sphere: {
                f32 Radius = (_Radius > 0.0f ? _Radius : 0.5f) * std::max({Scale.x, Scale.y, Scale.z});
                if (_Radius <= 0.0f) {
                    Float3 Min, Max;
                    if (GetBounds(Min, Max)) {
                        Radius = std::max({(Max.x - Min.x) * Scale.x,
                                           (Max.y - Min.y) * Scale.y,
                                           (Max.z - Min.z) * Scale.z}) *
                                 0.5f;
                        Shape.Offset.x += (Min.x + Max.x) * 0.5f * Scale.x;
                        Shape.Offset.y += (Min.y + Max.y) * 0.5f * Scale.y;
                        Shape.Offset.z += (Min.z + Max.z) * 0.5f * Scale.z;
                    }
                }
                Shape.Radius = std::max(Radius, MinFitHalfExtent);
                break;
            }
            case ShapeType::Capsule: {
                f32 Radius = (_Radius > 0.0f ? _Radius : 0.5f) * std::max(Scale.x, Scale.z);
                f32 Height = (_Height > 0.0f ? _Height : 2.0f) * Scale.y;
                if (_Radius <= 0.0f || _Height <= 0.0f) {
                    Float3 Min, Max;
                    if (GetBounds(Min, Max)) {
                        if (_Radius <= 0.0f) {
                            Radius = std::max((Max.x - Min.x) * Scale.x, (Max.z - Min.z) * Scale.z) * 0.5f;
                        }
                        if (_Height <= 0.0f) Height = (Max.y - Min.y) * Scale.y;
                        Shape.Offset.x += (Min.x + Max.x) * 0.5f * Scale.x;
                        Shape.Offset.y += (Min.y + Max.y) * 0.5f * Scale.y;
                        Shape.Offset.z += (Min.z + Max.z) * 0.5f * Scale.z;
                    }
                }
                Shape.Radius = std::max(Radius, MinFitHalfExtent);
                Shape.Height = std::max(Height, Shape.Radius * 2.0f);
                break;
            }
            case ShapeType::Mesh: {
                const AssetID Source = _MeshAsset.IsValid() ? _MeshAsset : (Mesh ? Mesh->GetMeshAsset() : AssetID {});
                if (!Source.IsValid() || !Meshes) {
                    LOG_ERR("RigidBody on '%s': a Mesh shape needs a MeshAsset or a MeshComponent", Owner->GetName().c_str());
                    break;
                }

                try {
                    const DecodedMesh Decoded = Meshes->DecodeAsset(Source);
                    Shape.Vertices.reserve(Decoded.Vertices.size());
                    for (const MeshVertex& V : Decoded.Vertices) {
                        Shape.Vertices.push_back({V.Position[0] * Scale.x, V.Position[1] * Scale.y, V.Position[2] * Scale.z});
                    }
                    if (Decoded.Info.IndexType == RHI::IndexType::U16) {
                        Shape.Indices.assign(Decoded.Indices16.begin(), Decoded.Indices16.end());
                    } else {
                        Shape.Indices = Decoded.Indices32;
                    }
                } catch (const std::exception& Error) {
                    LOG_ERR("RigidBody on '%s': couldn't read the collision mesh: %s", Owner->GetName().c_str(), Error.what());
                    Shape.Vertices.clear();
                    Shape.Indices.clear();
                }
                break;
            }
        }

        return Desc;
    }

    void RigidBodyComponent::BeginPlay() {
        PhysicsWorld* World = GetWorld();
        if (!World) return;

        const BodyDesc Desc = BuildDesc();
        _Body               = World->CreateBody(Desc);
        if (_Body == InvalidBody) {
            LOG_ERR("RigidBody on '%s' couldn't create its body", GetOwner()->GetName().c_str());
            return;
        }

        _LastPosition = _PrevPosition = _CurrPosition = Desc.Position;
        _LastRotation = _PrevRotation = _CurrRotation = Desc.Rotation;
        _Interpolated = false;
    }

    void RigidBodyComponent::EndPlay() {
        if (_Body == InvalidBody) return;

        if (PhysicsWorld* World = GetWorld()) World->DestroyBody(_Body);
        _Body = InvalidBody;
    }

    void RigidBodyComponent::FixedTick(const f32 FixedDelta) {
        PhysicsWorld* World = GetWorld();
        if (!World || _Body == InvalidBody) return;

        const Transform Pose = GetOwner()->GetWorldTransform();

        if (_Motion == BodyMotion::Kinematic) {
            // Carries and pushes other bodies with the matching velocity.
            World->MoveKinematic(_Body, Pose.Position, Pose.Rotation, FixedDelta);
        } else if (PoseDiffers(Pose.Position, Pose.Rotation, _LastPosition, _LastRotation)) {
            // Moved by the editor or game code, not by the simulation.
            World->SetBodyTransform(_Body, Pose.Position, Pose.Rotation);
            // A teleport, not motion to smooth over.
            _PrevPosition = _CurrPosition = Pose.Position;
            _PrevRotation = _CurrRotation = Pose.Rotation;
        }

        _LastPosition = Pose.Position;
        _LastRotation = Pose.Rotation;
    }

    void RigidBodyComponent::PullFromPhysics() {
        PhysicsWorld* World = GetWorld();
        if (!World || _Body == InvalidBody || _Motion != BodyMotion::Dynamic) return;

        Float3 Position;
        Quat Rotation;
        World->GetBodyTransform(_Body, Position, Rotation);

        _PrevPosition = _CurrPosition;
        _PrevRotation = _CurrRotation;
        _CurrPosition = Position;
        _CurrRotation = Rotation;

        Actor* Owner = GetOwner();
        Owner->SetWorldPositionAndRotation(Position, Rotation);

        // Read back rather than assume: under a parent, the round trip
        // through local space isn't bit-exact, and a mismatch here would
        // look like an outside move next step.
        const Transform Written = Owner->GetWorldTransform();
        _LastPosition           = Written.Position;
        _LastRotation           = Written.Rotation;
    }

    void RigidBodyComponent::ApplyInterpolation(const f32 Alpha) {
        if (_Body == InvalidBody || _Motion != BodyMotion::Dynamic) return;
        if (!PoseDiffers(_PrevPosition, _PrevRotation, _CurrPosition, _CurrRotation)) return;  // at rest

        using namespace DirectX;
        const f32 T = std::clamp(Alpha, 0.0f, 1.0f);

        Float3 Position;
        XMStoreFloat3(&Position, XMVectorLerp(XMLoadFloat3(&_PrevPosition), XMLoadFloat3(&_CurrPosition), T));
        Quat Rotation;
        XMStoreFloat4(&Rotation, XMQuaternionSlerp(XMLoadFloat4(&_PrevRotation), XMLoadFloat4(&_CurrRotation), T));

        Actor* Owner = GetOwner();
        Owner->SetWorldPositionAndRotation(Position, Rotation);

        const Transform Shown = Owner->GetWorldTransform();
        _AppliedPosition      = Shown.Position;
        _AppliedRotation      = Shown.Rotation;
        _Interpolated         = true;
    }

    void RigidBodyComponent::RestoreSimulatedPose() {
        if (!_Interpolated) return;
        _Interpolated = false;

        Actor* Owner          = GetOwner();
        const Transform Shown = Owner->GetWorldTransform();
        if (PoseDiffers(Shown.Position, Shown.Rotation, _AppliedPosition, _AppliedRotation)) {
            // Moved by game code since it was drawn: leave it where it was put.
            // FixedTick sees it differs from the simulated pose and teleports.
            return;
        }

        Owner->SetWorldPositionAndRotation(_CurrPosition, _CurrRotation);
        const Transform Restored = Owner->GetWorldTransform();
        _LastPosition            = Restored.Position;
        _LastRotation            = Restored.Rotation;
    }

    Float3 RigidBodyComponent::GetLinearVelocity() const {
        const PhysicsWorld* World = GetWorld();
        return World && _Body != InvalidBody ? World->GetLinearVelocity(_Body) : Float3 {};
    }

    void RigidBodyComponent::SetLinearVelocity(const Float3& Velocity) {
        if (PhysicsWorld* World = GetWorld(); World && _Body != InvalidBody) World->SetLinearVelocity(_Body, Velocity);
    }

    void RigidBodyComponent::AddForce(const Float3& Force) {
        if (PhysicsWorld* World = GetWorld(); World && _Body != InvalidBody) World->AddForce(_Body, Force);
    }

    void RigidBodyComponent::AddImpulse(const Float3& Impulse) {
        if (PhysicsWorld* World = GetWorld(); World && _Body != InvalidBody) World->AddImpulse(_Body, Impulse);
    }
}  // namespace Xen
