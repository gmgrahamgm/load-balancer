#ifndef IPBLOCKER_H
#define IPBLOCKER_H

#include <string>
#include <vector>
#include <cstdint>

/**
 * Utility class for blocking IP addresses using CIDR notation
 */
class IPBlocker {
private:
    /**
     * Represents a blocked IP range in CIDR notation
     */
    struct BlockedRange {
        uint32_t network_address;  // Base address of the network
        uint32_t netmask;           // Network mask for matching
        std::string cidr_string;    // Original CIDR string for logging
        
        BlockedRange(uint32_t addr, uint32_t mask, const std::string& cidr)
            : network_address(addr), netmask(mask), cidr_string(cidr) {}
    };
    
    std::vector<BlockedRange> blocked_ranges;
    
    /**
     * Parse CIDR notation string into network address and netmask
     * @param cidr_string CIDR notation (e.g., "192.168.1.0/24")
     * @param out_network Output: network address
     * @param out_netmask Output: netmask
     * @return true if parsing succeeded, false otherwise
     */
    bool parseCIDR(const std::string& cidr_string, uint32_t& out_network, uint32_t& out_netmask);
    
public:
    /**
     * Default constructor - creates empty blocker
     */
    IPBlocker();
    
    /**
     * Load blocked IP ranges from CIDR notation strings
     * @param cidr_list Vector of CIDR strings (e.g., {"192.168.1.0/24", "10.0.0.0/8"})
     * @return Number of successfully parsed ranges
     */
    int loadBlockedRanges(const std::vector<std::string>& cidr_list);
    
    /**
     * Check if an IP address is blocked
     * @param ip IP address string in dotted-decimal notation
     * @return true if IP is in any blocked range, false otherwise
     */
    bool isBlocked(const std::string& ip) const;
    
    /**
     * Get the number of blocked ranges currently loaded
     */
    size_t getBlockedRangeCount() const { return blocked_ranges.size(); }
    
    /**
     * Clear all blocked ranges
     */
    void clear() { blocked_ranges.clear(); }
};

#endif // IPBLOCKER_H
