.. _running:

Running QMCPACK
===============

QMCPACK requires at least one xml input file, and is invoked via:

``qmcpack [command line options] <XML input file(s)>``

.. _commandline:

Command line options
--------------------

QMCPACK offers several command line options that affect how calculations
are performed. If the flag is absent, then the corresponding
option is disabled:

- ``--dryrun`` Validate the input file without performing the simulation. This is a good way to ensure that QMCPACK will do what you think it will.

- ``--enable-timers=none|coarse|medium|fine`` Control the timer granularity when the build option ``ENABLE_TIMERS`` is enabled.

- ``help`` Print version information as well as a list of optional
  command-line arguments.

- ``noprint`` Do not print extra information on Jastrow or pseudopotential.
  If this flag is not present, QMCPACK will create several ``.dat`` files
  that contain information about pseudopotentials (one file per PP) and Jastrow
  factors (one per Jastrow factor). These file might be useful for visual inspection
  of the Jastrow, for example.

- ``--verbosity=low|high|debug`` Control the output verbosity. The default low verbosity is concise and, for example, does not include all electron or atomic positions for large systems to reduce output size. Use "high" to see this information and more details of initialization, allocations, QMC method settings, etc.

- ``version`` Print version information and optional arguments. Same as ``help``.

.. _inputs:

Input files
-----------

The input is one or more XML file(s), documented in :ref:`input-overview`.

Output files
------------

QMCPACK generates multiple files documented in :ref:`output-overview`.

Stopping a running simulation
-----------------------------

As detailed in :ref:`input-overview`, QMCPACK will cleanly stop execution at the end of the current block if it finds a file named
``project_id.STOP``, where ``project_id`` is the name of the project given in the input XML. You can also set the ``max_seconds``
parameter to establish an overall time limit.

.. _mixed_precision:

Using mixed precision
---------------------
To achieve better performance or reduce memory footprint, mixed-precision version can be enabled.
The current implementation uses single precision (SP) on most calculations, except for matrix inversions
and reductions where double precision (DP) is required to retain high accuracy. All the
constant spline data in wavefunction, pseudopotentials, and Coulomb potentials are initialized in double precision and later
stored in SP. The mixed-precision code is as accurate as the fully double precision code up to a certain system size, and
may have double the throughput.
Cross checking and verification of accuracy is always required but is particularly important above approximately 1,500 electrons.

.. _parallelrunning:

Running in parallel with MPI
----------------------------

QMCPACK is fully parallelized with MPI. When performing an ensemble job, all
the MPI ranks are first equally divided into groups that perform individual
QMC calculations. Within one calculation, all the walkers are fully distributed
across all the MPI ranks in the group. Each compute node must have at least one MPI rank.
Having one MPI rank per CPU core is a bad practice due to high total memory footprint
caused by datasets that have to be duplicated on each MPI rank.

We recommend users study the hardware architecture of a compute node before starting any calculation on it.
Suboptimal choice of the number of MPI ranks and their binding to the hardware may lead to significant waste of compute resource.
The rule of thumb is to have the number of MPI ranks per node equal to the number of memory domains with uniform access
attached to the dominant compute devices within a compute node. Fewer can be used when memory is constrained.
On most CPU-only machines, each CPU socket has its dedicated memory with uniform access from all its cores and cross-socket access is non-uniform.
Users may simply place one MPI rank per socket.
There are CPU sockets consisting of core clusters and cross-cluster memory access is non-uniform like Fujitsu A64FX.
In such case, the largest uniform access memory domain is a cluster and thus users should place one MPI rank per cluster for optimal code performance.
On machines with GPU accelerators, GPUs are the primary compute devices and thus users should count the number of
uniform access memory domains attached to GPUs. Usually each GPU card has a single GPU die with its own dedicated graphic memory, counted as one domain.
users may simply place one MPI rank per GPU card. High-end GPU cards may have more than a single GPU memory domain.
For example, AMD Instinct MI250X and Intel Data Center GPU Max 1550 cards both have two memory domains per card.
users should place one MPI rank per GPU memory domain (AMD GCD, Intel tile).

.. _openmprunning:

Using OpenMP threads
--------------------

Modern processors integrate multiple identical cores even with
hardware threads on a single die to increase the total performance and
maintain a reasonable power draw. QMCPACK takes advantage of this
compute capability by using threads directly via the OpenMP programming model
and indirectly via threaded linear algebra libraries. By default, QMCPACK is
always built with OpenMP enabled. When launching calculations, users
should instruct QMCPACK to create the right number of threads per MPI
rank by specifying environment variable OMP\_NUM\_THREADS.
It is recommended to set the number of OpenMP threads equal to the number
of physical CPU cores that can be exclusively assigned to each MPI rank.
Even when the GPU-acceleration is enabled, using threads significantly
reduces the time spent on the calculations performed by the CPU. Almost all the MPI launchers
require proper configuration to map the OpenMP threads to the processor cores correctly
and avoid assigning multiple threads to the same processor core. If this happens very significant
slowdowns result. Users should check their MPI documentation and verify performance before doing costly production calculations.

Nested OpenMP threads
~~~~~~~~~~~~~~~~~~~~~

Nested threading is an advanced feature requiring experienced users to finely tune runtime parameters to reach the best performance.

For small-to-medium problem sizes, using one thread per walker or for multiple walkers is most efficient. This is the default in QMCPACK and achieves the shortest time to solution.

For large problems of at least 1,000 electrons, use of nested OpenMP threading can be enabled to reduce the time to solution further, although at some loss of efficiency. In this scheme multiple threads are used in the computations of each walker. This capability is implemented for some of the key computational kernels: the 3D spline orbital evaluation, certain portions of the distance tables, and implicitly the BLAS calls in the determinant update. Use of the batched nonlocal pseudopotential evaluation is also recommended.

Nested threading is enabled by setting ``OMP_NUM_THREADS=AA,BB``, ``OMP_MAX_ACTIVE_LEVELS=2`` and ``OMP_NESTED=TRUE`` where the additional ``BB`` is the number of second-level threads.  Choosing the thread affinity is critical to the performance.
QMCPACK provides a tool qmc-check-affinity (source file src/QMCTools/check-affinity.cpp for details), which might help users investigate the affinity. Knowledge of how the operating system logical CPU cores (/prco/cpuinfo) are bound to the hardware is also needed.

For example, on Blue Gene/Q with a Clang compiler, the best way to fully use the 16 cores each with 4 hardware threads is

::

  OMP_NESTED=TRUE
  OMP_NUM_THREADS=16,4
  MAX_ACTIVE_LEVELS=2
  OMP_PLACES=threads
  OMP_PROC_BIND=spread,close

On Intel Xeon Phi KNL with an Intel compiler, to use 64 cores without using hardware threads:

::

  OMP_NESTED=TRUE
  OMP_WAIT_POLICY=ACTIVE
  OMP_NUM_THREADS=16,4
  MAX_ACTIVE_LEVELS=2
  OMP_PLACES=cores
  OMP_PROC_BIND=spread,close
  KMP_HOT_TEAMS_MODE=1
  KMP_HOT_TEAMS_MAX_LEVEL=2

Most multithreaded BLAS/LAPACK libraries do not spawn threads by default
when being called from an OpenMP parallel region. See the explanation in :ref:`threadedlibrary`.
This results in the use of only a single thread in each second-level thread team for BLAS/LAPACK operations.
Some vendor libraries like MKL support using multiple threads when being called from an OpenMP parallel region.
One way to enable this feature is using environment variables to override the default behavior.
However, this forces all the calls to the library to use the same number of threads.
As a result, small function calls are penalized with heavy overhead and heavy function calls are slow for not being able to use more threads.
Instead, QMCPACK uses the library APIs to turn on nested threading only at selected performance critical calls.
In the case of using a serial library, QMCPACK implements nested threading to distribute the workload wherever necessary.
Users do not need to control the threading behavior of the library.

.. _cpu-performance:

Performance considerations
~~~~~~~~~~~~~~~~~~~~~~~~~~

As walkers are the basic units of workload in QMC algorithms, they are loosely coupled and distributed across all the threads. For this reason, the best strategy to run QMCPACK efficiently is to feed enough walkers to the available threads.

In a VMC calculation, the code automatically raises the actual number of walkers per MPI rank to the number of available threads
if the user-specified number of walkers is smaller, see "walkers/mpi=XXX" in the VMC output.

In DMC, for typical small to mid-sized calculations choose the total number of walkers to be a significant multiple of the total number of
threads (MPI tasks * threads per task). This will ensure a good load balance. e.g., for a calculation on a few nodes with a total
512 threads, using 5120 walkers may keep the load imbalance around 10\%. For the very largest calculations, the target number of
walkers should be chosen to be slightly smaller than a multiple of the total number of available threads across all the MPI ranks.
This will reduce occurrences worse-case load imbalance e.g. where one thread has two walkers while all the others have one.

Memory considerations
~~~~~~~~~~~~~~~~~~~~~

When using threads, some memory objects are shared by all the threads. Usually these memory objects are read only when the walkers are evolving, for instance the ionic distance table and wavefunction coefficients.
If a wavefunction is represented by B-splines, the whole table is shared by all the threads. It usually takes a large chunk of memory when a large primitive cell was used in the simulation. Its actual size is reported as "MEMORY increase XXX MB BsplineSetReader" in the output file.
See details about how to reduce it in :ref:`spo-spline`.

The other memory objects that are distinct for each walker during random walks need to be
associated with individual walkers and cannot be shared. This part of memory grows linearly as the number of walkers per MPI rank. Those objects include wavefunction values (Slater determinants) at given electronic configurations and electron-related distance tables (electron-electron distance table). Those matrices dominate the :math:`N^2` scaling of the memory usage per walker.

.. _gpurunning:

Running on GPU machines
-----------------------

The GPU version is fully incorporated into the main source code.
It works on any GPUs with OpenMP offload support including NVIDIA, AMD and Intel GPUs.
Using batched drivers is required.

QMCPACK supports running on multi-GPU node architectures via MPI.
Each MPI rank gets assigned a primary GPU based on the list of GPUs visible to it and its rank id
in the smallest MPI communicator, usually the node local communicator, enclosing that list of GPUs.
When there are more GPUs than the MPI ranks, excessive GPUs will be left idle.
Please avoid this scenario in production runs.
When there are more MPI ranks than GPUs, the primary GPU will be assigned in the following way.
Performance portable implementation assigns GPUs to equal amount of blocks of MPI ranks.
MPI ranks within a block are assigned the same GPU as their primary GPU.
Legacy implementation assigns GPUs to MPI ranks in a round-robin order.
It is guaranteed that MPI ranks are distributed among GPUs as evenly as possbile.
Currently, for medium to large runs, 1 MPI task should be used per GPU per node.
For very smaller system sizes, use of multiple MPI tasks per GPU might yield improved performance.

.. _gpu-performance:

Performance considerations
~~~~~~~~~~~~~~~~~~~~~~~~~~

To run with high performance on GPUs it is crucial to perform some
benchmarking runs: the optimum configuration is system size, walker
count, and GPU model dependent. The GPU implementation vectorizes
operations over multiple walkers, so generally the more walkers that
are placed on a GPU, the higher the performance that will be
obtained. Performance also increases with electron count, up until the
memory on the GPU is exhausted. A good strategy is to perform a short
series of VMC runs with walker count increasing in multiples of
two. For systems with 100s of electrons, typically 128--256 walkers per
GPU use a sufficient number of GPU threads to operate the GPU
efficiently and to hide memory-access latency. For smaller systems,
thousands of walkers might be required. For QMC algorithms where the number of
walkers is fixed such as VMC, choosing a walker count the is a multiple of the
number of streaming multiprocessors can be most efficient. For
variable population DMC runs, this exact match is not possible.

Memory considerations
~~~~~~~~~~~~~~~~~~~~~

In the GPU implementation, each walker has a buffer in the GPU's
global memory to store temporary data associated with the
wavefunctions. Therefore, the amount of memory available on a GPU
limits the number of walkers and eventually the system size that it
can process. Additionally, for calculations using B-splines, this data
is stored on the GPU in a shared read-only buffer. Often the size of the
B-spline data limits the calculations that can be run on the GPU.

If the GPU memory is exhausted, first try reducing the number of walkers per GPU.
Coarsening the grids of the B-splines representation (by decreasing
the value of the mesh factor in the input file) can also lower the memory
usage, at the expense (risk) of obtaining less accurate results. Proceed
with caution if this option has to be considered.

.. bibliography:: /bibs/running.bib
