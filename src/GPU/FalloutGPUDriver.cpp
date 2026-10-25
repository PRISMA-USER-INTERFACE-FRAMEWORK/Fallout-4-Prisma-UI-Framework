#include "FalloutGPUDriver.h"

#include <cstddef>
#include <cstring>

namespace PrismaUI::GPU {

    DXGI_FORMAT FalloutGPUDriver::TextureFormat(ultralight::BitmapFormat format) noexcept {
        switch (format) {
            case ultralight::BitmapFormat::BGRA8_UNORM_SRGB:
                return DXGI_FORMAT_B8G8R8A8_UNORM;
            case ultralight::BitmapFormat::A8_UNORM:
                return DXGI_FORMAT_A8_UNORM;
            default:
                return DXGI_FORMAT_UNKNOWN;
        }
    }

    void FalloutGPUDriver::CreateTexture(std::uint32_t textureId, ultralight::RefPtr<ultralight::Bitmap> bitmap) {
        if (!bitmap || textureId == 0) {
            FailCommandTransport();
            return;
        }
        AcceleratedGenerationOperation operation;
        operation.type = AcceleratedGenerationOperationType::CreateTexture;
        operation.id = textureId;
        operation.width = bitmap->width();
        operation.height = bitmap->height();
        operation.format = static_cast<std::uint32_t>(TextureFormat(bitmap->format()));
        operation.rowBytes = bitmap->row_bytes();
        operation.renderTarget = bitmap->IsEmpty();
        if (operation.width == 0 || operation.height == 0 || operation.format == DXGI_FORMAT_UNKNOWN) {
            FailCommandTransport();
            return;
        }
        if (!operation.renderTarget) {
            const std::size_t bytesPerPixel = operation.format == DXGI_FORMAT_A8_UNORM ? 1u : 4u;
            if (operation.width > (static_cast<std::size_t>(-1) / bytesPerPixel) ||
                operation.rowBytes < operation.width * bytesPerPixel ||
                operation.height > static_cast<std::size_t>(-1) / operation.rowBytes)
                {
                    FailCommandTransport();
                    return;
                }
            const auto size = static_cast<std::size_t>(operation.rowBytes) * operation.height;
            if (size != bitmap->size() || size > AcceleratedGenerationMailbox::kMaxPayloadBytes) {
                FailCommandTransport();
                return;
            }
            try {
                operation.bytes.resize(size);
            } catch (...) {
                FailCommandTransport();
                return;
            }
            const void* pixels = bitmap->LockPixels();
            if (!pixels) {
                FailCommandTransport();
                return;
            }
            std::memcpy(operation.bytes.data(), pixels, size);
            bitmap->UnlockPixels();
        }
        (void)StageResourceOperation(std::move(operation));
    }

    void FalloutGPUDriver::UpdateTexture(std::uint32_t textureId, ultralight::RefPtr<ultralight::Bitmap> bitmap) {
        if (!bitmap || textureId == 0 || bitmap->IsEmpty()) {
            FailCommandTransport();
            return;
        }
        AcceleratedGenerationOperation operation;
        operation.type = AcceleratedGenerationOperationType::UpdateTexture;
        operation.id = textureId;
        operation.width = bitmap->width();
        operation.height = bitmap->height();
        operation.format = static_cast<std::uint32_t>(TextureFormat(bitmap->format()));
        operation.rowBytes = bitmap->row_bytes();
        if (operation.width == 0 || operation.height == 0 || operation.format == DXGI_FORMAT_UNKNOWN) {
            FailCommandTransport();
            return;
        }
        const std::size_t bytesPerPixel = operation.format == DXGI_FORMAT_A8_UNORM ? 1u : 4u;
        if (operation.width > (static_cast<std::size_t>(-1) / bytesPerPixel) ||
            operation.rowBytes < operation.width * bytesPerPixel ||
            operation.height > static_cast<std::size_t>(-1) / operation.rowBytes) {
            FailCommandTransport();
            return;
        }
        const auto size = static_cast<std::size_t>(operation.rowBytes) * operation.height;
        if (size != bitmap->size() || size > AcceleratedGenerationMailbox::kMaxPayloadBytes) {
            FailCommandTransport();
            return;
        }
        try {
            operation.bytes.resize(size);
        } catch (...) {
            FailCommandTransport();
            return;
        }
        const void* pixels = bitmap->LockPixels();
        if (!pixels) {
            FailCommandTransport();
            return;
        }
        std::memcpy(operation.bytes.data(), pixels, size);
        bitmap->UnlockPixels();
        (void)StageResourceOperation(std::move(operation));
    }

    void FalloutGPUDriver::DestroyTexture(std::uint32_t textureId) {
        AcceleratedGenerationOperation operation;
        operation.type = AcceleratedGenerationOperationType::DestroyTexture;
        operation.id = textureId;
        (void)StageResourceOperation(std::move(operation));
    }

    void FalloutGPUDriver::CreateRenderBuffer(std::uint32_t renderBufferId, const ultralight::RenderBuffer& buffer) {
        if (renderBufferId == 0 || buffer.texture_id == 0 || buffer.width == 0 || buffer.height == 0 ||
            buffer.has_stencil_buffer || buffer.has_depth_buffer) {
            FailCommandTransport();
            return;
        }
        AcceleratedGenerationOperation operation;
        operation.type = AcceleratedGenerationOperationType::CreateRenderBuffer;
        operation.id = renderBufferId;
        operation.auxiliaryId = buffer.texture_id;
        operation.width = buffer.width;
        operation.height = buffer.height;
        operation.hasStencilBuffer = buffer.has_stencil_buffer;
        operation.hasDepthBuffer = buffer.has_depth_buffer;
        (void)StageResourceOperation(std::move(operation));
    }

    void FalloutGPUDriver::DestroyRenderBuffer(std::uint32_t renderBufferId) {
        AcceleratedGenerationOperation operation;
        operation.type = AcceleratedGenerationOperationType::DestroyRenderBuffer;
        operation.id = renderBufferId;
        (void)StageResourceOperation(std::move(operation));
    }

    void FalloutGPUDriver::CreateGeometry(std::uint32_t geometryId, const ultralight::VertexBuffer& vertices,
                                          const ultralight::IndexBuffer& indices) {
        if (geometryId == 0 || !vertices.data || !indices.data || vertices.size == 0 || indices.size == 0 ||
            indices.size % sizeof(ultralight::IndexType) != 0 ||
            vertices.size > AcceleratedGenerationMailbox::kMaxPayloadBytes ||
            indices.size > AcceleratedGenerationMailbox::kMaxPayloadBytes) {
            FailCommandTransport();
            return;
        }
        AcceleratedGenerationOperation operation;
        operation.type = AcceleratedGenerationOperationType::CreateGeometry;
        operation.id = geometryId;
        operation.vertexFormat = static_cast<std::uint32_t>(vertices.format);
        operation.secondarySize = indices.size;
        try {
            operation.bytes.assign(vertices.data, vertices.data + vertices.size);
            operation.secondaryBytes.assign(indices.data, indices.data + indices.size);
        } catch (...) {
            FailCommandTransport();
            return;
        }
        (void)StageResourceOperation(std::move(operation));
    }

    void FalloutGPUDriver::UpdateGeometry(std::uint32_t geometryId, const ultralight::VertexBuffer& vertices,
                                          const ultralight::IndexBuffer& indices) {
        if (geometryId == 0 || !vertices.data || !indices.data || vertices.size == 0 || indices.size == 0 ||
            indices.size % sizeof(ultralight::IndexType) != 0 ||
            vertices.size > AcceleratedGenerationMailbox::kMaxPayloadBytes ||
            indices.size > AcceleratedGenerationMailbox::kMaxPayloadBytes) {
            FailCommandTransport();
            return;
        }
        AcceleratedGenerationOperation operation;
        operation.type = AcceleratedGenerationOperationType::UpdateGeometry;
        operation.id = geometryId;
        operation.vertexFormat = static_cast<std::uint32_t>(vertices.format);
        operation.secondarySize = indices.size;
        try {
            operation.bytes.assign(vertices.data, vertices.data + vertices.size);
            operation.secondaryBytes.assign(indices.data, indices.data + indices.size);
        } catch (...) {
            FailCommandTransport();
            return;
        }
        (void)StageResourceOperation(std::move(operation));
    }

    void FalloutGPUDriver::DestroyGeometry(std::uint32_t geometryId) {
        AcceleratedGenerationOperation operation;
        operation.type = AcceleratedGenerationOperationType::DestroyGeometry;
        operation.id = geometryId;
        (void)StageResourceOperation(std::move(operation));
    }

    bool FalloutGPUDriver::ExecuteResourceOperation(const AcceleratedGenerationOperation& operation) {
        switch (operation.type) {
            case AcceleratedGenerationOperationType::CreateTexture: return ExecuteCreateTexture(operation);
            case AcceleratedGenerationOperationType::UpdateTexture: return ExecuteUpdateTexture(operation);
            case AcceleratedGenerationOperationType::DestroyTexture: return ExecuteDestroyTexture(operation);
            case AcceleratedGenerationOperationType::CreateRenderBuffer: return ExecuteCreateRenderBuffer(operation);
            case AcceleratedGenerationOperationType::DestroyRenderBuffer: return ExecuteDestroyRenderBuffer(operation);
            case AcceleratedGenerationOperationType::CreateGeometry: return ExecuteCreateGeometry(operation);
            case AcceleratedGenerationOperationType::UpdateGeometry: return ExecuteUpdateGeometry(operation);
            case AcceleratedGenerationOperationType::DestroyGeometry: return ExecuteDestroyGeometry(operation);
            default:
                SetOperationFailure("unsupported-operation-type");
                return false;
        }
    }

}
