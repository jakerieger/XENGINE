//
// Created by Jake Rieger on 10/13/2026.
//

#include "ActorPresets.hpp"

#include <Xen/Components/CameraComponent.hpp>
#include <Xen/Components/CharacterControllerComponent.hpp>
#include <Xen/Components/DirectionalLightComponent.hpp>
#include <Xen/Components/EnvironmentComponent.hpp>
#include <Xen/Components/FPPlayerController.hpp>
#include <Xen/Components/PointLightComponent.hpp>
#include <Xen/Components/PostProcessComponent.hpp>
#include <Xen/Components/SpotLightComponent.hpp>

#include <iterator>

namespace Xen {
    namespace {
        constexpr ActorPresetInfo kPresets[] = {
          {ActorPreset::Empty, "Empty", "Actor", "An actor with no components."},
          {ActorPreset::Camera, "Camera", "Camera", "A perspective camera."},
          {ActorPreset::DirectionalLight,
           "Directional Light",
           "Sun",
           "Light from a distance, like the sun, with shadows."},
          {ActorPreset::PointLight, "Point Light", "Point Light", "Light that spreads in every direction."},
          {ActorPreset::SpotLight, "Spot Light", "Spot Light", "A cone of light, pointing down."},
          {ActorPreset::Environment,
           "Environment",
           "Environment",
           "Sky and image-based lighting from an HDR map."},
          {ActorPreset::PostProcess, "Post Process", "PostProcess", "Bloom and automatic exposure."},
          {ActorPreset::Player,
           "Player (First Person)",
           "Player",
           "A camera on a walking character: WASD, Shift, Space and mouse look."},
        };

        // Euler angles in degrees, pitch/yaw/roll order matching how the
        // editor's own inspector shows rotation.
        Quat Rotation(const f32 PitchDegrees, const f32 YawDegrees, const f32 RollDegrees = 0.0f) {
            using namespace DirectX;
            Quat Out;
            XMStoreFloat4(&Out,
                          XMQuaternionRotationRollPitchYaw(XMConvertToRadians(PitchDegrees),
                                                           XMConvertToRadians(YawDegrees),
                                                           XMConvertToRadians(RollDegrees)));
            return Out;
        }
    }  // namespace

    std::span<const ActorPresetInfo> GetActorPresets() {
        return kPresets;
    }

    const ActorPresetInfo& GetActorPresetInfo(const ActorPreset Preset) {
        for (const ActorPresetInfo& Info : kPresets) {
            if (Info.Preset == Preset) return Info;
        }
        return kPresets[0];
    }

    ActorHandle SpawnActorPreset(Scene& S, const ActorPreset Preset, const std::string& Name) {
        const ActorHandle Handle = S.Spawn(Name);
        Actor* A                 = S.Get(Handle);
        if (!A) return Handle;

        switch (Preset) {
            case ActorPreset::Empty:
                break;

            case ActorPreset::Camera: {
                A->SetPosition({0.0f, 1.0f, 10.0f});
                if (auto* Camera = A->AddComponent<CameraComponent>()) {
                    Camera->SetProjectionMode(ProjectionMode::Perspective);
                    Camera->SetFieldOfView(60.0f);
                }
                break;
            }

            case ActorPreset::DirectionalLight: {
                A->SetRotation(Rotation(-45.0f, 150.0f));
                if (auto* Light = A->AddComponent<DirectionalLightComponent>()) {
                    Light->SetIntensity(1.0f);
                    Light->SetShadowDistance(15.0f);
                }
                break;
            }

            case ActorPreset::PointLight: {
                A->SetPosition({0.0f, 2.0f, 0.0f});
                A->AddComponent<PointLightComponent>();
                break;
            }

            case ActorPreset::SpotLight: {
                A->SetPosition({0.0f, 3.0f, 0.0f});
                A->SetRotation(Rotation(-90.0f, 0.0f));
                A->AddComponent<SpotLightComponent>();
                break;
            }

            case ActorPreset::Environment: {
                if (auto* Env = A->AddComponent<EnvironmentComponent>()) {
                    Env->SetMapAsset(ASSET("ibl/maps/sky_spring.hdr"));
                    Env->SetShowBackground(true);
                }
                break;
            }

            case ActorPreset::PostProcess: {
                if (auto* PostFx = A->AddComponent<PostProcessComponent>()) {
                    auto& Settings               = PostFx->GetSettings();
                    Settings.BloomThreshold      = 0.8f;
                    Settings.BloomIntensity      = 0.075f;
                    Settings.BloomEnabled        = true;
                    Settings.AutoExposureEnabled = true;
                }
                break;
            }

            case ActorPreset::Player: {
                // The capsule's center, so the camera is at about head height
                // of a character standing on the ground at y = 0.
                A->SetPosition({0.0f, 1.0f, 0.0f});
                if (auto* Camera = A->AddComponent<CameraComponent>()) {
                    Camera->SetProjectionMode(ProjectionMode::Perspective);
                    Camera->SetFieldOfView(70.0f);
                }
                // Before the controller: it looks the character up in BeginPlay.
                A->AddComponent<CharacterControllerComponent>();
                A->AddComponent<FPPlayerController>();
                break;
            }
        }

        return Handle;
    }
}  // namespace Xen
