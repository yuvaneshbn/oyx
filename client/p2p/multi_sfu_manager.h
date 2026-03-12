#ifndef MULTI_SFU_MANAGER_H
#define MULTI_SFU_MANAGER_H

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

class MultiSFUManager {
public:
    void joinRoom(const std::string& room, const std::string& my_id, std::vector<std::string> peers);
    void updatePeers(const std::vector<std::string>& peers);

    std::vector<std::string> getMixersToSendTo() const;
    std::string getMixerToReceiveFrom() const;
    bool amIMixer() const;

private:
    void recomputeLocked(std::vector<std::string> peers);

    std::string my_id_;
    std::string room_;
    std::vector<std::string> mixers_;
    std::unordered_map<std::string, std::string> listener_to_mixer_;
    mutable std::mutex mutex_;
};

#endif
