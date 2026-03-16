#include "audio/audio_engine.h"
#include "net/control_client.h"
#include "p2p/distributed_sfu_manager.h"
#include "ui/main_window.h"
#include "ui/components/startup_dialog.h"
#include "../../shared/winsock_init.h"

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QFile>
#include <QMetaObject>
#include <QMessageBox>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>

namespace {
constexpr const char* DEFAULT_ROOM = "main";

struct RegisterOutcome {
    bool ok = false;
    bool taken = false;
    std::string message;
};

RegisterOutcome register_client(const std::string& server_ip,
                                const std::string& client_id,
                                int audio_port,
                                const std::string& secret) {
    const std::string command = "REGISTER:" + client_id + ":" + std::to_string(audio_port) + ":" + secret;
    const auto resp = send_control_command(server_ip, command);
    if (!resp.ok) {
        std::cerr << "[CLIENT] Registration error: " << resp.response << "\n";
        return {false, false, resp.response};
    }
    if (resp.response == "TAKEN") {
        std::cerr << "[CLIENT] Client ID " << client_id << " already taken\n";
        return {false, true, "TAKEN"};
    }
    if (resp.response != "OK") {
        std::cerr << "[CLIENT] Unexpected registration response: " << resp.response << "\n";
        return {false, false, resp.response};
    }

    const auto join_resp = send_control_command(server_ip, "JOIN:" + client_id + ":" + DEFAULT_ROOM);
    if (!join_resp.ok || join_resp.response.rfind("OK", 0) != 0) {
        std::cerr << "[CLIENT] JOIN failed for client " << client_id << ": " << join_resp.response << "\n";
        return {false, false, join_resp.response};
    }
    return {true, false, "OK"};
}

QString app_icon_path() {
    QDir dir(QCoreApplication::applicationDirPath());
    const QString icon_path = dir.filePath("technical-support.ico");
    return icon_path;
}

std::string wait_for_sfu_ip(DistributedSFUManager& manager, int timeout_ms) {
    const auto start = std::chrono::steady_clock::now();
    while (true) {
        const auto ip = manager.currentSfuIp();
        if (!ip.empty()) {
            return ip;
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        if (elapsed.count() >= timeout_ms) {
            return {};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace

int main(int argc, char* argv[]) {
    WinSockInit wsa;
    if (!wsa.ok()) {
        std::cerr << "[CLIENT] Winsock initialization failed\n";
        return 1;
    }

    QApplication app(argc, argv);
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    const QString icon_path = app_icon_path();
    if (QFile::exists(icon_path)) {
        app.setWindowIcon(QIcon(icon_path));
    }

    AudioEngine audio;
    const int audio_port = audio.port();
    std::cout << "[CLIENT] Audio engine initialized on port " << audio_port << "\n";

    const char* secret_env = std::getenv("VOICE_REGISTER_SECRET");
    const std::string secret = secret_env ? secret_env : "mysecret";

    QString client_id;
    std::unique_ptr<DistributedSFUManager> sfu_manager;
    std::string sfu_ip;

    while (true) {
        StartupDialog startup("Auto (LAN)", audio_port);
        if (startup.exec() != QDialog::Accepted) {
            std::cout << "[CLIENT] User cancelled client setup, exiting\n";
            audio.shutdown();
            return 0;
        }

        client_id = startup.clientId();
        std::cout << "[CLIENT] Selected Client ID: " << client_id.toStdString() << "\n";

        sfu_manager = std::make_unique<DistributedSFUManager>(client_id.toStdString());
        sfu_manager->start();

        sfu_ip = wait_for_sfu_ip(*sfu_manager, 4000);
        if (sfu_ip.empty()) {
            QMessageBox msg;
            msg.setIcon(QMessageBox::Warning);
            msg.setWindowTitle("SFU Discovery Failed");
            msg.setText("Could not determine the current SFU.");
            msg.setInformativeText("Please ensure other peers are reachable, or try again.");
            msg.exec();
            sfu_manager->stop();
            continue;
        }

        bool registered = false;
        RegisterOutcome outcome;
        for (int attempt = 0; attempt < 5; ++attempt) {
            outcome = register_client(sfu_ip, client_id.toStdString(), audio_port, secret);
            if (outcome.ok || outcome.taken) {
                registered = outcome.ok;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }

        if (registered) {
            break;
        }

        QMessageBox msg;
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle("Registration Failed");
        if (outcome.taken) {
            msg.setText(QString("Client ID %1 is already in use.").arg(client_id));
            msg.setInformativeText("Please choose a different client ID and try again.");
        } else {
            msg.setText("Could not register with the SFU.");
            msg.setInformativeText(QString::fromStdString(outcome.message));
        }
        msg.exec();

        sfu_manager->stop();
    }

    audio.setClientId(client_id.toStdString());
    std::cout << "[CLIENT] Registration successful - starting UI...\n";

    MainWindow window(client_id, QString::fromStdString(sfu_ip), &audio, QString::fromStdString(secret));

    sfu_manager->setOnSfuChanged([&window](const std::string& ip, bool self_sfu) {
        QMetaObject::invokeMethod(&window, [ip, self_sfu, &window]() {
            window.onSfuChanged(QString::fromStdString(ip), self_sfu);
        }, Qt::QueuedConnection);
    });

    window.show();
    const int rc = app.exec();
    if (sfu_manager) {
        sfu_manager->stop();
    }
    return rc;
}
