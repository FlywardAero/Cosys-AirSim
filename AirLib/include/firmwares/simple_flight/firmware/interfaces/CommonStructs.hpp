#pragma once

#include <exception>
#include <string>

namespace simple_flight {

typedef float TReal;

template<typename T>
class Axis3 {
public:
    Axis3(const T& x_val = T(), const T& y_val = T(), const T& z_val = T())
        : vals_ {x_val, y_val, z_val}
    {
    }

    //access by index
    virtual T& operator[] (unsigned int index)
    {
        return vals_[index];
    }
    virtual const T& operator[] (unsigned int index) const
    {
        return vals_[index];
    }

    bool equals3(const Axis3<T>& other) const
    {
        return vals_[0] == other.vals_[0] && vals_[1] == other.vals_[1] && vals_[2] == other.vals_[2];
    }

    Axis3<T> colWiseMultiply3(const Axis3<T>& other) const
    {
        return Axis3<T>(vals_[0] * other.vals_[0], vals_[1] * other.vals_[1], vals_[2] * other.vals_[2]);
    }

    //access as axis
    const T& x() const { return vals_[0]; }
    const T& y() const { return vals_[1]; }
    const T& z() const { return vals_[2]; }
    T& x() { return vals_[0]; }
    T& y() { return vals_[1]; }
    T& z() { return vals_[2]; }

    //access as angles
    const T& roll() const { return vals_[0]; }
    const T& pitch() const { return vals_[1]; }
    const T& yaw() const { return vals_[2]; }
    T& roll() { return vals_[0]; }
    T& pitch() { return vals_[1]; }
    T& yaw() { return vals_[2]; }

    static const Axis3<T>& zero()
    {
        static const Axis3<T> zero_val = Axis3<T>();
        return zero_val;
    }

    static constexpr unsigned int AxisCount()
    {
        return 3;
    }


private:
    T vals_[3];
};
typedef Axis3<TReal> Axis3r;

template<typename T>
class Axis4 : public Axis3<T> {
public:
    Axis4(const T& x_val = T(), const T& y_val = T(), const T& z_val = T(), const T& val4_val = T())
        : Axis3<T>(x_val, y_val, z_val), val4_(val4_val)
    {
    }

    Axis4(const Axis3<T>& axis3_val, const T& val4_val = T())
        : Axis3<T>(axis3_val), val4_(val4_val)
    {
    }

    void setAxis3(const Axis3<T>& axis3)
    {
        for(unsigned int axis = 0; axis < Axis3<T>::AxisCount(); ++axis)
            (*this)[axis] = axis3[axis];
    }

    T& val4()
    {
        return val4_;
    }
    const T& val4() const
    {
        return val4_;
    }

    //access by index
    virtual T& operator[] (unsigned int index) override
    {
        return const_cast<T&>(
            (* const_cast<const Axis4<T>*>(this))[index]);
    }
    virtual const T& operator[] (unsigned int index) const override
    {
        if (index <= 2)
            return Axis3<T>::operator[](index);
        else if (index == 3)
            return val4_;
        else throw std::out_of_range("index must be <= 3 but it was " + std::to_string(index));
    }

    bool equals4(const Axis4<T>& other) const
    {
        return (*this)[0] == other[0] && (*this)[1] == other[1] && 
            (*this)[2] == other[2] && (*this)[3] == other[3];
    }

    Axis4<T> colWiseMultiply4(const Axis4<T>& other) const
    {
        return Axis4<T>((*this)[0] * other[0], (*this)[1] * other[1],
            (*this)[2] * other[2], (*this)[3] * other[3]);    
    }

    T& throttle()
    {
        return val4_;
    }
    const T& throttle() const
    {
        return val4_;
    }

    static const Axis4<T>& zero()
    {
        static const Axis4<T> zero_val = Axis4<T>();
        return zero_val;
    }

    static constexpr unsigned int AxisCount()
    {
        return 4;
    }

    static Axis3<T> axis4ToXyz(const Axis4<T> axis4)
    {
        return Axis3<T>(axis4[0], axis4[1], axis4[3]);
    }
    static Axis4<T> xyzToAxis4(const Axis3<T> xyz)
    {
        //TODO: use nan instead 0?
        return Axis4<T>(xyz[0], xyz[1], 0, xyz[2]);
    }

private:
    T val4_ = 0;
};
typedef Axis4<TReal> Axis4r;

struct GeoPoint {
    double latitude = std::numeric_limits<double>::quiet_NaN(); 
    double longitude = std::numeric_limits<double>::quiet_NaN(); 
    float altiude = std::numeric_limits<float>::quiet_NaN();

    static const GeoPoint& nan()
    {
        const static GeoPoint val;
        return val;
    }
};


enum class VehicleStateType {
    Unknown, Inactive, BeingArmed, Armed, Active, BeingDisarmed, Disarmed
};

class VehicleState {
public:
    VehicleStateType getState() const
    {
        return state_;
    }
    void setState(VehicleStateType state, const GeoPoint& home_point = GeoPoint::nan())
    {
        if (state == VehicleStateType::Armed && std::isnan(home_point.latitude))
            throw std::invalid_argument("home_point must be supplied to set armed state");

        state_ = state;
    }

    const GeoPoint& getHomeGeoPoint() const
    {
        return home_point_;
    }

private:
    VehicleStateType state_ = VehicleStateType::Unknown;
    GeoPoint home_point_;
};

enum class GoalModeType {
    Unknown,
    Passthrough,
    AngleLevel,
    AngleRate,
    VelocityWorld,
    PositionWorld,
    ConstantOutput
};

class GoalMode : public Axis4<GoalModeType> {
public:
    GoalMode(GoalModeType x_val = GoalModeType::AngleLevel, GoalModeType y_val = GoalModeType::AngleLevel, 
        GoalModeType z_val = GoalModeType::AngleRate, GoalModeType val4_val = GoalModeType::Passthrough)
    : Axis4<GoalModeType>(x_val, y_val, z_val, val4_val)
    {
    }

    static const GoalMode& getStandardAngleMode() 
    {
        static const GoalMode mode = GoalMode();
        return mode;
    }

    static const GoalMode& getVelocityXYPosZMode() 
    {
        static const GoalMode mode = GoalMode(GoalModeType::VelocityWorld, 
            GoalModeType::VelocityWorld, GoalModeType::AngleRate, GoalModeType::PositionWorld);
        return mode;
    }

    static const GoalMode& getVelocityMode() 
    {
        static const GoalMode mode = GoalMode(GoalModeType::VelocityWorld, 
            GoalModeType::VelocityWorld, GoalModeType::AngleRate, GoalModeType::VelocityWorld);
        return mode;
    }

    static const GoalMode& getPositionMode() 
    {
        static const GoalMode mode = GoalMode(GoalModeType::PositionWorld, 
            GoalModeType::PositionWorld, GoalModeType::AngleRate, GoalModeType::PositionWorld);
        return mode;
    }

    static const GoalMode& getAllRateMode()
    {
        static const GoalMode mode = GoalMode(GoalModeType::AngleRate, 
            GoalModeType::AngleRate, GoalModeType::AngleRate, GoalModeType::Passthrough);
        return mode;
    }

    static const GoalMode& getUnknown()
    {
        static const GoalMode mode = GoalMode(GoalModeType::Unknown, 
            GoalModeType::Unknown, GoalModeType::Unknown, GoalModeType::Unknown);
        return mode;
    }
};

} //namespace