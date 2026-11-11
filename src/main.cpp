#include "content_data.h"
#include "content_sprites.h"
#include "converters.h"

#include <google/protobuf/stubs/common.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <iostream>
#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace {

// Rewrites <from>/Tibia.dat and Tibia.spr in the format of another client version, into <to>/.
bool convertClientVersion(int fromVersion, int toVersion, bool useAlpha) {
    const fs::path from = std::to_string(fromVersion);
    const fs::path to = std::to_string(toVersion);
    if (!fs::exists(from / "Tibia.dat") || !fs::exists(from / "Tibia.spr")) {
        std::cerr << "Tibia.dat or Tibia.spr doesn't exist\n";
        return false;
    }

    fs::create_directories(to);

    const otu::ContentData dat(otu::readFile(from / "Tibia.dat"), fromVersion);
    const auto newDat = dat.convertTo(toVersion);
    otu::writeFile(to / "Tibia.dat", newDat.data(), newDat.size());

    const otu::ContentSprites spr(otu::readFile(from / "Tibia.spr"), fromVersion, useAlpha);
    const auto newSpr = spr.convertTo(toVersion);
    otu::writeFile(to / "Tibia.spr", newSpr.data(), newSpr.size());

    std::cout << "Conversion to " << toVersion << " successful.\n";
    return true;
}

bool parseInt(const std::string& text, int& out) {
    try {
        size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used != text.size())
            return false;
        out = value;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace

int main(int argc, char** argv) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    constexpr const char* usage = "Options: --version=<n> (required), --build-version=<n> (required for 11+ clients), "
                                  "--alpha=<y/n>, --convert-to=<n>";

    int clientVersion = -1;
    int buildVersion = -1;
    int convertTo = -1;
    bool useAlpha = false;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        std::string value;
        if (const auto eq = arg.find('='); eq != std::string::npos) {
            value = arg.substr(eq + 1);
            arg.resize(eq);
        } else if (i + 1 < argc && !std::string_view(argv[i + 1]).starts_with("--")) {
            value = argv[++i];
        }

        if (arg == "--version") {
            if (!parseInt(value, clientVersion))
                clientVersion = -1;
        } else if (arg == "--build-version") {
            if (!parseInt(value, buildVersion))
                buildVersion = -1;
        } else if (arg == "--alpha") {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });
            useAlpha = value == "y" || value == "yes" || value == "true" || value == "1";
        } else if (arg == "--convert-to") {
            if (!parseInt(value, convertTo)) {
                std::cerr << "Invalid --convert-to value: " << value << '\n' << usage << '\n';
                return 1;
            }
        } else {
            std::cerr << "Unknown option: " << arg << '\n' << usage << '\n';
            return 1;
        }
    }

    if (clientVersion == -1) {
        std::cerr << "Invalid client version.\n" << usage << '\n';
        return 1;
    }
    if (clientVersion >= 1100 && buildVersion == -1) {
        std::cerr << "Invalid build version.\n";
        return 1;
    }

    if (clientVersion >= 1100)
        std::cout << "Loading version: " << clientVersion << '.' << buildVersion << '\n';
    else
        std::cout << "Loading version: " << clientVersion << '\n';

    const auto started = std::chrono::steady_clock::now();
    bool ok = false;
    try {
        if (convertTo != -1) {
            std::cout << "Converting to: " << convertTo << '\n';
            ok = convertClientVersion(clientVersion, convertTo, useAlpha);
        } else if (clientVersion < 1100) {
            ok = otu::convertLegacy(clientVersion, useAlpha);
        } else {
            ok = otu::convertProtobuf(clientVersion, buildVersion);
        }
    } catch (const std::exception& e) {
        std::cout << std::flush;
        std::cerr << "\nError: " << e.what() << '\n';
    }

    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - started;
    std::cout << "Time elapsed: " << elapsed.count() << " seconds.\n";

    google::protobuf::ShutdownProtobufLibrary();
    return ok ? 0 : 1;
}
