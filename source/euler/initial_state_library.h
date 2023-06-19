//
// SPDX-License-Identifier: MIT
// Copyright (C) 2020 - 2022 by the ryujin authors
//

#pragma once

#include "hyperbolic_system.h"
#include <initial_state.h>

#include "initial_state_becker_solution.h"
#include "initial_state_contrast.h"
#include "initial_state_isentropic_vortex.h"
#include "initial_state_leblanc.h"
#include "initial_state_noh.h"
#include "initial_state_radial_contrast.h"
#include "initial_state_ramp_up.h"
#include "initial_state_rarefaction.h"
#include "initial_state_shock_front.h"
#include "initial_state_twoD_contrast.h"
#include "initial_state_uniform.h"

namespace ryujin
{
  namespace Euler
  {
    struct InitialStateLibrary {
      /**
       * Populate a given container with all initial state defined in this
       * namespace
       *
       * @ingroup EulerEquations
       */
      template <int dim, typename Number, typename T>
      static void populate_initial_state_list(T &initial_state_list,
                                              const HyperbolicSystem &h,
                                              const std::string &s)
      {
        using state_type = HyperbolicSystem::state_type<dim, Number>;

        auto add = [&](auto &&object) {
          initial_state_list.emplace(std::move(object));
        };

        add(std::make_unique<BeckerSolution<dim, Number, state_type>>(h, s));
        add(std::make_unique<Contrast<dim, Number, state_type>>(h, s));
        add(std::make_unique<IsentropicVortex<dim, Number, state_type>>(h, s));
        add(std::make_unique<LeBlanc<dim, Number, state_type>>(h, s));
        add(std::make_unique<Noh<dim, Number, state_type>>(h, s));
        add(std::make_unique<RadialContrast<dim, Number, state_type>>(h, s));
        add(std::make_unique<RampUp<dim, Number, state_type>>(h, s));
        add(std::make_unique<Rarefaction<dim, Number, state_type>>(h, s));
        add(std::make_unique<ShockFront<dim, Number, state_type>>(h, s));
        add(std::make_unique<TwoDContrast<dim, Number, state_type>>(h, s));
        add(std::make_unique<Uniform<dim, Number, state_type>>(h, s));
      }
    };
  } // namespace Euler
} // namespace ryujin
