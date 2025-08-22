#pragma once

/**
 * @brief A generic threshold detector with hysteresis.
 *
 * This class template monitors a value of type T and determines whether it is above or below a specified limit,
 * incorporating hysteresis to prevent rapid state changes due to small fluctuations around the threshold.
 *
 * @tparam T           The type of the value to be monitored (e.g., int, float).
 * @tparam Limit       The threshold value for detection.
 * @tparam Hysteresis  The hysteresis value to avoid frequent toggling near the threshold.
 *
 * Usage:
 * - Call setValue() to update the monitored value.
 * - Use isOver() and isUnder() to query the current state.
 *
 * State transitions:
 * - Starts in Unknown state.
 * - Transitions to Over if value exceeds Limit.
 * - Transitions to Under if value falls below Limit.
 * - Uses hysteresis to avoid rapid toggling between Over and Under states.
 */
template <typename T, T Limit, T Hysteresis>
class ConstThresholdDetector {
public:
    ConstThresholdDetector()
        : value(T()), state(State::Unknown) {}

    void setValue(T val) {
        value = val;
        switch (state) {
            case State::Unknown:
                state = (value > Limit) ? State::Over : State::Under;
                break;
            case State::Under:
                if (value > Limit + Hysteresis) {
                    state = State::Over;
                }
                break;
            case State::Over:
                if (value < Limit - Hysteresis) {
                    state = State::Under;
                }
                break;
        }
    }
    
    T getValue() const { return value; }
    bool isOver() const { return state == State::Over; }
    bool isUnder() const { return state == State::Under; }

private:
    enum class State { Unknown, Under, Over };
    T value;
    State state;
};

/**
 * @brief A generic threshold detector with optional hysteresis.
 *
 * This class monitors a value of type T and determines whether it is above or below a specified threshold (`limit`),
 * with an optional hysteresis (`hysteresis`, default is zero) to prevent rapid toggling between states.
 * The detector maintains an internal state (`Unknown`, `Under`, or `Over`) and updates it as new values are set.
 *
 * @tparam T Type of the value to be monitored (e.g., int, float).
 * @param limit The threshold value for detection.
 * @param hysteresis The hysteresis value to avoid frequent toggling (default is zero).
 * 
 * Usage:
 * - Call setValue() to update the monitored value.
 * - Use isOver() and isUnder() to query the current state.
 *
 */
template <typename T>
class ThresholdDetector {
public:
    ThresholdDetector(T limit, T hysteresis = T())
        : limit(limit), hysteresis(hysteresis), value(T()), state(State::Unknown) {}

    void setValue(T val) {
        value = val;
        switch (state) {
            case State::Unknown:
                state = (value > limit) ? State::Over : State::Under;
                break;
            case State::Under:
                if (value > limit + hysteresis) {
                    state = State::Over;
                }
                break;
            case State::Over:
                if (value < limit - hysteresis) {
                    state = State::Under;
                }
                break;
        }
    }
    void setLimit(T newLimit) {
        limit = newLimit;
    }
    void setHysteresis(T newHysteresis) {
        hysteresis = newHysteresis;
    }

    T getValue() const { return value; }
    T getLimit() const { return limit; }
    T getHysteresis() const { return hysteresis; }
    bool isOver() const { return state == State::Over; }
    bool isUnder() const { return state == State::Under; }

private:
    enum class State { Unknown, Under, Over };
    T limit;
    T hysteresis;
    T value;
    State state;
};