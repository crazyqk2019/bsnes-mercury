#include <sfc/sfc.hpp>

#define MEMORY_CPP
namespace SuperFamicom {

Bus bus;

void Bus::map(
  const function<uint8 (unsigned)>& reader,
  const function<void (unsigned, uint8)>& writer,
  unsigned banklo, unsigned bankhi,
  unsigned addrlo, unsigned addrhi,
  unsigned size, unsigned base, unsigned mask,
  Cartridge::Mapping::fastmode_t fastmode, uint8* fast_ptr
) {
  assert(banklo <= bankhi && banklo <= 0xff);
  assert(addrlo <= addrhi && addrlo <= 0xffff);
  assert(idcount < 255);

  for(unsigned bank = banklo; bank <= bankhi; bank++) {
    for (unsigned addr = addrlo; addr<addrhi; addr+=fast_page_size) {
      unsigned offset=(bank<<16|addr)>>fast_page_size_bits;
      if (fastmode!=fastmode_slow) fast_read[offset] = fast_ptr - (bank<<16|addr);
      else fast_read[offset] = NULL;
      if (fastmode==fastmode_readwrite) fast_write[offset] = fast_ptr - (bank<<16|addr);
      else fast_write[offset] = NULL;
    }
    if (addrlo&fast_page_size_mask)
    {
      fast_read[(bank<<16|addrlo)>>fast_page_size_bits] = NULL;
      fast_write[(bank<<16|addrlo)>>fast_page_size_bits] = NULL;
    }
    if (addrhi&fast_page_size_mask)
    {
      fast_read[(bank<<16|addrhi)>>fast_page_size_bits] = NULL;
      fast_write[(bank<<16|addrhi)>>fast_page_size_bits] = NULL;
    }
  }

  unsigned id = idcount++;
  this->reader[id] = reader;
  this->writer[id] = writer;

  if (!(mask & (addrlo^addrhi)) && !size) {
    for(unsigned bank = banklo; bank <= bankhi; bank++) {
      unsigned offset = reduce(bank << 16 | addrlo, mask);
      unsigned pos = (bank<<16 | addrlo);
      unsigned end = (bank<<16 | addrhi);
      while (pos <= end) {
        lookup[pos] = id;
        target[pos] = offset++;
        pos++;
      }
    }
    return;
  }

  for(unsigned bank = banklo; bank <= bankhi; bank++) {
    for(unsigned addr = addrlo; addr <= addrhi; addr++) {
      unsigned offset = reduce(bank << 16 | addr, mask);
      if(size) offset = base + mirror(offset, size - base);
      lookup[bank << 16 | addr] = id;
      target[bank << 16 | addr] = offset;
    }
  }
}

void Bus::map_reset() {
  function<uint8 (unsigned)> reader = [](unsigned) { return cpu.regs.mdr; };
  function<void (unsigned, uint8)> writer = [](unsigned, uint8) {};

  idcount = 0;
  map(reader, writer, 0x00, 0xff, 0x0000, 0xffff, Cartridge::Mapping::fastmode_slow, NULL);
}

void Bus::map_xml() {
  for(auto& m : cartridge.mapping) {
    lstring part = m.addr.split<1>(":");
    lstring banks = part(0).split(",");
    lstring addrs = part(1).split(",");
    for(auto& bank : banks) {
      for(auto& addr : addrs) {
        lstring bankpart = bank.split<1>("-");
        lstring addrpart = addr.split<1>("-");
        unsigned banklo = hex(bankpart(0));
        unsigned bankhi = hex(bankpart(1, bankpart(0)));
        unsigned addrlo = hex(addrpart(0));
        unsigned addrhi = hex(addrpart(1, addrpart(0)));
        map(m.reader, m.writer, banklo, bankhi, addrlo, addrhi, m.size, m.base, m.mask, m.fastmode, m.fast_ptr);
      }
    }
  }
}

Bus::Bus() {
  lookup = new uint8 [16 * 1024 * 1024];
  target = new uint32[16 * 1024 * 1024];

  fast_read  = new uint8*[16*1024*1024/fast_page_size];
  fast_write = new uint8*[16*1024*1024/fast_page_size];
}

Bus::~Bus() {
  delete[] lookup;
  delete[] target;

  delete[] fast_read;
  delete[] fast_write;
}

}
