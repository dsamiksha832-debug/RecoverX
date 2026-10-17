#include "recoverx/versionchain.hpp"

namespace recoverx {

VersionChain::VersionChain() = default;
VersionChain::~VersionChain() = default;

void VersionChain::append(const VersionInfo& version) {
    auto node = std::make_unique<Node>(version);
    Node* raw = node.get();

    if (!head_) {
        head_ = std::move(node);
        tail_ = raw;
    } else {
        node->previous = tail_;
        tail_->next = std::move(node);
        tail_ = raw;
    }
    ++size_;
}

std::vector<VersionInfo> VersionChain::newestFirst() const {
    std::vector<VersionInfo> result;
    result.reserve(size_);
    for (Node* current = tail_; current != nullptr; current = current->previous) {
        result.push_back(current->value);
    }
    return result;
}

std::size_t VersionChain::size() const {
    return size_;
}

} // namespace recoverx
