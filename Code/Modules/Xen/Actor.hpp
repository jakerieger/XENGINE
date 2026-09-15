//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Common/XenCommon.hpp>
#include <Common/Exception.hpp>

#include "ActorHandle.hpp"
#include "Component.hpp"
#include "Transform.hpp"

#include <memory>
#include <string>
#include <vector>

namespace Xen {
    class Scene;

    _DefineEngineException(ActorException);

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

        glm::vec2 GetPosition() const { return _Transform.Position; }
        void SetPosition(const glm::vec2& Position) { _Transform.Position = Position; }

        f32 GetRotation() const { return _Transform.Rotation; }
        void SetRotation(const f32 Rotation) { _Transform.Rotation = Rotation; }

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
