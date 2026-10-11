//
// Created by Jake Rieger on 10/13/2026.
//
// The kinds of actor the editor can create ready-made: an empty one, and the
// common ones (camera, lights, environment, ...) set up with sensible
// defaults, so they don't have to be assembled from an empty actor and a
// string of Add Component clicks.

#pragma once

#include <Xen/Scene.hpp>

#include <span>
#include <string>

namespace Xen {
    enum class ActorPreset : u8 {
        Empty,
        Camera,
        DirectionalLight,
        PointLight,
        SpotLight,
        Environment,
        PostProcess,
        Player,
    };

    struct ActorPresetInfo {
        ActorPreset Preset;
        const char* Label;        // in menus and lists
        const char* DefaultName;  // the new actor's name unless one is given
        const char* Description;
    };

    /// @brief Every preset, in display order.
    std::span<const ActorPresetInfo> GetActorPresets();

    const ActorPresetInfo& GetActorPresetInfo(ActorPreset Preset);

    /// @brief Spawns an actor named Name into S, set up as Preset describes.
    /// Components are added after the actor is placed, so an actor in a scene
    /// that's already playing begins play where it belongs.
    ActorHandle SpawnActorPreset(Scene& S, ActorPreset Preset, const std::string& Name);
}  // namespace Xen
