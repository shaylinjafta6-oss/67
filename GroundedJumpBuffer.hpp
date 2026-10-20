#pragma once

// Pure logic for the grounded-jump buffer. It knows nothing about Geode or
// Geometry Dash, so it can be tested on its own. A future "predictive" module
// must live in a separate file and never be mixed into this one.

enum class BufferAction {
    None,
    PressJump,
    ReleaseJump,
};

// What happened to a buffered tap (used for on-screen debug messages).
enum class BufferNote {
    None,
    Fired,            // landed inside the window, buffered jump was fired
    Expired,          // window ran out before a jump could be fired
    LandedWhileHeld,  // landed but the button was still considered held
};

struct BufferReport {
    BufferNote note = BufferNote::None;
    float elapsed = 0.f;   // seconds since the tap
    bool grounded = false; // were we on the ground when this was decided
};

class GroundedJumpBuffer {
public:
    // Forget everything (death, mode change, setting turned off).
    void reset() {
        m_state = State::Idle;
        m_elapsed = 0.f;
        m_held = false;
        m_report = {};
    }

    // The player pressed jump. `airborne` is true if they were not on the ground.
    void onJumpPressed(bool airborne) {
        m_held = true;
        if (airborne) {
            m_state = State::Armed;
            m_elapsed = 0.f;
        }
        else {
            // A normal press on the ground: the game handles it, we step aside.
            m_state = State::Idle;
        }
    }

    // The player released jump.
    void onJumpReleased() {
        m_held = false;
        if (m_state == State::Pressing) {
            m_state = State::Idle;
        }
    }

    // Call once per player update. Returns what the caller should do.
    BufferAction onUpdate(float dt, bool grounded, float windowSeconds) {
        switch (m_state) {
            case State::Idle:
                return BufferAction::None;

            case State::Armed:
                m_elapsed += dt;
                if (m_elapsed > windowSeconds) {
                    m_report = {BufferNote::Expired, m_elapsed, grounded};
                    m_state = State::Idle; // tapped too early
                    return BufferAction::None;
                }
                if (!grounded) {
                    return BufferAction::None; // still in the air, keep waiting
                }
                if (m_held) {
                    // Still holding: the game's own hold-to-jump covers this.
                    m_report = {BufferNote::LandedWhileHeld, m_elapsed, true};
                    m_state = State::Idle;
                    return BufferAction::None;
                }
                // Tapped and released in the air, now landed inside the window.
                m_report = {BufferNote::Fired, m_elapsed, true};
                m_state = State::Pressing;
                m_elapsed = 0.f;
                m_held = true;
                return BufferAction::PressJump;

            case State::Pressing:
                m_elapsed += dt;
                if (!grounded || m_elapsed > kPressTimeoutSeconds) {
                    m_state = State::Idle; // jump started (or we gave up)
                    m_held = false;
                    return BufferAction::ReleaseJump;
                }
                return BufferAction::None;
        }
        return BufferAction::None;
    }

    // Returns the latest report once, then clears it.
    BufferReport takeReport() {
        BufferReport r = m_report;
        m_report = {};
        return r;
    }

private:
    enum class State { Idle, Armed, Pressing };

    static constexpr float kPressTimeoutSeconds = 0.05f;

    State m_state = State::Idle;
    float m_elapsed = 0.f;
    bool m_held = false;
    BufferReport m_report;
};
