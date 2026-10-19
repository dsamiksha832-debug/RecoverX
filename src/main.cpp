
#include "recoverx/crc32.hpp"
#include "recoverx/checksum.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

// Display all available RecoverX commands.
void showHelp() {
    cout << "\n========== RecoverX CLI ==========\n";
    cout << "Usage: recoverx <command> [arguments]\n\n";

    cout << "Available commands:\n";
    cout << "  help                         Show this help menu\n";
    cout << "  checksum                     Test CRC-32 and Internet checksum\n";
    cout << "  backup <file>                Create a backup (not implemented)\n";
    cout << "  list <file>                  List backup versions (not implemented)\n";
    cout << "  verify <file> <version-id>   Verify a version (not implemented)\n";
    cout << "  recover <file>               Recover a valid version (not implemented)\n";
    cout << "  undo <file>                  Undo recovery (not implemented)\n";
    cout << "  corrupt <file> <version-id> <mode>\n";
    cout << "                               Corrupt stored data (not implemented)\n";
    cout << "  bench                        Run experiments (not implemented)\n";

    cout << "\nCorruption modes:\n";
    cout << "  bitflip, burst, swap, truncate, zero, deletechunk\n";
    cout << "==================================\n";
}

// Test the existing checksum functions.
void runChecksum() {
    const vector<std::uint8_t> sample = {
        'R', 'e', 'c', 'o', 'v', 'e', 'r', 'X'
    };

    cout << "RecoverX checksum test\n";
    cout << "CRC-32: 0x" << hex << recoverx::crc32(sample) << '\n';
    cout << "Internet checksum: 0x"
         << recoverx::internetChecksum(sample) << '\n';

    cout << dec;
}

// Display a message for commands that are not implemented yet.
void showNotImplemented(const string& command) {
    cout << "The '" << command
         << "' command is not implemented yet.\n";
    cout << "No files have been modified.\n";
}

int main(int argc, char* argv[]) {
    // Show help if no command is provided.
    if (argc < 2) {
        showHelp();
        return 0;
    }

    string command = argv[1];

    // Handle the help command.
    if (command == "help" || command == "--help" || command == "-h") {
        showHelp();
        return 0;
    }

    // Run the existing checksum demonstration.
    if (command == "checksum") {
        runChecksum();
        return 0;
    }

    // Validate and handle backup.
    if (command == "backup") {
        if (argc != 3) {
            cout << "Usage: recoverx backup <file>\n";
            return 1;
        }

        showNotImplemented(command);
        return 0;
    }

    // Validate and handle list.
    if (command == "list") {
        if (argc != 3) {
            cout << "Usage: recoverx list <file>\n";
            return 1;
        }

        showNotImplemented(command);
        return 0;
    }

    // Validate and handle verify.
    if (command == "verify") {
        if (argc != 4) {
            cout << "Usage: recoverx verify <file> <version-id>\n";
            return 1;
        }

        showNotImplemented(command);
        return 0;
    }

    // Validate and handle recover.
    if (command == "recover") {
        if (argc != 3) {
            cout << "Usage: recoverx recover <file>\n";
            return 1;
        }

        showNotImplemented(command);
        return 0;
    }

    // Validate and handle undo.
    if (command == "undo") {
        if (argc != 3) {
            cout << "Usage: recoverx undo <file>\n";
            return 1;
        }

        showNotImplemented(command);
        return 0;
    }

    // Validate and handle corrupt.
    if (command == "corrupt") {
        if (argc != 5) {
            cout << "Usage: recoverx corrupt <file> <version-id> <mode>\n";
            return 1;
        }

        showNotImplemented(command);
        return 0;
    }

    // Handle benchmark command.
    if (command == "bench") {
        if (argc != 2) {
            cout << "Usage: recoverx bench\n";
            return 1;
        }

        showNotImplemented(command);
        return 0;
    }

    // Handle unknown commands.
    cout << "Unknown command: " << command << '\n';
    cout << "Run 'recoverx help' to see available commands.\n";
    return 1;
}
