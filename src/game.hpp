#pragma once

#include <memory>
#include <string>

class Game {
public:
    Game();
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) noexcept = default;
    Game& operator=(Game&&) noexcept = default;

    void Init();
    void Shutdown();
    bool ShouldClose() const;
    void Tick();
    bool SaveScreenshot(const std::string& mode, const std::string& path);
    bool RunVerification(std::string* report);

private:
    class Impl;
    std::unique_ptr<Impl> impl;
};
