//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include "Actor.hpp"
#include "Components/CameraComponent.hpp"
#include "EngineContext.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace Xen {
    /// @brief Owns every actor in a level and drives their lifecycle.
    ///
    /// Actors live in a slot array. Destroying one frees its slot for reuse
    /// and bumps that slot's generation, which is what lets stale ActorHandles
    /// fail to resolve instead of silently pointing at whichever actor
    /// inherited the slot.
    class Scene {
    public:
        explicit Scene(std::string Name = "Scene");
        Scene(std::string Name, const EngineContext& Context);

        ~Scene();

        Scene(const Scene&)            = delete;
        Scene& operator=(const Scene&) = delete;

        const std::string& GetName() const { return _Name; }
        void SetName(std::string Name) { _Name = std::move(Name); }

        const EngineContext& GetContext() const { return _Context; }
        void SetContext(const EngineContext& Context) { _Context = Context; }

        // --- Spawning ---------------------------------------------------

        /// @brief Creates an actor and returns a handle to it.
        ///
        /// If the scene has already begun play, the actor's BeginPlay runs
        /// right away. It will not Tick until the next frame, so an actor
        /// spawned mid-tick never runs a partial first frame.
        template<typename T = Actor, typename... Args>
        ActorHandle Spawn(Args&&... ActorArgs) {
            static_assert(std::is_base_of_v<Actor, T>, "T must derive from Actor");
            auto Owned = std::make_unique<T>(std::forward<Args>(ActorArgs)...);
            return AdoptActor(std::move(Owned));
        }

        /// @brief Spawns an actor carrying a specific persistent ID.
        ///
        /// For the deserializer only: saved references are stored as IDs, so
        /// a loaded actor must keep the ID it had when saved. Passing 0
        /// assigns a fresh one, exactly like Spawn.
        ActorHandle SpawnWithID(u64 ActorID, std::string Name = "Actor");

        /// @brief Destroys every actor and resets the scene. Unlike EndPlay,
        /// the scene stays usable and keeps its begun-play state - this is
        /// what loading a scene over an existing one uses.
        ///
        /// Safe to call from inside a Tick/FixedTick dispatch (e.g. from a
        /// component reacting to something mid-frame): the actual clear is
        /// deferred until the enclosing Tick/FixedTick's actor loop has fully
        /// returned, the same way Destroy() defers via _PendingDestroy. Doing
        /// it immediately would destroy the actor (and component) whose
        /// method is still on the call stack, and would empty _Slots out from
        /// under the loop that is still indexing it.
        void Clear();

        /// @brief Resolves a handle. Returns nullptr if the actor was
        /// destroyed, if the slot has since been reused, or if the handle was
        /// never set.
        Actor* Get(ActorHandle Handle) const;

        /// @brief True if the handle resolves to a live actor.
        bool IsValid(const ActorHandle Handle) const { return Get(Handle) != nullptr; }

        /// @brief Marks an actor for destruction at the end of the current
        /// tick. Its children are destroyed with it.
        void Destroy(ActorHandle Handle);

        // --- Iteration --------------------------------------------------

        /// @brief Visits every live actor. Safe to spawn or destroy during
        /// iteration: spawns are visited next frame, destroys take effect
        /// after the sweep.
        void ForEachActor(const std::function<void(Actor&)>& Fn) const;

        /// @brief Every live actor carrying a component of the given type.
        template<typename T>
        std::vector<Actor*> FindActorsWith() const {
            std::vector<Actor*> Out;
            for (const auto& Slot : _Slots) {
                if (!Slot.Actor || Slot.Actor->IsPendingDestroy()) continue;
                if (Slot.Actor->HasComponent<T>()) Out.push_back(Slot.Actor.get());
            }
            return Out;
        }

        /// @brief Resolves a persistent actor ID to its current handle.
        /// Used when loading a scene to re-link saved actor references.
        ActorHandle FindByActorID(u64 ActorID) const;

        /// @brief First live actor with the given name, or an unset handle.
        /// Linear - intended for setup and debugging, not per-frame lookups.
        ActorHandle FindActorByName(const std::string& Name) const;

        /// @brief Count of live actors, excluding any pending destruction.
        size_t GetActorCount() const;

        CameraComponent* GetMainCamera() const;

        // --- Lifecycle --------------------------------------------------

        /// @brief Runs BeginPlay on every actor. Calling twice is a no-op.
        void BeginPlay();

        /// @brief Advances one frame: ticks actors, then sweeps anything
        /// marked for destruction.
        void Tick(f32 DeltaTime);

        void FixedTick(f32 FixedDelta);

        /// @brief Runs EndPlay on everything and clears the scene.
        void EndPlay();

        bool HasBegunPlay() const { return _BeganPlay; }

    private:
        friend class Actor;

        struct ActorSlot {
            std::unique_ptr<Xen::Actor> Actor;
            u32 Generation {0};
        };

        ActorHandle AdoptActor(std::unique_ptr<Actor> Owned);
        void SweepPendingDestroys();
        void DestroyImmediate(u32 Index);
        void MarkForDestruction(ActorHandle Handle);
        void ClearImmediate();

        std::string _Name;
        EngineContext _Context {};
        std::vector<ActorSlot> _Slots;
        std::vector<u32> _FreeSlots;

        // Monotonic, never reused - unlike slot indices. Starts at 1 so 0
        // can mean "no actor" in saved references.
        u64 _NextActorID {1};

        bool _BeganPlay {false};
        bool _Ticking {false};
        bool _PendingClear {false};
        std::vector<ActorHandle> _PendingDestroy;
    };
}  // namespace Xen