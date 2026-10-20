#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/ui/Notification.hpp>

#include <chrono>
#include <string>

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

    bool debugEnabled() {
        return Mod::get()->getSettingValue<bool>("debug-messages");
    }

    void showMessage(std::string const& text) {
        Notification::create(text, NotificationIcon::Success, 1.2f)->show();
    }

    // Small "it fired" message. Limited to once per 400 ms so fast repeated
    // fires don't pile up a queue of messages.
    void flashIndicator() {
        using Clock = std::chrono::steady_clock;
        static Clock::time_point lastShown{};
        auto now = Clock::now();
        if (now - lastShown < std::chrono::milliseconds(400)) return;
        lastShown = now;
        Notification::create("Buffered jump fired", NotificationIcon::Success, 0.4f)->show();
    }

    // Events of the tap currently being followed (debug messages only).
    std::string g_trace;

    // Returns nullptr if this player may be buffered, otherwise the reason why not.
    // v0.1 only buffers the plain cube, and only for real players in a level.
    const char* ineligibleReason(PlayerObject* player) {
        if (!player->m_gameLayer) return "no gameLayer";
        if (!player->isPlayer1() && !player->isPlayer2()) return "not P1/P2";
        if (player->m_isDead) return "dead";
        if (
            player->m_isShip || player->m_isBird || player->m_isBall ||
            player->m_isDart || player->m_isRobot || player->m_isSpider ||
            player->m_isSwing
        ) return "not cube";
        return nullptr;
    }

    std::string describe(BufferReport const& r) {
        auto ms = std::to_string(static_cast<int>(r.elapsed * 1000.f));
        switch (r.note) {
            case BufferNote::Fired:
                return "FIRED after " + ms + "ms";
            case BufferNote::Expired:
                return "EXPIRED at " + ms + "ms, ground=" + std::string(r.grounded ? "yes" : "no");
            case BufferNote::LandedWhileHeld:
                return "LANDED but jump still held";
            case BufferNote::None:
                break;
        }
        return "";
    }
}

class $modify(BufferedPlayerObject, PlayerObject) {
    struct Fields {
        GroundedJumpBuffer buffer;
        bool announced = false;
    };

    bool pushButton(PlayerButton button) {
        if (button == PlayerButton::Jump && bufferEnabled()) {
            auto why = ineligibleReason(this);
            if (!why) {
                bool air = !m_isOnGround;
                m_fields->buffer.onJumpPressed(air);
                if (debugEnabled()) {
                    if (air) g_trace = "PUSH(air)";
                    else showMessage("PUSH on ground (nothing to buffer)");
                }
            }
            else if (debugEnabled()) {
                showMessage(std::string("PUSH ignored: ") + why);
            }
        }
        return PlayerObject::pushButton(button);
    }

    bool releaseButton(PlayerButton button) {
        if (button == PlayerButton::Jump && bufferEnabled() && !ineligibleReason(this)) {
            m_fields->buffer.onJumpReleased();
            if (debugEnabled() && !g_trace.empty()) g_trace += " > REL";
        }
        return PlayerObject::releaseButton(button);
    }

    void update(float dt) {
        PlayerObject::update(dt);

        if (!bufferEnabled() || ineligibleReason(this)) {
            m_fields->buffer.reset();
            return;
        }

        if (debugEnabled() && !m_fields->announced) {
            m_fields->announced = true;
            showMessage(isPlayer1() ? "Input Buffer: P1 update running" : "Input Buffer: P2 update running");
        }

        auto action = m_fields->buffer.onUpdate(dt, m_isOnGround, bufferWindowSeconds());

        if (action == BufferAction::PressJump) {
            log::debug("Input Buffer: firing buffered jump");
            // Calling PlayerObject:: here runs the game's original code,
            // so our own hook above does not see (or re-trigger on) it.
            PlayerObject::pushButton(PlayerButton::Jump);
        }
        else if (action == BufferAction::ReleaseJump) {
            PlayerObject::releaseButton(PlayerButton::Jump);
        }

        auto report = m_fields->buffer.takeReport();
        if (report.note != BufferNote::None) {
            if (debugEnabled()) {
                if (g_trace.empty()) g_trace = "(no PUSH seen)";
                showMessage(g_trace + " > " + describe(report));
                g_trace.clear();
            }
            else if (report.note == BufferNote::Fired && indicatorEnabled()) {
                flashIndicator();
            }
        }
    }
};
