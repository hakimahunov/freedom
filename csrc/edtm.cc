#include "edtm.h"
#include "debug_defines.h"
#include "encoding.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <cstring>
#include <fcntl.h> 
#include <unistd.h>

#define RV_X(x, s, n) \
  (((x) >> (s)) & ((1 << (n)) - 1))
#define ENCODE_ITYPE_IMM(x) \
  (RV_X(x, 0, 12) << 20)
#define ENCODE_STYPE_IMM(x) \
  ((RV_X(x, 0, 5) << 7) | (RV_X(x, 5, 7) << 25))
#define ENCODE_SBTYPE_IMM(x) \
  ((RV_X(x, 1, 4) << 8) | (RV_X(x, 5, 6) << 25) | (RV_X(x, 11, 1) << 7) | (RV_X(x, 12, 1) << 31))
#define ENCODE_UTYPE_IMM(x) \
  (RV_X(x, 12, 20) << 12)
#define ENCODE_UJTYPE_IMM(x) \
  ((RV_X(x, 1, 10) << 21) | (RV_X(x, 11, 1) << 20) | (RV_X(x, 12, 8) << 12) | (RV_X(x, 20, 1) << 31))

#define LOAD(xlen, dst, base, imm) \
  (((xlen) == 64 ? 0x00003003 : 0x00002003) \
   | ((dst) << 7) | ((base) << 15) | (uint32_t)ENCODE_ITYPE_IMM(imm))
#define STORE(xlen, src, base, imm) \
  (((xlen) == 64 ? 0x00003023 : 0x00002023) \
   | ((src) << 20) | ((base) << 15) | (uint32_t)ENCODE_STYPE_IMM(imm))
#define JUMP(there, here) (0x6f | (uint32_t)ENCODE_UJTYPE_IMM((there) - (here)))
#define BNE(r1, r2, there, here) (0x1063 | ((r1) << 15) | ((r2) << 20) | (uint32_t)ENCODE_SBTYPE_IMM((there) - (here)))
#define ADDI(dst, src, imm) (0x13 | ((dst) << 7) | ((src) << 15) | (uint32_t)ENCODE_ITYPE_IMM(imm))
#define SRL(dst, src, sh) (0x5033 | ((dst) << 7) | ((src) << 15) | ((sh) << 20))
#define FENCE_I 0x100f
#define EBREAK  0x00100073
#define X0 0
#define S0 8
#define S1 9

#define AC_AR_REGNO(x) ((0x1000 | x) << AC_ACCESS_REGISTER_REGNO_OFFSET)
#define AC_AR_SIZE(x)  (((x == 128)? 4 : (x == 64 ? 3 : 2)) << AC_ACCESS_REGISTER_SIZE_OFFSET)

#define WRITE 1
#define SET 2
#define CLEAR 3
#define CSRRx(type, dst, csr, src) (0x73 | ((type) << 12) | ((dst) << 7) | ((src) << 15) | (uint32_t)((csr) << 20))

#define get_field(reg, mask) (((reg) & (mask)) / ((mask) & ~((mask) << 1)))
#define set_field(reg, mask, val) (((reg) & ~(mask)) | (((val) * ((mask) & ~((mask) << 1))) & (mask)))

#define RUN_AC_OR_DIE(a, b, c, d, e) { \
    uint32_t cmderr = run_abstract_command(a, b, c, d, e);      \
    if (cmderr) {                                               \
      die(cmderr);                                              \
    }                                                           \
  }

#define LOG

unsigned int sock;
int ret;
uint8_t rbuffer[8] = {3, 165, 4, 0, 0, 0, 0, 0};
uint8_t wbuffer[12] = {2, 165, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0};
uint32_t edtm_t::do_command(edtm_t::req r)
{
  r.addr = 0x40000000 | (r.addr << 2);
  uint8_t *message;
  if(r.op == 2) {
    *(uint32_t *)(&wbuffer[4]) = (uint32_t)(r.addr);
    *(uint32_t *)(&wbuffer[8]) = (uint32_t)(r.data);
    message = wbuffer;
  }
  else {
    *(uint32_t *)(&rbuffer[4]) = (uint32_t)(r.addr);
    message = rbuffer;
  }
#ifdef LOG
    printf("%c A:0x%x D:0x%x\n",(r.op == 2) ? 'W':'R',r.addr,r.data);
#endif
  if( send(sock , message , r.op == 2 ? 12 : 8 , 0) < 0)
  {
    puts("Send failed");
    exit(1);
  }
  if( recv(sock , &ret, r.op == 2 ? 1 : 4 , 0) < 0)
  {
    puts("recv failed");
    exit(1);
  }
  if (r.op == 1) {
#ifdef LOG
    printf("Resp:0x%x\n",(uint32_t)ret);
#endif
    memset(&rbuffer[3],0,5);
    return (uint32_t)ret;
  } else if (rbuffer[0] == 1) {
    memset(&wbuffer[3],0,9);
    return 1;
  }
}

uint32_t edtm_t::read(uint32_t addr)
{
  return do_command((req){addr, 1, 0});
}

uint32_t edtm_t::write(uint32_t addr, uint32_t data)
{
  return do_command((req){addr, 2, data});
}

void edtm_t::nop()
{
  do_command((req){0, 0, 0});
}

void edtm_t::select_hart(int hartsel) {
  int dmcontrol = read(DMI_DMCONTROL);
  write (DMI_DMCONTROL, set_field(dmcontrol, DMI_DMCONTROL_HARTSEL, hartsel));
  current_hart = hartsel;
}

int edtm_t::enumerate_harts() {
  int dmstatus;
  int hartsel = 0;
  while(1) {
    select_hart(hartsel);
    dmstatus = read(DMI_DMSTATUS);
    if (get_field(dmstatus, DMI_DMSTATUS_ANYNONEXISTENT)) { 
      break;
    }
    hartsel++; 
  }
  return hartsel;
}

void edtm_t::halt(int hartsel)
{
  int dmcontrol = DMI_DMCONTROL_HALTREQ | DMI_DMCONTROL_DMACTIVE;
  dmcontrol = set_field(dmcontrol, DMI_DMCONTROL_HARTSEL, hartsel);
  write(DMI_DMCONTROL, dmcontrol);
  int dmstatus;
  do {
    dmstatus = read(DMI_DMSTATUS);
  } while(get_field(dmstatus, DMI_DMSTATUS_ALLHALTED) == 0);
  dmcontrol &= ~DMI_DMCONTROL_HALTREQ;
  write(DMI_DMCONTROL, dmcontrol);
  // Read dmstatus to avoid back-to-back writes to dmcontrol.
  read(DMI_DMSTATUS);
  current_hart = hartsel;
}

void edtm_t::resume(int hartsel)
{
  int dmcontrol = DMI_DMCONTROL_RESUMEREQ | DMI_DMCONTROL_DMACTIVE;
  dmcontrol = set_field(dmcontrol, DMI_DMCONTROL_HARTSEL, hartsel);
  write(DMI_DMCONTROL, dmcontrol);
  int dmstatus;
  do {
    dmstatus = read(DMI_DMSTATUS);
  } while (get_field(dmstatus, DMI_DMSTATUS_ALLRESUMEACK) == 0);
  dmcontrol &= ~DMI_DMCONTROL_RESUMEREQ;
  write(DMI_DMCONTROL, dmcontrol);
  // Read dmstatus to avoid back-to-back writes to dmcontrol.
  read(DMI_DMSTATUS);
  current_hart = hartsel;
}

uint64_t edtm_t::save_reg(unsigned regno)
{
  uint32_t data[xlen/(8*4)];
  uint32_t command = AC_ACCESS_REGISTER_TRANSFER | AC_AR_SIZE(xlen) | AC_AR_REGNO(regno);
  RUN_AC_OR_DIE(command, 0, 0, data, xlen / (8*4));

  uint64_t result = data[0];
  if (xlen > 32) {
    result |= ((uint64_t)data[1]) << 32;
  }
  return result;
}

void edtm_t::restore_reg(unsigned regno, uint64_t val)
{
  uint32_t data[xlen/(8*4)];
  data[0] = (uint32_t) val;
  if (xlen > 32) {
    data[1] = (uint32_t) (val >> 32);
  }

  uint32_t command = AC_ACCESS_REGISTER_TRANSFER |
    AC_ACCESS_REGISTER_WRITE |
    AC_AR_SIZE(xlen) |
    AC_AR_REGNO(regno);
  
  RUN_AC_OR_DIE(command, 0, 0, data, xlen / (8*4));

}

uint32_t edtm_t::run_abstract_command(uint32_t command,
                                     const uint32_t program[], size_t program_n,
                                     uint32_t data[], size_t data_n)
{ 
  assert(program_n <= ram_words);
  assert(data_n    <= data_words);
  
  for (size_t i = 0; i < program_n; i++) {
    write(DMI_PROGBUF0 + i, program[i]);
  }

  if (get_field(command, AC_ACCESS_REGISTER_WRITE) &&
      get_field(command, AC_ACCESS_REGISTER_TRANSFER)) {
    for (size_t i = 0; i < data_n; i++) {
      write(DMI_DATA0 + i, data[i]);
    }
  }
  
  write(DMI_COMMAND, command);
  
  // Wait for not busy and then check for error.
  uint32_t abstractcs;
  do {
    abstractcs = read(DMI_ABSTRACTCS);
  } while (abstractcs & DMI_ABSTRACTCS_BUSY);

  if ((get_field(command, AC_ACCESS_REGISTER_WRITE) == 0) &&
      get_field(command, AC_ACCESS_REGISTER_TRANSFER)) {
    for (size_t i = 0; i < data_n; i++){
      data[i] = read(DMI_DATA0 + i);
    }
  }
  
  return get_field(abstractcs, DMI_ABSTRACTCS_CMDERR);

}

size_t edtm_t::chunk_align()
{
  return xlen / 8;
}

void edtm_t::coh_read_chunk(uint64_t taddr, size_t len, void* dst)
{
  uint32_t prog[ram_words];
  uint32_t data[data_words];

  uint8_t * curr = (uint8_t*) dst;

  halt(current_hart);

  uint64_t s0 = save_reg(S0);
  uint64_t s1 = save_reg(S1);
  
  prog[0] = LOAD(xlen, S1, S0, 0);
  prog[1] = ADDI(S0, S0, xlen/8);
  prog[2] = EBREAK;

  data[0] = (uint32_t) taddr;
  if (xlen > 32) {
    data[1] = (uint32_t) (taddr >> 32);
  }

  // Write s0 with the address, then execute program buffer.
  // This will get S1 with the data and increment s0.
  uint32_t command = AC_ACCESS_REGISTER_TRANSFER |
    AC_ACCESS_REGISTER_WRITE |
    AC_ACCESS_REGISTER_POSTEXEC |
    AC_AR_SIZE(xlen) | 
    AC_AR_REGNO(S0);

  RUN_AC_OR_DIE(command, prog, 3, data, xlen/(4*8));

  // TODO: could use autoexec here.
  for (size_t i = 0; i < (len * 8 / xlen); i++){
    command = AC_ACCESS_REGISTER_TRANSFER |
      AC_AR_SIZE(xlen) |
      AC_AR_REGNO(S1);
    if ((i + 1) < (len * 8 / xlen)) {
      command |= AC_ACCESS_REGISTER_POSTEXEC;
    }
    
    RUN_AC_OR_DIE(command, 0, 0, data, xlen/(4*8));

    memcpy(curr, data, xlen/8);
    curr += xlen/8;
  }

  restore_reg(S0, s0);
  restore_reg(S1, s1);

  resume(current_hart); 

}

void edtm_t::read_chunk(uint64_t taddr, size_t len, void* dst)
{
  if (len == 8) coh_read_chunk(taddr,len,dst);
  else {
  ret = 0;
  char data[8];
  data[1] = 165;
  data[0] = 1;
  while (len / 1440) {
    *(uint16_t *)(&data[2]) = 1440;
    *(uint32_t *)(&data[4]) = taddr;
    data[7] = (1 << 4) + ((char)(data[7] << 4) >> 4);
    ret = ::write(sock , (void *)data , 8); 
    if ( ret == 0 || ret < 0) {
      printf("Send failed 0x%x \n",taddr);
      exit(1);
    }
    ::read(sock , dst, 1440);
    dst += (size_t)1440;
    taddr += 1440;
    len -= 1440;
#ifdef LOG
    printf(" TA:0x%x L:0x%x DSTP:0x%x \n",taddr,len,dst);
#endif
  } 
  if (len > 0) {
    *(uint16_t *)(&data[2]) = len;
    *(uint32_t *)(&data[4]) = taddr;
    data[7] = (1 << 4) + ((char)(data[7] << 4) >> 4);
    ret = ::write(sock, (void *)data, 8);
    if ( ret == 0 || ret < 0) {
      printf("Send failed 0x%x \n",taddr);
      exit(1);
    }    
    ::read(sock, dst, len);
  }
  }
}

void edtm_t::write_chunk(uint64_t taddr, size_t len, const void* src)
{  
  ret = 0;
  uint8_t data[1448];
  uint8_t resp;
  data[1] = 165;
  data[0] = 0;
  while (len / 1440) {
    *(uint16_t *)(&data[2]) = 1440;
    *(uint32_t *)(&data[4]) = taddr;
    data[7] = (1 << 4) + ((char)(data[7] << 4) >> 4);
    memcpy(&data[8],src,1440);
    ret = ::write(sock , data , 1448);
    if ( ret == 0 || ret < 0) {
      printf("Send failed 0x%x \n",taddr);
      exit(1);
    }
    ::read(sock , &ret, 1);
    if ((char)ret != 1) {
      printf("failed write \n");
      exit(1); 
    } 
    src += (size_t)1440;
    taddr += 1440;
    len -= 1440;
#ifdef LOG
    printf(" TA:0x%x L:0x%x SRCP:0x%x \n",taddr,len,src);
#endif
  } 
  if (len > 0) {
    *(uint16_t *)(&data[2]) = len;
    *(uint32_t *)(&data[4]) = taddr;
    data[7] = (1 << 4) + ((char)(data[7] << 4) >> 4);
    memcpy(&data[8],src,len);
    ret = ::write(sock , data , len + 8);
    if ( ret == 0 || ret < 0) {
      printf("Send failed 0x%x \n",taddr);
      exit(1);
    }
    ::read(sock , &ret, 1);
    if ((char)ret != 1) {
      printf("failed write \n");
      exit(1); 
    }
  }
}

void edtm_t::die(uint32_t cmderr)
{
  const char * codes[] = {
    "OK",
    "BUSY",
    "NOT_SUPPORTED",
    "EXCEPTION",
    "HALT/RESUME"
  };
  const char * msg;
  if (cmderr < (sizeof(codes) / sizeof(*codes))){
    msg = codes[cmderr];
  } else {
    msg = "OTHER";
  }
  //throw std::runtime_error("Debug Abstract Command Error #" + std::to_string(cmderr) + "(" +  msg + ")");
  printf("ERROR: %s:%d, Debug Abstract Command Error #%d (%s)", __FILE__, __LINE__, cmderr, msg);
  printf("ERROR: %s:%d, Should die, but allowing simulation to continue and fail.", __FILE__, __LINE__);
  write(DMI_ABSTRACTCS, DMI_ABSTRACTCS_CMDERR);

}

void edtm_t::clear_chunk(uint64_t taddr, size_t len)
{
  uint32_t data[len] = {0};
  write_chunk(taddr, len, data);
}

uint64_t edtm_t::write_csr(unsigned which, uint64_t data)
{
  return modify_csr(which, data, WRITE);
}

uint64_t edtm_t::set_csr(unsigned which, uint64_t data)
{
  return modify_csr(which, data, SET);
}

uint64_t edtm_t::clear_csr(unsigned which, uint64_t data)
{
  return modify_csr(which, data, CLEAR);
}

uint64_t edtm_t::read_csr(unsigned which)
{
  return set_csr(which, 0);
}

uint64_t edtm_t::modify_csr(unsigned which, uint64_t data, uint32_t type)
{
  halt(current_hart);

  // This code just uses DSCRATCH to save S0
  // and data_base to do the transfer so we don't
  // need to run more commands to save and restore
  // S0.
  uint32_t prog[] = {
    CSRRx(WRITE, S0, CSR_DSCRATCH, S0),
    LOAD(xlen, S0, X0, data_base),
    CSRRx(type, S0, which, S0),
    STORE(xlen, S0, X0, data_base),
    CSRRx(WRITE, S0, CSR_DSCRATCH, S0),
    EBREAK
  };

  //TODO: Use transfer = 0. For now both HW and OpenOCD
  // ignore transfer bit, so use "store to X0" NOOP.
  // We sort of need this anyway because run_abstract_command
  // needs the DATA to be written so may as well use the WRITE flag.
  
  uint32_t adata[] = {(uint32_t) data,
                      (uint32_t) (data >> 32)};
  
  uint32_t command = AC_ACCESS_REGISTER_POSTEXEC |
    AC_ACCESS_REGISTER_TRANSFER |
    AC_ACCESS_REGISTER_WRITE |
    AC_AR_SIZE(xlen) |
    AC_AR_REGNO(X0);
  
  RUN_AC_OR_DIE(command, prog, sizeof(prog) / sizeof(*prog), adata, xlen/(4*8));
  
  uint64_t res = read(DMI_DATA0);//adata[0];
  if (xlen == 64)
    res |= read(DMI_DATA0 + 1);//((uint64_t) adata[1]) << 32;
  
  resume(current_hart);
  return res;  
}

size_t edtm_t::chunk_max_size()
{
  // Arbitrary choice. 4k Page size seems reasonable.
  return 4096;
}

uint32_t edtm_t::get_xlen()
{
  // Attempt to read S0 to find out what size it is.
  // You could also attempt to run code, but you need to save registers
  // to do that anyway. If what you really want to do is figure out
  // the size of S0 so you can save it later, then do that.
  uint32_t command = AC_ACCESS_REGISTER_TRANSFER | AC_AR_REGNO(S0);
  uint32_t cmderr;
  
  const uint32_t prog[] = {};
  uint32_t data[] = {};

  cmderr = run_abstract_command(command | AC_AR_SIZE(128), prog, 0, data, 0);
  if (cmderr == 0){
    throw std::runtime_error("FESVR DTM Does not support 128-bit");
    abort();
    return 128;
  }
  write(DMI_ABSTRACTCS, DMI_ABSTRACTCS_CMDERR);

  cmderr = run_abstract_command(command | AC_AR_SIZE(64), prog, 0, data, 0);
  if (cmderr == 0){
    return 64;
  }
  write(DMI_ABSTRACTCS, DMI_ABSTRACTCS_CMDERR);

  cmderr = run_abstract_command(command | AC_AR_SIZE(32), prog, 0, data, 0);
  if (cmderr == 0){
    return 32;
  }
  
  throw std::runtime_error("FESVR DTM can't determine XLEN. Aborting");
}

void edtm_t::fence_i()
{
  halt(current_hart);

  const uint32_t prog[] = {
    FENCE_I,
    EBREAK
  };

  //TODO: Use the transfer = 0.
  uint32_t command = AC_ACCESS_REGISTER_POSTEXEC |
    AC_ACCESS_REGISTER_TRANSFER |
    AC_ACCESS_REGISTER_WRITE |
    AC_AR_SIZE(xlen) |
    AC_AR_REGNO(X0);

  RUN_AC_OR_DIE(command, prog, sizeof(prog)/sizeof(*prog), 0, 0);
  
  resume(current_hart);

}

void host_thread_main(void* arg)
{
  ((edtm_t*)arg)->producer_thread();
}

void edtm_t::reset()
{
  for (int hartsel = 0; hartsel < num_harts; hartsel ++ ){
    select_hart(hartsel);
    // this command also does a halt and resume
    fence_i();
    // after this command, the hart will run from _start.
    write_csr(0x7b1, get_entry_point());
  }
  // In theory any hart can handle the memory accesses,
  // this will enforce that hart 0 handles them.
  select_hart(0);
  read(DMI_DMSTATUS);
} 

void edtm_t::idle()
{
  for (int idle_cycles = 0; idle_cycles < 10; idle_cycles++)
    nop();
}

void edtm_t::producer_thread()
{
  // Learn about the Debug Module and assert things we
  // depend on in this code.

  // These are checked every time we run an abstract command.
  uint32_t abstractcs = read(DMI_ABSTRACTCS);
  ram_words = get_field(abstractcs, DMI_ABSTRACTCS_PROGSIZE);
  data_words = get_field(abstractcs, DMI_ABSTRACTCS_DATACOUNT);

  // These things are only needed for the 'modify_csr' function.
  // That could be re-written to not use these at some performance
  // overhead.
  uint32_t hartinfo = read(DMI_HARTINFO);
  assert(get_field(hartinfo, DMI_HARTINFO_NSCRATCH) > 0);
  assert(get_field(hartinfo, DMI_HARTINFO_DATAACCESS));

  data_base = get_field(hartinfo, DMI_HARTINFO_DATAADDR);

  // Enable the debugger.
  write(DMI_DMCONTROL, DMI_DMCONTROL_DMACTIVE);
  
  num_harts = enumerate_harts();
  halt(0);
  // Note: We don't support systems with heterogeneous XLEN.
  // It's possible to do this at the cost of extra cycles.
  xlen = get_xlen();
  resume(0);
  int exit_code = htif_t::run();
  if(exit_code != 0)  exit(1);
  else {
    printf("Sucess\n");
    exit(0);
  }
}

void edtm_t::start_host_thread()
{
  struct sockaddr_in server;
  //Create socket
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == -1)
  {
      printf("Could not create socket");
      exit(1);
  }
  server.sin_addr.s_addr = inet_addr("192.168.1.10");
  server.sin_family = AF_INET;
  server.sin_port = htons(12000);
  //Connect to remote server
  if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
  {
      perror("connect failed. Error");
      //exit(1);
  }
  host.init(host_thread_main, this);
  host.switch_to();
}

edtm_t::edtm_t(int argc, char** argv)
  : htif_t(argc, argv)
{
  start_host_thread();
}

edtm_t::~edtm_t()
{
}

int main(int argc, char** argv)
{
  uint64_t max_cycles = 0;
  const char* loadmem = NULL;
  const char* failure = NULL;
  for (int i = 1; i < argc; i++)
  {
    std::string arg = argv[i];
    if (arg.substr(0, 12) == "+max-cycles=")
      max_cycles = atoll(argv[i]+12);
  }

  edtm_t *dtm = new edtm_t(argc,argv);
}
