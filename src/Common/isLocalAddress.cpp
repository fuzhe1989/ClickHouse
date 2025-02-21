#include <Common/isLocalAddress.h>

#include <ifaddrs.h>
#include <cstring>
#include <optional>
#include <common/types.h>
#include <Common/Exception.h>
#include <Poco/Net/IPAddress.h>
#include <Poco/Net/SocketAddress.h>


namespace DB
{

namespace ErrorCodes
{
    extern const int SYSTEM_ERROR;
}

namespace
{

struct NetworkInterfaces
{
    ifaddrs * ifaddr;
    NetworkInterfaces()
    {
        if (getifaddrs(&ifaddr) == -1)
        {
            throwFromErrno("Cannot getifaddrs", ErrorCodes::SYSTEM_ERROR);
        }
    }

    bool hasAddress(const Poco::Net::IPAddress & address) const
    {
        ifaddrs * iface;
        for (iface = ifaddr; iface != nullptr; iface = iface->ifa_next)
        {
            auto family = iface->ifa_addr->sa_family;
            std::optional<Poco::Net::IPAddress> interface_address;
            switch (family)
            {
                /// We interested only in IP-adresses
                case AF_INET:
                {
                    interface_address.emplace(*(iface->ifa_addr));
                    break;
                }
                case AF_INET6:
                {
                    interface_address.emplace(&reinterpret_cast<const struct sockaddr_in6*>(iface->ifa_addr)->sin6_addr, sizeof(struct in6_addr));
                    break;
                }
                default:
                    continue;
            }

            /** Compare the addresses without taking into account `scope`.
              * Theoretically, this may not be correct - depends on `route` setting
              *  - through which interface we will actually access the specified address.
              */
            if (interface_address->length() == address.length()
                && 0 == memcmp(interface_address->addr(), address.addr(), address.length()))
                return true;
        }
        return false;
    }

    ~NetworkInterfaces()
    {
        freeifaddrs(ifaddr);
    }
};

}


bool isLocalAddress(const Poco::Net::IPAddress & address)
{
    NetworkInterfaces interfaces;
    return interfaces.hasAddress(address);
}

bool isLocalAddress(const Poco::Net::SocketAddress & address, UInt16 clickhouse_port)
{
    return clickhouse_port == address.port() && isLocalAddress(address.host());
}


size_t getHostNameDifference(const std::string & local_hostname, const std::string & host)
{
    size_t hostname_difference = 0;
    for (size_t i = 0; i < std::min(local_hostname.length(), host.length()); ++i)
        if (local_hostname[i] != host[i])
            ++hostname_difference;
    return hostname_difference;
}

}
