#include "render/body_chain.h"

#include <cmath>

namespace render {

// MEASURED by assets/character/sudburian_src/measure_body_chain.py
// off assets/sled/indy650.glb -- 44 skin joints, 34878 drawn vertices.
// Seat pan at z=0 is 0.218000 m thick; the longest link is 0.209254 m.
const std::array<BodyChainStation, kBodyChainStations>& body_chain_stations() {
    static const std::array<BodyChainStation, kBodyChainStations> t = {{
        // grip
        {{0.000000f, 0.786695f, 0.269000f},
         {{{0.351797f, -0.004814f, 0.033894f, 0.098473f, 0.052912f, 0.089786f},
           {-0.325873f, -0.022647f, 0.014076f, 0.124728f, 0.052912f,
            0.066016f}}}},
        // hand
        {{0.010954f, 0.833937f, 0.174941f},
         {{{0.340917f, -0.041029f, -0.003850f, 0.095404f, 0.052912f, 0.068187f},
           {-0.348992f, 0.009980f, 0.002548f, 0.071946f, 0.018458f,
            0.066623f}}}},
        // forearm_2
        {{0.009027f, 0.862539f, 0.130021f},
         {{{0.371235f, 0.013061f, -0.000085f, 0.081451f, 0.039874f, 0.082532f},
           {-0.366115f, 0.007291f, 0.008641f, 0.080440f, 0.045401f,
            0.074346f}}}},
        // forearm_1
        {{0.005172f, 0.919742f, 0.040182f},
         {{{0.386480f, 0.004872f, 0.001569f, 0.094958f, 0.053287f, 0.092773f},
           {-0.380650f, 0.000648f, 0.012868f, 0.093738f, 0.053106f,
            0.089410f}}}},
        // elbow
        {{0.001317f, 0.976945f, -0.049658f},
         {{{0.367334f, 0.012348f, -0.002093f, 0.132835f, 0.058733f, 0.097441f},
           {-0.373111f, 0.001343f, 0.012744f, 0.138079f, 0.069937f,
            0.101483f}}}},
        // upperarm
        {{0.000658f, 1.048785f, -0.173264f},
         {{{0.306850f, -0.002838f, -0.004352f, 0.193881f, 0.071484f, 0.117514f},
           {-0.306666f, -0.002499f, 0.007673f, 0.192457f, 0.071484f,
            0.119788f}}}},
        // shoulder
        {{0.000000f, 1.120626f, -0.296870f},
         {{{0.129285f, -0.004729f, 0.006681f, 0.246034f, 0.073000f, 0.286087f},
           {-0.131215f, -0.004868f, -0.001537f, 0.246430f, 0.073000f,
            0.295695f}}}},
        // spine_03
        {{0.000000f, 1.019206f, -0.401894f},
         {{{0.146127f, -0.001322f, 0.013208f, 0.145683f, 0.073000f, 0.206779f},
           {-0.150870f, 0.000736f, -0.005712f, 0.149248f, 0.073000f,
            0.209463f}}}},
        // spine_02
        {{0.000000f, 0.905167f, -0.493060f},
         {{{0.140806f, -0.002360f, 0.021644f, 0.140346f, 0.073000f, 0.225435f},
           {-0.143601f, -0.000965f, 0.014433f, 0.141873f, 0.073000f,
            0.221893f}}}},
        // spine_01
        {{0.000000f, 0.775782f, -0.560701f},
         {{{0.128307f, 0.000868f, -0.000493f, 0.127828f, 0.073000f, 0.218443f},
           {-0.128362f, 0.001166f, -0.005752f, 0.127857f, 0.073000f,
            0.224298f}}}},
        // pelvis
        {{-0.000000f, 0.683900f, -0.584840f},
         {{{0.121083f, -0.009092f, 0.020616f, 0.147710f, 0.104627f, 0.161880f},
           {-0.115963f, -0.009007f, 0.020050f, 0.153748f, 0.104627f,
            0.161571f}}}},
        // thigh
        {{-0.000013f, 0.690311f, -0.375684f},
         {{{0.193301f, 0.002420f, 0.000874f, 0.192068f, 0.104627f, 0.128876f},
           {-0.193326f, 0.001761f, 0.000586f, 0.193263f, 0.104627f,
            0.129015f}}}},
        // knee
        {{-0.000026f, 0.696722f, -0.166529f},
         {{{0.259174f, -0.003740f, 0.020304f, 0.154184f, 0.104627f, 0.132201f},
           {-0.260107f, -0.003760f, 0.020089f, 0.154072f, 0.104627f,
            0.132234f}}}},
        // shin_1
        {{-0.000039f, 0.574751f, -0.077005f},
         {{{0.281360f, -0.006256f, 0.007317f, 0.117640f, 0.075650f, 0.115438f},
           {-0.282260f, -0.006273f, 0.007039f, 0.117665f, 0.075650f,
            0.115427f}}}},
        // shin_2
        {{-0.000052f, 0.452780f, 0.012519f},
         {{{0.295006f, 0.005656f, 0.002234f, 0.109942f, 0.075650f, 0.097819f},
           {-0.295904f, 0.005663f, 0.001854f, 0.109964f, 0.075650f,
            0.097744f}}}},
        // ankle
        {{-0.000065f, 0.330809f, 0.102044f},
         {{{0.302565f, -0.010711f, 0.003691f, 0.099080f, 0.075650f, 0.143073f},
           {-0.303472f, -0.010845f, 0.003337f, 0.099125f, 0.075650f,
            0.143190f}}}},
        // toe
        {{-0.000077f, 0.288351f, 0.232630f},
         {{{0.284358f, 0.072195f, -0.044345f, 0.098897f, 0.068658f, 0.096239f},
           {-0.285233f, 0.072393f, -0.044882f, 0.098890f, 0.068658f,
            0.096075f}}}},
    }};
    return t;
}

float body_chain_link_len(int i) {
    const std::array<BodyChainStation, kBodyChainStations>& t =
        body_chain_stations();
    if (i < 1 || i > kBodyChainSegments) return 0.0f;
    const glm::vec3 d = t[static_cast<std::size_t>(i)].rest_model -
                        t[static_cast<std::size_t>(i - 1)].rest_model;
    return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
}

float body_chain_total_len() {
    float s = 0.0f;
    for (int i = 1; i <= kBodyChainSegments; ++i) s += body_chain_link_len(i);
    return s;
}

void body_chain_fill(TrailChainParams& pr) {
    const std::array<BodyChainStation, kBodyChainStations>& t =
        body_chain_stations();
    pr.segments = kBodyChainSegments;
    // The lengths are DERIVED from the station positions, never a second
    // typed table: one source of truth, so a re-measure cannot leave the
    // lengths describing a body the positions no longer describe. (That is the
    // law this program has now paid for seven times -- a constant that
    // describes the shipped table stops describing it the moment the table
    // moves.)
    pr.n_seg_len = kBodyChainSegments;
    for (int i = 1; i <= kBodyChainSegments; ++i)
        pr.seg_len_tbl[i - 1] = body_chain_link_len(i);
    pr.seg_len_m = pr.seg_len_tbl[0];  // the scalar a table-blind caller sees
    pr.n_probe_stations = kBodyChainStations;
    for (int i = 0; i < kBodyChainStations; ++i) {
        for (int s = 0; s < 2; ++s) {
            const BodyChainProbe& b = t[static_cast<std::size_t>(i)]
                                          .probe[static_cast<std::size_t>(s)];
            pr.probe[i][s].u_m = b.u_m;
            pr.probe[i][s].v_m = b.v_m;
            pr.probe[i][s].w_m = b.w_m;
            pr.probe[i][s].hx_m = b.hx_m;
            pr.probe[i][s].hy_m = b.hy_m;
            pr.probe[i][s].hz_m = b.hz_m;
        }
    }
    // ★★★ WHICH STATIONS CARRY TWO LIMBS -- and why the flag is OFF.
    //
    // Chad ruled per-limb grading for the leg stations on 2026-08-28, and the
    // mechanism is BUILT and GATED (TrailChainParams::probe_per_limb,
    // station_push, test_body_drive case 4). It is not switched on here, and
    // that is a fact he is owed rather than a decision taken quietly.
    //
    // He ruled it to kill a 0.4 m residual in a DIFFERENT fix -- one that
    // legalized the spring's target by PROJECTING the seated pose out of the
    // seat. That fix was then measured (pelvis 115 mm, worst leg 73 mm) and
    // abandoned for the pose ALLOWANCE, which respects the seated pose exactly
    // and leaves no residual for per-limb grading to correct. The ruling
    // answered a question the design no longer asks.
    //
    // And switching it on now COSTS, measured on case 5 of test_body_chain:
    // the drawn limb's sink into the machine goes from 4.4 mm to 120 mm. Both
    // limbs deep in the solid each prefer their own lateral exit, those cancel,
    // and the bone gets no push at all -- so the chain's keep-out stops being
    // what holds his legs out of the seat. Against his own words for this
    // mechanism ("what must not enter the machine is the DRAWN man"), 120 mm
    // is not a trade to take on my own authority.
    //
    // The straddle problem itself is real and it IS handled -- in
    // render/body_blend.cpp, where the two limbs finally exist as separate
    // geometry and each is pushed out of the solid on its own. That is the half
    // of his ruling a midline particle could never deliver.
    //
    // ⚠ OPEN, AWAITING HIS RULING. Flip this one literal to re-arm it.
    const int first_leg = body_chain_station_of("thigh");
    const int last_leg = body_chain_station_of("toe");
    // Named lookups, and they are CHECKED -- a silent -1 would leave the
    // question unanswerable rather than answered.
    if (first_leg >= 0 && last_leg >= first_leg &&
        last_leg < kBodyChainStations)
        for (int i = first_leg; i <= last_leg; ++i)
            pr.probe_per_limb[i] = false;
}

// ★★★ THE PER-SIDE JOINT OFFSETS. See render/body_chain.h for why a probe
// centre is the wrong quantity and this is the right one. MEASURED by
// measure_body_chain.py's "THE PER-SIDE JOINT OFFSETS" block; pasted, never
// retyped. The near-zero v/w components are the bind pose being symmetric
// about its own midline, which is exactly what a midline chain should see --
// they are kept rather than rounded away so a re-measure of an ASYMMETRIC
// stance cannot silently look identical.
const std::array<BodyChainLegJoint, kBodyChainLegTargets>&
body_chain_leg_joints() {
    static const std::array<BodyChainLegJoint, kBodyChainLegTargets> j = {{
        // thigh
        {11,
         {{{0.181689f, -0.000006f, 0.000375f},
           {-0.181689f, 0.000006f, -0.000375f}}}},
        // knee
        {12,
         {{{0.268379f, -0.000022f, 0.000318f},
           {-0.268379f, 0.000022f, -0.000318f}}}},
        // ankle
        {15,
         {{{0.300026f, -0.000028f, 0.000292f},
           {-0.300026f, 0.000028f, -0.000292f}}}},
        // toe
        {16,
         {{{0.294240f, -0.000028f, 0.000293f},
           {-0.294240f, 0.000028f, -0.000293f}}}},
    }};
    return j;
}

// The station names, in table order. ONE list, so a lookup and the table can
// never disagree about which index is the knee.
int body_chain_station_of(const char* name) {
    static const char* const kNames[kBodyChainStations] = {
        "grip",     "hand",     "forearm_2", "forearm_1", "elbow",
        "upperarm", "shoulder", "spine_03",  "spine_02",  "spine_01",
        "pelvis",   "thigh",    "knee",      "shin_1",    "shin_2",
        "ankle",    "toe"};
    if (name == nullptr) return -1;
    for (int i = 0; i < kBodyChainStations; ++i) {
        const char* a = kNames[i];
        const char* b = name;
        while (*a != '\0' && *a == *b) {
            ++a;
            ++b;
        }
        if (*a == '\0' && *b == '\0') return i;
    }
    return -1;
}

}  // namespace render
