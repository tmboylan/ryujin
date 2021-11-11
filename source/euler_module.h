//
// SPDX-License-Identifier: MIT
// Copyright (C) 2020 - 2021 by the ryujin authors
//

#pragma once

#include <compile_time_options.h>

#include "convenience_macros.h"
#include "simd.h"

#include "limiter.h"

#include "initial_values.h"
#include "offline_data.h"
#include "problem_description.h"
#include "sparse_matrix_simd.h"

#include <deal.II/base/parameter_acceptor.h>
#include <deal.II/base/timer.h>
#include <deal.II/lac/sparse_matrix.templates.h>
#include <deal.II/lac/vector.h>

#include <functional>

namespace ryujin
{
  /**
   * An enum controlling the behavior on detection of an invariant domain
   * or CFL violation. Such a case might occur for either aggressive CFL
   * numbers > 1, and/or later stages in the Runge Kutta scheme when the
   * time step tau is prescribed.
   *
   * The invariant domain violation is detected in the limiter and
   * typically implies that the low-order update is already out of
   * bounds. We further do a quick sanity check whether the computed
   * step size tau_max and the prescribed step size tau are within an
   * acceptable tolerance of about 10%.
   */
  enum class IDViolationStrategy {
    /**
     * Warn about an invariant domain violation but take no further
     * action.
     */
    warn,

    /**
     * Raise a ryujin::Restart exception on domain violation. This
     * exception can be caught in TimeIntegrator and various different
     * actions (adapt CFL and retry) can be taken depending on chosen
     * strategy.
     */
    raise_exception,
  };


  /**
   * A class signalling a restart, thrown in EulerModule::single_step and
   * caught at various places.
   */
  class Restart final
  {
  };


  /**
   * Explicit forward Euler time-stepping for hyperbolic systems with
   * convex limiting.
   *
   * This module is described in detail in @cite ryujin-2021-1, Alg. 1.
   *
   * @ingroup EulerModule
   */
  template <int dim, typename Number = double>
  class EulerModule final : public dealii::ParameterAcceptor
  {
  public:
    /**
     * @copydoc ProblemDescription::problem_dimension
     */
    // clang-format off
    static constexpr unsigned int problem_dimension = ProblemDescription::problem_dimension<dim>;
    // clang-format on

    /**
     * @copydoc ProblemDescription::state_type
     */
    using state_type = ProblemDescription::state_type<dim, Number>;

    /**
     * @copydoc ProblemDescription::flux_type
     */
    using flux_type = ProblemDescription::flux_type<dim, Number>;

    /**
     * @copydoc OfflineData::scalar_type
     */
    using scalar_type = typename OfflineData<dim, Number>::scalar_type;

    /**
     * @copydoc OfflineData::vector_type
     */
    using vector_type = typename OfflineData<dim, Number>::vector_type;

    /**
     * Constructor.
     */
    EulerModule(const MPI_Comm &mpi_communicator,
                std::map<std::string, dealii::Timer> &computing_timer,
                const ryujin::OfflineData<dim, Number> &offline_data,
                const ryujin::ProblemDescription &problem_description,
                const ryujin::InitialValues<dim, Number> &initial_values,
                const std::string &subsection = "EulerModule");

    /**
     * Prepare time stepping. A call to @ref prepare() allocates temporary
     * storage and is necessary before any of the following time-stepping
     * functions can be called.
     */
    void prepare();

    /**
     * @name Functons for performing explicit time steps
     */
    //@{

    /**
     * Given a reference to a previous state vector @ref old_U perform an
     * explicit euler step (and store the result in @ref new_U). The
     * function returns the computed maximal time step size tau_max
     * according to the CFL condition.
     *
     * The time step is performed with either tau_max (if @ref tau is set
     * to 0), or tau (if @ref tau is nonzero). Here, tau_max is the
     * computed maximal time step size and @ref tau is the last parameter
     * of the function.
     *
     * The function takes an optional array of states @ref stage_U
     * and high-order graph viscosities @ref stage_dij together with a an
     * array of weights @ref stage_weights to construct a modified
     * high-order flux. The standard high-order flux reads (cf @cite
     * ryujin-2021-1, Eq. 12):
     * \f{align}
     *   \newcommand{\bR}{{\boldsymbol R}}
     *   \newcommand{\bU}{{\boldsymbol U}}
     *   \newcommand\bUni{\bU^n_i}
     *   \newcommand\bUnj{\bU^n_j}
     *   \newcommand{\polf}{{\mathbb f}}
     *   \newcommand\Ii{\mathcal{I}(i)}
     *   \newcommand{\bc}{{\boldsymbol c}}
     *   \sum_{j\in\Ii} \frac{m_{ij}}{m_{j}}
     *   \;
     *   \frac{m_{j}}{\tau_n}\big(
     *   \tilde\bU_j^{H,n+1} - \bU_j^{n}\big)
     *   \;=\;
     *   \bR^n_i,
     *   \qquad\text{with}\quad
     *   \bR^n_i\;:=\;
     *   \sum_{j\in\Ii}\Big(-\polf(\bUnj) \cdot\bc_{ij}
     *   +d_{ij}^{H,n}\big(\bUnj-\bUni\big)\Big).
     * \f}
     * Instead, the function assembles the modified high-order flux:
     * \f{align}
     *   \newcommand{\bR}{{\boldsymbol R}}
     *   \newcommand{\bU}{{\boldsymbol U}}
     *   \newcommand\bUnis{\bU^{s,n}_i}
     *   \newcommand\bUnjs{\bU^{s,n}_j}
     *   \newcommand{\polf}{{\mathbb f}}
     *   \newcommand\Ii{\mathcal{I}(i)}
     *   \newcommand{\bc}{{\boldsymbol c}}
     *   \tilde{\bR}^n_i\;:=\;
     *   \big(1-\sum_{s=\{1:stages\}}\omega_s\big)\bR^n_i
     *   \;+\;
     *   \sum_{s=\{1:stages\}}\omega_s
     *   \sum_{j\in\Ii}\Big(-\polf(\bUnjs) \cdot\bc_{ij}
     *   +d_{ij}^{H,s,n}\big(\bUnjs-\bUnis\big)\Big),
     * \f}
     * where \f$\omega_s\f$ denotes the weigths for the given stages.
     *
     * If the template parameter @ref record_dij is set to true, then the
     * new high-order flux is written back into the supplied @ref new_dij
     * matrix. If the template parameter is false, then the supplied
     * parameter is ignored.
     *
     * @note The routine does not automatically update ghost vectors of the
     * distributed vector @ref new_U. It is best to simply call
     * EulerModule::apply_boundary_conditions() on the appropriate vector
     * immediately after performing a time step.
     */
    template <int stages, bool record_dij = false>
    Number
    step(const vector_type &old_U,
         std::array<std::reference_wrapper<const vector_type>, stages> stage_U,
         std::array<std::reference_wrapper<const SparseMatrixSIMD<Number>>,
                    stages> stage_dij,
         const std::array<Number, stages> stage_weights,
         vector_type &new_U,
         SparseMatrixSIMD<Number> &new_dij,
         Number tau = Number(0.)) const;

    /**
     * This function postprocesses a given state @ref U to conform with all
     * prescribed boundary conditions at time @ref t. This implies that on
     * slip (and no-slip) boundaries the normal momentum is set to zero; on
     * Dirichlet boundaries the appropriate state at time @ref t is
     * substituted; and on "flexible" boundaries depending on the fact
     * whether we have supersonic or subsonic inflow/outflow the
     * appropriate Riemann invariant is prescribed. See @cite ryujin-2021-3
     * for details.
     *
     * @note The routine does update ghost vectors of the distributed
     * vector @ref U
     */
    void apply_boundary_conditions(vector_type &U, Number t) const;

    /**
     * Sets the relative CFL number used for computing an appropriate
     * time-step size to the given value. The CFL number must be a positive
     * value. If chosen to be within the interval \f$(0,1)\f$ then the
     * low-order update and limiting stages guarantee invariant domain
     * preservation.
     */
    void cfl(Number new_cfl) const
    {
      Assert(cfl_ > Number(0.), dealii::ExcInternalError());
      cfl_ = new_cfl;
    }

    mutable IDViolationStrategy id_violation_strategy_;

  private:

   //@}
    /**
     * @name Run time options
     */
    //@{

    unsigned int limiter_iter_;

    bool cfl_with_boundary_dofs_;

    //@}

    //@}
    /**
     * @name Internal data
     */
    //@{

    const MPI_Comm &mpi_communicator_;
    std::map<std::string, dealii::Timer> &computing_timer_;

    dealii::SmartPointer<const ryujin::OfflineData<dim, Number>> offline_data_;
    dealii::SmartPointer<const ryujin::ProblemDescription> problem_description_;
    dealii::SmartPointer<const ryujin::InitialValues<dim, Number>>
        initial_values_;

    mutable Number cfl_;
    ACCESSOR_READ_ONLY(cfl)

    mutable unsigned int n_restarts_;
    ACCESSOR_READ_ONLY(n_restarts)

    mutable unsigned int n_warnings_;
    ACCESSOR_READ_ONLY(n_warnings)

    mutable scalar_type alpha_;
    mutable scalar_type second_variations_;
    mutable scalar_type specific_entropies_;
    mutable scalar_type evc_entropies_;

    mutable MultiComponentVector<Number, Limiter<dim, Number>::n_bounds> bounds_;

    mutable vector_type r_;

    mutable SparseMatrixSIMD<Number> dij_matrix_;
    mutable SparseMatrixSIMD<Number> lij_matrix_;
    mutable SparseMatrixSIMD<Number> lij_matrix_next_;
    mutable SparseMatrixSIMD<Number, problem_dimension> pij_matrix_;

    //@}
  };

} /* namespace ryujin */
