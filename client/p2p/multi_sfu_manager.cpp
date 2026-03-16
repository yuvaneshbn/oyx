#include "multi_sfu_manager.h"

#include <algorithm>

void MultiSFUManager::joinRoom(const std::string& room,
                               const std::string& my_id,
                               std::vector<std::string> peers) {
    std::lock_guard<std::mutex> lock(mutex_);
    room_ = room;
    my_id_ = my_id;
    recomputeLocked(std::move(peers));
}

void MultiSFUManager::updatePeers(const std::vector<std::string>& peers) {
    std::lock_guard<std::mutex> lock(mutex_);
    recomputeLocked(peers);
}

std::vector<std::string> MultiSFUManager::getMixersToSendTo() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return mixers_;
}

std::string MultiSFUManager::getMixerToReceiveFrom() const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = listener_to_mixer_.find(my_id_);
    if (it != listener_to_mixer_.end()) {
        return it->second;
    }
    return mixers_.empty() ? std::string() : mixers_.front();
}

bool MultiSFUManager::amIMixer() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::find(mixers_.begin(), mixers_.end(), my_id_) != mixers_.end();
}

void MultiSFUManager::recomputeLocked(std::vector<std::string> peers) {
    if (!my_id_.empty()) {
        peers.push_back(my_id_);
    }

    std::sort(peers.begin(), peers.end());
    peers.erase(std::unique(peers.begin(), peers.end()), peers.end());

    mixers_.clear();
    listener_to_mixer_.clear();

    const size_t mixer_count = std::min<size_t>(3, peers.size());
    mixers_.assign(peers.begin(), peers.begin() + static_cast<long long>(mixer_count));
    if (mixers_.empty()) {
        return;
    }

    for (size_t i = 0; i < peers.size(); ++i) {
        listener_to_mixer_[peers[i]] = mixers_[i % mixers_.size()];
    }
}
