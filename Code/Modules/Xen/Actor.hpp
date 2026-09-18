//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "ActorHandle.hpp"
#include "Component.hpp"
#include "Transform.hpp"

#include <memory>
#include <string>
#include <vector>

namespace Xen {
    class Scene;

    DEFINE_ENGINE_EXCEPTION(ActorException);

    class Actor : public IReflectable {
    public:
        explicit Actor(const std::string& Name = "Actor");
        ~Actor() override;

        Actor(const Actor&)            = delete;
        Actor& operator=(const Actor&) = delete;

        // --- Identity -----------------------------------------------------------------------------------------------

        const std::string& GetName() const { return _Name; }
        void SetName(const std::string& Name) { _Name = Name; }

        ActorHandle GetHandle() const { return _Handle; }
        u64 GetActorID() const { return _ActorID; }
        Scene* GetScene() const { return _Scene; }

        // --- Transform ----------------------------------------------------------------------------------------------

        const Transform& GetLocalTransform() const { return _Transform; }
        Transform& GetLocalTransform() { return _Transform; }
        void SetLocalTransform(const Transform& Transform) { _Transform = Transform; }

        Float3 GetPosition() const { return _Transform.Position; }
        void SetPosition(const Float3& Position) { _Transform.Position = Position; }

        /// @brief 2D convenience: sets X/Y and leaves Z exactly as it was,
        /// rather than zeroing it - the widened-Float3 overload above would
        /// otherwise silently stomp any Z an actor had if a 2D call site kept
        /// writing a 2-component position.
        void SetPosition(const Float2& Position) {
            _Transform.Position = {Position.x, Position.y, _Transform.Position.z};
        }

        Quat GetRotation() const { return _Transform.Rotation; }
        void SetRotation(const Quat& Rotation) { _Transform.Rotation = Rotation; }

        /// @brief 2D convenience, matching the old single-float Rotation this
        /// superseded - see Transform::GetRotationZ/SetRotationZ.
        f32 GetRotationZ() const { return _Transform.GetRotationZ(); }
        void SetRotationZ(const f32 Rotation) { _Transform.SetRotationZ(Rotation); }

        Float3 GetScale() const { return _Transform.Scale; }
        void SetScale(const Float3& Scale) { _Transform.Scale = Scale; }

        Transform GetWorldTransform() const;

        // --- Hierarchy ----------------------------------------------------------------------------------------------

        void AttachTo(ActorHandle Parent);
        void Detach() { AttachTo(ActorHandle::Invalid()); }

        ActorHandle GetParent() const { return _Parent; }
        const std::vector<ActorHandle>& GetChildren() const { return _Children; }

        // --- Components ---------------------------------------------------------------------------------------------

        template<typename T, typename... Args>
        T* AddComponent(Args&&... ComponentArgs) {
            static_assert(std::is_base_of_v<IComponent, T>, "T must derive from IComponent");

            auto Owned  = std::unique_ptr<T>(new T(std::forward<Args>(ComponentArgs)...));
            T* Raw      = Owned.get();
            Raw->_Owner = this;
            _Components.push_back(std::move(Owned));

            if (_BeganPlay) {
                Raw->BeginPlay();
                Raw->_BeganPlay = true;
            }

            return Raw;
        }

        template<typename T>
        T* GetComponent() const {
            for (const auto& C : _Components) {
                if (C->GetTypeID() == T::StaticTypeID()) return CAST<T*>(C.get());
            }

            return nullptr;
        }

        template<typename T>
        std::vector<T*> GetComponents() const {
            std::vector<T*> Out;
            for (const auto& C : _Components) {
                if (C->GetTypeID() == T::StaticTypeID()) Out.push_back(CAST<T*>(C.get()));
            }

            return Out;
        }

        template<typename T>
        bool HasComponent() const {
            return GetComponent<T>() != nullptr;
        }

        template<typename T>
        bool RemoveComponent() {
            for (size_t i = 0; i < _Components.size(); ++i) {
                if (_Components[i]->GetTypeID() != T::StaticTypeID()) continue;
                if (_Components[i]->_BeganPlay) _Components[i]->EndPlay();
                _Components.erase(_Components.begin() + CAST<long>(i));
                return true;
            }

            return false;
        }

        void ForEachComponent(const std::function<void(IComponent*)>& Fn) const {
            for (size_t i = 0; i < _Components.size(); ++i) {
                Fn(_Components[i].get());
            }
        }

        size_t GetComponentCount() const { return _Components.size(); }

        IComponent* GetComponentAt(const size_t Index) const {
            return Index < _Components.size() ? _Components[Index].get() : nullptr;
        }

        IComponent* AdoptComponent(std::unique_ptr<IComponent> Owned);

        // --- State --------------------------------------------------------------------------------------------------

        bool IsEnabled() const { return _Enabled; }
        void SetEnabled(const bool Enabled) { _Enabled = Enabled; }

        bool IsPendingDestroy() const { return _PendingDestroy; }
        void Destroy() const;

        // --- Lifecycle (called by Scene) ----------------------------------------------------------------------------

        virtual void BeginPlay() {}
        virtual void Tick(const f32 DeltaTime) { (void)DeltaTime; }
        virtual void FixedTick(const f32 FixedDelta) { (void)FixedDelta; }
        virtual void EndPlay() {}

        void Reflect(IReflector& R) override;

    private:
        friend class Scene;

        void DispatchBeginPlay();
        void DispatchTick(f32 DeltaTime);
        void DispatchFixedTick(f32 FixedDelta);
        void DispatchEndPlay();

        std::string _Name;
        Transform _Transform;

        Scene* _Scene {nullptr};
        ActorHandle _Handle;
        u64 _ActorID {0};
        ActorHandle _Parent;
        std::vector<ActorHandle> _Children;

        std::vector<std::unique_ptr<IComponent>> _Components;

        bool _Enabled {true};
        bool _BeganPlay {false};
        bool _PendingDestroy {false};
    };
}  // namespace Xen
