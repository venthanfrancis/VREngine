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
//
// Deliberately NOT here, until Vulkan (M8) reveals real requirements:
// pipelines/shaders (no PipelineHandle/PipelineDesc — see
// docs/ARCHITECTURE.md, "M4 Implementation Notes"), texture upload or
// content, and frame lifecycle/presentation (owned by Frame + Runtime —
// see "RHI Presentation").

#include "AREngine/Rendering/Handles.hpp"
#include "AREngine/Rendering/BufferDesc.hpp"
#include "AREngine/Rendering/TextureDesc.hpp"
#include "AREngine/Rendering/DrawCommand.hpp"
#include "AREngine/Rendering/RenderDevice.hpp"
#include "AREngine/Rendering/NullRenderDevice.hpp"
