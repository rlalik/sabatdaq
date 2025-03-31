#include "lib.hpp"

auto main() -> int
{
  auto const lib = library {};

  return lib.name == "sabatdaq" ? 0 : 1;
}
