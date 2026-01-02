#include "IPBlocker.h"
#include "Request.h"
#include <sstream>
#include <iostream>

IPBlocker::IPBlocker() {
    // Empty constructor
}

bool IPBlocker::parseCIDR(const std::string& cidr_string, uint32_t& out_network, uint32_t& out_netmask) {
    // Find the '/' separator
    size_t slash_pos = cidr_string.find('/');
    if (slash_pos == std::string::npos) {
        std::cerr << "Warning: Invalid CIDR notation (no /): " << cidr_string << std::endl;
        return false;
    }
    
    // Extract IP and prefix length
    std::string ip_part = cidr_string.substr(0, slash_pos);
    std::string prefix_part = cidr_string.substr(slash_pos + 1);
    
    // Parse prefix length
    int prefix_length;
    try {
        prefix_length = std::stoi(prefix_part);
    } catch (...) {
        std::cerr << "Warning: Invalid prefix length: " << prefix_part << std::endl;
        return false;
    }
    
    // Validate prefix length (0-32 for IPv4)
    if (prefix_length < 0 || prefix_length > 32) {
        std::cerr << "Warning: Prefix length out of range (0-32): " << prefix_length << std::endl;
        return false;
    }
    
    // Parse IP address
    uint32_t ip_addr = Request::ipToUint32(ip_part);
    if (ip_addr == 0 && ip_part != "0.0.0.0") {
        std::cerr << "Warning: Invalid IP address: " << ip_part << std::endl;
        return false;
    }
    
    // Calculate netmask
    if (prefix_length == 0) {
        out_netmask = 0x00000000;
    } else {
        out_netmask = ~((1u << (32 - prefix_length)) - 1);
    }
    
    // Calculate network address by applying mask
    out_network = ip_addr & out_netmask;
    
    return true;
}

int IPBlocker::loadBlockedRanges(const std::vector<std::string>& cidr_list) {
    int loaded_count = 0;
    
    for (const auto& cidr : cidr_list) {
        uint32_t network, netmask;
        if (parseCIDR(cidr, network, netmask)) {
            blocked_ranges.emplace_back(network, netmask, cidr);
            loaded_count++;
        }
    }
    
    return loaded_count;
}

bool IPBlocker::isBlocked(const std::string& ip) const {
    // Convert IP to 32-bit integer
    uint32_t ip_addr = Request::ipToUint32(ip);
    
    // Invalid IP addresses are not blocked
    if (ip_addr == 0 && ip != "0.0.0.0") {
        return false;
    }
    
    // Check against each blocked range (linear search)
    for (const auto& range : blocked_ranges) {
        // Apply netmask to IP and compare with network address
        if ((ip_addr & range.netmask) == range.network_address) {
            return true;  // IP is in this blocked range
        }
    }
    
    return false;  // IP is not in any blocked range
}
