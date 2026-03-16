#include "rtp_transport.h"

#include <ws2tcpip.h>

#include <algorithm>

RTPTransport::RTPTransport(uint16_t port)
    : port_(port) {}

void RTPTransport::setDestinations(const std::vector<std::string>& destinations) {
    std::vector<Destination> resolved;
    for (const auto& ip : destinations) {
        if (ip.empty()) {
            continue;
        }

        auto duplicate = std::find_if(resolved.begin(), resolved.end(), [&ip](const Destination& entry) {
            return entry.ip == ip;
        });
        if (duplicate != resolved.end()) {
            continue;
        }

        Destination entry;
        entry.ip = ip;
        entry.addr.sin_family = AF_INET;
        entry.addr.sin_port = htons(static_cast<u_short>(port_));
        if (inet_pton(AF_INET, ip.c_str(), &entry.addr.sin_addr) != 1) {
            continue;
        }
        resolved.push_back(std::move(entry));
    }

    std::lock_guard<std::mutex> lock(mutex_);
    destinations_ = std::move(resolved);
}

std::vector<std::string> RTPTransport::destinations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> out;
    out.reserve(destinations_.size());
    for (const auto& entry : destinations_) {
        out.push_back(entry.ip);
    }
    return out;
}

bool RTPTransport::hasDestinations() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !destinations_.empty();
}

int RTPTransport::sendPacket(SOCKET sock, const std::vector<uint8_t>& packet) const {
    std::vector<Destination> destinations;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        destinations = destinations_;
    }

    int success_count = 0;
    for (const auto& entry : destinations) {
        const int rc = sendto(sock,
                              reinterpret_cast<const char*>(packet.data()),
                              static_cast<int>(packet.size()),
                              0,
                              reinterpret_cast<const sockaddr*>(&entry.addr),
                              sizeof(entry.addr));
        if (rc != SOCKET_ERROR) {
            ++success_count;
        }
    }
    return success_count;
}
