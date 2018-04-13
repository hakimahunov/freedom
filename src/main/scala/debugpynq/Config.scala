// See LICENSE for license details.
package sifive.freedom.debug.pynq

import Chisel._
import freechips.rocketchip.config._
import freechips.rocketchip.subsystem._
import freechips.rocketchip.devices.debug._
import freechips.rocketchip.devices.tilelink._
import freechips.rocketchip.diplomacy._
import freechips.rocketchip.system._
import freechips.rocketchip.tile._

class DefaultDebugPynqConfig extends Config (
  new WithNBreakpoints(2)        ++
  new WithNExtTopInterrupts(1)   ++
  new DualChannelConfig          ++
  new WithNBigCores(1)
)

class ZynqHardPeripherals extends Config((site, here, up) => {
  case ExtBus => MasterPortParams(
                      base = x"6000_0000",
                      size = x"2000_0000",
                      beatBytes = 4,
                      idBits = 6)
  case ExtMem => MasterPortParams(
                      base = x"8000_0000",
                      size = x"1000_0000",
                      beatBytes = site(MemoryBusKey).beatBytes,
                      idBits = 6)
})

class PLSoftPeripherals extends Config((site, here, up) => {
  case PeripheryMaskROMKey => List(
    MaskROMParams(address = 0x10000, name = "BootROM"))
})

class DebugPynqConfig extends Config(
  new PLSoftPeripherals      ++
  new ZynqHardPeripherals    ++
  new DefaultDebugPynqConfig().alter((site,here,up) => {
    case DTSTimebase => BigInt(32768)
  })
)
