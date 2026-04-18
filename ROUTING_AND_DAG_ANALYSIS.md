# Bass Studio: Track Routing, Effect Routing, and DAG Transformation

## Overview

Bass Studio implements a sophisticated audio routing system that manages both **track-level routing** (how tracks connect to each other) and **effect-level routing** (how effects are chained within a track). The core architecture transforms routing information into a **Directed Acyclic Graph (DAG)** for efficient processing order determination and latency compensation.

---

## 1. Routing Information Storage in Project Structure

### 1.1 Project-Level Data Structures

#### Track I/O Configuration (`track_io_configuration_snapshot_t`)
Located in [src/snapshot/trackrouting-snapshot.hpp](src/snapshot/trackrouting-snapshot.hpp):

```cpp
struct io_configuration_snapshot_t {
    int32_t type                = 0;           // INPUT_EXTERNAL_AUDIO, INPUT_AUDIOSTAGE, INPUT_AUDIOSTAGE_EFFECT
    int32_t stageId             = -1;          // Target audio stage ID
    int32_t stageEndPointType   = 0;           // stage_bufferpoint::INPUT, OUTPUT_POST, etc.
    int32_t externalInputType   = 0;           // STEREO, MONO_LEFT, etc.
    int32_t projectGlobalId     = 0;           // Effect plugin ID (for effect connections)
    int32_t externalInputIdx    = 0;           // Audio device input index
    int32_t srcChannelOffset    = 0;           // Source channel offset
    int32_t dstChannelOffset    = 0;           // Destination channel offset
};

struct track_io_configuration_snapshot_t {
    io_configuration_snapshot_t input;                  // Audio input routing
    io_configuration_snapshot_t output;                 // Audio output routing
    std::vector<io_midi_snapshot_t> midiInputs;        // MIDI input routings (multiple inputs)
    io_midi_snapshot_t midiOutput;                      // MIDI output routing
};
```

#### Effect Routing Configuration (`track_effect_routing_snapshot_t`)
```cpp
struct track_effect_routing_snapshot_t {
    int32_t routingState = 0;                              // DEFAULT or CUSTOM
    std::vector<io_configuration_snapshot_t> inputRoutingOutputStage;  // Post-effect output routing
    std::map<int32_t, std::vector<io_configuration_snapshot_t>> inputRoutingEffects;  // Per-effect input routings
};
```

### 1.2 Runtime Audio Stage Structure

The runtime representation is in [src/host/track/track_impl.hpp](src/host/track/track_impl.hpp):

```cpp
struct audio_stage_t : public IDelayLineStorage {
    audio_stage_id_t stageId{};                          // Multi-part ID for this stage
    audiostageflags_t flags = audiostageflags_t::NONE;   // Solo, solo-parent, etc.
    audiostagerouting_state_t routingState;              // DEFAULT or CUSTOM
    
    audiotrack_t audioInput;                             // Input buffer
    audiotrack_t audioOutput;                            // Output pre-effect buffer
    AudioBlock outputPost;                               // Post-effect output buffer
    
    std::vector<effectbase*> effects;                    // Effect chain
    std::vector<DAW::channel_ref_t> postEffectRouting;   // Routing after all effects
    std::vector<audio_stage_t*> children;                // Child audio stages (for hierarchy)
    std::shared_ptr<DAW::effect_processing_graph_t> processingGraph;
    
    samplecount_t latencyInput = 0;                      // Input latency from dependencies
    samplecount_t latencyInternal = 0;                   // Internal latency (effects + plugins)
    samplecount_t latencyOuput = 0;                      // Total output latency
};
```

### 1.3 Channel Reference Types

Defined in [src/host/daw_channel.hpp](src/host/daw_channel.hpp):

```cpp
struct channel_ref_t {
    stage_type type;                      // INPUT_EXTERNAL_AUDIO, INPUT_AUDIOSTAGE, INPUT_AUDIOSTAGE_EFFECT
    channel_pairing externalInputType;    // For external I/O: STEREO, MONO_LEFT, MONO_RIGHT
    audio_channel_ref_t stage;            // Target stage and buffer endpoint
    int32_t projectGlobalId;              // Effect ID if connecting to effect
    channelnum_t externalInputIdx;        // Device index if external
    channelnum_t srcChannelOffset;        // Source channel offset (for submix routing)
    channelnum_t dstChannelOffset;        // Destination channel offset
    String name;                          // Debug name
};
```

### 1.4 Persistence: Serialization/Deserialization

[src/file/projectfile-v1.cpp](src/file/projectfile-v1.cpp) handles project file I/O:

```cpp
// Create snapshot (DAW → disk)
void audio_stage_t::createRoutingSnapshot(track_effect_routing_snapshot_t& snapshot) {
    for (auto& channel : this->postEffectRouting) {
        io_configuration_snapshot_t cfg;
        createDawChannelRefSnapshot(channel, cfg);  // Convert runtime to serializable form
        snapshot.inputRoutingOutputStage.push_back(cfg);
    }
    for (effectbase* effect : effects) {
        auto& vec = snapshot.inputRoutingEffects[static_cast<int32_t>(effect->projectGlobalId)];
        for (auto& channel : effect->inputChannels) {
            io_configuration_snapshot_t cfg;
            createDawChannelRefSnapshot(channel, cfg);
            vec.push_back(cfg);
        }
    }
    snapshot.routingState = static_cast<int32_t>(this->routingState);
}

// Load snapshot (disk → DAW)
void audio_stage_t::loadRoutingSnapshot(const track_effect_routing_snapshot_t& snapshot) {
    this->postEffectRouting.clear();
    for (const auto& cfg : snapshot.inputRoutingOutputStage) {
        DAW::channel_ref_t channel;
        loadDawChannelRefSnapshot(cfg, channel);
        this->postEffectRouting.push_back(channel);
    }
    for (const auto& mapEntry : snapshot.inputRoutingEffects) {
        auto* plugin = getPluginById(mapEntry.first);
        if (plugin) {
            plugin->inputChannels.clear();
            for (const io_configuration_snapshot_t& effInputSnapshot : mapEntry.second) {
                DAW::channel_ref_t channel;
                loadDawChannelRefSnapshot(effInputSnapshot, channel);
                plugin->inputChannels.push_back(channel);
            }
        }
    }
    this->routingState = static_cast<audiostagerouting_state_t>(snapshot.routingState);
}
```

---

## 2. Routing States and Routing Modes

### 2.1 Routing States

The `audiostagerouting_state_t` enum defines two routing modes:

- **DEFAULT**: Standard effect chain routing (effects process sequentially, output goes to post-effect routing)
- **CUSTOM**: Advanced routing with manual effect input/output configuration per effect

### 2.2 Track I/O Connection Types

Tracks can connect to:

1. **INPUT_EXTERNAL_AUDIO** - External audio device input
2. **INPUT_AUDIOSTAGE** - Another track's audio stage (audio routing)
3. **INPUT_AUDIOSTAGE_EFFECT** - Effect plugin output (for sub-mixing)
4. **INPUT_DEFAULT** - Default routing (resolved at runtime)

Effects can additionally connect to:

5. **MIDI inputs/outputs** - MIDI data flow between tracks

---

## 3. DAG Construction Process

### 3.1 Two-Level Graph Architecture

Bass Studio uses **two separate DAG systems**:

#### Level 1: Track Routing Graph (`track_graph_t`)
High-level graph showing how tracks depend on each other.

**Location**: [src/host/graph/track_graph.hpp](src/host/graph/track_graph.hpp)

```cpp
struct track_graph_t {
    DAW::SegmentedVector<track_node_t, 32> memPoolTrackNodes;  // Memory pool for nodes
    std::vector<track_node_t*> roots;                          // Output nodes (Master, submix buses)
    std::vector<track_node_ptr> nodes;                         // All nodes in this graph
    std::vector<track_source_t> externalOutputRouting;         // Hardware outputs
    samplecount_t maxLatencySamples = 0;                       // Max path latency
};
```

#### Level 2: Effect Routing Graph (`effect_graph_t`)
Per-track graph showing how effects are chained within a track.

```cpp
using effect_node_t              = track_node_t;              // Same structure as track nodes
using effect_graph_t             = track_graph_t;             // Same structure as track graph
```

**Effect nodes represent**:
- **AUDIOSTAGE** nodes: Track input/output stage boundaries
- **EFFECT** nodes: Individual effect plugins

### 3.2 Node Data Structure

Defined in [src/host/graph/track_graph.hpp](src/host/graph/track_graph.hpp):

```cpp
struct track_node_t {
    track_node_type_t type        = track_node_type_t::TRACK;     // TRACK, AUDIOSTAGE, or EFFECT
    audiostageid_i32 stageId      = TRACKID_INVALID_I32;          // Unique stage ID
    std::vector<audiostageid_i32> dependencies;                   // IDs of dependencies (children in DAG)
    std::vector<track_source_t> pulls;                            // Audio pulled from dependencies
    std::vector<track_source_t> pushs;                            // Audio pushed to dependents
    std::vector<track_node_t*> parents;                           // Pointers to parents
    std::vector<track_node_t*> children;                          // Pointers to children
    samplecount_t internalLatency = INVALID_SAMPLE_OFFSET_U32;    // Latency added by this node
    samplecount_t inputLatency    = INVALID_SAMPLE_OFFSET_U32;    // Latency from dependencies
};

struct track_source_t {
    uint32_t trackEdgeId = 0;                     // Unique edge ID
    channel_ref_t channel{};                      // Connection details
    automation_routing_t gainAutomation{};        // Send gain automation (if applicable)
    automation_routing_t panAutomation{};         // Send pan automation
    samplerate_t latency = 0U;                    // Latency from source
    audiostageflags_t flags;                      // Source stage flags
};
```

### 3.3 Building Track Routing Graph

**Function**: `buildTrackRoutingGraph()` in [src/host/graph/track_graph.cpp](src/host/graph/track_graph.cpp)

**Algorithm**:

```
1. For each track in the project:
   a. Create a node for the track's main stage
   b. Check audio INPUT connection:
      - If connects to another track → add dependency, create pull entry
      - If external audio → add pull entry
   c. Check audio OUTPUT connection:
      - If connects to another track → add parent-child relationship
      - If external audio → add to externalOutputRouting
   d. Check MIDI INPUT connections (creates dependencies)
   e. Check SEND routings to return tracks

2. For each node:
   a. Check for feedback loops using depth-first traversal
   b. Mark nodes with no parents as roots (output nodes)
   c. Deduplicate children/parents/dependencies

3. Return graph with populated nodes and roots
```

**Key Entry Point** ([track_graph.cpp](src/host/graph/track_graph.cpp) line ~560):

```cpp
bool buildTrackRoutingGraph(const Host::Host* const host, const project_t* const project, 
                            const track_vector& tracksFlat, std::shared_ptr<track_graph_t>& out_graph) {
    // Create nodes for each track
    for (track_t* track : tracksFlat) {
        auto& trackCfg = makeOrGetTrackNode(trackImpl->stageId.stageId);
        
        // Build dependencies from connections
        if (isChannelConnected(inputChannel)) {
            if (inputChannel.getType() == stage_type::INPUT_AUDIOSTAGE) {
                // Pull from another track
                trackCfg.dependencies.push_back(srcStageId);
                trackCfg.pulls.push_back(track_source_t{...});
                // ... establish parent-child pointers
            }
        }
    }
    
    // Validate no feedback loops
    for (track_node_ptr node : nodes) {
        if (hasFeedbackLoop(node, unresolved)) {
            return false;  // Cycle detected
        }
    }
}
```

### 3.4 Building Effect Routing Graph (Per-Track)

**Function**: `buildEffectRoutingGraph()` in [src/host/graph/effect_graph.cpp](src/host/graph/effect_graph.cpp)

**Algorithm** (for a single track's effects):

```
1. Create AUDIOSTAGE nodes for track input and output stages
2. For each effect plugin on the track:
   a. Create an EFFECT node with plugin latency
   b. For each input channel of the effect:
      - If default connection → resolve default destination
      - Create pull entries and dependencies
      - Connect from:
        * Another effect's output
        * Track input stage
        * External audio input

3. Create post-effect output node:
   a. Determine what the track output connects to
   b. Link all effects that feed into output

4. Topological sort and cycle detection
```

**Example Graph Structure** (Default routing):

```
Track Input Stage
    ↓
[Effect 1] ← pulls from Track Input
    ↓
[Effect 2] ← pulls from Effect 1
    ↓
[Effect 3] ← pulls from Effect 2
    ↓
Track Output Stage
```

**Custom Routing** (more flexible):

```
Track Input Stage
    ↓              ↓
[Effect 1]    [Effect 2]
    ↓              ↓
    Track Output Stage
```

### 3.5 Cycle Detection

**Function**: `hasFeedbackLoop()` in [track_graph.cpp](src/host/graph/track_graph.cpp#L175)

```cpp
bool hasFeedbackLoop(const track_node_ptr node, std::vector<track_node_ptr>& unresolved) {
    unresolved.push_back(node);
    for (auto child : node->children) {
        if (STL_CONTAINS(unresolved, child)) {
            return true;  // Found a cycle!
        }
        if (hasFeedbackLoop(child, unresolved)) {
            return true;
        }
    }
    removeEntry(unresolved, node);
    return false;
}
```

**DFS Algorithm**:
- Maintains `unresolved` stack (currently visiting)
- If child is in `unresolved` → cycle detected
- Backtracks when node fully explored

---

## 4. Processing Graph Generation

Processing graphs convert static DAGs into actionable processing order with latency info.

### 4.1 Processing Graph Structure

```cpp
struct processing_track_node_t : public track_node_t {
    track_t* trackOptional             = nullptr;  // Pointer to actual track
    effectbase* effectOptional         = nullptr;  // Pointer to effect plugin (for effect nodes)
    audio_stage_t* stage               = nullptr;  // Pointer to audio stage
    processing_track_node_state_t state = UNPROCESSED;  // Processing state
};

struct processing_graph_t {
    DAW::SegmentedVector<processing_track_node_t, 32> memPoolProcNodes;
    std::vector<processing_track_node_t*> nodesFlatOrdered;  // Processing order
    std::vector<processing_track_node_t*> roots;            // Nodes with no parents
    std::vector<processing_track_node_ptr> nodes;
    std::shared_ptr<track_graph_t> trackGraph;
    samplecount_t maxLatencySamples = 0;
};
```

### 4.2 Building Processing Graph

**Function**: `buildProcessingGraphFromRoutingGraph()` in [track_graph.cpp](src/host/graph/track_graph.cpp#L233)

**Algorithm**:

```
1. Convert routing graph nodes to processing nodes:
   a. Add pointers to actual track/effect/stage objects
   b. Set node type (TRACK, AUDIOSTAGE, EFFECT)
   c. Copy dependencies and children/parents

2. Dependency resolution (topological sort):
   a. Perform depth-first traversal from root nodes
   b. Order nodes such that dependencies come before dependents
   c. Build nodesFlatOrdered in correct processing sequence

3. Latency propagation (bottom-up):
   a. Start with leaf nodes (no children)
   b. For each node: inputLatency = max(child.inputLatency + child.internalLatency)
   c. Propagate upward to roots
   d. Update track stages with: latencyInput, latencyInternal, latencyOutput
   e. Calculate global maxLatencySamples
```

**Latency Calculation Detail**:

```cpp
for (auto const ptrNode : graph.nodesFlatOrdered) {
    ptrNode->inputLatency = 0;
    for (const auto nodeIdx : ptrNode->dependencies) {
        const auto ptrChNode = map.at(nodeIdx);
        ptrNode->inputLatency = std::max(ptrNode->inputLatency, 
                                        ptrChNode->inputLatency + ptrChNode->internalLatency);
    }
    maxLatency = std::max(maxLatency, ptrNode->inputLatency + ptrNode->internalLatency);
}
```

### 4.3 Output: Processing Order

After processing graph generation, nodes in `nodesFlatOrdered` are in dependency order:

- **Leaf nodes** processed first (inputs from external sources)
- **Intermediate nodes** after their dependencies
- **Root nodes** (Master, submix outputs) processed last

---

## 5. Validation and Error Handling

### 5.1 Input Validation

**Function**: `validateTrackRoutings()` in [track_graph.cpp](src/host/graph/track_graph.cpp)

Validates:
- MIDI input channels connect to valid stages
- Audio input/output channels reference existing stages
- Default connections can be resolved
- No stale references to deleted stages

**Function**: `validateEffectRoutings()` in [effect_graph.cpp](src/host/graph/effect_graph.cpp)

Validates:
- Effect input channels reference valid stages/effects
- Post-effect routing connects to valid destinations

### 5.2 Cycle Detection Validation

**Occurs at**:
- Track routing graph building
- Effect routing graph building (per-track)

**Behavior on cycle**:
- Error logged: `"unexpected: dependecy index >= this index!!"`
- `buildProcessingGraph()` returns `false`
- Processing skipped; user sees disabled effects/routing

### 5.3 Debug Assertions

Compile-time assertion at optimized builds (`VALIDATE_GRAPH_CORRECTNESS`):

```cpp
#ifdef VALIDATE_GRAPH_CORRECTNESS
    // Verify dependency ordering
    for (auto itStageIdx = graph.nodesFlatOrdered.begin(); 
         itStageIdx < graph.nodesFlatOrdered.end(); ++itStageIdx) {
        for (auto depNodeIdx : trackNode->dependencies) {
            auto itDependency = std::find_if(...);
            dbgassert(itDependency < itStageIdx);  // Dependency must come before this node
        }
    }
    
    // Verify latency calculations
    for (auto const trNodeChild : ptrNode->children) {
        dbgassert(ptrNode->inputLatency >= trNodeChild->inputLatency + trNodeChild->internalLatency);
    }
#endif
```

---

## 6. Practical Routing Examples

### Example 1: Simple Linear Routing

**Setup**:
- Track 1 (Drums) → Master
- Track 2 (Bass) → Master

**Resulting DAG**:

```
Drums ─┐
       ├─→ Master
Bass ──┘

nodesFlatOrdered: [Drums, Bass, Master]
```

### Example 2: Submix with Effect Send

**Setup**:
- Track 1 (Vocal) → output: Vocal Bus
- Vocal Bus → Master
- Vocal Bus has EQ effect

**Resulting DAG (Track level)**:

```
Vocal ────→ Vocal Bus ────→ Master
```

**Resulting DAG (Vocal Bus effect level)**:

```
Vocal Input Stage
    ↓
[EQ Effect]
    ↓
Vocal Output Stage (to Master)
```

### Example 3: Custom Effect Routing

**Setup**:
- Track with 3 effects in custom routing:
  - Effect A pulls from input
  - Effect B pulls from input (parallel)
  - Effect C pulls from A and B (parallel mix)

**Resulting DAG**:

```
Track Input
    ├─→ [Effect A] ─┐
    │               ├─→ [Effect C] ─→ Output
    └─→ [Effect B] ─┤
```

---

## 7. Critical Sections for Audio Engine

### 7.1 Graph Rebuilding

Graphs must be rebuilt when:
- Track added/removed
- Track routing changed
- Effect added/removed/reordered
- Effect input routing changed
- Project loaded

**Function**: `Host::onPluginsChanged()` (in [host.cpp](src/host/host.cpp))

```cpp
void onPluginsChanged() {
    // Rebuild track graphs
    buildProcessingGraph(...);
    
    // Update solo flags based on new graph
    updateSoloFlag(...);
    
    // Rebuild solo graph if needed
    if (soloActive) {
        buildProcessingGraphSolo(...);
    }
}
```

### 7.2 Solo Processing

Solo mode uses filtered processing graph:

**Function**: `buildProcessingGraphSolo()` in [track_graph.cpp](src/host/graph/track_graph.cpp#L371)

- Filters roots to only soloed tracks
- Rebuilds processing order for only soloed + upstream dependencies
- Ancestors of soloed tracks marked with `SOLO_PARENT` flag

### 7.3 Processing Dispatch

During audio block processing, nodes from `processing_graph_t::nodesFlatOrdered` are processed in order:

```cpp
for (auto node : processingGraph->nodesFlatOrdered) {
    // Process audio through node
    if (node->trackOptional) {
        processTrack(node->trackOptional);
    } else if (node->effectOptional) {
        processEffect(node->effectOptional);
    }
}
```

---

## 8. Known Issues and Limitations

From [TODO-2026.md](TODO-2026.md):

1. ⚠️ **Custom routing unstable after project reload** - graph rebuild corrupts some connections
2. ⚠️ **Cycles not fully prevented** - some invalid routings slip through validation
3. ⚠️ **GPU synth latency compensation** incorrect vs tape-based effects

---

## 9. Data Flow Summary

```
┌─────────────────────────────────────────────────────────────────┐
│ Project File (JSON/Archive)                                     │
│ ├─ track_io_configuration_snapshot_t (audio routing)           │
│ ├─ track_effect_routing_snapshot_t (effect routing)            │
│ └─ track_modulation_routing_snapshot_t (effect modulations)    │
└─────────────────────✓ deserialize ────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│ Runtime Structures                                              │
│ ├─ track_impl_t (inputChannel, outputChannel, midiInputChannels)
│ ├─ audio_stage_t (effects[], postEffectRouting[])             │
│ └─ effectbase (inputChannels[])                               │
└─────────────────────────────────────────────────────────────────┘
                           ↓
                    buildTrackRoutingGraph()
                    + buildEffectRoutingGraph()
                    (per-track)
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│ Static Routing Graphs                                           │
│ ├─ track_graph_t (track-level DAG)                            │
│ └─ effect_graph_t (per-track effect DAG)                      │
│    ├─ nodes (track_node_t[])                                   │
│    ├─ roots (output nodes)                                     │
│    └─ maxLatencySamples                                        │
└─────────────────────────────────────────────────────────────────┘
                           ↓
           buildProcessingGraphFromRoutingGraph()
                           ↓
┌─────────────────────────────────────────────────────────────────┐
│ Processing Graphs (executable)                                  │
│ ├─ processing_graph_t (track-level)                           │
│ ├─ processing_effect_graph_t (per-track effect)               │
│    ├─ nodesFlatOrdered (topologically sorted)                 │
│    ├─ roots (no parents)                                      │
│    ├─ latency info (per-node input/internal/output)           │
│    └─ maxLatencySamples (total compensation needed)           │
└─────────────────────────────────────────────────────────────────┘
                           ↓
                   Audio Loop Processing
            (process nodes in nodesFlatOrdered order)
                           ↓
              Audio output to hardware or file
```

---

## Key Files Reference

| File | Purpose |
|------|---------|
| [src/host/graph/track_graph.hpp](src/host/graph/track_graph.hpp) | Node/graph definitions |
| [src/host/graph/track_graph.cpp](src/host/graph/track_graph.cpp) | Track DAG building & processing |
| [src/host/graph/effect_graph.hpp](src/host/graph/effect_graph.hpp) | Effect graph type aliases |
| [src/host/graph/effect_graph.cpp](src/host/graph/effect_graph.cpp) | Effect DAG building & validation |
| [src/host/track/track_impl.hpp](src/host/track/track_impl.hpp) | Runtime audio stage structure |
| [src/host/track/track.cpp](src/host/track/track.cpp) | Routing snapshot save/load |
| [src/host/daw_channel.hpp](src/host/daw_channel.hpp) | Channel reference types |
| [src/snapshot/trackrouting-snapshot.hpp](src/snapshot/trackrouting-snapshot.hpp) | Serializable snapshot structures |
| [src/file/projectfile-v1.cpp](src/file/projectfile-v1.cpp) | Project file serialization |
| [src/host/host.hpp](src/host/host.hpp) | Host main API |
| [src/gui/track/trackctr_nodes.cpp](src/gui/track/trackctr_nodes.cpp) | GUI routing editor integration |

