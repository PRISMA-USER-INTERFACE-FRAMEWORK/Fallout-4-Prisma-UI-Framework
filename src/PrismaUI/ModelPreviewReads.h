#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace PrismaUI::ModelPreview {

bool ReadGameFile(const std::string& rawPath, std::vector<uint8_t>& out);
std::string LowerStr(const std::string& in);

bool GameLoadActive() noexcept;
void SetReadsGameLoadActive(bool active) noexcept;
void SetWorkerThread(bool isWorker) noexcept;
void MarkTickCoreSeen() noexcept;

void BeginReadShutdown() noexcept;
void ClearMissingReadPaths() noexcept;
void NotifyReadWaiters() noexcept;

void RequestEngineReadService();
void ServiceEngineReadsOnGameThread();

int ReadQueueDepth() noexcept;
int WorkersWaitingRead() noexcept;

}
