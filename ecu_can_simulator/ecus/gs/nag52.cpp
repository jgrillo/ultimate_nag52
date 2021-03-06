//
// Created by ashcon on 2/20/21.
//

#include <cstdio>
#include "nag52.h"
#include "../../canbus/car_sim.h"

void nag52::setup() {
    gs418.raw = 0x504D7404DD00C000;
    gs218.raw = 0x0000DD4923003060;
    gs338.raw = 0x0000000000000000;
    this->limp_mode = false;
    this->handle_press = false;
    this->prog = DriveProgramMode::Sport;
    this->curr_fwd_gear = 1;
}

void nag52::simulate_tick() {
    gs418.set_ALL_WHEEL(false);
    gs418.set_CVT(false);
    gs418.set_T_GET(this->iface.get_oil_temp()+40); // 80C
    gs218.set_GSP_OK(!this->limp_mode);
    gs218.set_GS_NOTL(this->limp_mode);

    output_shaft_rpm = this->iface.get_n2_rpm();

    int curr_rpm = ms308.get_NMOT();
    if (this->last_rpm > curr_rpm) { // Car is slower
        if (ms308.get_NMOT() < 1500 && ewm230.get_WHC() == WHC::GS_D) {
            if(downshift()) {
                sim->get_engine()->force_set_rpm(this->force_engine_rpm(), this->get_gear_ratio());
            }
        }
    } else { // Car is faster
        if (ms308.get_NMOT() > 3000 && ewm230.get_WHC() == WHC::GS_D) {
            if(upshift()) {
                sim->get_engine()->force_set_rpm(this->force_engine_rpm(), this->get_gear_ratio());
            }
        }
    }
    this->last_rpm = curr_rpm;

    switch(ewm230.get_WHC()) {
        case GS_P:
            gs418.set_WHST(0);
            gs418.set_FSC(80);
            this->iface.set_ewm_position(0);
            break;
        case GS_D:
        case GS_PLUS:
        case GS_MINUS:
            gs418.set_WHST(4);
            gs418.set_FSC(68); // TODO - Range restrict mode
            this->iface.set_ewm_position(3);
            break;
        case GS_N:
            gs418.set_WHST(2);
            gs418.set_FSC(78);
            this->iface.set_ewm_position(2);
            break;
        case GS_R:
            gs418.set_WHST(1);
            gs418.set_FSC(82);
            this->iface.set_ewm_position(1);
            break;
        case GS_SNV:
            gs418.set_WHST(7);
            gs418.set_FSC(255);
            break;
        default:
            break;
    }

    // IC only listens to FSC value (Not sure about the rest of the car)
    switch(this->prog) {
        case DriveProgramMode::Sport:
            gs418.set_FPC(DrivingProgram::S);
            break;
        case DriveProgramMode::Comfort:
            gs418.set_FPC(DrivingProgram::C);
            break;
        case DriveProgramMode::Agility:
            gs418.set_FPC(DrivingProgram::A);
            break;
        case DriveProgramMode::Manual:
            gs418.set_FPC(DrivingProgram::M);
            break;
        case DriveProgramMode::Fail:
            gs418.set_FPC(DrivingProgram::F);
            break;
        default:
            break;
    }
    if (ewm230.get_FPT()) {
        this->handle_btn_press();
    } else {
        this->handle_press = false;
    }


#ifdef W5A330
    gs418.set_MECH(GearVariant::NAG2_KLEIN2);
#else
    gs418.set_MECH(GearVariant::NAG2_GROSS2);
#endif



}

void nag52::handle_btn_press() {
    if (this->limp_mode) { // Ignore this request in limp!
        this->prog = DriveProgramMode::Fail;
        return;
    }
    if (!this->handle_press) {
        // S -> C -> A -> M -> S
        switch (prog) {
            case DriveProgramMode::Sport:
                this->prog = DriveProgramMode::Comfort;
                break;
            case DriveProgramMode::Comfort:
                this->prog = DriveProgramMode::Agility;
                break;
            case DriveProgramMode::Agility:
                this->prog = DriveProgramMode::Manual;
                break;
            case DriveProgramMode::Manual:
                this->prog = DriveProgramMode::Sport;
                break;
            default:
                break;
        }
        this->handle_press = true;
    }
}

bool nag52::upshift() {
    bool ret = true;
    switch (this->curr_fwd_gear) {
        case 1:
            this->iface.set_one_two(255);
            this->curr_fwd_gear = 2;
            break;
        case 2:
            this->iface.set_two_three(255);
            this->curr_fwd_gear = 3;
            break;
        case 3:
            this->iface.set_three_four(255);
            this->curr_fwd_gear = 4;
            break;
        case 4:
            this->iface.set_one_two(255);
            this->curr_fwd_gear = 5;
            break;
        case 5:
        default:
            ret = false;
            break;
    }
    return ret;
}

bool nag52::downshift() {
    bool ret = true;
    switch (this->curr_fwd_gear) {
        case 5:
            this->iface.set_one_two(255);
            this->curr_fwd_gear = 4;
            break;
        case 4:
            this->iface.set_three_four(255);
            this->curr_fwd_gear = 3;
            break;
        case 3:
            this->iface.set_two_three(255);
            this->curr_fwd_gear = 2;
            break;
        case 2:
            this->iface.set_one_two(255);
            this->curr_fwd_gear = 1;
            break;
        case 1:
        default:
            ret = false;
            break;
    }
    return ret;
}

// Assumes shift has just 'completed' engine RPM must be set
int nag52::force_engine_rpm() {
    switch (this->curr_fwd_gear) {
        case 1:
            return output_shaft_rpm * D1_RAT;
        case 2:
            return output_shaft_rpm * D2_RAT;
        case 3:
            return output_shaft_rpm * D3_RAT;
        case 4:
            return output_shaft_rpm * D4_RAT;
        case 5:
            return output_shaft_rpm * D5_RAT;
        default:
            return 0;
    }
}

float nag52::get_gear_ratio() {
    switch(this->curr_fwd_gear) {
        case 1:
            return D1_RAT;
        case 2:
            return D2_RAT;
        case 3:
            return D3_RAT;
        case 4:
            return D4_RAT;
        case 5:
            return D5_RAT;
        default:
            return 1.0;
    }
}

int virtual_nag_iface::get_n2_rpm() {
    int engine_rpm = ms308.get_NMOT();
    int output_rpm = 0;
    switch(this->virtual_gear) {
        case V_GEAR::D_1:
            output_rpm = engine_rpm / D1_RAT;
            break;
        case V_GEAR::D_2:
            output_rpm = engine_rpm / D2_RAT;
            break;
        case V_GEAR::D_3:
            output_rpm = engine_rpm / D3_RAT;
            break;
        case V_GEAR::D_4:
            output_rpm = engine_rpm / D4_RAT;
            break;
        case V_GEAR::D_5:
            output_rpm = engine_rpm / D5_RAT;
            break;
        case V_GEAR::PARK:
        case V_GEAR::NEUTRAL:
        case V_GEAR::REV_1:
        case V_GEAR::REV_2:
        default:
            break;

    }
    return output_rpm;
}

int virtual_nag_iface::get_n3_rpm() {
    int engine_rpm = ms308.get_NMOT();
    int output_rpm = 0;
    switch(this->virtual_gear) {
        case V_GEAR::D_2:
            output_rpm = D2_RAT * engine_rpm;
            break;
        case V_GEAR::D_3:
            output_rpm = D3_RAT * engine_rpm;
            break;
        case V_GEAR::D_4:
            output_rpm = D4_RAT * engine_rpm;
            break;
        case V_GEAR::REV_1:
            output_rpm = R1_RAT * engine_rpm;
            break;
        case V_GEAR::REV_2:
            output_rpm = D2_RAT * engine_rpm;
            break;
        case V_GEAR::D_1:
        case V_GEAR::D_5:
        case V_GEAR::PARK:
        case V_GEAR::NEUTRAL:
        default:
            break;
    }
    return output_rpm;
}

int virtual_nag_iface::get_oil_temp() {
    return 80;
}

int virtual_nag_iface::get_vbatt_mv() {
    return 14300; // 14.3V
}

void virtual_nag_iface::set_tcc(uint8_t pwm) {

}

void virtual_nag_iface::set_mpc(uint8_t pwm) {
    // Ignore
}

void virtual_nag_iface::set_spc(uint8_t pwm) {
    // Ignore
}

void virtual_nag_iface::set_three_four(uint8_t pwm) {
    if (this->virtual_gear == V_GEAR::D_3) {
        this->virtual_gear = V_GEAR::D_4;
    } else if (this->virtual_gear == V_GEAR::D_4) {
        this->virtual_gear = V_GEAR::D_3;
    }
}

void virtual_nag_iface::set_two_three(uint8_t pwm) {
    if (this->virtual_gear == V_GEAR::D_2) {
        this->virtual_gear = V_GEAR::D_3;
    } else if (this->virtual_gear == V_GEAR::D_3) {
        this->virtual_gear = V_GEAR::D_2;
    }
}

void virtual_nag_iface::set_one_two(uint8_t pwm) {
    if (this->virtual_gear == V_GEAR::D_1) {
        this->virtual_gear = V_GEAR::D_2;
    } else if (this->virtual_gear == V_GEAR::D_2) {
        this->virtual_gear = V_GEAR::D_1;
    } else if (this->virtual_gear == V_GEAR::D_4) {
        this->virtual_gear = V_GEAR::D_5;
    } else if (this->virtual_gear == V_GEAR::D_5) {
        this->virtual_gear = V_GEAR::D_4;
    }
}

void virtual_nag_iface::set_ewm_position(int g) {
    if (g == 0) {
        this->virtual_gear = V_GEAR::PARK;
    } else if (g == 1) {
        this->virtual_gear = V_GEAR::REV_1;
    } else if (g == 2) {
        this->virtual_gear = V_GEAR::NEUTRAL;
    } else if (g == 3) {
        if (
                this->virtual_gear == V_GEAR::PARK
                || this->virtual_gear == V_GEAR::REV_1
                || this->virtual_gear == V_GEAR::REV_2
                || this->virtual_gear == V_GEAR::NEUTRAL
                ) {
            this->virtual_gear = V_GEAR::D_1;
        }
    }
}
