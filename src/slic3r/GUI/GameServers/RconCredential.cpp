#include "RconCredential.hpp"
#include "ServerModel.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wincred.h>
#include <iphlpapi.h>
#include <cstring>
#include <chrono>
#include <vector>
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Iphlpapi.lib")
#endif

namespace Slic3r { namespace GUI { namespace GameServers {
#ifdef _WIN32
namespace {
std::wstring target(const std::string &server)
{
    auto hash=sha1_hex(server);
    return L"BambuStudio/ManagedMinecraft/Rcon/"+std::wstring(hash.begin(),hash.end());
}
struct Socket {
    SOCKET fd = INVALID_SOCKET;
    ~Socket() { if (fd!=INVALID_SOCKET) closesocket(fd); WSACleanup(); }
};
// Identify the accepted side of this exact connection before any credential bytes leave the vault.
bool is_owned_peer(SOCKET client, unsigned short port, DWORD expected)
{
    sockaddr_in local{};
    int len=sizeof(local);
    if (!expected || getsockname(client,reinterpret_cast<sockaddr*>(&local),&len)!=0) return false;
    for (ULONG family : {ULONG(AF_INET),ULONG(AF_INET6)}) {
        DWORD bytes=0;
        if (GetExtendedTcpTable(nullptr,&bytes,FALSE,family,TCP_TABLE_OWNER_PID_CONNECTIONS,0)!=ERROR_INSUFFICIENT_BUFFER || bytes>32*1024*1024) continue;
        std::vector<BYTE> buffer(bytes);
        if (GetExtendedTcpTable(buffer.data(),&bytes,FALSE,family,TCP_TABLE_OWNER_PID_CONNECTIONS,0)!=NO_ERROR) continue;
        if (family==AF_INET) {
            const auto *table=reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(buffer.data());
            for (DWORD i=0;i<table->dwNumEntries;++i) {
                const auto &r=table->table[i];
                if (r.dwOwningPid==expected && r.dwState==MIB_TCP_STATE_ESTAB && r.dwLocalAddr==htonl(INADDR_LOOPBACK) &&
                    r.dwRemoteAddr==local.sin_addr.s_addr && r.dwLocalPort==htons(port) && r.dwRemotePort==local.sin_port) return true;
            }
        } else {
            // A dual-stack Java listener reports the accepted IPv4 peer as ::ffff:127.0.0.1.
            const BYTE mapped[]={0,0,0,0,0,0,0,0,0,0,255,255,127,0,0,1};
            const auto *table=reinterpret_cast<const MIB_TCP6TABLE_OWNER_PID*>(buffer.data());
            for (DWORD i=0;i<table->dwNumEntries;++i) {
                const auto &r=table->table[i];
                if (r.dwOwningPid==expected && r.dwState==MIB_TCP_STATE_ESTAB && std::memcmp(r.ucLocalAddr,mapped,16)==0 &&
                    std::memcmp(r.ucRemoteAddr,mapped,16)==0 && r.dwLocalPort==htons(port) && r.dwRemotePort==local.sin_port) return true;
            }
        }
    }
    return false;
}
using Deadline=std::chrono::steady_clock::time_point;
bool transfer(SOCKET fd,char *p,std::size_t count,bool write,Deadline until)
{
    while (count) {
        auto left=std::chrono::duration_cast<std::chrono::milliseconds>(until-std::chrono::steady_clock::now()).count();
        if (left<=0) return false;
        DWORD wait=DWORD(left);
        if (setsockopt(fd,SOL_SOCKET,write?SO_SNDTIMEO:SO_RCVTIMEO,reinterpret_cast<const char*>(&wait),sizeof(wait))!=0) return false;
        const int n=write ? send(fd,p,int(count),0) : recv(fd,p,int(count),0);
        if (n<=0) return false;
        p+=n;count-=std::size_t(n);
    }
    return true;
}
bool packet(SOCKET fd,int id,int type,const BYTE *value,std::size_t bytes,Deadline until)
{
    std::vector<char> data(bytes+14,0);
    const std::uint32_t size=std::uint32_t(bytes+10);
    // Windows x86, x64 and ARM64 are little endian, as is Source RCON.
    std::memcpy(data.data(),&size,4);std::memcpy(data.data()+4,&id,4);std::memcpy(data.data()+8,&type,4);
    std::memcpy(data.data()+12,value,bytes);
    bool ok=transfer(fd,data.data(),data.size(),true,until);
    SecureZeroMemory(data.data(),data.size());
    return ok;
}
bool reply(SOCKET fd,int &id,int &type,Deadline until)
{
    std::uint32_t size=0;
    if (!transfer(fd,reinterpret_cast<char*>(&size),4,false,until) || size<10 || size>65536) return false;
    std::vector<char> data(size);
    if (!transfer(fd,data.data(),size,false,until)) return false;
    if (data[size-2]!=0 || data[size-1]!=0) return false;
    std::memcpy(&id,data.data(),4); std::memcpy(&type,data.data()+4,4);
    return true;
}
}
#endif

bool save_rcon_credential(const std::string &server,const std::string &value)
{
#ifdef _WIN32
    if (server.empty() || value.empty() || value.size()>CRED_MAX_CREDENTIAL_BLOB_SIZE || value.find('\0')!=std::string::npos) return false;
    auto name=target(server);
    CREDENTIALW c{};
    c.Type=CRED_TYPE_GENERIC; c.TargetName=name.data(); c.Persist=CRED_PERSIST_LOCAL_MACHINE;
    c.CredentialBlob=reinterpret_cast<BYTE*>(const_cast<char*>(value.data())); c.CredentialBlobSize=DWORD(value.size());
    return CredWriteW(&c,0)!=FALSE;
#else
    return false;
#endif
}
bool clear_rcon_credential(const std::string &server)
{
#ifdef _WIN32
    if (server.empty()) return false;
    return CredDeleteW(target(server).c_str(),CRED_TYPE_GENERIC,0)!=FALSE || GetLastError()==ERROR_NOT_FOUND;
#else
    return false;
#endif
}

bool verify_local_rcon(const std::string &server,int port,unsigned long expected_pid,std::string &reason)
{
#ifdef _WIN32
    if (server.empty() || port<1 || port>65535) {reason="Invalid server or RCON port.";return false;}
    PCREDENTIALW c=nullptr;
    if (!CredReadW(target(server).c_str(),CRED_TYPE_GENERIC,0,&c)) {reason="No saved RCON credential is available for this server.";return false;}
    struct Release { PCREDENTIALW p; ~Release(){ if (p->CredentialBlob) SecureZeroMemory(p->CredentialBlob,p->CredentialBlobSize); CredFree(p); } } secret{c};
    if (!c->CredentialBlobSize || c->CredentialBlobSize>CRED_MAX_CREDENTIAL_BLOB_SIZE) {reason="The saved credential cannot be used.";return false;}
    WSADATA w{};
    if (WSAStartup(MAKEWORD(2,2),&w)) {reason="Winsock initialization failed.";return false;}
    Socket s;
    s.fd=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if (s.fd==INVALID_SOCKET) {reason="Could not open a local TCP socket.";return false;}
    sockaddr_in addr{}; addr.sin_family=AF_INET;addr.sin_port=htons(static_cast<unsigned short>(port));addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if (connect(s.fd,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))!=0) {reason="The configured loopback RCON endpoint did not accept the connection.";return false;}
    if (!is_owned_peer(s.fd,static_cast<unsigned short>(port),expected_pid)) {reason="The loopback endpoint does not belong to the selected running server.";return false;}
    const auto limit=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    int id=0,type=0;
    if (!packet(s.fd,1,3,c->CredentialBlob,c->CredentialBlobSize,limit) || !reply(s.fd,id,type,limit)) {
        reason="The endpoint did not return a valid RCON authentication response.";return false;
    }
    // Some Source implementations precede AUTH_RESPONSE with an empty RESPONSE_VALUE.
    if (id==1 && type==0 && !reply(s.fd,id,type,limit)) {reason="The authentication response was incomplete.";return false;}
    if (id!=1 || type!=2) {reason="The RCON endpoint did not accept the credential.";return false;}
    const BYTE command[]={'l','i','s','t'};
    if (!packet(s.fd,2,2,command,sizeof(command),limit) || !reply(s.fd,id,type,limit) || id!=2 || type!=0) {
        reason="Authenticated, but the server did not answer the player-list command.";return false;
    }
    return true;
#else
    reason="The local credential vault is only available on Windows.";
    return false;
#endif
}
}}}
