//
// Created by Jake Rieger on 10/9/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include <Xen/Component.hpp>
#include <Xen/ComponentRegistry.hpp>

namespace Xen {
    XEN_COMPONENT(PrefabSpawnerComponent)

    /// @brief Spawns copies of a prefab asset at its actor's position and
    /// rotation, once or on a repeating timer.
    ///
    /// Spawns on Tick, so nothing is spawned while a scene is being edited -
    /// only once the simulation runs - and the spawned actors are ordinary
    /// actors that go away with the scene like any other.
    class PrefabSpawnerComponent final : public IComponent {
    public:
        XEN_COMPONENT_STATICS(PrefabSpawnerComponent)
        PrefabSpawnerComponent() = default;

        void Reflect(IReflector& R) override;
        void BeginPlay() override;
        void Tick(f32 DeltaTime) override;

        /// @brief Spawns one copy now, regardless of the timer.
        void Spawn();

    private:
        AssetID _PrefabAsset {};
        /// Seconds between spawns. Zero spawns once, on the first Tick.
        f32 _Interval {0.0f};
        /// Stop after this many spawns. Zero is no limit (only meaningful
        /// with an Interval).
        i32 _MaxSpawns {0};

        // Runtime only.
        f32 _Timer {0.0f};
        i32 _Spawned {0};
        bool _SpawnedFirst {false};
    };
}  // namespace Xen
