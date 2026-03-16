#ifndef DISTRIBUTED_SFU_MANAGER_H
#define DISTRIBUTED_SFU_MANAGER_H

#include "peer_discovery.h"
#include "../server/voice_server.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

class DistributedSFUManager {
public:
    explicit DistributedSFUManager(std::string my_id);
    ~DistributedSFUManager();

    void start();
    void stop();

    std::string currentSfuId() const;
    std::string currentSfuIp() const;
    bool isSelfSfu() const;

    void setOnSfuChanged(std::function<void(const std::string&, bool)> callback);

private:
    void electionLoop();
    void applyLeader(const std::string& leader_id, const std::string& leader_ip, bool self_leader);

    std::string my_id_;
    PeerDiscovery discovery_;
    VoiceServer sfu_server_;

    std::atomic<bool> running_{false};
    std::thread thread_;

    mutable std::mutex state_mutex_;
    std::string sfu_id_;
    std::string sfu_ip_;
    bool self_sfu_ = false;

    std::function<void(const std::string&, bool)> on_sfu_changed_;
};

#endif
