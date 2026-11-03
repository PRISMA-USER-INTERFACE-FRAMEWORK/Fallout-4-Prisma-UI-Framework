#pragma once

#include <string>

namespace RE::BSScript { class IVirtualMachine; }

namespace PrismaUI::PapyrusVM
{

    bool Register(RE::BSScript::IVirtualMachine* a_vm);

    void DispatchEvent(const std::string& a_event, const std::string& a_data);
}
