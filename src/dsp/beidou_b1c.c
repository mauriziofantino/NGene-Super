/** @file beidou_b1c.c @brief Truncated Weil codes and B1C primary modulation. */
#include "ngene/dsp/beidou_b1c.h"
#include "ngene/channel.h"

#define WEIL_LENGTH 10243u

typedef struct {
    uint16_t shift;
    uint16_t start; /**< One-based truncation point. */
} PrimaryParameters;

/* BDS-SIS-ICD-B1C-1.0, Tables 5-2 and 5-3, in ascending PRN order. */
static const PrimaryParameters dataParameters[] = {
    {2678, 699},  {4802, 694},  {958, 7318},  {859, 2127},   {3843, 715},  {2232, 6682},
    {124, 7850},  {4352, 5495}, {1816, 1162}, {1126, 7682},  {1860, 6792}, {4800, 9973},
    {2267, 6596}, {424, 2092},  {4192, 19},   {4333, 10151}, {2656, 6297}, {4148, 5766},
    {243, 2359},  {1330, 7136}, {1593, 1706}, {1470, 2128},  {882, 6827},  {3202, 693},
    {5095, 9729}, {2546, 1620}, {1733, 6805}, {4795, 534},   {4577, 712},  {1627, 1929},
    {3638, 5355}, {2553, 6139}, {3646, 6339}, {1087, 1470},  {1843, 6867}, {216, 7851},
    {2245, 1162}, {726, 7659},  {1966, 1156}, {670, 2672},   {4130, 6043}, {53, 2862},
    {4830, 180},  {182, 2663},  {2181, 6940}, {2006, 1645},  {1080, 1582}, {2288, 951},
    {2027, 6878}, {271, 7701},  {915, 1823},  {497, 2391},   {139, 2606},  {3693, 822},
    {2054, 6403}, {4342, 239},  {3342, 442},  {2592, 6769},  {1007, 2560}, {310, 2502},
    {4203, 5072}, {455, 7268},  {4318, 341}};
static const PrimaryParameters pilotParameters[] = {
    {796, 7575},   {156, 2369},  {4198, 5688}, {3941, 539},  {1374, 2270},  {1338, 7306},
    {1833, 6457},  {2521, 6254}, {3175, 5644}, {168, 7119},  {2715, 1402},  {4408, 5557},
    {3160, 5764},  {2796, 1073}, {459, 7001},  {3594, 5910}, {4813, 10060}, {586, 2710},
    {1428, 1546},  {2371, 6887}, {2285, 1883}, {3377, 5613}, {4965, 5062},  {3779, 1038},
    {4547, 10170}, {1646, 6484}, {1430, 1718}, {607, 2535},  {2118, 1158},  {4709, 526},
    {1149, 7331},  {3283, 5844}, {2473, 6423}, {1006, 6968}, {3670, 1280},  {1817, 1838},
    {771, 1989},   {2173, 6468}, {740, 2091},  {1433, 1581}, {2458, 1453},  {3459, 6252},
    {2155, 7122},  {1205, 7711}, {413, 7216},  {874, 2113},  {2463, 1095},  {1106, 1628},
    {1590, 1713},  {3873, 6102}, {4026, 6123}, {4272, 6070}, {3556, 1115},  {128, 8047},
    {1200, 6795},  {130, 2575},  {4494, 53},   {1871, 1729}, {3073, 6388},  {4386, 682},
    {4098, 5565},  {1923, 7160}, {1176, 2277}};
_Static_assert(sizeof(dataParameters) / sizeof(dataParameters[0]) == NG_SATELLITE_MAX_PRN,
               "B1C data table must cover all assigned PRNs");
_Static_assert(sizeof(pilotParameters) / sizeof(pilotParameters[0]) == NG_SATELLITE_MAX_PRN,
               "B1C pilot table must cover all assigned PRNs");

int NgBeidouB1cGenerate(unsigned prn, TNgBeidouB1cComponent component,
                        int8_t chips[NG_BEIDOU_B1C_LENGTH], TNgError *error)
{
    uint8_t legendre[WEIL_LENGTH] = {0};
    const PrimaryParameters *parameters;
    if (!chips || !NgSatellitePrnValid(NG_CONSTELLATION_BEIDOU, prn))
        return NgFail(error, "B1C primary requires a buffer and PRN in 1..63");
    if (component != NG_BEIDOU_B1C_DATA && component != NG_BEIDOU_B1C_PILOT)
        return NgFail(error, "Invalid B1C primary component");

    /* Each nonzero square has two roots, x and N-x. The zero entry stays zero. */
    for (uint32_t x = 1; x <= WEIL_LENGTH / 2; ++x)
        legendre[x * x % WEIL_LENGTH] = 1;

    parameters = component == NG_BEIDOU_B1C_DATA ? dataParameters : pilotParameters;
    const unsigned start = parameters[prn - 1].start - 1u;
    const unsigned shift = parameters[prn - 1].shift;
    for (unsigned n = 0; n < NG_BEIDOU_B1C_LENGTH; ++n) {
        unsigned k = (start + n) % WEIL_LENGTH;
        chips[n] = legendre[k] == legendre[(k + shift) % WEIL_LENGTH] ? 1 : -1;
    }
    return NG_OK;
}

static int8_t ChipAt(const int8_t chips[NG_BEIDOU_B1C_LENGTH], uint64_t phase)
{
    return chips[(phase / NG_BEIDOU_B1C_SUBCHIPS) % NG_BEIDOU_B1C_LENGTH];
}

static float Boc1(uint64_t phase)
{
    return phase % NG_BEIDOU_B1C_SUBCHIPS < NG_BEIDOU_B1C_SUBCHIPS / 2 ? 1.0f : -1.0f;
}

float NgBeidouB1cDataAt(const int8_t chips[NG_BEIDOU_B1C_LENGTH], uint64_t phase)
{
    return 0.5f * ChipAt(chips, phase) * Boc1(phase);
}

TNgComplex NgBeidouB1cPilotAt(const int8_t chips[NG_BEIDOU_B1C_LENGTH], uint64_t phase)
{
    /* Eq. 4-11 in the joint data/pilot I/Q frame: BOC(6,1) in I and BOC(1,1) in Q. */
    const float boc6 = phase % 2 == 0 ? 1.0f : -1.0f;
    const float chip = ChipAt(chips, phase);
    TNgComplex result = {
        0.301511345f * chip * boc6,       /* sqrt(1/11) */
        0.811844141f * chip * Boc1(phase) /* sqrt(29/44) */
    };
    return result;
}
