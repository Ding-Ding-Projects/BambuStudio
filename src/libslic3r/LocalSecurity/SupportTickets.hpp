#pragma once
#include "LocalSecurity.hpp"
#include <filesystem>
namespace Slic3r::LocalSecurity {
enum class TicketCategory { ForgottenAnswer, MissingAuthenticator, LocalReset };
enum class TicketStage { Created, Reviewed, Resolution };
struct SupportTicket { std::string id; TicketCategory category; TicketStage stage; unsigned severity; std::string description; };
class SupportTickets {
public:
    SupportTickets(Vault& vault, std::filesystem::path application_data):m_vault(vault),m_file(std::move(application_data)/"local_security"/"support-tickets-v1.enc"){}
    std::vector<SupportTicket> list();
    std::string create(TicketCategory,unsigned severity,std::string description);
    void advance(const std::string& id);
    void remove(const std::string& id);
    std::string export_text();
private:
    void save(const std::vector<SupportTicket>&);
    Vault& m_vault;
    std::filesystem::path m_file;
};
}
