#include "FalloutGPUDriver.h"

#include <cstring>

namespace PrismaUI::GPU {

    bool FalloutGPUDriver::ExecuteCreateGeometry(const AcceleratedGenerationOperation& operation) {
        if (!context_ || !context_->Ready() || operation.id == 0 || operation.bytes.empty() ||
            operation.secondaryBytes.empty() || operation.bytes.size() > UINT_MAX ||
            operation.secondaryBytes.size() > UINT_MAX || operation.secondaryBytes.size() % sizeof(ultralight::IndexType) != 0 ||
            operation.secondarySize != operation.secondaryBytes.size() ||
            operation.vertexFormat > static_cast<std::uint32_t>(ultralight::VertexBufferFormat::_2f_4ub_2f_2f_28f)) {
            SetOperationFailure("create-geometry-invalid-payload-or-format");
            return false;
        }
        {
            std::lock_guard lock{resourcesMutex_};
            if (geometry_.contains(operation.id)) {
                SetOperationFailure("create-geometry-duplicate-id");
                return false;
            }
        }
        D3D11_BUFFER_DESC vertexDesc{};
        vertexDesc.Usage = D3D11_USAGE_DYNAMIC;
        vertexDesc.ByteWidth = static_cast<UINT>(operation.bytes.size());
        vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vertexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        D3D11_SUBRESOURCE_DATA vertexData{};
        vertexData.pSysMem = operation.bytes.data();
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
        const auto vertexResult = context_->Device()->CreateBuffer(&vertexDesc, &vertexData, vertexBuffer.GetAddressOf());
        if (FAILED(vertexResult)) {
            SetOperationFailure("create-vertex-buffer", vertexResult);
            return false;
        }
        if (!vertexBuffer) {
            SetOperationFailure("create-vertex-buffer-null");
            return false;
        }
        D3D11_BUFFER_DESC indexDesc{};
        indexDesc.Usage = D3D11_USAGE_DYNAMIC;
        indexDesc.ByteWidth = static_cast<UINT>(operation.secondaryBytes.size());
        indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        indexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        D3D11_SUBRESOURCE_DATA indexData{};
        indexData.pSysMem = operation.secondaryBytes.data();
        Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
        const auto indexResult = context_->Device()->CreateBuffer(&indexDesc, &indexData, indexBuffer.GetAddressOf());
        if (FAILED(indexResult)) {
            SetOperationFailure("create-index-buffer", indexResult);
            return false;
        }
        if (!indexBuffer) {
            SetOperationFailure("create-index-buffer-null");
            return false;
        }
        std::lock_guard lock{resourcesMutex_};
        if (geometry_.contains(operation.id)) {
            SetOperationFailure("create-geometry-duplicate-at-commit");
            return false;
        }
        GeometryEntry entry;
        entry.format = static_cast<ultralight::VertexBufferFormat>(operation.vertexFormat);
        entry.vertexBuffer = std::move(vertexBuffer);
        entry.indexBuffer = std::move(indexBuffer);
        entry.vertexCapacity = static_cast<std::uint32_t>(operation.bytes.size());
        entry.indexCapacity = static_cast<std::uint32_t>(operation.secondaryBytes.size());
        entry.vertexSize = entry.vertexCapacity;
        entry.indexSize = entry.indexCapacity;
        entry.epoch = resourceEpoch_;
        entry.vertexBytes = operation.bytes;
        entry.indexBytes = operation.secondaryBytes;
        geometry_.emplace(operation.id, std::move(entry));
        return true;
    }

    bool FalloutGPUDriver::ExecuteUpdateGeometry(const AcceleratedGenerationOperation& operation) {
        if (!context_ || !context_->Ready() || operation.id == 0 || operation.bytes.empty() ||
            operation.secondaryBytes.empty() || operation.bytes.size() > UINT_MAX ||
            operation.secondaryBytes.size() > UINT_MAX || operation.secondaryBytes.size() % sizeof(ultralight::IndexType) != 0 ||
            operation.secondarySize != operation.secondaryBytes.size() ||
            operation.vertexFormat > static_cast<std::uint32_t>(ultralight::VertexBufferFormat::_2f_4ub_2f_2f_28f)) {
            SetOperationFailure("update-geometry-invalid-payload-or-format");
            return false;
        }
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
        std::uint32_t vertexCapacity = 0;
        std::uint32_t indexCapacity = 0;
        ultralight::VertexBufferFormat storedFormat{};
        {
            std::lock_guard lock{resourcesMutex_};
            const auto it = geometry_.find(operation.id);
            if (it == geometry_.end()) {
                SetOperationFailure("update-geometry-missing");
                return false;
            }
            vertexBuffer = it->second.vertexBuffer;
            indexBuffer = it->second.indexBuffer;
            vertexCapacity = it->second.vertexCapacity;
            indexCapacity = it->second.indexCapacity;
            storedFormat = it->second.format;
        }
        const auto requestedFormat = static_cast<ultralight::VertexBufferFormat>(operation.vertexFormat);
        if (!vertexBuffer || !indexBuffer) {
            SetOperationFailure("update-geometry-buffer-missing");
            return false;
        }
        if (operation.bytes.size() > vertexCapacity || operation.secondaryBytes.size() > indexCapacity ||
            requestedFormat != storedFormat) {
            AcceleratedGenerationOperation replacement = operation;
            replacement.type = AcceleratedGenerationOperationType::CreateGeometry;
            std::lock_guard lock{resourcesMutex_};
            if (!geometry_.contains(operation.id)) {
                SetOperationFailure("update-geometry-missing-before-replacement");
                return false;
            }
            D3D11_BUFFER_DESC vertexDesc{};
            vertexDesc.Usage = D3D11_USAGE_DYNAMIC;
            vertexDesc.ByteWidth = static_cast<UINT>(replacement.bytes.size());
            vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            vertexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            D3D11_SUBRESOURCE_DATA vertexData{replacement.bytes.data(), 0, 0};
            Microsoft::WRL::ComPtr<ID3D11Buffer> replacementVertex;
            const auto vertexResult = context_->Device()->CreateBuffer(
                &vertexDesc, &vertexData, replacementVertex.GetAddressOf());
            if (FAILED(vertexResult)) {
                SetOperationFailure("replace-vertex-buffer", vertexResult);
                return false;
            }
            if (!replacementVertex) {
                SetOperationFailure("replace-vertex-buffer-null");
                return false;
            }
            D3D11_BUFFER_DESC indexDesc{};
            indexDesc.Usage = D3D11_USAGE_DYNAMIC;
            indexDesc.ByteWidth = static_cast<UINT>(replacement.secondaryBytes.size());
            indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
            indexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            D3D11_SUBRESOURCE_DATA indexData{replacement.secondaryBytes.data(), 0, 0};
            Microsoft::WRL::ComPtr<ID3D11Buffer> replacementIndex;
            const auto indexResult = context_->Device()->CreateBuffer(
                &indexDesc, &indexData, replacementIndex.GetAddressOf());
            if (FAILED(indexResult)) {
                SetOperationFailure("replace-index-buffer", indexResult);
                return false;
            }
            if (!replacementIndex) {
                SetOperationFailure("replace-index-buffer-null");
                return false;
            }
            auto& entry = geometry_[operation.id];
            entry.format = requestedFormat;
            entry.vertexBuffer = std::move(replacementVertex);
            entry.indexBuffer = std::move(replacementIndex);
            entry.vertexCapacity = static_cast<std::uint32_t>(replacement.bytes.size());
            entry.indexCapacity = static_cast<std::uint32_t>(replacement.secondaryBytes.size());
            entry.vertexSize = entry.vertexCapacity;
            entry.indexSize = entry.indexCapacity;
            entry.epoch = resourceEpoch_;
            entry.vertexBytes = replacement.bytes;
            entry.indexBytes = replacement.secondaryBytes;
            return true;
        }

        auto* immediate = context_->Context();
        if (!immediate) {
            SetOperationFailure("update-geometry-immediate-context-missing");
            return false;
        }
        D3D11_MAPPED_SUBRESOURCE vertexMapped{};
        const auto vertexMapResult = immediate->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &vertexMapped);
        if (FAILED(vertexMapResult) || !vertexMapped.pData) {
            if (vertexMapped.pData) immediate->Unmap(vertexBuffer.Get(), 0);
            SetOperationFailure("update-vertex-buffer-map", vertexMapResult);
            return false;
        }
        std::memcpy(vertexMapped.pData, operation.bytes.data(), operation.bytes.size());
        immediate->Unmap(vertexBuffer.Get(), 0);
        D3D11_MAPPED_SUBRESOURCE indexMapped{};
        const auto indexMapResult = immediate->Map(indexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &indexMapped);
        if (FAILED(indexMapResult) || !indexMapped.pData) {
            if (indexMapped.pData) immediate->Unmap(indexBuffer.Get(), 0);
            SetOperationFailure("update-index-buffer-map", indexMapResult);
            return false;
        }
        std::memcpy(indexMapped.pData, operation.secondaryBytes.data(), operation.secondaryBytes.size());
        immediate->Unmap(indexBuffer.Get(), 0);
        std::lock_guard lock{resourcesMutex_};
        const auto it = geometry_.find(operation.id);
        if (it == geometry_.end() || it->second.vertexBuffer.Get() != vertexBuffer.Get() ||
            it->second.indexBuffer.Get() != indexBuffer.Get()) {
            SetOperationFailure("update-geometry-changed-before-commit");
            return false;
        }
        it->second.vertexSize = static_cast<std::uint32_t>(operation.bytes.size());
        it->second.indexSize = static_cast<std::uint32_t>(operation.secondaryBytes.size());
        it->second.vertexBytes = operation.bytes;
        it->second.indexBytes = operation.secondaryBytes;
        return true;
    }

    bool FalloutGPUDriver::ExecuteDestroyGeometry(const AcceleratedGenerationOperation& operation) {
        if (operation.id == 0) {
            SetOperationFailure("destroy-geometry-invalid-id");
            return false;
        }
        std::lock_guard lock{resourcesMutex_};
        if (geometry_.erase(operation.id) == 0) {
            SetOperationFailure("destroy-geometry-missing");
            return false;
        }
        return true;
    }

}
