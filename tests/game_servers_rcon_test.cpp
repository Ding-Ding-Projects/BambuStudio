#include "slic3r/GUI/GameServers/RconCredential.cpp"
#include "slic3r/GUI/GameServers/ServerModel.cpp"
#include <iostream>
int main(int argc,char **argv)
{
    using namespace Slic3r::GUI::GameServers;
    if (argc!=4) return 2;
    std::string supplied,reason;
    if (!std::getline(std::cin,supplied) || supplied.empty()) return 3;
    const std::string id=argv[1];
    const auto port=std::stoi(argv[2]);
    const auto pid=std::stoul(argv[3]);
    if (!save_rcon_credential(id,supplied)) return 4;
    struct Erase { std::string id; ~Erase(){ clear_rcon_credential(id); } } erase{id};
    std::fill(supplied.begin(),supplied.end(),'\0');
    if (verify_local_rcon(id,port,0,reason)) return 5;
    if (!verify_local_rcon(id,port,pid,reason)) {std::cerr<<reason;return 6;}
    if (!clear_rcon_credential(id)) return 7;
    if (verify_local_rcon(id,port,pid,reason)) return 8;
    std::cout<<"RCON credential write, peer binding, authentication, response and clear passed.\n";
}
