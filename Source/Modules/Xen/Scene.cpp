//
// Created by Jake Rieger on 9/8/2026.
//

#include "Scene.hpp"
#include "ComponentRegistry.hpp"
#include "SceneSerializer.hpp"

#include <algorithm>

namespace Xen {
    Scene::Scene(std::string Name) : _Name(std::move(Name)) {}

    Scene::Scene(std::string Name, const EngineContext& Context) : _Name(std::move(Name)), _Context(Context) {}

    Scene::~Scene() {
        // Tear down cleanly even if the caller never called EndPlay, so
        // components always get the EndPlay that pairs with their BeginPlay.
        if (_BeganPlay) EndPlay();
    }

    ActorHandle Scene::AdoptActor(std::unique_ptr<Actor> Owned) {
        u32 Index;

        if (!_FreeSlots.empty()) {
            Index = _FreeSlots.back();
            _FreeSlots.pop_back();
        } else {
            Index = CAST<u32>(_Slots.size());
            _Slots.emplace_back();
        }

        ActorSlot& Slot = _Slots[Index];

        // Generation starts at 1 and only ever increases, so generation 0 can
        // stay reserved as "unset handle".
        if (Slot.Generation == 0) Slot.Generation = 1;

        Slot.Actor = std::move(Owned);

        const ActorHandle Handle {Index, Slot.Generation};
        Slot.Actor->_Scene  = this;
        Slot.Actor->_Handle = Handle;
        if (Slot.Actor->_ActorID == 0) Slot.Actor->_ActorID = _NextActorID++;

        // Spawned mid-play, so initialize now. Ticking waits until the next
        // frame - the current tick loop has already snapshotted its range.
        if (_BeganPlay) Slot.Actor->DispatchBeginPlay();

        return Handle;
    }

    ActorHandle Scene::SpawnWithID(const u64 ActorID, std::string Name) {
        auto Owned = std::make_unique<Actor>(std::move(Name));

        // Set before adopting so AdoptActor keeps it rather than assigning a
        // fresh one.
        Owned->_ActorID = ActorID;

        const ActorHandle H = AdoptActor(std::move(Owned));

        // Keep the counter ahead of every ID seen, so actors spawned after a
        // load never collide with loaded ones.
        if (ActorID >= _NextActorID) _NextActorID = ActorID + 1;

        return H;
    }

    void Scene::Clear() {
        if (_Ticking) {
            // Deferred exactly like Destroy()/_PendingDestroy: a caller mid-
            // dispatch (e.g. a component's own Tick/FixedTick) is still on the
            // call stack of an actor this would destroy, and Tick/FixedTick's
            // loop is still indexing _Slots - clearing it out now would be a
            // use-after-free plus an out-of-bounds read the moment control
            // returns to that loop.
            _PendingClear = true;
            return;
        }
        ClearImmediate();
    }

    void Scene::ClearImmediate() {
        for (size_t i = _Slots.size(); i > 0; --i) {
            if (_Slots[i - 1].Actor) _Slots[i - 1].Actor->DispatchEndPlay();
        }
        _Slots.clear();
        _FreeSlots.clear();
        _PendingDestroy.clear();
        _PendingClear = false;
        _NextActorID  = 1;
    }

    Actor* Scene::Get(const ActorHandle Handle) const {
        if (!Handle.IsSet() || Handle.Index >= _Slots.size()) return nullptr;

        const ActorSlot& Slot = _Slots[Handle.Index];

        // The generation check is the whole point of handles: a stale handle
        // into a recycled slot fails here rather than returning a different
        // actor than the caller meant.
        if (!Slot.Actor || Slot.Generation != Handle.Generation) return nullptr;

        return Slot.Actor.get();
    }

    void Scene::Destroy(const ActorHandle Handle) {
        MarkForDestruction(Handle);

        // Outside a tick there is no iteration to protect, so sweep now and
        // keep the scene's state immediately consistent.
        if (!_Ticking) SweepPendingDestroys();
    }

    ActorHandle Scene::Clone(const ActorHandle Handle) {
        Actor* Source = Get(Handle);
        if (!Source) return ActorHandle::Invalid();

        // Save side: Source's own properties, then each component's, using
        // the same reflectors SceneSerializer::SaveToJson uses for a whole
        // scene - see this method's own header comment for why.
        JsonSaveReflector PropSaver(*this);
        Source->Reflect(PropSaver);
        const Json SavedProperties = PropSaver.Result();

        struct SavedComponent {
            ComponentTypeID TypeID;
            Json Properties;
        };
        std::vector<SavedComponent> SavedComponents;
        SavedComponents.reserve(Source->GetComponentCount());
        for (size_t i = 0; i < Source->GetComponentCount(); ++i) {
            IComponent* C = Source->GetComponentAt(i);

            JsonSaveReflector CompSaver(*this);
            C->Reflect(CompSaver);

            SavedComponents.push_back({C->GetTypeID(), CompSaver.Result()});
        }

        // Load side: a fresh actor with a fresh handle/persistent ID - a
        // real independent copy, not a reference to Source - then the saved
        // properties and components replayed onto it. No actor-to-actor
        // references (parent/children handles) to remap here: Actor::Reflect
        // only ever visits this actor's own scalar/Transform/Enabled fields,
        // never _Parent/_Children (see Actor::Reflect's own body) - parenting
        // is re-established explicitly below instead, same as
        // SceneSerializer's own separate parenting pass.
        const ActorHandle CloneHandle = Spawn(Source->GetName() + " (Copy)");
        Actor* NewActor               = Get(CloneHandle);

        const std::unordered_map<u64, ActorHandle> NoRemap;
        JsonLoadReflector PropLoader(SavedProperties, NoRemap);
        NewActor->Reflect(PropLoader);

        // Reflect() above just loaded Source's own (un-suffixed) Name back
        // over the one Spawn set - Actor::Reflect visits "Name" too (see its
        // own body) - so it has to be reasserted after, not before.
        NewActor->SetName(Source->GetName() + " (Copy)");

        for (const SavedComponent& Saved : SavedComponents) {
            std::unique_ptr<IComponent> Created = ComponentRegistry::Get().Create(Saved.TypeID);
            if (!Created) {
                // Shouldn't happen for a component that's live on Source right
                // now - every concrete component type registers itself via
                // REGISTER_COMPONENT at static-init time - but skip rather
                // than crash an editor's Duplicate action over it.
                LOG_WARN("Scene::Clone: component type %llu on actor '%s' has no registered factory - skipped",
                         Saved.TypeID,
                         Source->GetName().c_str());
                continue;
            }

            JsonLoadReflector CompLoader(Saved.Properties, NoRemap);
            Created->Reflect(CompLoader);

            NewActor->AdoptComponent(std::move(Created));
        }

        // Same parent Source had - AttachTo(Invalid()) if Source has none,
        // which is exactly what Detach() already does, so the clone ends up
        // a sibling of Source rather than silently reparented to the root.
        NewActor->AttachTo(Source->GetParent());

        // Recurse over Source's children so duplicating a parent duplicates
        // its whole subtree - a "Duplicate" that silently dropped every
        // child would be an easy-to-miss surprise compared to how e.g.
        // Unity/Unreal's own duplicate already behaves. Copied first, same
        // reasoning as MarkForDestruction's own copy: cloning a child
        // attaches it, which would otherwise mutate the list this loop is
        // still walking.
        const std::vector<ActorHandle> Children = Source->GetChildren();
        for (const ActorHandle ChildHandle : Children) {
            const ActorHandle ClonedChildHandle = Clone(ChildHandle);
            if (Actor* ClonedChild = Get(ClonedChildHandle)) { ClonedChild->AttachTo(CloneHandle); }
        }

        return CloneHandle;
    }

    void Scene::MarkForDestruction(const ActorHandle Handle) {
        Actor* A = Get(Handle);
        if (!A || A->IsPendingDestroy()) return;

        A->_PendingDestroy = true;
        _PendingDestroy.push_back(Handle);

        // Children go with the parent. Copied first because MarkForDestroy
        // detaches as it goes, which mutates the child list.
        const std::vector<ActorHandle> Children = A->_Children;
        for (const ActorHandle Child : Children) {
            MarkForDestruction(Child);
        }
    }

    void Scene::SweepPendingDestroys() {
        // Indexed, not range-based: an EndPlay may destroy further actors and
        // append to this list while we walk it.
        for (size_t i = 0; i < _PendingDestroy.size(); ++i) {
            const ActorHandle Handle = _PendingDestroy[i];
            if (!Handle.IsSet() || Handle.Index >= _Slots.size()) continue;

            ActorSlot& Slot = _Slots[Handle.Index];
            if (!Slot.Actor || Slot.Generation != Handle.Generation) continue;

            Slot.Actor->DispatchEndPlay();
        }

        for (const ActorHandle Handle : _PendingDestroy) {
            if (!Handle.IsSet() || Handle.Index >= _Slots.size()) continue;
            ActorSlot& Slot = _Slots[Handle.Index];
            if (!Slot.Actor || Slot.Generation != Handle.Generation) continue;
            DestroyImmediate(Handle.Index);
        }

        _PendingDestroy.clear();
    }

    void Scene::DestroyImmediate(const u32 Index) {
        ActorSlot& Slot = _Slots[Index];
        if (!Slot.Actor) return;

        // Unhook from the parent so it isn't left holding a handle to a slot
        // that is about to be recycled.
        if (Slot.Actor->_Parent.IsSet()) {
            if (Actor* Parent = Get(Slot.Actor->_Parent)) { std::erase(Parent->_Children, Slot.Actor->_Handle); }
        }

        Slot.Actor.reset();

        // Bumping the generation is what invalidates every outstanding handle
        // to this slot. Wrapping past 0 would resurrect stale handles, so skip
        // 0 on overflow.
        ++Slot.Generation;
        if (Slot.Generation == 0) Slot.Generation = 1;

        _FreeSlots.push_back(Index);
    }

    void Scene::ForEachActor(const std::function<void(Actor&)>& Fn) const {
        for (const auto& Slot : _Slots) {
            if (!Slot.Actor || Slot.Actor->IsPendingDestroy()) continue;
            Fn(*Slot.Actor);
        }
    }

    ActorHandle Scene::FindByActorID(const u64 ActorID) const {
        if (ActorID == 0) return ActorHandle::Invalid();
        for (const auto& Slot : _Slots) {
            if (!Slot.Actor || Slot.Actor->IsPendingDestroy()) continue;
            if (Slot.Actor->GetActorID() == ActorID) return Slot.Actor->GetHandle();
        }
        return ActorHandle::Invalid();
    }

    ActorHandle Scene::FindActorByName(const std::string& Name) const {
        for (const auto& Slot : _Slots) {
            if (!Slot.Actor || Slot.Actor->IsPendingDestroy()) continue;
            if (Slot.Actor->GetName() == Name) return Slot.Actor->GetHandle();
        }
        return ActorHandle::Invalid();
    }

    size_t Scene::GetActorCount() const {
        size_t Count = 0;
        for (const auto& Slot : _Slots) {
            if (Slot.Actor && !Slot.Actor->IsPendingDestroy()) ++Count;
        }
        return Count;
    }

    CameraComponent* Scene::GetMainCamera() const {
        CameraComponent* Best = nullptr;

        ForEachActor([&](const Actor& A) {
            if (!A.IsEnabled()) return;
            for (CameraComponent* C : A.GetComponents<CameraComponent>()) {
                if (!C->IsEnabled()) continue;
                if (!Best || C->GetPriority() > Best->GetPriority()) Best = C;
            }
        });

        return Best;
    }

    void Scene::BeginPlay() {
        if (_BeganPlay) return;
        _BeganPlay = true;

        // Indexed and re-checked each step: a BeginPlay may spawn actors,
        // which appends to _Slots and can reallocate it.
        for (size_t i = 0; i < _Slots.size(); ++i) {
            if (_Slots[i].Actor) _Slots[i].Actor->DispatchBeginPlay();
        }
    }

    void Scene::Tick(const f32 DeltaTime) {
        if (!_BeganPlay) BeginPlay();

        _Ticking = true;

        // Snapshot the range so actors spawned during this tick are not ticked
        // until the next frame - otherwise a spawn chain could run for an
        // unbounded number of frames' worth of logic in one frame.
        const size_t Count = _Slots.size();
        for (size_t i = 0; i < Count; ++i) {
            Actor* A = _Slots[i].Actor.get();
            if (A && !A->IsPendingDestroy()) A->DispatchTick(DeltaTime);
        }

        _Ticking = false;

        if (_PendingClear) ClearImmediate();
        else SweepPendingDestroys();
    }

    void Scene::FixedTick(const f32 FixedDelta) {
        if (!_BeganPlay) BeginPlay();

        _Ticking = true;

        const size_t Count = _Slots.size();
        for (size_t i = 0; i < Count; ++i) {
            if (Actor* A = _Slots[i].Actor.get(); A && !A->IsPendingDestroy()) A->DispatchFixedTick(FixedDelta);
        }

        _Ticking = false;

        if (_PendingClear) ClearImmediate();
        else SweepPendingDestroys();
    }

    void Scene::EndPlay() {
        if (!_BeganPlay) return;

        for (size_t i = _Slots.size(); i > 0; --i) {
            if (_Slots[i - 1].Actor) _Slots[i - 1].Actor->DispatchEndPlay();
        }

        _Slots.clear();
        _FreeSlots.clear();
        _PendingDestroy.clear();
        _BeganPlay = false;
    }
}  // namespace Xen