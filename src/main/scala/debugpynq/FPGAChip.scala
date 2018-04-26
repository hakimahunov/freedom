// See LICENSE for license details.
package sifive.freedom.debug.pynq

import chisel3._
import chisel3.core.{attach}
import chisel3.experimental.{withClockAndReset}

import freechips.rocketchip.config._
import freechips.rocketchip.subsystem._
import freechips.rocketchip.diplomacy._
import freechips.rocketchip.util._

import sifive.fpgashells.shell.xilinx.zynqshell.{ZynqShell, PS7_base_io, PS7AXI4Bundle}

class PS7_param_io(implicit val p: Parameters) extends ParameterizedBundle()(p) with PS7_base_io {
    /* Look at builds/debugpynq/.ip_user_files/ip/processing_system7_0/processing_system7_0.veo
     * if "case" of PS7 port names are incorrect */
    val M_AXI_GP0_ACLK = Input(Clock())
    val S_AXI_GP0_ACLK = Input(Clock())
    val M_AXI_GP0 = new PS7AXI4Bundle(AXIParams.a12xi32)
    val S_AXI_GP0 = Flipped(new PS7AXI4Bundle(AXIParams.a6xi32))
    val S_AXI_HP0_ACLK = (!p(ExtMem).isEmpty).option(Input(Clock()))
    val S_AXI_HP2_ACLK = (!p(ExtMem).isEmpty).option(Input(Clock()))
    val S_AXI_HP0 = (!p(ExtMem).isEmpty).option(Flipped(new PS7AXI4Bundle(AXIParams.axi64)))
    val S_AXI_HP2 = (!p(ExtMem).isEmpty).option(Flipped(new PS7AXI4Bundle(AXIParams.axi64)))

    ElaborationArtefacts.add(
        """varPS7.tcl""",
        """set_property -dict [ list \
            CONFIG.PCW_USE_S_AXI_HP0 {""" + {if(p(ExtMem) == None) 0 else 1} + """} \
            CONFIG.PCW_USE_S_AXI_HP2 {""" + {if(p(ExtMem) == None) 0 else 1} + """} \
            ] $::ippointer
           """
    )
}


class processing_system7_0(implicit val p: Parameters) extends BlackBox {
    val io = IO(new PS7_param_io)
}

class DebugPynqFPGAChip(implicit override val p: Parameters) extends ZynqShell {

    val ps7 = Module(new processing_system7_0())
    connectBaseIO(ps7.io)

  withClockAndReset(clock_slow, fclk_reset) {
    val dut = Module(new DebugPynqPlatform)
    ps7.io.M_AXI_GP0.connectSlave(dut.io.m_gp0)
    ps7.io.S_AXI_GP0.connectMaster(dut.io.s_gp0)
    ps7.io.M_AXI_GP0_ACLK := clock_slow
    ps7.io.S_AXI_GP0_ACLK := clock_slow
    if (!p(ExtMem).isEmpty) {
        ps7.io.S_AXI_HP0.get.connectMaster(dut.io.s_hp0.get)
        ps7.io.S_AXI_HP2.get.connectMaster(dut.io.s_hp2.get)
        ps7.io.S_AXI_HP0_ACLK.get := clock_slow
        ps7.io.S_AXI_HP2_ACLK.get := clock_slow
    }
  }
}
