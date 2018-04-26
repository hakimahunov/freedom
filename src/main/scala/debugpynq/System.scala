// See LICENSE for license details.
package sifive.freedom.debug.pynq

import Chisel._

import freechips.rocketchip.config._
import freechips.rocketchip.subsystem._
import freechips.rocketchip.devices.debug._
import freechips.rocketchip.devices.tilelink._
import freechips.rocketchip.diplomacy._
import freechips.rocketchip.system._

class DebugPynqSystem(implicit p: Parameters) extends RocketSubsystem
    with HasAsyncExtInterrupts
    with HasPeripheryBootROM
    with CanHaveMasterAXI4MemPort
    with CanHaveMasterAXI4MMIOPort
    with HasSystemErrorSlave {
  override lazy val module = new DebugPynqSystemModule(this)
}

class DebugPynqSystemModule[+L <: DebugPynqSystem](_outer: L)
  extends RocketSubsystemModuleImp(_outer)
    with CanHaveMasterAXI4MemPortModuleImp
    with CanHaveMasterAXI4MMIOPortModuleImp
    with HasPeripheryBootROMModuleImp
    with HasExtInterruptsModuleImp {
  // Reset vector is set to the location of the mask rom
  //val maskROMParams = p(PeripheryMaskROMKey)
  //global_reset_vector := maskROMParams(0).address.U
}
