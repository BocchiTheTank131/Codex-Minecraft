# Voxel Frontier Code Review and Architecture Audit

**Target Version:** v2.20.0 (*Villages & Living World*)  
**Architecture:** C++17, OpenGL 3.3 Core, GLFW, GLAD, CMake  
**Date:** October 2026  
**Audit Scope:** Concurrency, Persistence & Snapshots, Chunk Streaming, World Generation, NPC & Village AI, Physics & Movement, Rendering, Memory/Resource Lifetimes, Input Handling.

---

## 1. Executive Summary & Architecture Map

Voxel Frontier is an optimized, deterministic voxel game engine featuring immutable snapshot saving, background chunk streaming, procedural generation (frozen generators V1–V24), NPC village simulations, and an OpenGL 3.3 renderer.

### Subsystem Map

| Subsystem | Key Source Files | Architecture & Concurrency Model |
| :--- | :--- | :--- |
| **Persistence & Checkpoints** | [`src/SaveFile.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/SaveFile.cpp), [`src/SaveSnapshot.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/SaveSnapshot.cpp), [`include/SaveFile.h`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/include/SaveFile.h) | Two-phase atomic checkpoint commit with `.checkpoint-old` rotation, CRC64 manifest verification (`VXMAN1`), named mutex inter-process protection (`Local\VoxelFrontierSave-<hash>`), and immutable snapshot staging. |
| **Streaming & Multithreading** | [`src/WorldStreaming.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/WorldStreaming.cpp), [`src/ChunkStreaming.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/ChunkStreaming.cpp), [`include/World.h`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/include/World.h) | Producer-consumer thread pools for terrain generation (`generationWorkers_`) and mesh building (`meshWorkers_`). Uses revision tokens (`std::atomic<uint64_t>`) for stale mesh cancellation, thread-safe recycling queues, and explicit worker budgets. |
| **Terrain & Structures** | [`src/World.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/World.cpp), [`src/Structure.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/Structure.cpp), [`src/UndergroundStructures.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/UndergroundStructures.cpp) | Frozen deterministic procedural generators (V1–V24). Regional structure planning (villages, outposts, pyramids) with spatial caching (`majorCache_`). |
| **NPC & Village Simulation** | [`src/Villages.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/Villages.cpp), [`src/VillageBehavior.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/VillageBehavior.cpp), [`include/Villages.h`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/include/Villages.h) | State-driven daily schedules (work, gather, rest, flee), spatial door/road pathing, economic trade limits, community request rotation, and project construction. |
| **Physics & Player Controller** | [`src/Player.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/Player.cpp), [`src/Survival.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/Survival.cpp), [`src/Transportation.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/Transportation.cpp) | Swept-AABB axis-separated continuous collision resolution with sub-stepping, step-up assists for slabs/stairs, water buoyancy, and boat physics. |
| **Machinery & Automation** | [`src/Automation.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/Automation.cpp), [`src/Building.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/Building.cpp) | Deterministic logic simulation: power propagation, repeaters, logic gates, hoppers, and pistons. Strictly bounded tick budgets (2048 evaluations, 512 commits). |
| **Rendering & Geometry** | [`src/Renderer.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/Renderer.cpp), [`src/TerrainVertex.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/TerrainVertex.cpp), [`src/TerrainOcclusion.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/TerrainOcclusion.cpp) | OpenGL 3.3 Core profile. Bit-packed terrain vertices (32 bytes reduced to 24 bytes), multi-sampled FBOs, hardware occlusion queries with temporal coherence, and atlas-mapped billboards. |
| **Game Loop & UI** | [`src/Game.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/Game.cpp), [`src/InputManager.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/InputManager.cpp), [`src/InventoryUI.cpp`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/InventoryUI.cpp) | Fixed/variable time-stepped simulation, GLFW input polling, inventory transfer transactions, and modal UI routing. |

---

## 2. Ranked Findings

### High Severity Findings

#### Finding 1: Cross-Thread Mutex Contention in `Checkpoint::commitAsync`
- **Classification:** Confirmed Bug (Concurrency Race)
- **File & Lines:** [`src/SaveFile.cpp:301-309`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/SaveFile.cpp#L301-L309)
- **Root Cause:** In [`src/SaveFile.cpp:287`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/SaveFile.cpp#L287), the `Checkpoint::Impl` constructor acquires the named mutex `Local\VoxelFrontierSave-<hash>` via `writer(journal)`. The `Checkpoint::commitAsync` method marks `impl_->committed = true` and launches an asynchronous worker thread using `std::async(std::launch::async, ...)`:
  ```cpp
  return std::async(std::launch::async,[journal=impl_->journal,entries=std::move(impl_->entries),status]()mutable{
      Checkpoint checkpoint(journal);
      checkpoint.impl_->entries=std::move(entries);
      if(status){status->permanentFailure=checkpoint.permanentFailure();status->reason=checkpoint.failureReason();}
      return checkpoint.commit();
  });
  ```
  The worker constructs a new `Checkpoint(journal)`, which invokes `WriterLock writer(journal)`. In [`src/SaveFile.cpp:109`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/SaveFile.cpp#L109), `WriterLock` performs a non-blocking mutex acquisition with zero timeout:
  ```cpp
  const auto state=handle?WaitForSingleObject(handle,0):WAIT_FAILED;
  acquired=state==WAIT_OBJECT_0||state==WAIT_ABANDONED;
  ```
  Because Win32 Mutex ownership is per-thread, the calling thread still holds the mutex until the caller destroys its `Checkpoint` instance. If the asynchronous worker thread begins executing before the caller destructs its `Checkpoint`, `WaitForSingleObject` fails immediately with `WAIT_TIMEOUT` (0 ms). Consequently, `checkpoint.ready()` evaluates to `false`, `failureReason()` is set to `"Save writer is busy"`, and the asynchronous commit aborts.
- **Impact:** Asynchronous checkpoint commits initiated via `Checkpoint::commitAsync` spuriously fail with `"Save writer is busy"`. (Note: The engine's higher-level `Game::captureCheckpointAsync` relies on `commitSnapshotAsync`, which avoids this by not constructing a `Checkpoint` prior to worker launch; however, `Checkpoint::commitAsync` remains broken in the public API).
- **Evidence:** Tested with caller holding `Checkpoint` reference; worker returns `false` with `reason = "Save writer is busy"`.
- **Recommended Fix:** Transfer or release ownership of `impl_->writer` before scheduling the asynchronous task, or pass the existing lock/journal context directly into the background worker instead of re-instantiating `Checkpoint`:
  ```cpp
  std::future<bool> Checkpoint::commitAsync(std::shared_ptr<CommitStatus> status) {
      if(!impl_->ready || impl_->committed) { /* handle error */ }
      impl_->committed = true;
      auto impl = std::move(impl_);
      return std::async(std::launch::async, [impl = std::move(impl), status]() mutable {
          Checkpoint checkpoint(std::move(impl));
          return checkpoint.commit();
      });
  }
  ```
- **Verification Method:** Create a unit test in `tests/stability/` that instantiates `Checkpoint batch(journal)`, queues entries, invokes `auto f = batch.commitAsync()`, and waits on `f.get()` before destroying `batch`. Verify that the commit succeeds rather than failing with `"Save writer is busy"`.

---

#### Finding 2: Cold-Cache Failure for Village Plans and Public Works on World Reload
- **Classification:** Confirmed Bug (Game State / AI Inconsistency)
- **File & Lines:** [`src/VillageBehavior.cpp:30-35`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/VillageBehavior.cpp#L30-L35), [`src/Villages.cpp:48, 101`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/Villages.cpp#L48), [`src/Structure.cpp:1047`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/Structure.cpp#L1047)
- **Root Cause:** [`World::cachedVillageNear`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/VillageBehavior.cpp#L30) searches for regional village plans by invoking `structures_.peekMajorPlan(rx + x, rz + z)`. In [`src/Structure.cpp:1047`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/Structure.cpp#L1047):
  ```cpp
  std::shared_ptr<const StructurePlan> StructureGenerator::peekMajorPlan(int x,int z)const{
      std::lock_guard<std::mutex> lock(cacheMutex_);
      auto i=majorCache_.find(std::int64_t((std::uint64_t(std::uint32_t(x))<<32)|std::uint32_t(z)));
      return i==majorCache_.end()?nullptr:i->second;
  }
  ```
  `peekMajorPlan` only inspects `majorCache_` and never computes missing plans. When a saved world is loaded from disk, chunks are loaded directly from disk files (`.vxw`). The world generation stage (`StructureGenerator::applyToChunk`) is never executed for existing chunks. As a result, `majorCache_` remains completely empty (cold).
- **Impact:**
  1. **NPC Binding Failure:** When an unbound villager (`villagerId == 0`) attempts to bind via `Villages::bind`, `cachedVillageNear` returns `nullptr`. The villager is permanently relegated to an "Independent Traveler" with no assigned home village, generic merchant trade tables, and no access to village requests or public works.
  2. **Public Works Project Block:** In [`src/Villages.cpp:101`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/Villages.cpp#L101), `projectCells` calls `w.cachedVillageNear(v.center)`. Because the cache is empty, it returns empty edits. `Villages::projectSite` reports `"SITE BLOCKED OR UNLOADED / FUNDING IS REFUNDABLE"`, and `Villages::construct` rejects construction with `"No loaded, clear approved site / funding retained"`.
  3. **Pathfinding Degradation:** In [`src/VillageBehavior.cpp:47`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/VillageBehavior.cpp#L47), doorway and road routing fails when `cachedVillageNear` is null, falling back to direct line-of-sight walking into exterior walls.
  4. **Test Masking:** The test harness in [`tests/stability/village.cpp:29`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/tests/stability/village.cpp#L29) artificially primed the cache by explicitly executing `w.structures_.majorPlanForRegion(x, z, terrain())` prior to running test assertions, which masked this bug in automated test runs.
- **Recommended Fix:** If `peekMajorPlan` returns `nullptr` in `cachedVillageNear`, fallback to computing and caching the plan on-demand using the deterministic terrain sampler:
  ```cpp
  std::shared_ptr<const StructurePlan> World::cachedVillageNear(glm::ivec3 home) const {
      // ...
      auto p = structures_.peekMajorPlan(rx + x, rz + z);
      if (!p) {
          p = structures_.cachedMajorPlan(rx + x, rz + z, [this](int x, int z) {
              return sampleStructureTerrain(x, z);
          });
      }
      // ...
  }
  ```
- **Verification Method:** Save a world containing a populated village. Restart the application and reload the saved world directly without exploring new chunks. Verify that villagers bind to the village, `projectSite` locates the approved site, and public works construction can be completed.

---

### Medium Severity Findings

#### Finding 3: Uncaught Exception Risk in Background Meshing Worker (`World::meshWorkerLoop`)
- **Classification:** Confirmed Robustness Flaw / Crash Bug
- **File & Lines:** [`src/WorldStreaming.cpp:340`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/WorldStreaming.cpp#L340), [`src/TerrainVertex.cpp:46, 56`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/TerrainVertex.cpp#L46)
- **Root Cause:** [`TerrainPacking::pack`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/TerrainVertex.cpp#L43) validates precision guarantees on vertex attributes and throws `std::logic_error`:
  ```cpp
  if(normal==10) throw std::logic_error("Unsupported terrain normal: packing would lose precision");
  // ...
  if(sun==64 || light<0 || light>15 || sunBits()[light]!=bits(vertex.blockLight) ||
     ao<0 || ao>64 || float(ao)/64.f!=vertex.ao)
      throw std::logic_error("Unsupported terrain light/AO: packing would lose precision");
  ```
  In [`src/WorldStreaming.cpp:340`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/WorldStreaming.cpp#L340), `packMeshOutput(result)` is invoked directly inside `World::meshWorkerLoop` on a background `std::thread` without a `try ... catch` block.
- **Impact:** Any floating-point discrepancy, unexpected custom model normal, or corrupted lighting value throws an exception that escapes the top-level thread function, invoking `std::terminate()` and crashing the entire game without saving.
- **Recommended Fix:** Wrap `buildMesh` and `packMeshOutput` in `meshWorkerLoop` in a `try ... catch (const std::exception& e)` block. Log the error with chunk coordinates, safely drop the corrupted mesh, and decrement `meshWorkersBusy_`.
- **Verification Method:** Inject a synthetic vertex with `normal = glm::vec3(0.5f, 0.5f, 0.0f)` into a mesh build input and ensure the worker thread catches the error, logs diagnostic information, and keeps running.

---

#### Finding 4: Floating-Point Literal Mismatch in `Automation::update`
- **Classification:** Likely Bug (Subtle Timing Drift)
- **File & Lines:** [`src/Automation.cpp:128`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/Automation.cpp#L128)
- **Root Cause:** The automation tick loop operates on `float accumulator_`:
  ```cpp
  accumulator_+=dt;
  unsigned steps=0;
  while(accumulator_>=.1 && steps++<4){
      accumulator_-=.1;
      tick(w,mobs,player,std::clamp(std::isfinite(light)?light:0.f,0.f,1.f),sounds);
  }
  ```
  Comparing `float accumulator_` against `double` literal `.1` causes implicit conversions. In IEEE 754:
  - `float(0.1)` is `0.100000001490116119384765625`
  - `double(0.1)` is `0.100000000000000005551115123126...`
  Because `float(0.1) > double(0.1)`, subtracting `.1` (double arithmetic coerced back to float) introduces minor rounding drift over long simulation intervals.
- **Impact:** Minor tick scheduling jitter when catching up under non-standard delta times.
- **Recommended Fix:** Use explicit single-precision literals `0.1f`:
  ```cpp
  while(accumulator_ >= 0.1f && steps++ < 4) {
      accumulator_ -= 0.1f;
      // ...
  }
  ```
- **Verification Method:** Run static analysis and verify tick consistency over a multi-hour headless simulation run.

---

### Low Severity Findings

#### Finding 5: Invalid Enum Error Spammed Each Frame in `InputManager::pollEvents`
- **Classification:** Confirmed Bug (API Contract Violation)
- **File & Lines:** [`src/InputManager.cpp:59-61`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/InputManager.cpp#L59-L61)
- **Root Cause:** The keyboard polling loop queries GLFW keys starting from index 0:
  ```cpp
  for (int key = 0; key <= GLFW_KEY_LAST && key < KeyCount; ++key) {
      keys_[key] = glfwGetKey(window_, key) == GLFW_PRESS;
  }
  ```
  In GLFW 3, key values `0` through `31` do not exist (the lowest valid key enum is `GLFW_KEY_SPACE = 32`). According to GLFW specification:
  > *"If key is not a valid key, `GLFW_INVALID_ENUM` is generated."*
- **Impact:** Triggers 32 internal GLFW error events on every frame, generating unnecessary CPU overhead and polluting error callbacks if one is installed.
- **Recommended Fix:** Start the loop at `GLFW_KEY_SPACE`:
  ```cpp
  for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST && key < KeyCount; ++key) {
      keys_[key] = glfwGetKey(window_, key) == GLFW_PRESS;
  }
  ```
- **Verification Method:** Install a temporary `glfwSetErrorCallback` and run `pollEvents()`. Verify that errors cease once the loop starts at `GLFW_KEY_SPACE`.

---

#### Finding 6: Missing Bounds Guards in Building Meshing Slicing Arrays
- **Classification:** Speculative Risk (Defensive Boundary Check Missing)
- **File & Lines:** [`src/World.cpp:4409-4418`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/World.cpp#L4409-L4418)
- **Root Cause:** Slicing building geometry uses fixed stack arrays:
  ```cpp
  std::array<Rect,48> covers{};int nc=0;
  // ...
  covers[nc++] = ...;
  // ...
  std::array<float,98> cutsA{},cutsB{};int na=2,nb=2;
  for(int j=0;j<nc;++j){cutsA[na++]=covers[j].a;cutsA[na++]=covers[j].A;cutsB[nb++]=covers[j].b;cutsB[nb++]=covers[j].B;}
  ```
  `BuildingBoxes` supports up to 24 boxes (`BuildingBoxes::MaxBoxes = 24`). If a complex model with 24 boxes abuts another model with 24 boxes, `nc` can reach `23 + 24 = 47`, which fits within 48. However, there are no bounds assertions or checks before writing to `covers[nc++]`, `cutsA[na++]`, or `cutsB[nb++]`.
- **Impact:** Any future increase in `BuildingBoxes::MaxBoxes` or model complexity would cause an unchecked stack buffer overflow.
- **Recommended Fix:** Add defensive bounds assertions or saturation checks:
  ```cpp
  if (nc < covers.size()) covers[nc++] = ...;
  ```
- **Verification Method:** Audit with static analysis tools and unit tests with complex synthetic multi-box shapes.

---

#### Finding 7: OpenGL Shader Object Leak on Program Link Failure
- **Classification:** Low (Failure Path Cleanup)
- **File & Lines:** [`src/Renderer.cpp:804-807`, `src/Renderer.cpp:1073`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/Renderer.cpp#L804-L807)
- **Root Cause:** In `Renderer::Renderer()`, `makeProgram` compiles two shader objects `v` and `f`, then calls `linkProgram(v, f)`. If `linkProgram` fails, it throws `std::runtime_error("Shader linking failed: " + log)`. The subsequent calls to `glDeleteShader(v)` and `glDeleteShader(f)` are bypassed during stack unwinding.
- **Impact:** Compiled OpenGL shader objects remain allocated on the GPU driver when shader linking fails.
- **Recommended Fix:** Use RAII guards for shader handles or wrap `linkProgram` in `try ... catch` to call `glDeleteShader` before re-throwing.
- **Verification Method:** Intentionally pass malformed fragment shader code and verify shader object deletion in OpenGL debug output.

---

## 3. Architectural & Performance Improvements

1. **Eliminate Redundant Mutex Cycle in `World::requestChunksAround`**
   - **Location:** [`src/World.cpp:2050-2054`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/World.cpp#L2050-L2054)
   - **Observation:** `World::requestChunksAround` locks `generationMutex_`, checks `pendingKeys_.size() >= RefillThreshold`, releases the lock, and then immediately re-acquires `generationMutex_` on the very next line:
     ```cpp
     {
         std::lock_guard<std::mutex> lock(generationMutex_);
         if (pendingKeys_.size() >= RefillThreshold)
             return;
     }
     std::lock_guard<std::mutex> lock(generationMutex_);
     ```
   - **Recommendation:** Merge into a single lock scope to eliminate an unnecessary mutex release and re-acquisition cycle on the main simulation thread.

2. **Chunk Generation Queue Sort Overhead**
   - **Location:** [`src/World.cpp:2064`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/World.cpp#L2064)
   - **Observation:** When hundreds of chunks are queued during high-speed travel, `std::stable_sort` is invoked on the entire `generationQueue_` under `generationMutex_` each frame.
   - **Recommendation:** Sort chunks using a lightweight priority queue or bucketed distance rings upon insertion, rather than performing full array sorts inside the lock.

3. **GPU Occlusion Query Readback Pipelining**
   - **Location:** [`src/TerrainOcclusion.cpp:85-115`](file:///c:/Users/Win/Documents/Codex/Voxel/Codex-Minecraft/src/TerrainOcclusion.cpp#L85-L115)
   - **Observation:** Terrain occlusion query results are queried on the current frame. Under high GPU load, this can result in GPU-CPU pipeline stalls if the query result is not yet available.
   - **Recommendation:** Pipeline query results with an N-frame latency delay (using results from $N-1$ or $N-2$ frames ago) to ensure `GL_QUERY_RESULT_AVAILABLE` is always true without pipeline bubbles.

---

## 4. Suggested Regression Tests

1. **SaveFile Concurrency Stress Test (`tests/stability/persistence.cpp`)**
   - Test scenario: Launch an asynchronous checkpoint commit via `batch.commitAsync()` while keeping the `Checkpoint batch` instance in scope on the main thread.
   - Assertion: Commit must succeed without `"Save writer is busy"` errors.

2. **Cold-Cache Village & Public Works Verification (`tests/stability/village.cpp`)**
   - Test scenario: Create a test that loads a village directly from chunk and entity fixture files without calling `majorPlanForRegion` beforehand. Summon an unbound villager and issue public works project commands.
   - Assertion: `w.cachedVillageNear` must successfully resolve the village structure plan on demand, unbound villagers must associate with the village, and `Villages::construct` must accept construction on approved sites.

3. **Background Mesher Exception Containment (`tests/stability/streaming.cpp`)**
   - Test scenario: Feed an invalid vertex attribute (e.g. out-of-range light or unindexed normal) into the meshing worker pipeline.
   - Assertion: The worker thread must catch the exception, discard the erroneous chunk mesh, remain alive, and allow subsequent chunk meshes to process normally.

4. **Input Polling Specification Conformance (`tests/stability/input.cpp`)**
   - Test scenario: Register a GLFW error callback with `glfwSetErrorCallback` and execute `InputManager::pollEvents()`.
   - Assertion: No `GLFW_INVALID_ENUM` error events should be dispatched.

---

## 5. Audit Conclusion

The Voxel Frontier codebase exhibits strong engineering rigor:
- Zero dynamic allocations on per-frame hotpaths.
- Coherent immutable snapshots for background persistence.
- Strict inventory and metadata conservation guarantees.
- Byte-for-byte generator stability across historical versions.

The identified findings (notably the `Checkpoint::commitAsync` lock contention and the cold-cache village plan resolution) are actionable, verifiable, and can be addressed without compromising backward compatibility or saved world invariants.
