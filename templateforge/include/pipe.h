#pragma once
#include <string>
#include <deque>
#include <optional>

// ---------------------------------------------------------------------------
// Pipe — FIFO inter-process communication buffer
// ---------------------------------------------------------------------------
class Pipe {
public:
    explicit Pipe(int id);

    void                    write(const std::string& data);
    std::optional<std::string> read();
    std::optional<std::string> peek() const;

    bool is_empty() const;
    int  size()     const;
    int  id()       const { return id_; }

private:
    int                  id_;
    std::deque<std::string> buffer_;
};
