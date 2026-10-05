//#include "D:/dev/bamboo_slicer/build_release/src/slic3r/CMakeFiles/libslic3r_gui.dir/Release/cmake_pch.hxx"
#include "DevHMS.h"
#include "../DeviceManager.hpp"
#include "../PrinterHistory.hpp"

namespace Slic3r
{

bool DevHMSItem::parse_hms_info(unsigned attr, unsigned code)
{
    bool result = true;
    unsigned int model_id_int = (attr >> 24) & 0xFF;
    this->m_module_id = (ModuleID)model_id_int;
    this->m_module_num = (attr >> 16) & 0xFF;
    this->m_part_id = (attr >> 8) & 0xFF;
    this->m_reserved = (attr >> 0) & 0xFF;
    unsigned msg_level_int = code >> 16;
    if (msg_level_int < (unsigned)HMS_MSG_LEVEL_MAX)
    {
        this->m_msg_level = (HMSMessageLevel)msg_level_int;
    }
    else
    {
        this->m_msg_level = HMS_UNKNOWN;
    }

    this->m_msg_code = code & 0xFFFF;
    return result;
}

std::string DevHMSItem::get_long_error_code() const
{
    char buf[64];
    ::sprintf(buf, "%02X%02X%02X%02X00%02X%04X",
        this->m_module_id,
        this->m_module_num,
        this->m_part_id,
        this->m_reserved,
        (int)this->m_msg_level,
        this->m_msg_code);
    return std::string(buf);
}

void DevHMS::ParseHMSItems(const json& hms_json, bool fresh_report, bool complete_report)
{
    // Validate a complete candidate before replacing the display state or
    // resolving history. A malformed packet must never imply recovery.
    std::vector<DevHMSItem> items;
    std::vector<GUI::PrinterIncidentObservation> observations;
    try
    {
        if (!hms_json.is_array() || hms_json.size() > GUI::PrinterHistory::DEFAULT_MAX_ENTRIES)
            return;
        for (const auto &value : hms_json)
        {
            if (!value.is_object() || !value.contains("attr") || !value.contains("code") ||
                !value["attr"].is_number_integer() || !value["code"].is_number_integer() ||
                (value["attr"].is_number_integer() && !value["attr"].is_number_unsigned() && value["attr"].get<std::int64_t>() < 0) ||
                (value["code"].is_number_integer() && !value["code"].is_number_unsigned() && value["code"].get<std::int64_t>() < 0) ||
                value["attr"].get<std::uint64_t>() > UINT32_MAX || value["code"].get<std::uint64_t>() > UINT32_MAX)
                return;
            DevHMSItem item;
            item.parse_hms_info(value["attr"].get<unsigned>(), value["code"].get<unsigned>());
            GUI::PrinterIncidentObservation observation;
            observation.code = item.get_long_error_code();
            observation.severity = static_cast<int>(item.get_level());
            // Only persist text explicitly included in this telemetry. No
            // dictionary request or device connection is initiated by history.
            if (value.contains("description") && value["description"].is_string())
                observation.description = value["description"].get<std::string>().substr(0, 4096);
            if (observation.description.empty()) observation.description = "HMS " + observation.code;
            observations.push_back(std::move(observation));
            items.push_back(item);
        }
        m_hms_list = std::move(items);
        if (fresh_report && m_object)
            GUI::PrinterHistory::instance().observe(m_object->get_dev_id(), observations, complete_report);
    }
    catch (const std::exception&)
    {
        // Keep the previous valid state; malformed telemetry is not a clear.
    }
}
}
