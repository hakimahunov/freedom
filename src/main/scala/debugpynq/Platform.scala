// See LICENSE for license details.
package sifive.freedom.debug.pynq

import chisel3._
import chisel3.util._

import freechips.rocketchip.config._
import freechips.rocketchip.subsystem._
import freechips.rocketchip.devices.debug._
import freechips.rocketchip.devices.tilelink._
import freechips.rocketchip.diplomacy._
import freechips.rocketchip.amba.axi4._
import freechips.rocketchip.system._
import freechips.rocketchip.util._

object AXIParams {
  val a6xi32 = new AXI4BundleParameters(32,32,6,0)
  val a12xi32 = new AXI4BundleParameters(32,32,12,0)
  val axi64 = new AXI4BundleParameters(32,64,6,0)  
}

class AXI4toDMI()(implicit p: Parameters) extends Module {
  val io = IO(new Bundle {
    val ps_slave = Flipped(new AXI4Bundle(AXIParams.a12xi32))
    val dmi = new DMIIO()
  })
  //Initialize IO
  io.ps_slave.w.ready := false.B 
  io.ps_slave.aw.ready := false.B
  io.ps_slave.ar.ready := false.B
  io.ps_slave.b.valid := false.B
  io.ps_slave.r.valid := false.B
  io.ps_slave.r.bits := 0.U.asTypeOf(new AXI4BundleR(AXIParams.a12xi32))
  io.ps_slave.b.bits := 0.U.asTypeOf(new AXI4BundleR(AXIParams.a12xi32))
  io.dmi.req.valid := false.B
  io.dmi.req.bits := 0.U.asTypeOf(new DMIReq(p(DebugModuleParams).nDMIAddrSize))
  io.dmi.resp.ready := false.B
  val op = Reg(UInt(2.W))
  val id = Reg(UInt(12.W))
  val waddr = io.ps_slave.aw.bits.addr
  val raddr = io.ps_slave.ar.bits.addr
  
  when (io.ps_slave.aw.valid && io.ps_slave.w.valid) {
    io.dmi.req.bits.addr := io.ps_slave.aw.bits.addr(p(DebugModuleParams).nDMIAddrSize + 1,0) >> 2
    io.dmi.req.bits.op := DMIConsts.dmi_OP_WRITE
    op := DMIConsts.dmi_OP_WRITE
    id := io.ps_slave.aw.bits.id
    io.dmi.req.bits.data := io.ps_slave.w.bits.data
    io.dmi.req.valid := true.B
    io.ps_slave.w.ready := io.dmi.req.ready 
    io.ps_slave.aw.ready := io.dmi.req.ready
  } .elsewhen (io.ps_slave.ar.valid) {
    io.dmi.req.bits.addr := io.ps_slave.ar.bits.addr(p(DebugModuleParams).nDMIAddrSize + 1,0) >> 2
    io.dmi.req.bits.op := DMIConsts.dmi_OP_READ
    op := DMIConsts.dmi_OP_READ
    id := io.ps_slave.ar.bits.id
    io.dmi.req.valid := true.B
    io.ps_slave.ar.ready := io.dmi.req.ready
  } .otherwise {
    io.dmi.req.valid := false.B
  }

  io.ps_slave.r.bits.id := id
  io.ps_slave.b.bits.id := id
  io.ps_slave.r.bits.data := io.dmi.resp.bits.data
  io.ps_slave.b.bits.resp := io.dmi.resp.bits.resp
  when (io.dmi.resp.valid) {
    when (op === DMIConsts.dmi_OP_READ) {
      io.ps_slave.r.valid := true.B
      io.ps_slave.r.bits.last := true.B
    } .elsewhen (op === DMIConsts.dmi_OP_WRITE) {
      io.ps_slave.b.valid := true.B
    }
  }

  when (op === DMIConsts.dmi_OP_READ) {
    io.dmi.resp.ready := io.ps_slave.r.ready
  } .elsewhen (op === DMIConsts.dmi_OP_WRITE) {
    io.dmi.resp.ready := io.ps_slave.b.ready
  }

  when (io.dmi.resp.fire()) {
    op := DMIConsts.dmi_OP_NONE
  }
}

class DebugPynqPlatformIO(implicit val p: Parameters) extends Bundle {
  val m_gp0     = Flipped(new AXI4Bundle(AXIParams.a12xi32))
  val s_gp0     = new AXI4Bundle(AXIParams.a6xi32)
  val s_hp0     = (!p(ExtMem).isEmpty).option(new AXI4Bundle(AXIParams.axi64))
  val s_hp2     = (!p(ExtMem).isEmpty).option(new AXI4Bundle(AXIParams.axi64))
}

class DebugPynqPlatform(implicit val p: Parameters) extends Module {
  val sys = Module(LazyModule(new DebugPynqSystem).module)
  val io = IO(new DebugPynqPlatformIO)

  val convert = Module(new AXI4toDMI())
  convert.io.ps_slave <> io.m_gp0
  if (!p(ExtMem).isEmpty) {
    io.s_hp0.get <> sys.mem_axi4.elts(0)
    io.s_hp2.get <> sys.mem_axi4.elts(1)
    // change base address of memory
    // map 0x80000000 -> 0x10000000
    io.s_hp0.get.aw.bits.addr := Cat(1.asUInt(4.W),sys.mem_axi4.elts(0).aw.bits.addr(27,0))
    io.s_hp0.get.ar.bits.addr := Cat(1.asUInt(4.W),sys.mem_axi4.elts(0).ar.bits.addr(27,0))
    io.s_hp2.get.aw.bits.addr := Cat(1.asUInt(4.W),sys.mem_axi4.elts(1).aw.bits.addr(27,0))
    io.s_hp2.get.ar.bits.addr := Cat(1.asUInt(4.W),sys.mem_axi4.elts(1).ar.bits.addr(27,0))
  }
  io.s_gp0 <> sys.mmio_axi4.elts(0)
  // change base address of zynq i/o config registers
  // map 0x60000000 -> 0xE0000000
  io.s_gp0.aw.bits.addr := Cat(7.asUInt(3.W),sys.mmio_axi4.elts(0).aw.bits.addr(28,0))
  io.s_gp0.ar.bits.addr := Cat(7.asUInt(3.W),sys.mmio_axi4.elts(0).ar.bits.addr(28,0))
  sys.debug.clockeddmi.foreach { dbg =>
    dbg.dmi <> convert.io.dmi
    dbg.dmiClock := clock
    dbg.dmiReset := reset
  }

  sys.tieOffInterrupts()
}
