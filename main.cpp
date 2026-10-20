#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/ui/Notification.hpp>

#include <chrono>

#include "GroundedJumpBuffer.hpp"

using namespace geode::prelude;

namespace {
    bool bufferEnabled() {
        return Mod::get()->getSettingValue<bool>("buffer-enabled");
    }

    float bufferWindowSeconds() {
        auto ms = Mod::get()->getSettingValue<int64_t>("buffer-window-ms");
        return static_cast<float>(ms) / 1000.f;
    }

    bool indicatorEnabled() {
        return Mod::get()->getSettingValue<bool>("show-indicator");
    }

    // Small on-screen message for testing. Limited to once per 400 ms so that
    // fast repeated fires don't pile up a queue of messages.
    void flashIndicator() {
        using Clock = std::chrono::steady_clock;
        static Clock::time_point lastShown{};
        auto now = Clock::now();
        if (now - lastShown < std::chrono::milliseconds(400)) return;
        lastShown = now;
        Notification::create("Buffered jump fired", NotificationIcon::Success, 0.4f)->show();
    }

    // v0.1 only buffers the plain cube, and only for real players in a level.
    bool isEligible(PlayerObject* player) {
        if (!player->m_gameLayer) return false;
        if (!player->isPlayer1() && !player->isPlayer2()) return false;
        if (player->m_isDead) return false;
        return !(
            player->m_isShip || player->m_isBird || player->m_isBall ||
            player->m_isDart || player->m_isRobot || player->m_isSpider ||
            player->m_isSwing
        );
    }
}

class $modify(BufferedPlayerObject, PlayerObject) {
    struct Fields {
        GroundedJumpBuffer buffer;
    };

    bool pushButton(PlayerButton button) {
        if (button == PlayerButton::Jump && bufferEnabled() && isEligible(this)) {
            m_fields->buffer.onJumpPressed(!m_isOnGround);
        }
        return PlayerObject::pushButton(button);
    }

    bool releaseButton(PlayerButton button) {
        if (button == PlayerButton::Jump && bufferEnabled() && isEligible(this)) {
            m_fields->buffer.onJumpReleased();
        }
        return PlayerObject::releaseButton(button);
    }

    void update(float dt) {
        PlayerObject::update(dt);

        if (!bufferEnabled() || !isEligible(this)) {
            m_fields->buffer.reset();
            return;
        }

        switch (m_fields->buffer.onUpdate(dt, m_isOnGround, bufferWindowSeconds())) {
            case BufferAction::PressJump:
                log::debug("Input Buffer: firing buffered jump");
                if (indicatorEnabled()) flashIndicator();
                // Calling PlayerObject:: here runs the game's original code,
                // so our own hook above does not see (or re-trigger on) it.
                PlayerObject::pushButton(PlayerButton::Jump);
                break;
            case BufferAction::ReleaseJump:
                PlayerObject::releaseButton(PlayerButton::Jump);
                break;
            case BufferAction::None:
                break;
        }
    }
};
