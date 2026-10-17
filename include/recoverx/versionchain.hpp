#pragma once
#include "recoverx/common.hpp"
#include <cstddef>
#include <memory>
#include <vector>

namespace recoverx {

// Doubly linked list is intentional here because version history is a core DSA.
// Keep ownership private; expose operations rather than raw node pointers.
class VersionChain {
public:
    VersionChain();
    ~VersionChain();

    VersionChain(const VersionChain&) = delete;
    VersionChain& operator=(const VersionChain&) = delete;

    void append(const VersionInfo& version);
    std::vector<VersionInfo> newestFirst() const;
    std::size_t size() const;

private:
    struct Node {
        VersionInfo value;
        std::unique_ptr<Node> next;
        Node* previous = nullptr;
        explicit Node(const VersionInfo& v) : value(v) {}
    };

    std::unique_ptr<Node> head_;
    Node* tail_ = nullptr; // non-owning pointer to the final node
    std::size_t size_ = 0;
};

} // namespace recoverx
