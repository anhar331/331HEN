#include "util_platform.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <type_traits>

static_assert(sizeof(util_module_section_t) == 16);
static_assert(offsetof(util_module_section_t, size) == 8);
static_assert(offsetof(util_module_section_t, prot) == 12);
static_assert(offsetof(util_module_info_t, sections) == 216);
static_assert(offsetof(util_module_info_t, libname) == 1480);
static_assert(offsetof(util_module_info_t, sandboxed_path) == 1612);
static_assert(sizeof(util_module_info_t) == 2648);
static_assert(std::is_same_v<util_module_info_t, module_info_t>);

int main() {
  // Bytes at documented syscall ABI offsets, independent of struct layout.
  std::array<unsigned char, 2648> response{};
  const uint64_t base = 0x12340000;
  const uint32_t size = 0x240000, protection = 5;
  std::memcpy(response.data() + 216, &base, 8);
  std::memcpy(response.data() + 224, &size, 4);
  std::memcpy(response.data() + 228, &protection, 4);
  std::memcpy(response.data() + 1480, "eboot.bin", 10);
  std::memcpy(response.data() + 1612, "/app0/eboot.bin", 16);
  util_module_info_t module{};
  std::memcpy(&module, response.data(), response.size());
  assert(module.sections[0].vaddr == base);
  assert(module.sections[0].size == size);
  assert(module.sections[0].prot == protection);
  assert(std::strcmp(module.libname, "eboot.bin") == 0);
  assert(std::strcmp(module.sandboxed_path, "/app0/eboot.bin") == 0);
  std::cout << "PASS: module syscall ABI and bounded section sizes\n";
}
