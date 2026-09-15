//
// Created by Jake Rieger on 9/8/2026.
//

#include "Actor.hpp"
#include "Scene.hpp"

#include <algorithm>

namespace Xen {
    Scene* IComponent::GetScene() const {
        return _Owner ? _Owner->GetScene() : nullptr;
    }

    Actor::Actor(const std::string& Name) : _Name(Name) {}

    Actor::~Actor() = default;

    Transform Actor::GetWorldTransform() const {
        if (!_Parent.IsSet() || !_Scene) return _Transform;

        const Actor* ParentActor = _Scene->Get(_Parent);
        if (!ParentActor) return _Transform;

        return _Transform.ComposedWith(ParentActor->GetWorldTransform());
    }

    void Actor::AttachTo(const ActorHandle Parent) {
        if (!_Scene) { THROW_ENGINE_EXCEPTION(ActorException, "cannot attach an actor that is not in a scene"); }
        if (Parent == _Handle) { THROW_ENGINE_EXCEPTION(ActorException, "cannot attach an actor to itself"); }

        if (Parent.IsSet()) {
            const Actor* Ancestor = _Scene->Get(Parent);
            while (Ancestor) {
                if (Ancestor == this) {
                    THROW_ENGINE_EXCEPTION(ActorException,
                                           "cannot attach an actor to its own descendant (would form a cycle)");
                }

                Ancestor = Ancestor->_Parent.IsSet() ? _Scene->Get(Ancestor->_Parent) : nullptr;
            }
        }

        if (_Parent.IsSet()) {
            if (Actor* Old = _Scene->Get(_Parent)) { std::erase(Old->_Children, _Handle); }
        }

        _Parent = Parent;

        if (Parent.IsSet()) {
            if (Actor* New = _Scene->Get(Parent)) { New->_Children.push_back(_Handle); }
        }
    }

    IComponent* Actor::AdoptComponent(std::unique_ptr<IComponent> Owned) {
        if (!Owned) return nullptr;

        IComponent* Raw = Owned.get();
        Raw->_Owner     = this;
        _Components.push_back(std::move(Owned));

        if (_BeganPlay) {
            Raw->BeginPlay();
            Raw->_BeganPlay = true;
        }

        return Raw;
    }

    void Actor::Destroy() const {
        if (_Scene) _Scene->Destroy(_Handle);
    }

    void Actor::Reflect(IReflector& R) {
        R.Property("Name", _Name, {.DisplayName = "Name", .Category = "Actor"});
        R.Property("Transform", _Transform, {.Category = "Actor"});
        R.Property("Enabled", _Enabled, {.Category = "Actor"});
    }

    void Actor::DispatchBeginPlay() {
        if (_BeganPlay) return;
        _BeganPlay = true;

        BeginPlay();

        for (size_t i = 0; i < _Components.size(); ++i) {
            if (_Components[i]->_BeganPlay) continue;
            _Components[i]->BeginPlay();
            _Components[i]->_BeganPlay = true;
        }
    }

    void Actor::DispatchTick(f32 DeltaTime) {
        if (!_Enabled || _PendingDestroy) return;

        Tick(DeltaTime);

        for (size_t i = 0; i < _Components.size(); ++i) {
            if (IComponent* C = _Components[i].get(); C->IsEnabled() && C->_BeganPlay) C->Tick(DeltaTime);
        }
    }

    void Actor::DispatchFixedTick(const f32 FixedDelta) {
        if (!_Enabled || _PendingDestroy) return;

        FixedTick(FixedDelta);

        for (size_t i = 0; i < _Components.size(); ++i) {
            if (IComponent* C = _Components[i].get(); C->IsEnabled() && C->_BeganPlay) C->FixedTick(FixedDelta);
        }
    }

    void Actor::DispatchEndPlay() {
        if (!_BeganPlay) return;

        // Reverse order, so a component that depends on an earlier one still
        // sees it alive during teardown.
        for (size_t i = _Components.size(); i > 0; --i) {
            if (IComponent* C = _Components[i - 1].get(); C->_BeganPlay) {
                C->EndPlay();
                C->_BeganPlay = false;
            }
        }

        EndPlay();
        _BeganPlay = false;
    }
}  // namespace Xen