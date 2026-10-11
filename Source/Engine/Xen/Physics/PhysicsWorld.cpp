//
// Created by Jake Rieger on 10/6/2026.
//

#include "PhysicsWorld.hpp"

#include <Common/Log.hpp>

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <thread>
#include <unordered_map>

namespace Xen {
    namespace {
        // Two object layers: everything that never moves, and everything that
        // can. Static bodies don't collide with each other, so the pair filter
        // is just "at least one side is Moving".
        namespace Layers {
            constexpr JPH::ObjectLayer NonMoving = 0;
            constexpr JPH::ObjectLayer Moving    = 1;
            constexpr JPH::uint Count            = 2;
        }  // namespace Layers

        namespace BroadPhaseLayers {
            constexpr JPH::BroadPhaseLayer NonMoving(0);
            constexpr JPH::BroadPhaseLayer Moving(1);
            constexpr JPH::uint Count = 2;
        }  // namespace BroadPhaseLayers

        class BroadPhaseLayerMap final : public JPH::BroadPhaseLayerInterface {
        public:
            JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::Count; }

            JPH::BroadPhaseLayer GetBroadPhaseLayer(const JPH::ObjectLayer Layer) const override {
                return Layer == Layers::NonMoving ? BroadPhaseLayers::NonMoving : BroadPhaseLayers::Moving;
            }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
            const char* GetBroadPhaseLayerName(const JPH::BroadPhaseLayer Layer) const override {
                return Layer == BroadPhaseLayers::NonMoving ? "NonMoving" : "Moving";
            }
#endif
        };

        class ObjectVsBroadPhaseFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
        public:
            bool ShouldCollide(const JPH::ObjectLayer Layer, const JPH::BroadPhaseLayer BroadPhase) const override {
                if (Layer == Layers::NonMoving) return BroadPhase == BroadPhaseLayers::Moving;
                return true;
            }
        };

        class ObjectLayerPairs final : public JPH::ObjectLayerPairFilter {
        public:
            bool ShouldCollide(const JPH::ObjectLayer A, const JPH::ObjectLayer B) const override {
                return A == Layers::Moving || B == Layers::Moving;
            }
        };

        // --- Jolt global state --------------------------------------------------------------------------------------

        // Jolt's allocator, type registry and factory are process-global, so
        // they're set up by the first PhysicsWorld and torn down by the last.
        std::atomic<int> GlobalRefCount {0};

        void JoltTrace(const char* Format, ...) {
            char Buffer[1024];
            va_list Args;
            va_start(Args, Format);
            std::vsnprintf(Buffer, sizeof(Buffer), Format, Args);
            va_end(Args);
            LOG_DBG("Jolt: %s", Buffer);
        }

        void AcquireJolt() {
            if (GlobalRefCount.fetch_add(1) != 0) return;

            JPH::RegisterDefaultAllocator();
            JPH::Trace = JoltTrace;
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        }

        void ReleaseJolt() {
            if (GlobalRefCount.fetch_sub(1) != 1) return;

            JPH::UnregisterTypes();
            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }

        // --- Conversions --------------------------------------------------------------------------------------------

        JPH::Vec3 ToJolt(const Float3& V) {
            return {V.x, V.y, V.z};
        }

        JPH::Quat ToJolt(const Quat& Q) {
            return {Q.x, Q.y, Q.z, Q.w};
        }

        Float3 FromJolt(const JPH::Vec3 V) {
            return {V.GetX(), V.GetY(), V.GetZ()};
        }

        Quat FromJolt(const JPH::Quat Q) {
            return {Q.GetX(), Q.GetY(), Q.GetZ(), Q.GetW()};
        }

        JPH::BodyID ToBodyID(const BodyHandle Handle) {
            return JPH::BodyID(Handle);
        }

        // Jolt requires every shape to be built from positive, finite sizes
        // and a convex radius no bigger than the smallest half extent.
        JPH::RefConst<JPH::Shape> BuildShape(const ShapeDesc& Desc, const bool AllowMesh) {
            JPH::RefConst<JPH::Shape> Shape;

            switch (Desc.Type) {
                case ShapeType::Box: {
                    const JPH::Vec3 Half = JPH::Vec3::sMax(ToJolt(Desc.HalfExtents), JPH::Vec3::sReplicate(0.005f));
                    const float Convex   = std::min(0.05f, Half.ReduceMin());
                    const JPH::BoxShapeSettings Settings(Half, Convex);
                    const auto Result = Settings.Create();
                    if (Result.HasError()) {
                        LOG_ERR("Physics: box shape failed: %s", Result.GetError().c_str());
                        return nullptr;
                    }
                    Shape = Result.Get();
                    break;
                }
                case ShapeType::Sphere: {
                    const JPH::SphereShapeSettings Settings(std::max(Desc.Radius, 0.005f));
                    const auto Result = Settings.Create();
                    if (Result.HasError()) {
                        LOG_ERR("Physics: sphere shape failed: %s", Result.GetError().c_str());
                        return nullptr;
                    }
                    Shape = Result.Get();
                    break;
                }
                case ShapeType::Capsule: {
                    const f32 Radius       = std::max(Desc.Radius, 0.005f);
                    const f32 HalfCylinder = std::max(Desc.Height * 0.5f - Radius, 0.005f);
                    const JPH::CapsuleShapeSettings Settings(HalfCylinder, Radius);
                    const auto Result = Settings.Create();
                    if (Result.HasError()) {
                        LOG_ERR("Physics: capsule shape failed: %s", Result.GetError().c_str());
                        return nullptr;
                    }
                    Shape = Result.Get();
                    break;
                }
                case ShapeType::Mesh: {
                    if (!AllowMesh) {
                        LOG_ERR("Physics: a mesh shape is only supported on Static bodies");
                        return nullptr;
                    }
                    if (Desc.Vertices.empty() || Desc.Indices.size() < 3) {
                        LOG_ERR("Physics: mesh shape has no triangles");
                        return nullptr;
                    }

                    JPH::VertexList Vertices;
                    Vertices.reserve(Desc.Vertices.size());
                    for (const Float3& V : Desc.Vertices)
                        Vertices.push_back(JPH::Float3(V.x, V.y, V.z));

                    JPH::IndexedTriangleList Triangles;
                    Triangles.reserve(Desc.Indices.size() / 3);
                    for (size_t i = 0; i + 2 < Desc.Indices.size(); i += 3)
                        Triangles.push_back(JPH::IndexedTriangle(Desc.Indices[i], Desc.Indices[i + 1], Desc.Indices[i + 2]));

                    const JPH::MeshShapeSettings Settings(std::move(Vertices), std::move(Triangles));
                    const auto Result = Settings.Create();
                    if (Result.HasError()) {
                        LOG_ERR("Physics: mesh shape failed: %s", Result.GetError().c_str());
                        return nullptr;
                    }
                    Shape = Result.Get();
                    break;
                }
            }

            if (Shape && (Desc.Offset.x != 0.0f || Desc.Offset.y != 0.0f || Desc.Offset.z != 0.0f)) {
                const JPH::RotatedTranslatedShapeSettings Offset(ToJolt(Desc.Offset), JPH::Quat::sIdentity(), Shape);
                const auto Result = Offset.Create();
                if (Result.HasError()) {
                    LOG_ERR("Physics: shape offset failed: %s", Result.GetError().c_str());
                    return nullptr;
                }
                Shape = Result.Get();
            }

            return Shape;
        }
    }  // namespace

    struct PhysicsWorld::Impl {
        BroadPhaseLayerMap BroadPhaseLayerMap;
        ObjectVsBroadPhaseFilter ObjectVsBroadPhase;
        ObjectLayerPairs ObjectPairs;

        JPH::PhysicsSystem System;
        JPH::TempAllocatorImpl TempAllocator {10 * 1024 * 1024};
        JPH::JobSystemThreadPool Jobs {JPH::cMaxPhysicsJobs,
                                       JPH::cMaxPhysicsBarriers,
                                       CAST<int>(std::max(1u, std::thread::hardware_concurrency()) - 1)};

        struct Character {
            JPH::Ref<JPH::CharacterVirtual> Body;
            JPH::Vec3 StepUp {0, 0.4f, 0};
        };
        std::unordered_map<CharacterHandle, Character> Characters;
        CharacterHandle NextCharacter {1};

        // Static bodies added since the last broad-phase optimization.
        bool BroadPhaseDirty {false};
    };

    PhysicsWorld::PhysicsWorld() {
        AcquireJolt();

        _Impl = std::make_unique<Impl>();
        _Impl->System.Init(8192,
                           0,
                           16384,
                           8192,
                           _Impl->BroadPhaseLayerMap,
                           _Impl->ObjectVsBroadPhase,
                           _Impl->ObjectPairs);
        _Impl->System.SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
    }

    PhysicsWorld::~PhysicsWorld() {
        if (_Impl) Clear();
        _Impl.reset();

        ReleaseJolt();
    }

    void PhysicsWorld::Step(const f32 DeltaTime) {
        if (DeltaTime <= 0.0f) return;

        if (_Impl->BroadPhaseDirty) {
            _Impl->System.OptimizeBroadPhase();
            _Impl->BroadPhaseDirty = false;
        }

        _Impl->System.Update(DeltaTime, 1, &_Impl->TempAllocator, &_Impl->Jobs);

        const JPH::Vec3 Gravity = _Impl->System.GetGravity();
        for (auto& [Handle, Character] : _Impl->Characters) {
            JPH::CharacterVirtual::ExtendedUpdateSettings Settings;
            Settings.mStickToFloorStepDown = JPH::Vec3(0.0f, -0.5f, 0.0f);
            Settings.mWalkStairsStepUp     = Character.StepUp;

            Character.Body->ExtendedUpdate(DeltaTime,
                                           Gravity,
                                           Settings,
                                           _Impl->System.GetDefaultBroadPhaseLayerFilter(Layers::Moving),
                                           _Impl->System.GetDefaultLayerFilter(Layers::Moving),
                                           {},
                                           {},
                                           _Impl->TempAllocator);
        }
    }

    void PhysicsWorld::SetGravity(const Float3& Gravity) {
        _Impl->System.SetGravity(ToJolt(Gravity));
    }

    Float3 PhysicsWorld::GetGravity() const {
        return FromJolt(_Impl->System.GetGravity());
    }

    void PhysicsWorld::Clear() {
        _Impl->Characters.clear();

        JPH::BodyInterface& Bodies = _Impl->System.GetBodyInterface();
        JPH::BodyIDVector All;
        _Impl->System.GetBodies(All);
        for (const JPH::BodyID ID : All) {
            Bodies.RemoveBody(ID);
            Bodies.DestroyBody(ID);
        }
    }

    // --- Bodies -----------------------------------------------------------------------------------------------------

    BodyHandle PhysicsWorld::CreateBody(const BodyDesc& Desc) {
        const bool Static = Desc.Motion == BodyMotion::Static;
        const auto Shape  = BuildShape(Desc.Shape, Static);
        if (!Shape) return InvalidBody;

        const JPH::EMotionType Motion = Desc.Motion == BodyMotion::Static      ? JPH::EMotionType::Static
                                        : Desc.Motion == BodyMotion::Kinematic ? JPH::EMotionType::Kinematic
                                                                               : JPH::EMotionType::Dynamic;

        JPH::BodyCreationSettings Settings(Shape,
                                           ToJolt(Desc.Position),
                                           ToJolt(Desc.Rotation).Normalized(),
                                           Motion,
                                           Static ? Layers::NonMoving : Layers::Moving);
        Settings.mFriction       = Desc.Friction;
        Settings.mRestitution    = Desc.Restitution;
        Settings.mLinearDamping  = Desc.LinearDamping;
        Settings.mAngularDamping = Desc.AngularDamping;
        Settings.mGravityFactor  = Desc.GravityScale;
        if (Desc.Motion == BodyMotion::Dynamic) {
            Settings.mOverrideMassProperties          = JPH::EOverrideMassProperties::CalculateInertia;
            Settings.mMassPropertiesOverride.mMass    = std::max(Desc.Mass, 0.001f);
        }

        const JPH::BodyID ID = _Impl->System.GetBodyInterface().CreateAndAddBody(
          Settings,
          Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
        if (ID.IsInvalid()) {
            LOG_ERR("Physics: the world is out of bodies");
            return InvalidBody;
        }

        if (Static) _Impl->BroadPhaseDirty = true;
        return ID.GetIndexAndSequenceNumber();
    }

    void PhysicsWorld::DestroyBody(const BodyHandle Body) {
        if (!IsValid(Body)) return;

        JPH::BodyInterface& Bodies = _Impl->System.GetBodyInterface();
        Bodies.RemoveBody(ToBodyID(Body));
        Bodies.DestroyBody(ToBodyID(Body));
    }

    bool PhysicsWorld::IsValid(const BodyHandle Body) const {
        return Body != InvalidBody && _Impl->System.GetBodyInterface().IsAdded(ToBodyID(Body));
    }

    void PhysicsWorld::GetBodyTransform(const BodyHandle Body, Float3& Position, Quat& Rotation) const {
        if (!IsValid(Body)) return;

        JPH::RVec3 Pos;
        JPH::Quat Rot;
        _Impl->System.GetBodyInterface().GetPositionAndRotation(ToBodyID(Body), Pos, Rot);
        Position = FromJolt(JPH::Vec3(Pos));
        Rotation = FromJolt(Rot);
    }

    void PhysicsWorld::SetBodyTransform(const BodyHandle Body, const Float3& Position, const Quat& Rotation) {
        if (!IsValid(Body)) return;

        JPH::BodyInterface& Bodies = _Impl->System.GetBodyInterface();
        const bool Static          = Bodies.GetMotionType(ToBodyID(Body)) == JPH::EMotionType::Static;

        Bodies.SetPositionAndRotation(ToBodyID(Body),
                                      ToJolt(Position),
                                      ToJolt(Rotation).Normalized(),
                                      Static ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
        if (!Static) Bodies.SetLinearAndAngularVelocity(ToBodyID(Body), JPH::Vec3::sZero(), JPH::Vec3::sZero());
        else _Impl->BroadPhaseDirty = true;
    }

    void PhysicsWorld::MoveKinematic(const BodyHandle Body,
                                     const Float3& Position,
                                     const Quat& Rotation,
                                     const f32 DeltaTime) {
        if (!IsValid(Body)) return;

        _Impl->System.GetBodyInterface().MoveKinematic(ToBodyID(Body),
                                                       ToJolt(Position),
                                                       ToJolt(Rotation).Normalized(),
                                                       std::max(DeltaTime, 0.0001f));
    }

    Float3 PhysicsWorld::GetLinearVelocity(const BodyHandle Body) const {
        if (!IsValid(Body)) return {};
        return FromJolt(_Impl->System.GetBodyInterface().GetLinearVelocity(ToBodyID(Body)));
    }

    void PhysicsWorld::SetLinearVelocity(const BodyHandle Body, const Float3& Velocity) {
        if (!IsValid(Body)) return;
        _Impl->System.GetBodyInterface().SetLinearVelocity(ToBodyID(Body), ToJolt(Velocity));
    }

    void PhysicsWorld::AddForce(const BodyHandle Body, const Float3& Force) {
        if (!IsValid(Body)) return;
        _Impl->System.GetBodyInterface().AddForce(ToBodyID(Body), ToJolt(Force));
    }

    void PhysicsWorld::AddImpulse(const BodyHandle Body, const Float3& Impulse) {
        if (!IsValid(Body)) return;
        _Impl->System.GetBodyInterface().AddImpulse(ToBodyID(Body), ToJolt(Impulse));
    }

    RaycastHit PhysicsWorld::Raycast(const Float3& Origin,
                                     const Float3& Direction,
                                     const f32 MaxDistance,
                                     const BodyHandle Ignore) const {
        RaycastHit Out;

        const JPH::Vec3 Dir = ToJolt(Direction);
        if (MaxDistance <= 0.0f || Dir.LengthSq() < 1e-12f) return Out;

        const JPH::Vec3 Unit = Dir.Normalized();
        const JPH::RRayCast Ray(ToJolt(Origin), Unit * MaxDistance);

        JPH::RayCastResult Result;
        const JPH::IgnoreSingleBodyFilter IgnoreFilter(Ignore == InvalidBody ? JPH::BodyID() : ToBodyID(Ignore));
        if (!_Impl->System.GetNarrowPhaseQuery().CastRay(Ray, Result, {}, {}, IgnoreFilter)) return Out;

        const JPH::Vec3 Point = Ray.GetPointOnRay(Result.mFraction);

        Out.Hit      = true;
        Out.Distance = Result.mFraction * MaxDistance;
        Out.Point    = FromJolt(Point);
        Out.Body     = Result.mBodyID.GetIndexAndSequenceNumber();

        const JPH::BodyLockRead Lock(_Impl->System.GetBodyLockInterface(), Result.mBodyID);
        if (Lock.Succeeded()) {
            Out.Normal = FromJolt(Lock.GetBody().GetWorldSpaceSurfaceNormal(Result.mSubShapeID2, Point));
        }

        return Out;
    }

    // --- Characters -------------------------------------------------------------------------------------------------

    CharacterHandle PhysicsWorld::CreateCharacter(const CharacterDesc& Desc) {
        const f32 Radius       = std::max(Desc.Radius, 0.05f);
        const f32 HalfCylinder = std::max(Desc.Height * 0.5f - Radius, 0.05f);

        JPH::RefConst<JPH::Shape> Capsule = new JPH::CapsuleShape(HalfCylinder, Radius);

        JPH::CharacterVirtualSettings Settings;
        Settings.mShape        = Capsule;
        Settings.mMass         = std::max(Desc.Mass, 1.0f);
        Settings.mMaxSlopeAngle = JPH::DegreesToRadians(Desc.MaxSlopeDegrees);
        Settings.mUp           = JPH::Vec3::sAxisY();
        // The part of the capsule that counts as "standing on something":
        // everything below the cylinder's lower end, i.e. the bottom cap.
        Settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), HalfCylinder);

        // Keep only yaw: a character capsule never tilts.
        JPH::Quat Rotation = ToJolt(Desc.Rotation).Normalized();
        Rotation           = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), Rotation.GetRotationAngle(JPH::Vec3::sAxisY()));

        Impl::Character Character;
        Character.Body = new JPH::CharacterVirtual(&Settings, ToJolt(Desc.Position), Rotation, 0, &_Impl->System);
        Character.StepUp = JPH::Vec3(0.0f, std::max(Desc.StepHeight, 0.0f), 0.0f);

        const CharacterHandle Handle = _Impl->NextCharacter++;
        _Impl->Characters.emplace(Handle, std::move(Character));
        return Handle;
    }

    void PhysicsWorld::DestroyCharacter(const CharacterHandle Character) {
        _Impl->Characters.erase(Character);
    }

    void PhysicsWorld::SetCharacterVelocity(const CharacterHandle Character, const Float3& Velocity) {
        if (const auto It = _Impl->Characters.find(Character); It != _Impl->Characters.end()) {
            It->second.Body->SetLinearVelocity(ToJolt(Velocity));
        }
    }

    void PhysicsWorld::SetCharacterPosition(const CharacterHandle Character, const Float3& Position) {
        if (const auto It = _Impl->Characters.find(Character); It != _Impl->Characters.end()) {
            It->second.Body->SetPosition(ToJolt(Position));
            It->second.Body->SetLinearVelocity(JPH::Vec3::sZero());
        }
    }

    CharacterState PhysicsWorld::GetCharacterState(const CharacterHandle Character) const {
        CharacterState State;
        const auto It = _Impl->Characters.find(Character);
        if (It == _Impl->Characters.end()) return State;

        const JPH::CharacterVirtual& Body = *It->second.Body;
        State.Position = FromJolt(JPH::Vec3(Body.GetPosition()));
        State.Velocity = FromJolt(Body.GetLinearVelocity());
        State.Grounded = Body.GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
        return State;
    }
}  // namespace Xen
