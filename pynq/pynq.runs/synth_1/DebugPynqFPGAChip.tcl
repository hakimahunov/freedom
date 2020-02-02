# 
# Synthesis run script generated by Vivado
# 

set TIME_start [clock seconds] 
proc create_report { reportName command } {
  set status "."
  append status $reportName ".fail"
  if { [file exists $status] } {
    eval file delete [glob $status]
  }
  send_msg_id runtcl-4 info "Executing : $command"
  set retval [eval catch { $command } msg]
  if { $retval != 0 } {
    set fp [open $status w]
    close $fp
    send_msg_id runtcl-5 warning "$msg"
  }
}
set_param synth.incrementalSynthesisCache ./.Xil/Vivado-6438-riscv-VirtualBox/incrSyn
set_msg_config -id {Common 17-41} -limit 10000000
set_msg_config -id {Synth 8-256} -limit 10000
set_msg_config -id {Synth 8-638} -limit 10000
create_project -in_memory -part xa7z020clg400-1Q

set_param project.singleFileAddWarning.threshold 0
set_param project.compositeFile.enableAutoGeneration 0
set_param synth.vivado.isSynthRun true
set_msg_config -source 4 -id {IP_Flow 19-2162} -severity warning -new_severity info
set_property webtalk.parent_dir /home/risc-v/Documents/freedom/pynq/pynq.cache/wt [current_project]
set_property parent.project_path /home/risc-v/Documents/freedom/pynq/pynq.xpr [current_project]
set_property default_lib xil_defaultlib [current_project]
set_property target_language Verilog [current_project]
set_property ip_output_repo /home/risc-v/Documents/freedom/pynq/pynq.cache/ip [current_project]
set_property ip_cache_permissions {read write} [current_project]
read_verilog -library xil_defaultlib {
  /home/risc-v/Documents/freedom/pynq/pynq.srcs/sources_1/imports/vsrc/AsyncResetReg.v
  /home/risc-v/Documents/freedom/pynq/pynq.srcs/sources_1/imports/vsrc/plusarg_reader.v
  /home/risc-v/Documents/freedom/pynq/pynq.srcs/sources_1/imports/debugpynq/sifive.freedom.debug.pynq.SmallPynqConfig.v
}
read_ip -quiet /home/risc-v/Documents/freedom/pynq/pynq.srcs/sources_1/ip/processing_system7_0/processing_system7_0.xci
set_property used_in_implementation false [get_files -all /home/risc-v/Documents/freedom/pynq/pynq.srcs/sources_1/ip/processing_system7_0/processing_system7_0.xdc]

read_ip -quiet /home/risc-v/Documents/freedom/pynq/pynq.srcs/sources_1/ip/reset_sys/reset_sys.xci
set_property used_in_implementation false [get_files -all /home/risc-v/Documents/freedom/pynq/pynq.srcs/sources_1/ip/reset_sys/reset_sys_board.xdc]
set_property used_in_implementation false [get_files -all /home/risc-v/Documents/freedom/pynq/pynq.srcs/sources_1/ip/reset_sys/reset_sys.xdc]
set_property used_in_implementation false [get_files -all /home/risc-v/Documents/freedom/pynq/pynq.srcs/sources_1/ip/reset_sys/reset_sys_ooc.xdc]

# Mark all dcp files as not used in implementation to prevent them from being
# stitched into the results of this synthesis run. Any black boxes in the
# design are intentionally left as such for best results. Dcp files will be
# stitched into the design at a later time, either when this synthesis run is
# opened, or when it is stitched into a dependent implementation run.
foreach dcp [get_files -quiet -all -filter file_type=="Design\ Checkpoint"] {
  set_property used_in_implementation false $dcp
}
read_xdc /home/risc-v/Documents/freedom/pynq/pynq.srcs/constrs_1/imports/constraints/pynq-master.xdc
set_property used_in_implementation false [get_files /home/risc-v/Documents/freedom/pynq/pynq.srcs/constrs_1/imports/constraints/pynq-master.xdc]

set_param ips.enableIPCacheLiteLoad 1
close [open __synthesis_is_running__ w]

synth_design -top DebugPynqFPGAChip -part xa7z020clg400-1Q


# disable binary constraint mode for synth run checkpoints
set_param constraints.enableBinaryConstraints false
write_checkpoint -force -noxdef DebugPynqFPGAChip.dcp
create_report "synth_1_synth_report_utilization_0" "report_utilization -file DebugPynqFPGAChip_utilization_synth.rpt -pb DebugPynqFPGAChip_utilization_synth.pb"
file delete __synthesis_is_running__
close [open __synthesis_is_complete__ w]
