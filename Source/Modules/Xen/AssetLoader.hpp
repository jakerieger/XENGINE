//
// Created by Jake Rieger on 9/20/2026.
//
// Loads a batch of assets with the expensive part - unpacking (decrypt +
// decompress) and decoding - on worker threads, while the GPU part (creating
// and uploading the resource, inserting into the cache) stays on the calling
// thread, which is the only one allowed to touch the render device. That's
// what lets a loading screen keep animating and the window keep responding
// through a stall that used to freeze the whole process: the main thread just
// calls Pump() once a frame.

#pragma once

#include <Common/XenCommon.hpp>
#include <XenPAK/AssetID.hpp>

#include "MeshCache.hpp"
#include "Reflection.hpp"
#include "TextureCache.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

namespace Xen {
    /// @brief One asset to load.
    struct LoadRequest {
        AssetID ID {};
        AssetKind Kind {AssetKind::Unknown};

        /// Textures only: upload as sRGB (a color map fed into lighting math).
        /// Has to be decided up front, because a texture already resident is
        /// reused as-is regardless of what a later Acquire asks for - see
        /// TextureCache::Acquire.
        bool Srgb {false};
    };

    class AssetLoader {
    public:
        AssetLoader() = default;
        ~AssetLoader();

        AssetLoader(const AssetLoader&)            = delete;
        AssetLoader& operator=(const AssetLoader&) = delete;

        /// @brief Starts decoding Requests on worker threads. Requests for an
        /// asset already resident in its cache are dropped up front. Only
        /// Texture and Mesh kinds are loadable; anything else is ignored.
        /// Neither cache is touched from a worker - only their const
        /// DecodeAsset - so both must outlive this loader.
        void Begin(std::vector<LoadRequest> Requests, TextureCache* Textures, MeshCache* Meshes);

        /// @brief Main thread, once per frame: finishes whatever the workers
        /// have decoded (the GPU half), stopping once BudgetMs has elapsed - an
        /// upload is atomic, so the budget is checked between assets, and at
        /// least one is finished per call if any is ready. Returns true once
        /// every request has been finished.
        ///
        /// A failure on a worker (a missing or undecodable asset) is rethrown
        /// here, on the main thread, where the old synchronous load threw it.
        bool Pump(f64 BudgetMs);

        /// @brief Stops the workers (after the asset each is decoding) and
        /// joins them; whatever they'd decoded but not yet handed over is
        /// dropped. Safe to call repeatedly.
        void Cancel();

        NODISCARD bool IsActive() const { return _Active; }
        NODISCARD size_t Done() const { return _Done; }
        NODISCARD size_t Total() const { return _Requests.size(); }

    private:
        struct Result {
            size_t Index {0};
            DecodedTexture Texture;
            DecodedMesh Mesh;
            std::exception_ptr Error;
        };

        void WorkerMain();

        std::vector<LoadRequest> _Requests;
        TextureCache* _Textures {nullptr};
        MeshCache* _Meshes {nullptr};

        std::vector<std::thread> _Workers;

        // Guards everything below it. _Ready is what the workers hand over.
        std::mutex _Mutex;
        std::condition_variable _WorkerWake;
        std::deque<Result> _Ready;
        size_t _NextIndex {0};  // next request a worker will claim
        size_t _Decoding {0};   // claimed but not yet in _Ready
        bool _Cancelled {false};

        // A decoded 8K environment map is ~100 MB, so a worker won't claim
        // another request while this many are decoded-and-waiting or in flight.
        static constexpr size_t MaxPending = 3;

        // Main thread only.
        size_t _Done {0};
        bool _Active {false};
    };
}  // namespace Xen
