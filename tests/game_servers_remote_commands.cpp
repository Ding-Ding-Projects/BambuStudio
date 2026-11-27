#include "slic3r/GUI/GameServers/NativeServerTarget.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

int main(int argc,char **argv)
{
    using namespace Slic3r::GUI::GameServers;
    if(argc!=8)return 2;
    const bool bedrock=std::string(argv[5])=="bedrock";
    const auto command=std::string(argv[1])=="setup"?
        ssh_server_setup("ssh",argv[2],argv[3],argv[4],bedrock,std::stoi(argv[6]),argv[7]):
        ssh_server_launch("ssh",argv[2],argv[3],bedrock,argv[7]);
    if(command.empty())return 3;
    std::cout<<nlohmann::json(command).dump();
}
