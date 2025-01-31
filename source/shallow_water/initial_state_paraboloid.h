//
// SPDX-License-Identifier: MIT
// Copyright (C) 2020 - 2022 by the ryujin authors
//

#pragma once

#include "hyperbolic_system.h"
#include <initial_state.h>

namespace ryujin
{
  namespace ShallowWater
  {
    /**
     * A 1D/2D Paraboloid configuration. See following reference:
     *
     * W. C. Thacker, Some exact solution to the nonlinear shallo-water wave
     * equations, J. Fluid. Mech., 107:499-508, 1981.
     *
     * @ingroup ShallowWaterEquations
     */
    template <int dim, typename Number, typename state_type>
    class Paraboloid : public InitialState<dim, Number, state_type, 1>
    {
    public:
      Paraboloid(const HyperbolicSystem &hyperbolic_system,
                 const std::string subsection)
          : InitialState<dim, Number, state_type, 1>("paraboloid", subsection)
          , hyperbolic_system(hyperbolic_system)
      {
        a_ = 1.;
        this->add_parameter(
            "free surface radius", a_, "Radius of the circular free surface");

        h_0_ = 0.1;
        this->add_parameter(
            "water height", h_0_, "Water height at central point");

        eta_ = 0.5;
        this->add_parameter("eta", eta_, "The eta parameter");
      }

      state_type compute(const dealii::Point<dim> &point, Number t) final
      {
        /* Initialize primitive variables */
        Number h = 0.;   // water depth
        Number v_x = 0.; // velocity in x-direction
        Number v_y = 0.; // velocity in y-direction (if in 2D)

        /* Common quantities */
        const auto z = compute_bathymetry(point);
        const auto g = hyperbolic_system.gravity();
        const Number omega = std::sqrt(2. * g * h_0_) / a_;

        /* Define profiles for each dimension */
        switch (dim) {
        case 1:
          /* Fake 1D configuration */
          {
            const Number &x = point[0];

            h = eta_ * h_0_ / (a_ * a_) * (2. * x * std::cos(omega * t)) - z;
            h = std::max(h, Number(0.));

            v_x = -eta_ * omega * std::sin(omega * t);
          }
          break;

        case 2:
          /* 2D configuration as described in reference above */
          {
            const Number &x = point[0];
            const Number &y = point[1];

            h = eta_ * h_0_ / (a_ * a_) *
                    (2. * x * std::cos(omega * t) +
                     2 * y * std::sin(omega * t)) -
                z;
            h = std::max(h, Number(0.));

            v_x = -eta_ * omega * std::sin(omega * t);
            v_y = eta_ * omega * std::cos(omega * t);
          }
          break;
        }

        /* Set final state */
        if constexpr (dim == 1)
          return hyperbolic_system.template expand_state<dim>(
              HyperbolicSystem::state_type<1, Number>{{h, h * v_x}});
        else if constexpr (dim == 2)
          return hyperbolic_system.template expand_state<dim>(
              HyperbolicSystem::state_type<2, Number>{{h, h * v_x, h * v_y}});
        else {
          AssertThrow(false, dealii::ExcNotImplemented());
          __builtin_trap();
        }
      }

      auto initial_precomputations(const dealii::Point<dim> &point) ->
          typename InitialState<dim, Number, state_type, 1>::precomputed_type
          final
      {
        /* Compute bathymetry: */
        return {compute_bathymetry(point)};
      }

    private:
      const HyperbolicSystem &hyperbolic_system;

      DEAL_II_ALWAYS_INLINE inline Number
      compute_bathymetry(const dealii::Point<dim> &point) const
      {
        return -h_0_ * (Number(1.) - point.norm_square() / (a_ * a_));
      }

      Number a_;
      Number h_0_;
      Number eta_;
    };

  } // namespace ShallowWater
} // namespace ryujin
