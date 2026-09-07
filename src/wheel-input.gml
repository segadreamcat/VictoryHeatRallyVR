if(variable_global_exists("vhr_wheel") && global.vhr_wheel && parent==obj_main.camera[0].target) {
    var _wheel_ok=external_call(global.vhr_wpoll);
    if(_wheel_ok && !_stop_car && !obj_main.pause) {
        var _view_button=external_call(global.vhr_wvalue,4)>0.5;
        if(_view_button && !global.vhr_view_prev) global.vhr_cockpit=!global.vhr_cockpit;
        global.vhr_view_prev=_view_button;
        var _raw_steer=external_call(global.vhr_wvalue,0);
        hstick=clamp(sign(_raw_steer)*max(0,abs(_raw_steer)-0.008)*3.05,-1,1);
        _input_gas=external_call(global.vhr_wvalue,1);
        _input_brake=external_call(global.vhr_wvalue,2);
        var _wd=external_call(global.vhr_wvalue,3)>0.5;
        _input_drift=_input_drift || _wd;
        _press_drift=_press_drift || (_wd && !global.vhr_drift_prev);
        global.vhr_drift_prev=_wd;
    } else {
        global.vhr_drift_prev=false;
        external_call(global.vhr_wforce,0,0,0);
    }
}

