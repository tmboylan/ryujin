//
// SPDX-License-Identifier: MIT
// Copyright (C) 2020 - 2022 by the ryujin authors
//

#include "time_loop.template.h"
#include <instantiate.h>

namespace ryujin
{
  /* instantiations */
  template class TimeLoop<Description, DIM, NUMBER>;

} // namespace ryujin
