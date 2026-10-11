//
// Created by Jake Rieger on 9/8/2026.
//

#pragma once

#include <Common/XenCommon.hpp>

#include <functional>

namespace Xen {
    struct ActorHandle {
        u32 Index {0};
        u32 Generation {0};  // 0 = "never assigned"

        constexpr ActorHandle() = default;
        constexpr ActorHandle(const u32 InIndex, const u32 InGeneration) : Index(InIndex), Generation(InGeneration) {}

        NODISCARD constexpr bool IsSet() const { return Generation != 0; }

        constexpr bool operator==(const ActorHandle& Other) const {
            return Index == Other.Index && Generation == Other.Generation;
        }

        constexpr bool operator!=(const ActorHandle& Other) const { return !(*this == Other); }

        static constexpr ActorHandle Invalid() { return {}; }
    };
}  // namespace Xen

template<>
struct std::hash<Xen::ActorHandle> {
    size_t operator()(const Xen::ActorHandle& Handle) const noexcept {
        return std::hash<Xen::u64>()((Xen::CAST<Xen::u64>(Handle.Generation) << 32) | Handle.Index);
    }
};