using System;
using System.IO;
using UndertaleModLib.Compiler;
EnsureDataLoaded();
if(Data.IsYYC()) throw new Exception("VM GameMaker data required.");
string root=Path.Combine(Environment.GetEnvironmentVariable("VHRVR_PACKAGE") ?? Environment.CurrentDirectory,"src");
string Read(string name)=>File.ReadAllText(Path.Combine(root,name));
string Stock(string entry,string function) {
    // Recover original code only from the player's local, verified game file.
    string body=GetDecompiledText("gml_Object_"+entry, null, new Underanalyzer.Decompiler.DecompileSettings());
    body=System.Text.RegularExpressions.Regex.Replace(body,@"UnknownEnum\.Value_(\d+)","$1");
    body=System.Text.RegularExpressions.Regex.Replace(body,@"enum UnknownEnum\s*\{[^}]*\}","");
    if(entry=="obj_HUD_Draw_0") body=body.Replace("view_current != target.view_num","false");
    if(entry=="parent_pausemenu_Draw_74") body=body.Replace("options_display_menu(42,","options_display_menu((global.vhr_enabled ? 154 : 42),");
    return "function "+function+"() {\n"+body+"\n}\n";
}
string RaceStock() {
    string result="";
    foreach(string entry in new[]{"obj_count_Draw_64","obj_gameovertext_Draw_64","parent_pausemenu_Draw_74","obj_options_menu_Draw_74","obj_lapcount_Draw_64"})
        result+=Stock(entry,"vhr_gui_"+entry);
    return result;
}
string stockMap=Stock("obj_HUD_Draw_64","vhr_stock_map"), stockHud=Stock("obj_HUD_Draw_0","vhr_stock_hud"), stockRace=RaceStock();
CodeImportGroup group=new(Data){ MainThreadAction=MainThreadAction };
group.QueueReplace("gml_GlobalScript_vhr_helpers",Read("helpers.gml")+"\n"+Read("sky.gml")+"\n"+Read("hud.gml")+"\n"+stockMap+"\n"+stockHud+"\n"+Read("chase-hud.gml")+"\n"+Read("theatre.gml")+"\n"+Read("race-gui.gml")+stockRace+"\n"+Read("wheel-feedback.gml"));
group.QueuePrepend("gml_Object_obj_HUD_Draw_0","if(global.vhr_enabled) { if(global.vhr_cockpit) vhr_draw_hud(); else vhr_draw_chase_hud(); exit; }");
group.QueuePrepend("gml_Object_obj_HUD_Draw_64","if(variable_global_exists(\"vhr_enabled\") && global.vhr_enabled) exit;");
group.QueueFindReplace("gml_GlobalScript_draw_parallax","var _cam = obj_main.camera[view_current];","if(variable_global_exists(\"vhr_enabled\") && global.vhr_enabled) { vhr_draw_sky(arg0,arg1); return; }\n    var _cam = obj_main.camera[view_current];");
group.QueueAppend("gml_Object_obj_camera_Create_0",Read("camera-create.gml"));
group.QueueAppend("gml_Object_obj_camera_Step_0",Read("camera-step.gml"));
group.QueueAppend("gml_Object_obj_camera_Step_2",Read("camera-seat.gml"));
group.QueueFindReplace("gml_Object_obj_camera_Draw_0","if (view_current == view)","if (view_current == view || (global.vhr_enabled && view == 0 && view_current < 3))");
group.QueueFindReplace("gml_Object_obj_camera_Draw_0","var perspective = matrix_build_projection_perspective_fov(cam.fov, aspect, 1, 20000);","var perspective = matrix_build_projection_perspective_fov(cam.fov, aspect, 1, 20000);\n"+Read("camera-projection.gml"));
// Only the primary race-camera instance owns the two VR eyes.
group.QueuePrepend("gml_Object_obj_camera_Draw_0","if(global.vhr_enabled && view!=0) exit;");
// Draw the interior with the same eye matrices and depth buffer as the race world.
group.QueueAppend("gml_Object_obj_camera_Draw_0",Read("camera-cockpit-draw.gml"));
group.QueuePrepend("gml_Object_obj_camera_Draw_73","if(global.vhr_enabled && view!=0) exit;");
group.QueuePrepend("gml_Object_obj_camera_CleanUp_0",Read("camera-cleanup.gml"));
// Hide only the local car's billboard while the driver sits inside the replacement cockpit.
string hideLocal="\n    if (variable_global_exists(\"vhr_cockpit\") && global.vhr_cockpit && instance_exists(obj_camera) && obj_camera.vhr_active && instance_exists(obj_camera.target) && parent == obj_camera.target) return;";
group.QueueFindReplace("gml_GlobalScript_draw_car","function draw_car()\n{","function draw_car()\n{"+hideLocal);
group.QueueFindReplace("gml_GlobalScript_draw_car","function draw_car2()\n{","function draw_car2()\n{"+hideLocal);
// Keep the game from resizing the stereo surface back to its normal 16:9 output.
group.QueuePrepend("gml_Object_obj_main_Alarm_10","if(variable_global_exists(\"vhr_enabled\") && global.vhr_enabled) exit;");
group.QueueFindReplace("gml_Object_obj_car_Step_0","hstick = cunt.obj.lstick.x;","hstick = cunt.obj.lstick.x;\n"+Read("wheel-input.gml"));
group.QueueAppend("gml_Object_obj_main_Create_0","vhr_theatre_boot();");
group.QueuePrepend("gml_Object_obj_main_Step_0","vhr_menu_wheel_release(); if(variable_global_exists(\"vhr_mpoll\")) external_call(global.vhr_mpoll,!vhr_wheel_racing());");
group.QueueAppend("gml_Object_obj_main_Step_0","vhr_theatre_boot();");
group.QueuePrepend("gml_Object_obj_main_CleanUp_0","if(variable_global_exists(\"vhr_theatre_ready\") && global.vhr_theatre_ready) { external_call(global.vhr_stop);global.vhr_theatre_ready=false; }");
group.QueueFindReplace("gml_Object_obj_main_Draw_64","if (!global.stop && !global.finished","if (!global.vhr_enabled && !global.stop && !global.finished");
group.QueueFindReplace("gml_GlobalScript_input_check","case 2:\n                    var _hold;","case 2:\n                    if (!vhr_wheel_racing() && (arg1==\"up\" || arg1==\"down\" || arg1==\"left\" || arg1==\"right\")) break;\n                    var _hold;");
group.QueueAppend("gml_Object_obj_camera_Draw_73","if(global.vhr_enabled && view==0 && view_current==2) global.vhr_mirror_ready=true;");
group.QueueFindReplace("gml_GlobalScript_input_check","var _s = ds_map_find_value(obj_main.input_map, arg1);","if(variable_global_exists(\"vhr_mkey\") && !vhr_wheel_racing()) { var _md=-1; if(arg1==\"up\") _md=0; if(arg1==\"down\") _md=1; if(arg1==\"left\") _md=2; if(arg1==\"right\") _md=3; if(_md>=0 && external_call(global.vhr_mkey,_md,arg0)) return true; }\n    var _s = ds_map_find_value(obj_main.input_map, arg1);");
group.QueueReplace("gml_Object_obj_count_Draw_64","vhr_race_gui(vhr_gui_obj_count_Draw_64);");
group.QueueReplace("gml_Object_obj_gameovertext_Draw_64","vhr_race_gui(vhr_gui_obj_gameovertext_Draw_64);");
group.QueueReplace("gml_Object_parent_pausemenu_Draw_74","vhr_race_gui(vhr_gui_parent_pausemenu_Draw_74);");
group.QueueReplace("gml_Object_obj_options_menu_Draw_74","vhr_race_gui(vhr_gui_obj_options_menu_Draw_74);");
group.QueueReplace("gml_Object_obj_lapcount_Draw_64","vhr_race_gui(vhr_gui_obj_lapcount_Draw_64);");
group.QueueAppend("gml_Object_obj_car_Step_0","with(obj) vhr_wheel_feedback();");
group.QueueFindReplace("gml_GlobalScript_controller_vibrate","var controller = global.controller[arg0];","vhr_wheel_impact(arg0,arg1,arg2);\n    var controller = global.controller[arg0];");
group.Import();
ScriptMessage("Compiled custom cockpit and SteamVR bridge hooks.");





