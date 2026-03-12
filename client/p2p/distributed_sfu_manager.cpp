#include "distributed_sfu_manager.h"

#include <algorithm>
#include <chrono>
#include <iostream>

namespace {
constexpr int ELECTION_INTERVAL_MS = 500;
} // namespace

DistributedSFUManager::DistributedSFUManager(std::string my_id)
    : my_id_(std::move(my_id)) {}

DistributedSFUManager::~DistributedSFUManager() {
    stop();
}

void DistributedSFUManager::start() {
    if (running_.load()) {
        return;
    }
    running_.store(true);
    discovery_.start(my_id_);
    thread_ = std::thread(&DistributedSFUManager::electionLoop, this);
}

void DistributedSFUManager::stop() {
    running_.store(false);
    discovery_.stop();
    if (thread_.joinable()) {
        thread_.join();
    }
    if (sfu_server_.isRunning()) {
        sfu_server_.stop();
    }
}

std::string DistributedSFUManager::currentSfuId() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return sfu_id_;
}

std::string DistributedSFUManager::currentSfuIp() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return sfu_ip_;
}

bool DistributedSFUManager::isSelfSfu() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return self_sfu_;
}

void DistributedSFUManager::setOnSfuChanged(std::function<void(const std::string&, bool)> callback) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    on_sfu_changed_ = std::move(callback);
}

void DistributedSFUManager::applyLeader(const std::string& leader_id,
                                        const std::string& leader_ip,
                                        bool self_leader) {
    std::function<void(const std::string&, bool)> callback;
    bool changed = false;

    {
        std::lock_guard<std::mutex> lock(state_mutex_);
        if (sfu_id_ != leader_id || sfu_ip_ != leader_ip || self_sfu_ != self_leader) {
            sfu_id_ = leader_id;
            sfu_ip_ = leader_ip;
            self_sfu_ = self_leader;
            callback = on_sfu_changed_;
            changed = true;
        }
    }

    if (!changed) {
        return;
    }

    if (self_leader) {
        if (!sfu_server_.isRunning()) {
            if (!sfu_server_.start()) {
                std::cerr << "[SFU] Failed to start embedded server\n";
            } else {
                std::cout << "[SFU] Embedded server started\n";
            }
        }
    } else {
        if (sfu_server_.isRunning()) {
            sfu_server_.stop();
            std::cout << "[SFU] Embedded server stopped (new leader: " << leader_id << ")\n";
        }
    }

    if (callback && !leader_ip.empty()) {
        callback(leader_ip, self_leader);
    }
}

void DistributedSFUManager::electionLoop() {
    while (running_.load()) {
        auto peers = discovery_.peers();

        std::string leader_id = my_id_;
        for (const auto& peer : peers) {
            if (!peer.id.empty() && peer.id < leader_id) {
                leader_id = peer.id;
            }
        }

        bool self_leader = leader_id == my_id_;
        std::string leader_ip;
        if (self_leader) {
            leader_ip = "127.0.0.1";
        } else {
            for (const auto& peer : peers) {
                if (peer.id == leader_id) {
                    leader_ip = peer.ip;
                    break;
                }
            }
        }

        applyLeader(leader_id, leader_ip, self_leader);

        for (int i = 0; i < ELECTION_INTERVAL_MS / 50 && running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
}
