// ECE 462/562 - BaselineConfig plus the clamp8 custom instruction
//
// Requires course/patches/clamp8/rocket-chip.patch to be applied first: it
// adds the useClamp8 core parameter used below. That is why this file lives
// here and not in course/configs/, which is linked into every build.
//
// Build from chipyard/sims/verilator:
//   make CONFIG=Clamp8Config
//
package chipyard

import org.chipsalliance.cde.config._
import freechips.rocketchip.rocket._

/** Turns on clamp8 in every Rocket core of the configuration it is stacked on. */
class WithClamp8 extends RocketCoreConfig(_.copy(useClamp8 = true))

class Clamp8Config extends Config(
  new WithClamp8 ++
  new BaselineConfig)
