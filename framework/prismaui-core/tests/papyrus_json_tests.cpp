#include "PrismaUI/PapyrusJson.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
int failures = 0;
void Check(bool v, const char* msg) {
    if (!v) {
        std::cerr << "FAIL: " << msg << '\n';
        ++failures;
    }
}
}

int main() {
    using namespace PrismaUI::PapyrusJson;

    Check(IsSafeCallbackId("1"), "digit id");
    Check(IsSafeCallbackId("42"), "multi digit");
    Check(!IsSafeCallbackId(""), "empty id");
    Check(!IsSafeCallbackId("1');alert(1);//"), "injection id");
    Check(!IsSafeCallbackId("1\"),evil"), "quote id");
    Check(!IsSafeCallbackId(std::string(17, '1')), "overlong id");

    Object o;
    Check(ParseFlatObject(R"({"op":"getGlobal","id":"3","esp":"MyMod.esp","formId":"001234","value":1.5})", o),
          "parse flat");
    Check(GetString(o, "op") == "getGlobal", "op");
    Check(GetString(o, "id") == "3", "id");
    Check(GetString(o, "esp") == "MyMod.esp", "esp");
    Check(GetNumber(o, "value") == 1.5, "value");

    Check(ParseFlatObject(R"({"op":"setGlobal","value":1e-7})", o), "parse exponent");
    Check(GetNumber(o, "value") == 1e-7, "exponent value");
    Check(ParseFlatObject(R"({"op":"setGlobal","value":-2.5E+3})", o), "parse signed exponent");
    Check(GetNumber(o, "value") == -2500.0, "signed exponent value");
    Check(!ParseFlatObject(R"({"op":"setGlobal","value":1e})", o), "reject exponent without digits");
    Check(!ParseFlatObject(R"({"op":"setGlobal","value":1e+})", o), "reject signed exponent without digits");

    Object evil;
    Check(ParseFlatObject(R"({"op":"getGlobal","id":"1'); /* attacker */ //","esp":"x.esp"})", evil),
          "parse injected id as string (not as JS)");
    Check(GetString(evil, "id") == "1'); /* attacker */ //", "id preserved as data");
    Check(!IsSafeCallbackId(GetString(evil, "id")), "injected id rejected by allowlist");

    Object esc;
    Check(ParseFlatObject(R"({"data":"a\"b\\c","op":"emit"})", esc), "escaped quotes");
    Check(GetString(esc, "data") == "a\"b\\c", "unescape");
    Check(GetString(esc, "op") == "emit", "second key");

    Check(!ParseFlatObject("not json", o), "reject junk");
    Check(!ParseFlatObject(R"({"op":)", o), "reject truncated");

    const std::string lit = JsStringLiteral("1');alert(1);//");
    Check(lit.front() == '"' && lit.back() == '"', "quoted");
    Check(lit.find("alert") != std::string::npos, "payload stays inside quotes");
    Check(lit.find("\\'") != std::string::npos, "quote escaped");

    Check(EspAllowedForView("MyMod.dll", "MyMod.esp", true), "owner write own esp");
    Check(EspAllowedForView("MyMod.dll", "MyMod.esm", true), "owner write own esm stem");
    Check(!EspAllowedForView("MyMod.dll", "Other.esp", true), "cross plugin write denied");
    Check(!EspAllowedForView("MyMod.dll", "Fallout4.esm", true), "vanilla write denied");
    Check(EspAllowedForView("MyMod.dll", "Fallout4.esm", false), "vanilla read allowed");
    Check(!EspAllowedForView("", "MyMod.esp", true), "empty owner write denied");

    Check(PluginStem("C:\\\\mods\\\\MyMod.dll") == "mymod", "stem strips path");

    Check(FormatNumber(1e-7) != "0.000000", "tiny value survives formatting");
    for (const double x : {1e-7, -0.1, 123.456, 0.0}) {
        Check(std::strtod(FormatNumber(x).c_str(), nullptr) == x, "format round trips");
    }

    if (failures) {
        std::cerr << failures << " failures\n";
        return 1;
    }
    std::cout << "papyrus_json_tests ok\n";
    return 0;
}
