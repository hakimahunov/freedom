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

class BaseRocketConfig extends Config(new WithDTS("freechips,rocketchip-unknown", Nil) ++ new BaseSubsystemConfig())

class DefaultRocketConfig extends Config (new WithNExtTopInterrupts(1) ++ new WithNMemoryChannels(2) ++ new WithNBigCores(1) ++ new BaseRocketConfig)

class TinyRocketConfig extends Config (new WithNMemoryChannels(0)  ++  new WithIncoherentTiles ++  new With1TinyCore ++ new WithDebugSBA  ++  new BaseRocketConfig)

class SmallRocketConfig extends Config (new WithNExtTopInterrupts(1) ++ new WithNMemoryChannels(2) ++ new WithNSmallCores(1) ++ new WithDebugSBA ++ new BaseRocketConfig)

class ZynqHardPeripherals extends Config((site, here, up) => {
  case ExtBus => Some(MasterPortParams(
                      base = x"6000_0000",
                      size = x"2000_0000",
                      beatBytes = 4,
                      idBits = 6))
  case ExtMem => Some(MasterPortParams(
                      base = x"8000_0000",
                      size = x"1000_0000",
                      beatBytes = site(MemoryBusKey).beatBytes,
                      idBits = 6))
})

class PLSoftPeripherals extends Config((site, here, up) => {
  /*case PeripheryMaskROMKey => List(
    MaskROMParams(address = 0x10000, name = "BootROM"))*/
    case BootROMParams => BootROMParams(contentFileName = "./rocket-chip/bootrom/bootrom.img")
})

class DebugPynqConfig extends Config(new PLSoftPeripherals  ++  new ZynqHardPeripherals ++  new DefaultRocketConfig)

class TinyPynqConfig extends Config(new WithNoMemPort ++ new PLSoftPeripherals ++  new ZynqHardPeripherals ++  new TinyConfig)

class SmallPynqConfig extends Config(new PLSoftPeripherals  ++ new ZynqHardPeripherals ++  new SmallRocketConfig)