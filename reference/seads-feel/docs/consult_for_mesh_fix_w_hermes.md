Yes, absolutely. Pairing Hermes with Claude Code is one of the most effective ways to stop burning through credits on trial-and-error 3D mesh debugging.

When you carve tunnels into a 3D sphere planet, naive mesh displacement or basic mesh booleans almost always produce non-manifold geometry, coplanar face flickering, inverted normals, or invisible "ghost collisions" caused by floating zero-thickness triangles. Trying to fix this by asking an LLM to guess-and-check C++ code while you fly-test manually will eat through credits fast.

Combining Hermes and Claude Code establishes an automated, deterministic pipeline that makes the tunnel mesh firm and predictable.

Why the Mesh is Breaking (The Geometry Issue)
When a tunnel enters a curved sphere surface, standard polygon clipping usually leaves:

Inverted Normals: The physics engine thinks the inside of the tunnel wall is the "outside," pushing your ship into invisible geometry.
Double-Sided T-Junctions: Where the sphere's outer skin meets the tunnel's inner skin, small gaps or overlapping faces confuse bounding volume hierarchies (BVH trees).
Ghost Collisions: Non-manifold edges where two triangles share an edge, but three or more triangles meet at the same point.
The Two-Agent Architecture: Hermes + Claude Code
Hermes ships with a built-in claude-code skill that allows it to delegate coding tasks directly to Claude Code via terminal executions (claude -p "...") or an MCP bridge. You can run Claude Code on your subscription while Hermes acts as the persistent orchestrator and background tester.

 ┌─────────────────────────────────────────────────────────┐
 │                   HERMES AGENT                          │
 │  • Maintains persistent engine memory & SKILL.md       │
 │  • Runs background C++ mesh validation scripts         │
 │  • Catches ghost collisions & non-manifold vertices    │
 └────────────────────────────┬────────────────────────────┘
                              │
                    Spawns / Delegates via
                    claude-code skill
                              │
 ┌────────────────────────────▼────────────────────────────┐
 │                   CLAUDE CODE                           │
 │  • Running locally on your subscription                 │
 │  • Writes C++/OpenGL/GLSL patches                        │
 │  • Compiles build targets & updates math pipeline       │
 └─────────────────────────────────────────────────────────┘
How to Fix the Tunnels Without Wasting Credits
Instead of fly-testing every code tweak, use Hermes and Claude Code to build a 3-step automated solution:

1. Stop Fly-Testing: Build a Headless Mesh Auditor
Have Claude Code write a small, headless C++ script (./audit_tunnel_mesh) that loads your sphere + tunnel geometry directly from memory and tests it mathematically:

Runs raycasts from inside the central chamber outwards through all 4 tunnel mouths.
Checks for non-manifold edges, zero-area triangles, and backface normal alignment.
Outputs a clean JSON report of exact vertex coordinates where collision geometry breaks.
2. Let Hermes Run the Test-Driven Repair Loop
Instead of you flying through the tunnel to check for invisible walls:

Hermes runs ./audit_tunnel_mesh.
Hermes parses the error report, identifies where the surface-tunnel seam is failing, and feeds a structured prompt into Claude Code (claude -p "Fix seam normal calculation at chamber exit 2").
Claude Code edits the C++ file and recompiles.
Hermes re-runs the auditor to confirm the invisible collision is 100% gone before you ever launch the game.
3. Refactor to Signed Distance Fields (SDF) or Dual Contouring
If your current mesh boolean math is too brittle, have Hermes and Claude Code refactor the subterranean mesh generation to use Signed Distance Fields (SDFs):

Define the sphere as SDF_{\text{sphere}}(p) = \Vert{}p\Vert{} - r.
Define the central dogfight chamber as a hollow sphere SDF.
Define the 4 tunnel entrances as cylinder/capsule SDFs.
Combine them using smooth minimum operations (smin), then run Dual Contouring or Marching Cubes.
Why SDFs fix this forever: SDF math is mathematically continuous. It is impossible to generate non-manifold edges, ghost walls, or inverted normals when extracting meshes from SDFs.

Practical Setup
In Hermes: Verify the claude-code skill is enabled so Hermes can delegate tasks.
In your project root: Create a .claude/commands/mesh-test.md or a Hermes SKILL.md that explicitly defines your OpenGL mesh data structures and physics engine collision format.
Command Hermes:
"Hermes, write a headless C++ raycast audit tool for our Scare Skies tunnel generator. Once written, delegate to Claude Code to run the test, catch all coplanar vertex bugs, and refactor the tunnel-sphere join logic until the audit returns 0 collision errors."

This setup transforms your AI from a guess-and-check assistant into an automated QA engineer that fixes geometry offline while saving your subscription credits.