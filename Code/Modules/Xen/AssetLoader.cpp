//
// Created by Jake Rieger on 9/20/2026.
//

#include "AssetLoader.hpp"

#include <algorithm>
#include <chrono>

namespace Xen {
    AssetLoader::~AssetLoader() {
        Cancel();
    }

    void AssetLoader::Begin(std::vector<LoadRequest> Requests, TextureCache* Textures, MeshCache* Meshes) {
        Cancel();

        _Textures = Textures;
        _Meshes   = Meshes;
        _Requests.clear();

        // Only what can actually be loaded, and only what isn't resident yet -
        // this runs on the main thread, the only one allowed to read the
        // caches' maps.
        for (const LoadRequest& Request : Requests) {
            if (Request.Kind == AssetKind::Texture && Textures && !Textures->IsResident(Request.ID)) {
                _Requests.push_back(Request);
            } else if (Request.Kind == AssetKind::Mesh && Meshes && !Meshes->IsResident(Request.ID)) {
                _Requests.push_back(Request);
            }
        }

        _Done       = 0;
        _NextIndex  = 0;
        _Decoding   = 0;
        _Cancelled  = false;
        _Ready.clear();
        _Active = true;

        if (_Requests.empty()) return;

        const size_t Hardware = std::max(std::thread::hardware_concurrency(), 2u);
        const size_t Count    = std::min<size_t>({Hardware - 1, 3, _Requests.size()});
        for (size_t i = 0; i < std::max<size_t>(Count, 1); ++i) {
            _Workers.emplace_back([this] { WorkerMain(); });
        }
    }

    void AssetLoader::WorkerMain() {
        for (;;) {
            size_t Index = 0;
            {
                std::unique_lock Lock(_Mutex);
                _WorkerWake.wait(Lock, [this] {
                    return _Cancelled || _NextIndex >= _Requests.size() || _Ready.size() + _Decoding < MaxPending;
                });
                if (_Cancelled || _NextIndex >= _Requests.size()) return;

                Index = _NextIndex++;
                ++_Decoding;
            }

            const LoadRequest& Request = _Requests[Index];
            Result Out;
            Out.Index = Index;
            try {
                if (Request.Kind == AssetKind::Texture) Out.Texture = _Textures->DecodeAsset(Request.ID, Request.Srgb);
                else Out.Mesh = _Meshes->DecodeAsset(Request.ID);
            } catch (...) { Out.Error = std::current_exception(); }

            {
                std::lock_guard Lock(_Mutex);
                --_Decoding;
                _Ready.push_back(std::move(Out));
            }
        }
    }

    bool AssetLoader::Pump(const f64 BudgetMs) {
        if (!_Active) return true;

        const auto Start = std::chrono::steady_clock::now();

        while (_Done < _Requests.size()) {
            Result Next;
            {
                std::lock_guard Lock(_Mutex);
                if (_Ready.empty()) break;
                Next = std::move(_Ready.front());
                _Ready.pop_front();
            }
            // A slot just freed up for the workers.
            _WorkerWake.notify_all();

            if (Next.Error) {
                Cancel();
                std::rethrow_exception(Next.Error);
            }

            const LoadRequest& Request = _Requests[Next.Index];
            if (Request.Kind == AssetKind::Texture) _Textures->AdoptPreloaded(Request.ID, std::move(Next.Texture));
            else _Meshes->AdoptPreloaded(Request.ID, std::move(Next.Mesh));
            ++_Done;

            const f64 Elapsed = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - Start).count();
            if (Elapsed >= BudgetMs) break;
        }

        if (_Done < _Requests.size()) return false;

        Cancel();  // everything's in - just joins the now-idle workers
        return true;
    }

    void AssetLoader::Cancel() {
        {
            std::lock_guard Lock(_Mutex);
            _Cancelled = true;
        }
        _WorkerWake.notify_all();

        for (std::thread& Worker : _Workers) {
            if (Worker.joinable()) Worker.join();
        }
        _Workers.clear();

        {
            std::lock_guard Lock(_Mutex);
            _Ready.clear();
        }

        // _Done/_Requests are left as they are, so Done()/Total() still read
        // right after a normal finish; only "is anything still running" goes.
        _Active = false;
    }
}  // namespace Xen
