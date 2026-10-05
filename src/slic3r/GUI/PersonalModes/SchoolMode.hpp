#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace Slic3r::GUI::PersonalModes {
struct SchoolRecord {
    bool enabled = false;
    std::string display_name = "School mode";
    std::uint64_t revision = 0;
    std::string credential_generation;
    std::string updated_at;
};
enum class RecordStatus { Ready, Missing, Unavailable, Corrupt, Conflict };
struct SchoolRead { RecordStatus status = RecordStatus::Unavailable; SchoolRecord record; };
struct PresentationPreferences {
    std::string language = "en";
    int english_funny = 5;
    int cantonese_funny = 5;
    bool personal_vocabulary = true;
    bool dim_sum = true;
};
enum class PresentationCapability { English, Cantonese, Bilingual, FunnyLevel, PersonalVocabulary, DimSum, Attention };

// Final display adapters may read this atomic; only the GUI owner updates it.
// Until the shared record is read successfully, private presentation is suppressed.
inline std::atomic<bool> school_presentation_suppressed{true};

class SchoolMode {
public:
    struct Backend {
        std::function<SchoolRead()> read;
        std::function<RecordStatus(const SchoolRecord&, std::uint64_t, const std::function<bool(const std::string&)>&)> compare_exchange;
        std::function<void(const SchoolRecord&, RecordStatus)> changed;
    };
    explicit SchoolMode(Backend backend) : m_backend(std::move(backend)) { reload(); }
    bool suppressed() const { return m_status != RecordStatus::Ready && m_status != RecordStatus::Missing ? true : m_record.enabled; }
    RecordStatus status() const { return m_status; }
    const SchoolRecord& record() const { return m_record; }
    void reload() {
        const auto old = m_record;
        const auto old_status = m_status;
        auto read = m_backend.read ? m_backend.read() : SchoolRead{};
        m_status = read.status;
        if (read.status == RecordStatus::Ready) m_record = std::move(read.record);
        else if (read.status == RecordStatus::Missing) m_record = SchoolRecord{};
        school_presentation_suppressed.store(suppressed());
        if (m_backend.changed && (old_status != m_status || old.enabled != m_record.enabled ||
            old.display_name != m_record.display_name || old.revision != m_record.revision ||
            old.credential_generation != m_record.credential_generation)) m_backend.changed(m_record, m_status);
    }
    PresentationPreferences effective(PresentationPreferences base) const {
        if (suppressed()) {
            base.language = "en";
            base.english_funny = base.cantonese_funny = 1;
            base.personal_vocabulary = base.dim_sum = false;
        }
        return base;
    }
    bool available(PresentationCapability capability) const {
        return !suppressed() || capability == PresentationCapability::English || capability == PresentationCapability::Attention;
    }
    RecordStatus rename(const std::string& name) {
        if (!valid_name(name)) return RecordStatus::Corrupt;
        reload();
        auto next = m_record; next.display_name = name;
        return save(std::move(next));
    }
    // Pass the current vault generation. No credential bytes enter this service.
    RecordStatus enable(const std::string& configured_generation) {
        if (configured_generation.empty() || configured_generation.size() > 128) return RecordStatus::Unavailable;
        reload();
        auto next = m_record; next.enabled = true; next.credential_generation = configured_generation;
        return save(std::move(next));
    }
    // The adapter verifies the supplied answer locally and confirms that metadata
    // still has this exact generation. UI state is never an authentication result.
    RecordStatus disable(const std::function<bool(const std::string&)>& verify_current_generation) {
        reload();
        if (m_status != RecordStatus::Ready || !verify_current_generation || m_record.credential_generation.empty())
            return RecordStatus::Unavailable;
        auto next = m_record; next.enabled = false;
        return save(std::move(next), verify_current_generation);
    }
    RecordStatus credential_changed(const std::string& generation) {
        if (generation.empty() || generation.size() > 128) return RecordStatus::Unavailable;
        reload();
        auto next = m_record; next.credential_generation = generation;
        return save(std::move(next));
    }
    static bool valid_name(const std::string& name) {
        if (name.empty() || name.size() > 128) return false;
        bool content = false;
        for (std::size_t i = 0; i < name.size();) {
            const auto first = static_cast<unsigned char>(name[i++]);
            unsigned cp = first;
            unsigned continuation = 0;
            if (first >= 0xc2 && first <= 0xdf) { cp = first & 31; continuation = 1; }
            else if (first >= 0xe0 && first <= 0xef) { cp = first & 15; continuation = 2; }
            else if (first >= 0xf0 && first <= 0xf4) { cp = first & 7; continuation = 3; }
            else if (first >= 0x80) return false;
            const auto count = continuation;
            while (continuation--) {
                if (i >= name.size()) return false;
                const auto part = static_cast<unsigned char>(name[i++]);
                if ((part & 0xc0) != 0x80) return false;
                cp = (cp << 6) | (part & 63);
            }
            if ((count == 1 && cp < 0x80) || (count == 2 && cp < 0x800) || (count == 3 && cp < 0x10000) ||
                cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff) || cp < 32 || (cp >= 0x7f && cp <= 0x9f) ||
                (cp >= 0x2028 && cp <= 0x202e) || (cp >= 0x2066 && cp <= 0x2069) || cp == 0x200b || cp == 0xfeff) return false;
            if (cp != 32 && cp != 0xa0 && cp != 0x3000) content = true;
        }
        return content;
    }
private:
    RecordStatus save(SchoolRecord next, const std::function<bool(const std::string&)>& verify = {}) {
        if (m_status != RecordStatus::Ready && m_status != RecordStatus::Missing) return m_status;
        if (!m_backend.compare_exchange) return RecordStatus::Unavailable;
        const auto status = m_backend.compare_exchange(next, m_record.revision, verify);
        reload();
        return status;
    }
    Backend m_backend;
    SchoolRecord m_record;
    RecordStatus m_status = RecordStatus::Unavailable;
};
}
