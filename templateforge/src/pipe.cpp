#include "pipe.h"
#include <stdexcept>

Pipe::Pipe(int id) : id_(id) {}

void Pipe::write(const std::string& data) {
    buffer_.push_back(data);
}

std::optional<std::string> Pipe::read() {
    if (buffer_.empty()) return std::nullopt;
    std::string front = buffer_.front();
    buffer_.pop_front();
    return front;
}

std::optional<std::string> Pipe::peek() const {
    if (buffer_.empty()) return std::nullopt;
    return buffer_.front();
}

bool Pipe::is_empty() const { return buffer_.empty(); }
int  Pipe::size()     const { return static_cast<int>(buffer_.size()); }
