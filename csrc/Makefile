root := $(abspath ../)

edtm: ./edtm.cc
	$(CXX) $(CXXFLAGS) -I$(root)/rocket-chip/riscv-tools/riscv-fesvr/fesvr -o $@ $< $(root)/rocket-chip/riscv-tools/riscv-fesvr/build/libfesvr.so
