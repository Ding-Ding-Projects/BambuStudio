// Material Design 3 dynamic colour. See MD3DynamicColor.hpp for scope.
//
// Ported from material-color-utilities 0.4.0 (TypeScript), Copyright 2021-2025
// Google LLC, licensed under the Apache License, Version 2.0. The algorithms,
// matrices and the critical-plane table are carried over unchanged; JavaScript
// rounding (Math.round, which rounds half toward positive infinity) and its
// floating-point remainder are reproduced exactly so that results match the
// reference library byte for byte.

#include "MD3DynamicColor.hpp"

#include <algorithm>
#include <cmath>

namespace MD3 { namespace Dynamic {

namespace {

constexpr double kPi = 3.14159265358979323846;

// --- math_utils ------------------------------------------------------------

double signum(double v) { return v < 0.0 ? -1.0 : (v == 0.0 ? 0.0 : 1.0); }

double lerp(double start, double stop, double amount) { return (1.0 - amount) * start + amount * stop; }

// Math.round: half rounds toward positive infinity.
double js_round(double v) { return std::floor(v + 0.5); }

int clamp_int(int lo, int hi, int v) { return v < lo ? lo : (v > hi ? hi : v); }

double sanitize_degrees(double degrees)
{
    degrees = std::fmod(degrees, 360.0);
    if (degrees < 0.0)
        degrees += 360.0;
    return degrees;
}

struct Vec3 { double v[3]; double operator[](int i) const { return v[i]; } double &operator[](int i) { return v[i]; } };
using Mat3 = double[3][3];

Vec3 matrix_multiply(const Vec3 &row, const Mat3 &m)
{
    return { { row[0] * m[0][0] + row[1] * m[0][1] + row[2] * m[0][2],
               row[0] * m[1][0] + row[1] * m[1][1] + row[2] * m[1][2],
               row[0] * m[2][0] + row[1] * m[2][1] + row[2] * m[2][2] } };
}

// --- color_utils -----------------------------------------------------------

const Mat3 kSrgbToXyz = {
    { 0.41233895, 0.35762064, 0.18051042 },
    { 0.2126, 0.7152, 0.0722 },
    { 0.01932141, 0.11916382, 0.95034478 },
};

const double kWhitePointD65[3] = { 95.047, 100.0, 108.883 };

double lab_f(double t)
{
    const double e     = 216.0 / 24389.0;
    const double kappa = 24389.0 / 27.0;
    return t > e ? std::pow(t, 1.0 / 3.0) : (kappa * t + 16.0) / 116.0;
}

double lab_invf(double ft)
{
    const double e     = 216.0 / 24389.0;
    const double kappa = 24389.0 / 27.0;
    const double ft3   = ft * ft * ft;
    return ft3 > e ? ft3 : (116.0 * ft - 16.0) / kappa;
}

double linearized(int component)
{
    const double normalized = component / 255.0;
    if (normalized <= 0.040449936)
        return normalized / 12.92 * 100.0;
    return std::pow((normalized + 0.055) / 1.055, 2.4) * 100.0;
}

int delinearized(double component)
{
    const double normalized = component / 100.0;
    double d = 0.0;
    if (normalized <= 0.0031308)
        d = normalized * 12.92;
    else
        d = 1.055 * std::pow(normalized, 1.0 / 2.4) - 0.055;
    return clamp_int(0, 255, int(js_round(d * 255.0)));
}

Argb argb_from_linrgb(const Vec3 &linrgb)
{
    return argb_from_rgb(delinearized(linrgb[0]), delinearized(linrgb[1]), delinearized(linrgb[2]));
}

double y_from_lstar(double lstar) { return 100.0 * lab_invf((lstar + 16.0) / 116.0); }

Argb argb_from_lstar(double lstar)
{
    const int c = delinearized(y_from_lstar(lstar));
    return argb_from_rgb(c, c, c);
}

// --- viewing conditions (sRGB default) -------------------------------------

struct ViewingConditions
{
    double n, aw, nbb, ncb, c, nc, fl, fl_root, z;
    double rgb_d[3];
};

ViewingConditions make_default_viewing_conditions()
{
    const double *xyz                = kWhitePointD65;
    const double adapting_luminance  = (200.0 / kPi) * y_from_lstar(50.0) / 100.0;
    const double background_lstar    = 50.0;
    const double surround            = 2.0;

    const double r_w = xyz[0] * 0.401288 + xyz[1] * 0.650173 + xyz[2] * -0.051461;
    const double g_w = xyz[0] * -0.250268 + xyz[1] * 1.204414 + xyz[2] * 0.045854;
    const double b_w = xyz[0] * -0.002079 + xyz[1] * 0.048952 + xyz[2] * 0.953127;
    const double f   = 0.8 + surround / 10.0;
    const double c   = f >= 0.9 ? lerp(0.59, 0.69, (f - 0.9) * 10.0) : lerp(0.525, 0.59, (f - 0.8) * 10.0);
    double d = f * (1.0 - (1.0 / 3.6) * std::exp((-adapting_luminance - 42.0) / 92.0));
    d = d > 1.0 ? 1.0 : (d < 0.0 ? 0.0 : d);
    const double nc = f;
    const double rgb_d[3] = { d * (100.0 / r_w) + 1.0 - d, d * (100.0 / g_w) + 1.0 - d, d * (100.0 / b_w) + 1.0 - d };
    const double k    = 1.0 / (5.0 * adapting_luminance + 1.0);
    const double k4   = k * k * k * k;
    const double k4f  = 1.0 - k4;
    const double fl   = k4 * adapting_luminance + 0.1 * k4f * k4f * std::cbrt(5.0 * adapting_luminance);
    const double n    = y_from_lstar(background_lstar) / xyz[1];
    const double z    = 1.48 + std::sqrt(n);
    const double nbb  = 0.725 / std::pow(n, 0.2);
    const double ncb  = nbb;
    const double f0   = std::pow((fl * rgb_d[0] * r_w) / 100.0, 0.42);
    const double f1   = std::pow((fl * rgb_d[1] * g_w) / 100.0, 0.42);
    const double f2   = std::pow((fl * rgb_d[2] * b_w) / 100.0, 0.42);
    const double ra0  = (400.0 * f0) / (f0 + 27.13);
    const double ra1  = (400.0 * f1) / (f1 + 27.13);
    const double ra2  = (400.0 * f2) / (f2 + 27.13);
    const double aw   = (2.0 * ra0 + ra1 + 0.05 * ra2) * nbb;

    ViewingConditions vc {};
    vc.n = n; vc.aw = aw; vc.nbb = nbb; vc.ncb = ncb; vc.c = c; vc.nc = nc;
    vc.fl = fl; vc.fl_root = std::pow(fl, 0.25); vc.z = z;
    vc.rgb_d[0] = rgb_d[0]; vc.rgb_d[1] = rgb_d[1]; vc.rgb_d[2] = rgb_d[2];
    return vc;
}

const ViewingConditions &default_vc()
{
    static const ViewingConditions vc = make_default_viewing_conditions();
    return vc;
}

// --- HCT solver --------------------------------------------------------------

const Mat3 kScaledDiscountFromLinrgb = {
    { 0.001200833568784504, 0.002389694492170889, 0.0002795742885861124 },
    { 0.0005891086651375999, 0.0029785502573438758, 0.0003270666104008398 },
    { 0.00010146692491640572, 0.0005364214359186694, 0.0032979401770712076 },
};

const Mat3 kLinrgbFromScaledDiscount = {
    { 1373.2198709594231, -1100.4251190754821, -7.278681089101213 },
    { -271.815969077903, 559.6580465940733, -32.46047482791194 },
    { 1.9622899599665666, -57.173814538844006, 308.7233197812385 },
};

const double kYFromLinrgb[3] = { 0.2126, 0.7152, 0.0722 };

// clang-format off
const double kCriticalPlanes[255] = {
    0.015176349177441876, 0.045529047532325624, 0.07588174588720938,
    0.10623444424209313, 0.13658714259697685, 0.16693984095186062,
    0.19729253930674434, 0.2276452376616281, 0.2579979360165119,
    0.28835063437139563, 0.3188300904430532, 0.350925934958123,
    0.3848314933096426, 0.42057480301049466, 0.458183274052838,
    0.4976837250274023, 0.5391024159806381, 0.5824650784040898,
    0.6277969426914107, 0.6751227633498623, 0.7244668422128921,
    0.775853049866786, 0.829304845476233, 0.8848452951698498,
    0.942497089126609, 1.0022825574869039, 1.0642236851973577,
    1.1283421258858297, 1.1946592148522128, 1.2631959812511864,
    1.3339731595349034, 1.407011200216447, 1.4823302800086415,
    1.5599503113873272, 1.6398909516233677, 1.7221716113234105,
    1.8068114625156377, 1.8938294463134073, 1.9832442801866852,
    2.075074464868551, 2.1693382909216234, 2.2660538449872063,
    2.36523901573795, 2.4669114995532007, 2.5710888059345764,
    2.6777882626779785, 2.7870270208169257, 2.898822059350997,
    3.0131901897720907, 3.1301480604002863, 3.2497121605402226,
    3.3718988244681087, 3.4967242352587946, 3.624204428461639,
    3.754355295633311, 3.887192587735158, 4.022731918402185,
    4.160988767090289, 4.301978482107941, 4.445716283538092,
    4.592217266055746, 4.741496401646282, 4.893568542229298,
    5.048448422192488, 5.20615066083972, 5.3666897647573375,
    5.5300801301023865, 5.696336044816294, 5.865471690767354,
    6.037501145825082, 6.212438385869475, 6.390297286737924,
    6.571091626112461, 6.7548350853498045, 6.941541251256611,
    7.131223617812143, 7.323895587840543, 7.5195704746346665,
    7.7182615035334345, 7.919981813454504, 8.124744458384042,
    8.332562408825165, 8.543448553206703, 8.757415699253682,
    8.974476575321063, 9.194643831691977, 9.417930041841839,
    9.644347703669503, 9.873909240696694, 10.106627003236781,
    10.342513269534024, 10.58158024687427, 10.8238400726681,
    11.069304815507364, 11.317986476196008, 11.569896988756009,
    11.825048221409341, 12.083451977536606, 12.345119996613247,
    12.610063955123938, 12.878295467455942, 13.149826086772048,
    13.42466730586372, 13.702830557985108, 13.984327217668513,
    14.269168601521828, 14.55736596900856, 14.848930523210871,
    15.143873411576273, 15.44220572664832, 15.743938506781891,
    16.04908273684337, 16.35764934889634, 16.66964922287304,
    16.985093187232053, 17.30399201960269, 17.62635644741625,
    17.95219714852476, 18.281524751807332, 18.614349837764564,
    18.95068293910138, 19.290534541298456, 19.633915083172692,
    19.98083495742689, 20.331304511189067, 20.685334046541502,
    21.042933821039977, 21.404114048223256, 21.76888489811322,
    22.137256497705877, 22.50923893145328, 22.884842241736916,
    23.264076429332462, 23.6469514538663, 24.033477234264016,
    24.42366364919083, 24.817520537484558, 25.21505769858089,
    25.61628489293138, 26.021211842414342, 26.429848230738664,
    26.842203703840827, 27.258287870275353, 27.678110301598522,
    28.10168053274597, 28.529008062403893, 28.96010235337422,
    29.39497283293396, 29.83362889318845, 30.276079891419332,
    30.722335150426627, 31.172403958865512, 31.62629557157785,
    32.08401920991837, 32.54558406207592, 33.010999283389665,
    33.4802739966603, 33.953417292456834, 34.430438229418264,
    34.911345834551085, 35.39614910352207, 35.88485700094671,
    36.37747846067349, 36.87402238606382, 37.37449765026789,
    37.87891309649659, 38.38727753828926, 38.89959975977785,
    39.41588851594697, 39.93615253289054, 40.460400508064545,
    40.98864111053629, 41.520882981230194, 42.05713473317016,
    42.597404951718396, 43.141702194811224, 43.6900349931913,
    44.24241185063697, 44.798841244188324, 45.35933162437017,
    45.92389141541209, 46.49252901546552, 47.065252796817916,
    47.64207110610409, 48.22299226451468, 48.808024568002054,
    49.3971762874833, 49.9904556690408, 50.587870934119984,
    51.189430279724725, 51.79514187861014, 52.40501387947288,
    53.0190544071392, 53.637271562750364, 54.259673423945976,
    54.88626804504493, 55.517063457223934, 56.15206766869424,
    56.79128866487574, 57.43473440856916, 58.08241284012621,
    58.734331877617365, 59.39049941699807, 60.05092333227251,
    60.715611475655585, 61.38457167773311, 62.057811747619894,
    62.7353394731159, 63.417162620860914, 64.10328893648692,
    64.79372614476921, 65.48848194977529, 66.18756403501224,
    66.89098006357258, 67.59873767827808, 68.31084450182222,
    69.02730813691093, 69.74813616640164, 70.47333615344107,
    71.20291564160104, 71.93688215501312, 72.67524319850172,
    73.41800625771542, 74.16517879925733, 74.9167682708136,
    75.67278210128072, 76.43322770089146, 77.1981124613393,
    77.96744375590167, 78.74122893956174, 79.51947534912904,
    80.30219030335869, 81.08938110306934, 81.88105503125999,
    82.67721935322541, 83.4778813166706, 84.28304815182372,
    85.09272707154808, 85.90692527145302, 86.72564993000343,
    87.54890820862819, 88.3767072518277, 89.2090541872801,
    90.04595612594655, 90.88742016217518, 91.73345337380438,
    92.58406282226491, 93.43925555268066, 94.29903859396902,
    95.16341895893969, 96.03240364439274, 96.9059996312159,
    97.78421388448044, 98.6670533535366, 99.55452497210776,
};
// clang-format on

double sanitize_radians(double angle) { return std::fmod(angle + kPi * 8.0, kPi * 2.0); }

double true_delinearized(double component)
{
    const double normalized = component / 100.0;
    double d = 0.0;
    if (normalized <= 0.0031308)
        d = normalized * 12.92;
    else
        d = 1.055 * std::pow(normalized, 1.0 / 2.4) - 0.055;
    return d * 255.0;
}

double chromatic_adaptation(double component)
{
    const double af = std::pow(std::fabs(component), 0.42);
    return signum(component) * 400.0 * af / (af + 27.13);
}

double hue_of(const Vec3 &linrgb)
{
    const Vec3   sd = matrix_multiply(linrgb, kScaledDiscountFromLinrgb);
    const double rA = chromatic_adaptation(sd[0]);
    const double gA = chromatic_adaptation(sd[1]);
    const double bA = chromatic_adaptation(sd[2]);
    const double a  = (11.0 * rA + -12.0 * gA + bA) / 11.0;
    const double b  = (rA + gA - 2.0 * bA) / 9.0;
    return std::atan2(b, a);
}

bool are_in_cyclic_order(double a, double b, double c)
{
    return sanitize_radians(b - a) < sanitize_radians(c - a);
}

double intercept(double source, double mid, double target) { return (mid - source) / (target - source); }

Vec3 lerp_point(const Vec3 &source, double t, const Vec3 &target)
{
    return { { source[0] + (target[0] - source[0]) * t,
               source[1] + (target[1] - source[1]) * t,
               source[2] + (target[2] - source[2]) * t } };
}

Vec3 set_coordinate(const Vec3 &source, double coordinate, const Vec3 &target, int axis)
{
    return lerp_point(source, intercept(source[axis], coordinate, target[axis]), target);
}

bool is_bounded(double x) { return 0.0 <= x && x <= 100.0; }

Vec3 nth_vertex(double y, int n)
{
    const double kR = kYFromLinrgb[0], kG = kYFromLinrgb[1], kB = kYFromLinrgb[2];
    const double coord_a = n % 4 <= 1 ? 0.0 : 100.0;
    const double coord_b = n % 2 == 0 ? 0.0 : 100.0;
    const Vec3   none { { -1.0, -1.0, -1.0 } };
    if (n < 4) {
        const double g = coord_a, b = coord_b;
        const double r = (y - g * kG - b * kB) / kR;
        return is_bounded(r) ? Vec3 { { r, g, b } } : none;
    }
    if (n < 8) {
        const double b = coord_a, r = coord_b;
        const double g = (y - r * kR - b * kB) / kG;
        return is_bounded(g) ? Vec3 { { r, g, b } } : none;
    }
    const double r = coord_a, g = coord_b;
    const double b = (y - r * kR - g * kG) / kB;
    return is_bounded(b) ? Vec3 { { r, g, b } } : none;
}

void bisect_to_segment(double y, double target_hue, Vec3 &left, Vec3 &right)
{
    left  = { { -1.0, -1.0, -1.0 } };
    right = left;
    double left_hue = 0.0, right_hue = 0.0;
    bool   initialized = false;
    bool   uncut       = true;
    for (int n = 0; n < 12; ++n) {
        const Vec3 mid = nth_vertex(y, n);
        if (mid[0] < 0)
            continue;
        const double mid_hue = hue_of(mid);
        if (!initialized) {
            left = mid; right = mid;
            left_hue = mid_hue; right_hue = mid_hue;
            initialized = true;
            continue;
        }
        if (uncut || are_in_cyclic_order(left_hue, mid_hue, right_hue)) {
            uncut = false;
            if (are_in_cyclic_order(left_hue, target_hue, mid_hue)) {
                right = mid; right_hue = mid_hue;
            } else {
                left = mid; left_hue = mid_hue;
            }
        }
    }
}

Vec3 midpoint(const Vec3 &a, const Vec3 &b)
{
    return { { (a[0] + b[0]) / 2.0, (a[1] + b[1]) / 2.0, (a[2] + b[2]) / 2.0 } };
}

int critical_plane_below(double x) { return int(std::floor(x - 0.5)); }
int critical_plane_above(double x) { return int(std::ceil(x - 0.5)); }

Vec3 bisect_to_limit(double y, double target_hue)
{
    Vec3 left, right;
    bisect_to_segment(y, target_hue, left, right);
    double left_hue = hue_of(left);
    for (int axis = 0; axis < 3; ++axis) {
        if (left[axis] == right[axis])
            continue;
        int l_plane = -1;
        int r_plane = 255;
        if (left[axis] < right[axis]) {
            l_plane = critical_plane_below(true_delinearized(left[axis]));
            r_plane = critical_plane_above(true_delinearized(right[axis]));
        } else {
            l_plane = critical_plane_above(true_delinearized(left[axis]));
            r_plane = critical_plane_below(true_delinearized(right[axis]));
        }
        for (int i = 0; i < 8; ++i) {
            if (std::abs(r_plane - l_plane) <= 1)
                break;
            const int    m_plane = int(std::floor((l_plane + r_plane) / 2.0));
            const double coord   = kCriticalPlanes[m_plane];
            const Vec3   mid     = set_coordinate(left, coord, right, axis);
            const double mid_hue = hue_of(mid);
            if (are_in_cyclic_order(left_hue, target_hue, mid_hue)) {
                right   = mid;
                r_plane = m_plane;
            } else {
                left     = mid;
                left_hue = mid_hue;
                l_plane  = m_plane;
            }
        }
    }
    return midpoint(left, right);
}

double inverse_chromatic_adaptation(double adapted)
{
    const double abs_adapted = std::fabs(adapted);
    const double base        = std::max(0.0, 27.13 * abs_adapted / (400.0 - abs_adapted));
    return signum(adapted) * std::pow(base, 1.0 / 0.42);
}

Argb find_result_by_j(double hue_radians, double chroma, double y)
{
    double j = std::sqrt(y) * 11.0;
    const ViewingConditions &vc = default_vc();
    const double t_inner_coeff = 1.0 / std::pow(1.64 - std::pow(0.29, vc.n), 0.73);
    const double e_hue         = 0.25 * (std::cos(hue_radians + 2.0) + 3.8);
    const double p1            = e_hue * (50000.0 / 13.0) * vc.nc * vc.ncb;
    const double h_sin         = std::sin(hue_radians);
    const double h_cos         = std::cos(hue_radians);
    for (int round = 0; round < 5; ++round) {
        const double j_normalized = j / 100.0;
        const double alpha = (chroma == 0.0 || j == 0.0) ? 0.0 : chroma / std::sqrt(j_normalized);
        const double t     = std::pow(alpha * t_inner_coeff, 1.0 / 0.9);
        const double ac    = vc.aw * std::pow(j_normalized, 1.0 / vc.c / vc.z);
        const double p2    = ac / vc.nbb;
        const double gamma = 23.0 * (p2 + 0.305) * t / (23.0 * p1 + 11.0 * t * h_cos + 108.0 * t * h_sin);
        const double a     = gamma * h_cos;
        const double b     = gamma * h_sin;
        const double rA    = (460.0 * p2 + 451.0 * a + 288.0 * b) / 1403.0;
        const double gA    = (460.0 * p2 - 891.0 * a - 261.0 * b) / 1403.0;
        const double bA    = (460.0 * p2 - 220.0 * a - 6300.0 * b) / 1403.0;
        const Vec3   scaled { { inverse_chromatic_adaptation(rA), inverse_chromatic_adaptation(gA),
                                inverse_chromatic_adaptation(bA) } };
        const Vec3   linrgb = matrix_multiply(scaled, kLinrgbFromScaledDiscount);
        if (linrgb[0] < 0 || linrgb[1] < 0 || linrgb[2] < 0)
            return 0;
        const double fnj = kYFromLinrgb[0] * linrgb[0] + kYFromLinrgb[1] * linrgb[1] + kYFromLinrgb[2] * linrgb[2];
        if (fnj <= 0)
            return 0;
        if (round == 4 || std::fabs(fnj - y) < 0.002) {
            if (linrgb[0] > 100.01 || linrgb[1] > 100.01 || linrgb[2] > 100.01)
                return 0;
            return argb_from_linrgb(linrgb);
        }
        j = j - (fnj - y) * j / (2.0 * fnj);
    }
    return 0;
}

} // namespace

// --- public API --------------------------------------------------------------

double lstar_from_argb(Argb argb)
{
    const double y = kSrgbToXyz[1][0] * linearized(red_of(argb)) + kSrgbToXyz[1][1] * linearized(green_of(argb)) +
                     kSrgbToXyz[1][2] * linearized(blue_of(argb));
    return 116.0 * lab_f(y / 100.0) - 16.0;
}

Argb argb_from_hct(double hue, double chroma, double lstar)
{
    if (chroma < 0.0001 || lstar < 0.0001 || lstar > 99.9999)
        return argb_from_lstar(lstar);
    hue = sanitize_degrees(hue);
    const double hue_radians = hue / 180.0 * kPi;
    const double y           = y_from_lstar(lstar);
    const Argb   exact       = find_result_by_j(hue_radians, chroma, y);
    if (exact != 0)
        return exact;
    return argb_from_linrgb(bisect_to_limit(y, hue_radians));
}

Hct hct_from_argb(Argb argb)
{
    const ViewingConditions &vc = default_vc();
    const double red_l   = linearized(red_of(argb));
    const double green_l = linearized(green_of(argb));
    const double blue_l  = linearized(blue_of(argb));
    const double x = 0.41233895 * red_l + 0.35762064 * green_l + 0.18051042 * blue_l;
    const double y = 0.2126 * red_l + 0.7152 * green_l + 0.0722 * blue_l;
    const double z = 0.01932141 * red_l + 0.11916382 * green_l + 0.95034478 * blue_l;

    const double r_c = 0.401288 * x + 0.650173 * y - 0.051461 * z;
    const double g_c = -0.250268 * x + 1.204414 * y + 0.045854 * z;
    const double b_c = -0.002079 * x + 0.048952 * y + 0.953127 * z;
    const double r_d = vc.rgb_d[0] * r_c;
    const double g_d = vc.rgb_d[1] * g_c;
    const double b_d = vc.rgb_d[2] * b_c;
    const double r_af = std::pow(vc.fl * std::fabs(r_d) / 100.0, 0.42);
    const double g_af = std::pow(vc.fl * std::fabs(g_d) / 100.0, 0.42);
    const double b_af = std::pow(vc.fl * std::fabs(b_d) / 100.0, 0.42);
    const double r_a = signum(r_d) * 400.0 * r_af / (r_af + 27.13);
    const double g_a = signum(g_d) * 400.0 * g_af / (g_af + 27.13);
    const double b_a = signum(b_d) * 400.0 * b_af / (b_af + 27.13);
    const double a  = (11.0 * r_a + -12.0 * g_a + b_a) / 11.0;
    const double b  = (r_a + g_a - 2.0 * b_a) / 9.0;
    const double u  = (20.0 * r_a + 20.0 * g_a + 21.0 * b_a) / 20.0;
    const double p2 = (40.0 * r_a + 20.0 * g_a + b_a) / 20.0;
    const double atan_degrees = std::atan2(b, a) * 180.0 / kPi;
    const double hue = atan_degrees < 0 ? atan_degrees + 360.0 : (atan_degrees >= 360 ? atan_degrees - 360 : atan_degrees);
    const double ac  = p2 * vc.nbb;
    const double J   = 100.0 * std::pow(ac / vc.aw, vc.c * vc.z);
    const double hue_prime = hue < 20.14 ? hue + 360 : hue;
    const double e_hue = 0.25 * (std::cos(hue_prime * kPi / 180.0 + 2.0) + 3.8);
    const double p1    = 50000.0 / 13.0 * e_hue * vc.nc * vc.ncb;
    const double t     = p1 * std::sqrt(a * a + b * b) / (u + 0.305);
    const double alpha = std::pow(t, 0.9) * std::pow(1.64 - std::pow(0.29, vc.n), 0.73);
    const double C     = alpha * std::sqrt(J / 100.0);

    return { hue, C, lstar_from_argb(argb) };
}

Argb TonalPalette::tone(double t) const
{
    // The reference palette lightens tone 99 of yellow hues to the average of
    // tones 98 and 100, which otherwise reads as an off-white.
    if (t == 99.0 && m_hue >= 105.0 && m_hue < 125.0) {
        const Argb a = tone(98.0), b = tone(100.0);
        return argb_from_rgb(int(js_round((red_of(a) + red_of(b)) / 2.0)),
                             int(js_round((green_of(a) + green_of(b)) / 2.0)),
                             int(js_round((blue_of(a) + blue_of(b)) / 2.0)));
    }
    return argb_from_hct(m_hue, m_chroma, t);
}

const char *role_name(SchemeRole role)
{
    switch (role) {
    case SchemeRole::Primary: return "primary";
    case SchemeRole::OnPrimary: return "onPrimary";
    case SchemeRole::PrimaryContainer: return "primaryContainer";
    case SchemeRole::OnPrimaryContainer: return "onPrimaryContainer";
    case SchemeRole::InversePrimary: return "inversePrimary";
    case SchemeRole::Secondary: return "secondary";
    case SchemeRole::OnSecondary: return "onSecondary";
    case SchemeRole::SecondaryContainer: return "secondaryContainer";
    case SchemeRole::OnSecondaryContainer: return "onSecondaryContainer";
    case SchemeRole::Tertiary: return "tertiary";
    case SchemeRole::OnTertiary: return "onTertiary";
    case SchemeRole::TertiaryContainer: return "tertiaryContainer";
    case SchemeRole::OnTertiaryContainer: return "onTertiaryContainer";
    case SchemeRole::Error: return "error";
    case SchemeRole::OnError: return "onError";
    case SchemeRole::ErrorContainer: return "errorContainer";
    case SchemeRole::OnErrorContainer: return "onErrorContainer";
    case SchemeRole::Background: return "background";
    case SchemeRole::OnBackground: return "onBackground";
    case SchemeRole::Surface: return "surface";
    case SchemeRole::SurfaceDim: return "surfaceDim";
    case SchemeRole::SurfaceBright: return "surfaceBright";
    case SchemeRole::SurfaceContainerLowest: return "surfaceContainerLowest";
    case SchemeRole::SurfaceContainerLow: return "surfaceContainerLow";
    case SchemeRole::SurfaceContainer: return "surfaceContainer";
    case SchemeRole::SurfaceContainerHigh: return "surfaceContainerHigh";
    case SchemeRole::SurfaceContainerHighest: return "surfaceContainerHighest";
    case SchemeRole::OnSurface: return "onSurface";
    case SchemeRole::SurfaceVariant: return "surfaceVariant";
    case SchemeRole::OnSurfaceVariant: return "onSurfaceVariant";
    case SchemeRole::InverseSurface: return "inverseSurface";
    case SchemeRole::InverseOnSurface: return "inverseOnSurface";
    case SchemeRole::Outline: return "outline";
    case SchemeRole::OutlineVariant: return "outlineVariant";
    case SchemeRole::Shadow: return "shadow";
    case SchemeRole::Scrim: return "scrim";
    case SchemeRole::SurfaceTint: return "surfaceTint";
    case SchemeRole::PrimaryFixed: return "primaryFixed";
    case SchemeRole::PrimaryFixedDim: return "primaryFixedDim";
    case SchemeRole::OnPrimaryFixed: return "onPrimaryFixed";
    case SchemeRole::OnPrimaryFixedVariant: return "onPrimaryFixedVariant";
    case SchemeRole::SecondaryFixed: return "secondaryFixed";
    case SchemeRole::SecondaryFixedDim: return "secondaryFixedDim";
    case SchemeRole::OnSecondaryFixed: return "onSecondaryFixed";
    case SchemeRole::OnSecondaryFixedVariant: return "onSecondaryFixedVariant";
    case SchemeRole::TertiaryFixed: return "tertiaryFixed";
    case SchemeRole::TertiaryFixedDim: return "tertiaryFixedDim";
    case SchemeRole::OnTertiaryFixed: return "onTertiaryFixed";
    case SchemeRole::OnTertiaryFixedVariant: return "onTertiaryFixedVariant";
    case SchemeRole::Count: break;
    }
    return "";
}

Palettes tonal_spot_palettes(Argb seed)
{
    const Hct source = hct_from_argb(seed);
    Palettes  p;
    p.primary         = TonalPalette(source.hue, 36.0);
    p.secondary       = TonalPalette(source.hue, 16.0);
    p.tertiary        = TonalPalette(sanitize_degrees(source.hue + 60.0), 24.0);
    p.neutral         = TonalPalette(source.hue, 6.0);
    p.neutral_variant = TonalPalette(source.hue, 8.0);
    p.error           = TonalPalette(25.0, 84.0);
    return p;
}

Scheme tonal_spot(Argb seed, bool dark)
{
    const Palettes p = tonal_spot_palettes(seed);
    Scheme s;
    s.seed = seed;
    s.dark = dark;
    auto set = [&s](SchemeRole r, Argb c) { s.colors[static_cast<std::size_t>(r)] = c; };
    // Light tone, dark tone.
    auto pick = [dark](double light, double dark_tone) { return dark ? dark_tone : light; };

    set(SchemeRole::Primary, p.primary.tone(pick(40, 80)));
    set(SchemeRole::OnPrimary, p.primary.tone(pick(100, 20)));
    set(SchemeRole::PrimaryContainer, p.primary.tone(pick(90, 30)));
    set(SchemeRole::OnPrimaryContainer, p.primary.tone(pick(30, 90)));
    set(SchemeRole::InversePrimary, p.primary.tone(pick(80, 40)));

    set(SchemeRole::Secondary, p.secondary.tone(pick(40, 80)));
    set(SchemeRole::OnSecondary, p.secondary.tone(pick(100, 20)));
    set(SchemeRole::SecondaryContainer, p.secondary.tone(pick(90, 30)));
    set(SchemeRole::OnSecondaryContainer, p.secondary.tone(pick(30, 90)));

    set(SchemeRole::Tertiary, p.tertiary.tone(pick(40, 80)));
    set(SchemeRole::OnTertiary, p.tertiary.tone(pick(100, 20)));
    set(SchemeRole::TertiaryContainer, p.tertiary.tone(pick(90, 30)));
    set(SchemeRole::OnTertiaryContainer, p.tertiary.tone(pick(30, 90)));

    set(SchemeRole::Error, p.error.tone(pick(40, 80)));
    set(SchemeRole::OnError, p.error.tone(pick(100, 20)));
    set(SchemeRole::ErrorContainer, p.error.tone(pick(90, 30)));
    set(SchemeRole::OnErrorContainer, p.error.tone(pick(30, 90)));

    set(SchemeRole::Background, p.neutral.tone(pick(98, 6)));
    set(SchemeRole::OnBackground, p.neutral.tone(pick(10, 90)));
    set(SchemeRole::Surface, p.neutral.tone(pick(98, 6)));
    set(SchemeRole::SurfaceDim, p.neutral.tone(pick(87, 6)));
    set(SchemeRole::SurfaceBright, p.neutral.tone(pick(98, 24)));
    set(SchemeRole::SurfaceContainerLowest, p.neutral.tone(pick(100, 4)));
    set(SchemeRole::SurfaceContainerLow, p.neutral.tone(pick(96, 10)));
    set(SchemeRole::SurfaceContainer, p.neutral.tone(pick(94, 12)));
    set(SchemeRole::SurfaceContainerHigh, p.neutral.tone(pick(92, 17)));
    set(SchemeRole::SurfaceContainerHighest, p.neutral.tone(pick(90, 22)));
    set(SchemeRole::OnSurface, p.neutral.tone(pick(10, 90)));
    set(SchemeRole::SurfaceVariant, p.neutral_variant.tone(pick(90, 30)));
    set(SchemeRole::OnSurfaceVariant, p.neutral_variant.tone(pick(30, 80)));
    set(SchemeRole::InverseSurface, p.neutral.tone(pick(20, 90)));
    set(SchemeRole::InverseOnSurface, p.neutral.tone(pick(95, 20)));
    set(SchemeRole::Outline, p.neutral_variant.tone(pick(50, 60)));
    set(SchemeRole::OutlineVariant, p.neutral_variant.tone(pick(80, 30)));
    set(SchemeRole::Shadow, p.neutral.tone(0));
    set(SchemeRole::Scrim, p.neutral.tone(0));
    set(SchemeRole::SurfaceTint, p.primary.tone(pick(40, 80)));

    set(SchemeRole::PrimaryFixed, p.primary.tone(90));
    set(SchemeRole::PrimaryFixedDim, p.primary.tone(80));
    set(SchemeRole::OnPrimaryFixed, p.primary.tone(10));
    set(SchemeRole::OnPrimaryFixedVariant, p.primary.tone(30));
    set(SchemeRole::SecondaryFixed, p.secondary.tone(90));
    set(SchemeRole::SecondaryFixedDim, p.secondary.tone(80));
    set(SchemeRole::OnSecondaryFixed, p.secondary.tone(10));
    set(SchemeRole::OnSecondaryFixedVariant, p.secondary.tone(30));
    set(SchemeRole::TertiaryFixed, p.tertiary.tone(90));
    set(SchemeRole::TertiaryFixedDim, p.tertiary.tone(80));
    set(SchemeRole::OnTertiaryFixed, p.tertiary.tone(10));
    set(SchemeRole::OnTertiaryFixedVariant, p.tertiary.tone(30));
    return s;
}

}} // namespace MD3::Dynamic
