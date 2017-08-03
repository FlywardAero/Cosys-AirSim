#pragma once

#include <vector>
#include <cstdint>
#include "interfaces/IBoardClock.hpp"
#include "interfaces/IBoardInputPins.hpp"
#include "interfaces/IGoalInput.hpp"
#include "interfaces/CommonStructs.hpp"

namespace simple_flight {

class RemoteControl : public IGoalInput {
public:
    RemoteControl(const Params* params, const IBoardClock* clock, const IBoardInputPins* board_inputs, ICommLink* comm_link)
        : params_(params), clock_(clock), board_inputs_(board_inputs), comm_link_(comm_link)
    {
    }

    virtual void reset() override
    {
        IGoalInput::reset();

        goal_ = Axis4r::zero();
        goal_mode_ = params_->default_goal_mode;
        last_rec_read_ = 0;
        last_angle_mode_ = std::numeric_limits<TReal>::min();
        request_duration_ = 0;

        vehicle_state_ = params_->default_vehicle_state;
    }
    
    virtual void update() override
    {
        IGoalInput::update();

        uint64_t time = clock_->millis();

        //don't keep reading if not updated
        uint64_t dt = time - last_rec_read_;
        if (dt <= params_->rc.read_interval_ms)
            return;
        last_rec_read_ = time;

        //read channel values
        Axis4r channels;
        for (unsigned int axis = 0; axis < 3; ++axis)
            channels.axis3[axis] = board_inputs_->readChannel(params_->rc.channels.axis3[axis]);
        channels.throttle = board_inputs_->readChannel(params_->rc.channels.throttle);

        //set goal mode as per the switch position on RC
        updateGoalMode();

        //get any special action being requested by user such as arm/disarm
        RcRequestType rc_action = getActionRequest(channels);

        //state machine
        switch (vehicle_state_) {
        case VehicleState::Inactive:
            comm_link_->log(std::string("State:\t ").append("Inactive state"));

            if (rc_action == RcRequestType::ArmRequest) {
                comm_link_->log(std::string("State:\t ").append("Inactive state, Arm request recieved"));
                request_duration_ += dt;

                if (request_duration_ > params_->rc.arm_duration) {
                    vehicle_state_ = VehicleState::BeingArmed;
                    request_duration_ = 0;
                }
            }
            //else ignore
            break;
        case VehicleState::BeingArmed:
            comm_link_->log(std::string("State:\t ").append("Being armed"));

            //start the motors
            goal_.throttle = params_->Params::min_armed_throttle;
            goal_.axis3 = Axis3r::zero(); //neural activation while still being armed
            
            //we must wait until sticks are at neutral or we will have random behaviour
            if (rc_action == RcRequestType::NeutralRequest) {
                request_duration_ += dt;

                if (request_duration_ > params_->rc.neutral_duration) {
                    vehicle_state_ = VehicleState::Armed;
                    comm_link_->log(std::string("State:\t ").append("Armed"));
                    request_duration_ = 0;
                }
            }
            //else ignore
            break;
        case VehicleState::Armed:
            //unless diarm is being requested, set goal from stick position
            if (rc_action == RcRequestType::DisarmRequest) {
                comm_link_->log(std::string("State:\t ").append("Armed state, disarm request recieved"));
                request_duration_ += dt;

                if (request_duration_ > params_->rc.disarm_duration) {
                    vehicle_state_ = VehicleState::BeingDisarmed;
                    request_duration_ = 0;
                }
            }
            else {
                request_duration_ = 0; //if there was spurious disarm request
                updateGoal(channels);
            }
            break;
        case VehicleState::BeingDisarmed:
            comm_link_->log(std::string("State:\t ").append("Being state"));

            goal_.axis3 = Axis3r::zero(); //neutral activation while being disarmed
            vehicle_state_ = VehicleState::Disarmed;
            request_duration_ = 0;

            break;
        case VehicleState::Disarmed:
            comm_link_->log(std::string("State:\t ").append("Disarmed"));

            goal_.throttle = 0;
            goal_.axis3 = Axis3r::zero(); //neutral activation while being disarmed
            vehicle_state_ = VehicleState::Inactive;
            request_duration_ = 0;

            break;
        default:
            throw std::runtime_error("VehicleState has unknown value for RemoteControl::update()");
        }
    }

    virtual const Axis4r& getGoal() const override
    {
        return goal_;
    }

    virtual const GoalMode& getGoalMode() const override
    {
        return goal_mode_;
    }

private:
    enum class RcRequestType {
        None, ArmRequest, DisarmRequest, NeutralRequest
    };

    void updateGoalMode()
    {
        //set up RC mode as level or rate
        angle_mode_ = board_inputs_->readChannel(params_->rc.rate_level_mode_channel);
        if (last_angle_mode_ != angle_mode_) {
            //for 3 way switch, 1/3 value for each position
            if (angle_mode_ < params_->rc.max_angle_level_switch)
                goal_mode_ = GoalMode::getStandardAngleMode();
            else
                goal_mode_ = GoalMode::getAllRateMode();

            last_angle_mode_ = angle_mode_;
        }
    }

    void updateGoal(const Axis4r& channels)
    {
        //if throttle is too low then set all motors to same value as throttle because
        //otherwise values in pitch/roll/yaw would get clipped randomly and can produce random results
        //in other words: we can't do angling if throttle is too low
        if (channels.throttle <= params_->rc.min_angling_throttle)
            goal_.throttle = params_->rc.min_angling_throttle;
        else
            goal_.throttle = channels.throttle;

        if (angle_mode_ < params_->rc.max_angle_level_switch) { //for 3 way switch, 1/3 value for each position
            goal_.axis3 = params_->angle_level_pid.max_limit.colWiseMultiply(channels.axis3); //we are in control-by-level mode
        }
        else { //we are in control-by-rate mode
            goal_.axis3 = params_->angle_rate_pid.max_limit.colWiseMultiply(channels.axis3);
        }
    }

    static bool isInTolerance(TReal val, TReal tolerance, TReal center = TReal())
    {
        return val <= center + tolerance && val >= center  - tolerance;
    }

    RcRequestType getActionRequest(const Axis4r& channels)
    {
        TReal tolerance = params_->rc.action_request_tolerance;
        TReal stick_min = 1 - tolerance;

        bool yaw_action_positive = channels.axis3.yaw() >= stick_min;
        bool yaw_action_negative = channels.axis3.yaw() <= -stick_min;
        bool throttle_action = channels.throttle <= tolerance;

        bool roll_action_positive = channels.axis3.roll() >= stick_min;
        bool roll_action_negative = channels.axis3.roll() <= -stick_min;
        TReal normalized_pitch = (channels.axis3.pitch() + 1) / 2; //-1 to 1 --> 0 to 1
        bool pitch_action = normalized_pitch >= stick_min;

        if (yaw_action_positive && throttle_action && roll_action_negative && pitch_action)
            return RcRequestType::ArmRequest;
        else if (yaw_action_negative && throttle_action && roll_action_positive && pitch_action)
            return RcRequestType::DisarmRequest;
        else if (isInTolerance(channels.axis3.roll(), tolerance)
            && isInTolerance(channels.axis3.pitch(), tolerance)
            && isInTolerance(channels.axis3.yaw(), tolerance))
            return RcRequestType::NeutralRequest;
        else
            return RcRequestType::None;
    }

private:
    const IBoardClock* clock_;
    const IBoardInputPins* board_inputs_;
    const Params* params_;
    ICommLink* comm_link_;

    Axis4r goal_;
    GoalMode goal_mode_;

    uint64_t last_rec_read_;
    TReal angle_mode_, last_angle_mode_;

    uint64_t request_duration_;

    VehicleState vehicle_state_;
};


} //namespace