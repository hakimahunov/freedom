// See LICENSE for license details.
package sifive.freedom.debug.pynq

import Chisel._
import chisel3.core.{attach}
import chisel3.experimental.{withClockAndReset}

import freechips.rocketchip.config._
import freechips.rocketchip.diplomacy.{LazyModule}

import sifive.fpgashells.shell.xilinx.zynqshell.{ZynqShell}

class DebugPynqFPGAChip(implicit override val p: Parameters) extends ZynqShell {
  withClockAndReset(clock_fclk0, fclk_reset) {
    val dut = Module(new DebugPynqPlatform)
    ps7.io.M_AXI_GP0.connectSlave(dut.io.m_gp0)
    ps7.io.S_AXI_GP0.connectMaster(dut.io.s_gp0)
    ps7.io.S_AXI_HP0.connectMaster(dut.io.s_hp0)
    ps7.io.S_AXI_HP2.connectMaster(dut.io.s_hp2)
    ps7.io.M_AXI_GP0_ACLK := clock_fclk0
    ps7.io.S_AXI_GP0_ACLK := clock_fclk0
    ps7.io.S_AXI_HP0_ACLK := clock_fclk0
    ps7.io.S_AXI_HP2_ACLK := clock_fclk0
  }
}
