#pragma once

// AREngine::Rendering — convenience umbrella header.
//
// The RHI (Render Hardware Interface): GPU rendering operations only,
// kept intentionally small. Depends only on Core. Public headers expose
// zero backend-specific types.
//
// Includes:
//   - Handles.hpp             BufferHandle, TextureHandle
//   - BufferDesc.hpp
//   - TextureDesc.hpp
//   - DrawCommand.hpp
//   - RenderDevice.hpp         the generic interface
//   - NullRenderDevice.hpp     the M4 backend + CreateNullRenderDevice()
//   - MeshData.hpp             CPU-side mesh geometry (M8H)
//   - ProceduralMesh.hpp       CreateQuadMesh/CreateCubeMesh (M8H)
//
// Deliberately NOT here, until Vulkan (M8) reveals real requirements:
// pipelines/shaders (no PipelineHandle/PipelineDesc — see
// docs/ARCHITECTURE.md, "M4 Implementation Notes"), texture upload or
// content, and frame lifecycle/presentation (owned by Frame + Runtime —
// see "RHI Presentation"). MeshData is a deliberate, evidence-based
// exception to "wait for Vulkan to reveal requirements" — see
// docs/ARCHITECTURE.md, "Generic RenderDevice Review (M8H)" for why
// Mesh itself still isn't a RenderDevice concept even though its CPU
// data now is generic.

#include "AREngine/Rendering/Handles.hpp"
#include "AREngine/Rendering/BufferDesc.hpp"
#include "AREngine/Rendering/TextureDesc.hpp"
#include "AREngine/Rendering/DrawCommand.hpp"
#include "AREngine/Rendering/RenderDevice.hpp"
#include "AREngine/Rendering/NullRenderDevice.hpp"
#include "AREngine/Rendering/MeshData.hpp"
#include "AREngine/Rendering/ProceduralMesh.hpp"
