#include "slic3r/GUI/GameServers/ManagedContainerLifecycle.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <iostream>
#include <thread>

int main(int argc,char**argv){
    if(argc!=4)return 2;
    auto cli=[](const std::vector<std::string>&command){
        std::cout<<nlohmann::json({{"argv",command}}).dump()<<std::endl;
        std::string line;if(!std::getline(std::cin,line))return std::make_pair(1,std::string());
        auto response=nlohmann::json::parse(line);return std::make_pair(response.at("code").get<int>(),response.at("stdout").get<std::string>());
    };
    auto verdict=Slic3r::GUI::GameServers::managed_container_restart(argv[1],std::string(argv[2])=="bedrock",std::string(argv[3])=="warning",2,cli,
        []{return std::chrono::steady_clock::now();},[]{std::this_thread::sleep_for(std::chrono::milliseconds(100));});
    std::cout<<nlohmann::json({{"verdict",verdict}}).dump()<<std::endl;
}
