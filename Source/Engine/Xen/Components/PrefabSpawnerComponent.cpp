//
// Created by Jake Rieger on 10/9/2026.
//

#include "PrefabSpawnerComponent.hpp"

#include <Xen/Actor.hpp>
#include <Xen/Scene.hpp>

namespace Xen {
    void PrefabSpawnerComponent::Reflect(IReflector& R) {
        R.Property("PrefabAsset",
                   _PrefabAsset,
                   {.ToolTip = "The prefab to spawn.", .Category = "Spawner", .Asset = AssetKind::Prefab});
        R.Property("Interval",
                   _Interval,
                   {.ToolTip  = "Seconds between spawns. Zero spawns once, when the game starts running.",
                    .Category = "Spawner",
                    .Min      = 0.0f,
                    .Max      = 3600.0f});
        R.Property("MaxSpawns",
                   _MaxSpawns,
                   {.ToolTip  = "Stop after this many spawns. Zero is unlimited.",
                    .Category = "Spawner",
                    .Min      = 0.0f,
                    .Max      = 100000.0f});
    }

    void PrefabSpawnerComponent::BeginPlay() {
        _Timer        = 0.0f;
        _Spawned      = 0;
        _SpawnedFirst = false;
    }

    void PrefabSpawnerComponent::Spawn() {
        Scene* S = GetScene();
        if (!S || !_PrefabAsset.IsValid()) return;

        const Transform Pose = GetOwner()->GetWorldTransform();
        S->Instantiate(_PrefabAsset, Pose.Position, Pose.Rotation);
        ++_Spawned;
    }

    void PrefabSpawnerComponent::Tick(const f32 DeltaTime) {
        if (!_SpawnedFirst) {
            _SpawnedFirst = true;
            Spawn();
            return;
        }

        if (_Interval <= 0.0f) return;
        if (_MaxSpawns > 0 && _Spawned >= _MaxSpawns) return;

        _Timer += DeltaTime;
        if (_Timer >= _Interval) {
            _Timer -= _Interval;
            Spawn();
        }
    }
}  // namespace Xen
