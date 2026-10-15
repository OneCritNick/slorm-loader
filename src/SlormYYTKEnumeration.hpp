#pragma once
#include <YYToolkit/YYTK_Shared.hpp>
#include <set>
#include <string>

namespace Slorm {
struct EnumerationResult {
    Aurie::AurieStatus status = Aurie::AURIE_UNAVAILABLE;
    int32_t expected = -1;
    size_t visited = 0;
    bool complete = false;
    std::string reason;
};

// YYTK returns OBJECT_NOT_FOUND for both exhaustion and some member lookup failures.
// Stop explicitly on the last independently counted member, then require SUCCESS.
template<class Visitor>
EnumerationResult EnumerateMembersCompletely(YYTK::YYTKInterface* api,
                                            YYTK::RValue object, Visitor visitor) {
    EnumerationResult report;
    if (!api || object.m_Kind != YYTK::VALUE_OBJECT) {
        report.reason = "invalid_enumeration_target";
        return report;
    }
    const auto getKeys = api->GetRunnerInterface().StructGetKeys;
    if (!getKeys) {
        // The v5 builtin fallback has null leading entries; do not invoke that path.
        report.reason = "struct_key_enumeration_unavailable";
        return report;
    }
    report.expected = getKeys(&object, nullptr, nullptr);
    if (report.expected <= 0 || report.expected > 65536) {
        report.reason = "member_count_out_of_bounds";
        return report;
    }
    std::set<std::string> names;
    bool valid = true;
    report.status = api->EnumInstanceMembers(object, [&](const char* name, YYTK::RValue* value) {
        if (!name || !*name || !value || !names.insert(name).second ||
            report.visited >= static_cast<size_t>(report.expected)) {
            valid = false; report.reason = "invalid_or_duplicate_member"; return true;
        }
        ++report.visited;
        if (!visitor(name, value)) {
            valid = false; report.reason = "member_snapshot_incomplete"; return true;
        }
        return report.visited == static_cast<size_t>(report.expected);
    });
    const auto finalCount = getKeys(&object, nullptr, nullptr);
    report.complete = valid && Aurie::AurieSuccess(report.status) &&
        report.visited == static_cast<size_t>(report.expected) && finalCount == report.expected;
    if (!report.complete && report.reason.empty()) report.reason = "member_enumeration_incomplete";
    return report;
}
}
